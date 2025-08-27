#include "profile1.h"
#include "backend/dct_core.h"
#include "tools/p1tools.h"
#include "compact.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <zlib.h>

const uint16_t PROFILE1_DEPTHS[] = {8, 12, 16, 24, 32, 48, 64, 0};
const size_t PROFILE1_DEPTHS_COUNT = 7;

static double get_scale_factor(uint16_t bit_depth) {
    return pow(2.0, (double)bit_depth - 1.0);
}

encoded_packet* profile1_analogue(const double* pcm, size_t pcm_len, uint16_t bit_depth,
                                 uint16_t channels, uint32_t srate, double loss_level, bool little_endian) {
    (void)little_endian; // Not used in encoding

    if (bit_depth == 0) bit_depth = 16;
    double pcm_scale = get_scale_factor(bit_depth);
    loss_level = fmax(fabs(loss_level), 0.125);
    srate = get_valid_srate(srate);

    // 1. Pad PCM to nearest valid sample count
    size_t samples_per_channel = pcm_len / channels;
    size_t padded_samples = get_samples_min_ge(samples_per_channel);
    size_t padded_len = padded_samples * channels;

    vec_f64* pcm_vec = vec_f64_new(padded_len);
    if (!pcm_vec) return NULL;

    // Copy original PCM and pad with zeros
    for (size_t i = 0; i < pcm_len; i++) {
        vec_f64_push(pcm_vec, pcm[i]);
    }
    for (size_t i = pcm_len; i < padded_len; i++) {
        vec_f64_push(pcm_vec, 0.0);
    }

    // Prepare arrays for all channels
    int64_t* freqs_masked_all = (int64_t*)calloc(padded_len, sizeof(int64_t));
    int64_t* thres_all = (int64_t*)calloc(MOSLEN * channels, sizeof(int64_t));

    if (!freqs_masked_all || !thres_all) {
        vec_f64_free(pcm_vec);
        free(freqs_masked_all);
        free(thres_all);
        return NULL;
    }

    // 2. Process each channel separately
    for (size_t c = 0; c < channels; c++) {
        // Extract channel data
        vec_f64* pcm_chnl = vec_f64_new(0);
        for (size_t i = c; i < padded_len; i += channels) {
            vec_f64_push(pcm_chnl, pcm_vec->data[i]);
        }

        // 2.1 DCT
        vec_f64* freqs_chnl = dct(pcm_chnl);
        vec_f64_free(pcm_chnl);
        if (!freqs_chnl) {
            vec_f64_free(pcm_vec);
            free(freqs_masked_all);
            free(thres_all);
            return NULL;
        }

        // Scale frequencies for masking calculation
        vec_f64* freqs_scaled = vec_f64_new(freqs_chnl->size);
        for (size_t i = 0; i < freqs_chnl->size; i++) {
            vec_f64_push(freqs_scaled, freqs_chnl->data[i] * pcm_scale);
        }

        // 2.2 Calculate masking threshold
        vec_f64* thres_chnl = mask_thres_mos(freqs_scaled, srate, loss_level, SPREAD_ALPHA);
        vec_f64_free(freqs_scaled);
        if (!thres_chnl) {
            vec_f64_free(pcm_vec);
            vec_f64_free(freqs_chnl);
            free(freqs_masked_all);
            free(thres_all);
            return NULL;
        }

        // 2.3 Remap thresholds to DCT bins
        vec_f64* div_factor = mapping_from_opus(thres_chnl, freqs_chnl->size, srate);
        if (!div_factor) {
            vec_f64_free(pcm_vec);
            vec_f64_free(freqs_chnl);
            vec_f64_free(thres_chnl);
            free(freqs_masked_all);
            free(thres_all);
            return NULL;
        }

        // Replace zeros with infinity
        for (size_t i = 0; i < div_factor->size; i++) {
            if (div_factor->data[i] == 0.0) {
                div_factor->data[i] = INFINITY;
            }
        }

        // 2.4 Apply psychoacoustic masking
        vec_f64* freqs_masked_chnl = vec_f64_new(freqs_chnl->size);
        for (size_t i = 0; i < freqs_chnl->size; i++) {
            vec_f64_push(freqs_masked_chnl, freqs_chnl->data[i] / div_factor->data[i]);
        }
        vec_f64_free(freqs_chnl);
        vec_f64_free(div_factor);

        // 2.5 Quantization
        for (size_t i = 0; i < freqs_masked_chnl->size; i++) {
            freqs_masked_all[i * channels + c] = (int64_t)round(quant(freqs_masked_chnl->data[i] * pcm_scale));
        }
        vec_f64_free(freqs_masked_chnl);

        // Store thresholds
        for (size_t i = 0; i < MOSLEN && i < thres_chnl->size; i++) {
            double val = fmax(1.0, thres_chnl->data[i]);
            thres_all[i * channels + c] = (int64_t)round(dequant(log(val) / log(M_E / 2.0)));
        }
        vec_f64_free(thres_chnl);
    }

    vec_f64_free(pcm_vec);

    // 3. Exponential Golomb-Rice encoding
    vec_u8* freqs_gol = exp_golomb_encode(freqs_masked_all, padded_len);
    vec_u8* thres_gol = exp_golomb_encode(thres_all, MOSLEN * channels);

    free(freqs_masked_all);
    free(thres_all);

    if (!freqs_gol || !thres_gol) {
        vec_u8_free(freqs_gol);
        vec_u8_free(thres_gol);
        return NULL;
    }

    // 4. Combine data: [Thresholds length in u32be | Thresholds | Frequencies]
    vec_u8* combined = vec_u8_new(4 + thres_gol->size + freqs_gol->size);
    if (!combined) {
        vec_u8_free(freqs_gol);
        vec_u8_free(thres_gol);
        return NULL;
    }

    // Add threshold length as big-endian u32
    uint32_t thres_len = thres_gol->size;
    vec_u8_push(combined, (thres_len >> 24) & 0xFF);
    vec_u8_push(combined, (thres_len >> 16) & 0xFF);
    vec_u8_push(combined, (thres_len >> 8) & 0xFF);
    vec_u8_push(combined, thres_len & 0xFF);

    // Add thresholds
    for (size_t i = 0; i < thres_gol->size; i++) {
        vec_u8_push(combined, thres_gol->data[i]);
    }

    // Add frequencies
    for (size_t i = 0; i < freqs_gol->size; i++) {
        vec_u8_push(combined, freqs_gol->data[i]);
    }

    vec_u8_free(freqs_gol);
    vec_u8_free(thres_gol);

    // 5. Raw Deflate compression (no zlib header)
    z_stream strm;
    memset(&strm, 0, sizeof(strm));

    // Use deflateInit2 with -15 for raw deflate (no header)
    if (deflateInit2(&strm, Z_BEST_COMPRESSION, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
        vec_u8_free(combined);
        return NULL;
    }

    uLongf dest_len = deflateBound(&strm, combined->size);
    vec_u8* frad = vec_u8_new(dest_len);
    if (!frad) {
        deflateEnd(&strm);
        vec_u8_free(combined);
        return NULL;
    }

    strm.avail_in = combined->size;
    strm.next_in = combined->data;
    strm.avail_out = dest_len;
    strm.next_out = frad->data;

    int res = deflate(&strm, Z_FINISH);
    if (res != Z_STREAM_END) {
        deflateEnd(&strm);
        vec_u8_free(frad);
        vec_u8_free(combined);
        return NULL;
    }

    frad->size = strm.total_out;
    deflateEnd(&strm);
    vec_u8_free(combined);

    encoded_packet* packet = (encoded_packet*)malloc(sizeof(encoded_packet));
    if (!packet) {
        vec_u8_free(frad);
        return NULL;
    }

    packet->data = frad;

    // Find the correct bit depth index
    packet->bit_depth_index = 0;
    for (size_t i = 0; i < PROFILE1_DEPTHS_COUNT; i++) {
        if (PROFILE1_DEPTHS[i] == bit_depth) {
            packet->bit_depth_index = i;
            break;
        }
    }

    packet->channels = channels;
    packet->sample_rate = srate;

    return packet;
}

vec_f64* profile1_digital(const uint8_t* frad, size_t frad_len, uint16_t bit_depth_index,
                         uint16_t channels, uint32_t srate, uint32_t fsize, bool little_endian) {
    (void)little_endian; // Not used in profile1

    if (bit_depth_index >= PROFILE1_DEPTHS_COUNT) return NULL;

    uint16_t bit_depth = PROFILE1_DEPTHS[bit_depth_index];
    double pcm_scale = get_scale_factor(bit_depth);

    // 1. Raw Deflate decompression (no zlib header)
    z_stream strm;
    memset(&strm, 0, sizeof(strm));

    if (inflateInit2(&strm, -15) != Z_OK) {
        // Return silent frame on decompression error
        vec_f64* pcm = vec_f64_new(fsize * channels);
        if (pcm) {
            for (size_t i = 0; i < fsize * channels; i++) {
                vec_f64_push(pcm, 0.0);
            }
        }
        return pcm;
    }

    // Start with a reasonable initial size
    size_t decomp_capacity = frad_len * 4; // Typical compression ratio
    uint8_t* decompressed = (uint8_t*)malloc(decomp_capacity);
    if (!decompressed) {
        inflateEnd(&strm);
        return NULL;
    }

    strm.avail_in = frad_len;
    strm.next_in = (Bytef*)frad;
    strm.avail_out = decomp_capacity;
    strm.next_out = decompressed;

    int result;
    while ((result = inflate(&strm, Z_NO_FLUSH)) == Z_OK) {
        if (strm.avail_out == 0) {
            // Need more output space
            size_t used = decomp_capacity;
            decomp_capacity *= 2;
            uint8_t* new_buf = (uint8_t*)realloc(decompressed, decomp_capacity);
            if (!new_buf) {
                free(decompressed);
                inflateEnd(&strm);
                return NULL;
            }
            decompressed = new_buf;
            strm.avail_out = decomp_capacity - used;
            strm.next_out = decompressed + used;
        }
    }

    if (result != Z_STREAM_END) {
        inflateEnd(&strm);
        free(decompressed);
        // Return silent frame on decompression error
        vec_f64* pcm = vec_f64_new(fsize * channels);
        if (pcm) {
            for (size_t i = 0; i < fsize * channels; i++) {
                vec_f64_push(pcm, 0.0);
            }
        }
        return pcm;
    }

    unsigned long decomp_len = strm.total_out;
    inflateEnd(&strm);

    // 2. Split thresholds and frequencies
    if (decomp_len < 4) {
        free(decompressed);
        return NULL;
    }

    uint32_t thres_len = ((uint32_t)decompressed[0] << 24) |
                         ((uint32_t)decompressed[1] << 16) |
                         ((uint32_t)decompressed[2] << 8) |
                         ((uint32_t)decompressed[3]);

    if (thres_len > decomp_len - 4) {
        free(decompressed);
        return NULL;
    }

    // 3. Exponential Golomb-Rice decoding
    vec_u8* thres_gol = vec_u8_new(thres_len);
    for (size_t i = 0; i < thres_len; i++) {
        vec_u8_push(thres_gol, decompressed[4 + i]);
    }

    vec_u8* freqs_gol = vec_u8_new(decomp_len - 4 - thres_len);
    for (size_t i = 0; i < decomp_len - 4 - thres_len; i++) {
        vec_u8_push(freqs_gol, decompressed[4 + thres_len + i]);
    }

    free(decompressed);

    // Decode thresholds and frequencies
    size_t thres_count = 0, freqs_count = 0;
    int64_t* thres_decoded_raw = exp_golomb_decode(thres_gol, &thres_count);
    int64_t* freqs_decoded_raw = exp_golomb_decode(freqs_gol, &freqs_count);
    vec_u8_free(thres_gol);
    vec_u8_free(freqs_gol);

    if (!thres_decoded_raw || !freqs_decoded_raw) {
        free(thres_decoded_raw);
        free(freqs_decoded_raw);
        return NULL;
    }

    // Convert to vec_i64
    vec_i64* thres_decoded = vec_i64_new(thres_count);
    vec_i64* freqs_decoded = vec_i64_new(freqs_count);
    if (!thres_decoded || !freqs_decoded) {
        free(thres_decoded_raw);
        free(freqs_decoded_raw);
        vec_i64_free(thres_decoded);
        vec_i64_free(freqs_decoded);
        return NULL;
    }

    for (size_t i = 0; i < thres_count; i++) {
        vec_i64_push(thres_decoded, thres_decoded_raw[i]);
    }
    for (size_t i = 0; i < freqs_count; i++) {
        vec_i64_push(freqs_decoded, freqs_decoded_raw[i]);
    }

    free(thres_decoded_raw);
    free(freqs_decoded_raw);

    // Convert to floating point and dequantize
    vec_f64* freqs_masked = vec_f64_new(0);
    for (size_t i = 0; i < freqs_decoded->size && i < fsize * channels; i++) {
        double val = dequant((double)freqs_decoded->data[i]) / pcm_scale;
        vec_f64_push(freqs_masked, val);
    }
    // Pad with zeros if needed
    while (freqs_masked->size < fsize * channels) {
        vec_f64_push(freqs_masked, 0.0);
    }

    vec_f64* thres = vec_f64_new(0);
    for (size_t i = 0; i < thres_decoded->size && i < MOSLEN * channels; i++) {
        double val = pow(M_E / 2.0, quant((double)thres_decoded->data[i]));
        vec_f64_push(thres, val);
    }
    // Pad with zeros if needed
    while (thres->size < MOSLEN * channels) {
        vec_f64_push(thres, 0.0);
    }

    vec_i64_free(thres_decoded);
    vec_i64_free(freqs_decoded);

    // 4. Dequantisation and inverse masking
    vec_f64* pcm = vec_f64_new(fsize * channels);

    for (uint16_t c = 0; c < channels; c++) {
        // Extract channel data
        vec_f64* freqs_masked_chnl = vec_f64_new(0);
        vec_f64* thres_chnl = vec_f64_new(0);

        for (size_t i = c; i < fsize * channels; i += channels) {
            vec_f64_push(freqs_masked_chnl, freqs_masked->data[i]);
        }

        for (size_t i = c; i < MOSLEN * channels; i += channels) {
            vec_f64_push(thres_chnl, thres->data[i]);
        }

        // 4.1. Inverse masking
        vec_f64* mapping = mapping_from_opus(thres_chnl, fsize, srate);
        vec_f64* freqs_chnl = vec_f64_new(freqs_masked_chnl->size);

        for (size_t i = 0; i < freqs_masked_chnl->size && i < mapping->size; i++) {
            vec_f64_push(freqs_chnl, freqs_masked_chnl->data[i] * mapping->data[i]);
        }

        // 4.2. Inverse DCT
        vec_f64* pcm_chnl = idct(freqs_chnl);

        // Store results directly in interleaved format
        for (size_t i = 0; i < pcm_chnl->size && i < fsize; i++) {
            pcm->data[i * channels + c] = pcm_chnl->data[i];
        }

        vec_f64_free(freqs_masked_chnl);
        vec_f64_free(thres_chnl);
        vec_f64_free(mapping);
        vec_f64_free(freqs_chnl);
        vec_f64_free(pcm_chnl);
    }

    pcm->size = fsize * channels;

    vec_f64_free(freqs_masked);
    vec_f64_free(thres);

    return pcm;
}