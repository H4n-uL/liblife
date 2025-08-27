#include "profile2.h"
#include "backend/dct_core.h"
#include "tools/p1tools.h"
#include "tools/p2tools.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <zlib.h>

#define TNS_MAX_ORDER 12

// Updated bit depth table for profile 2
const uint16_t PROFILE2_DEPTHS[] = {8, 9, 10, 11, 12, 14, 16, 0};
const size_t PROFILE2_DEPTHS_COUNT = 7;

static double get_scale_factor(uint16_t bit_depth) {
    return pow(2.0, (double)bit_depth - 1.0);
}

encoded_packet* profile2_analogue(const double* pcm, size_t pcm_len, uint16_t bit_depth,
                                 uint16_t channels, uint32_t srate, bool little_endian) {
    (void)pcm; (void)pcm_len; (void)bit_depth;
    (void)channels; (void)srate; (void)little_endian;
    return NULL; // Encoding not fully implemented
}

vec_f64* profile2_digital(const uint8_t* frad, size_t frad_len, uint16_t bit_depth_index,
                         uint16_t channels, uint32_t srate, uint32_t fsize, bool little_endian) {
    (void)srate;
    (void)little_endian; // Not used in profile2

    if (bit_depth_index >= PROFILE2_DEPTHS_COUNT) return NULL;

    uint16_t bit_depth = PROFILE2_DEPTHS[bit_depth_index];
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

    // 2. Split LPC and frequencies
    if (decomp_len < 4) {
        free(decompressed);
        return NULL;
    }

    uint32_t lpc_len = ((uint32_t)decompressed[0] << 24) |
                       ((uint32_t)decompressed[1] << 16) |
                       ((uint32_t)decompressed[2] << 8) |
                       ((uint32_t)decompressed[3]);

    if (lpc_len > decomp_len - 4) {
        free(decompressed);
        return NULL;
    }

    // 3. Exponential Golomb-Rice decoding
    vec_u8* lpc_gol = vec_u8_new(lpc_len);
    for (size_t i = 0; i < lpc_len; i++) {
        vec_u8_push(lpc_gol, decompressed[4 + i]);
    }

    vec_u8* freqs_gol = vec_u8_new(decomp_len - 4 - lpc_len);
    for (size_t i = 0; i < decomp_len - 4 - lpc_len; i++) {
        vec_u8_push(freqs_gol, decompressed[4 + lpc_len + i]);
    }

    free(decompressed);

    // Decode LPC and frequencies
    size_t lpc_count = 0, freqs_count = 0;
    int64_t* lpc_decoded_raw = exp_golomb_decode(lpc_gol, &lpc_count);
    int64_t* freqs_decoded_raw = exp_golomb_decode(freqs_gol, &freqs_count);
    vec_u8_free(lpc_gol);
    vec_u8_free(freqs_gol);

    if (!lpc_decoded_raw || !freqs_decoded_raw) {
        free(lpc_decoded_raw);
        free(freqs_decoded_raw);
        return NULL;
    }

    // Convert to vec_i64
    vec_i64* lpc_decoded = vec_i64_new(lpc_count);
    vec_i64* freqs_decoded = vec_i64_new(freqs_count);
    if (!lpc_decoded || !freqs_decoded) {
        free(lpc_decoded_raw);
        free(freqs_decoded_raw);
        vec_i64_free(lpc_decoded);
        vec_i64_free(freqs_decoded);
        return NULL;
    }

    for (size_t i = 0; i < lpc_count; i++) {
        vec_i64_push(lpc_decoded, lpc_decoded_raw[i]);
    }
    for (size_t i = 0; i < freqs_count; i++) {
        vec_i64_push(freqs_decoded, freqs_decoded_raw[i]);
    }

    free(lpc_decoded_raw);
    free(freqs_decoded_raw);

    // Convert to floating point
    vec_f64* tns_freqs = vec_f64_new(0);
    for (size_t i = 0; i < freqs_decoded->size && i < fsize * channels; i++) {
        double val = (double)freqs_decoded->data[i] / pcm_scale;
        vec_f64_push(tns_freqs, val);
    }
    // Pad with zeros if needed
    while (tns_freqs->size < fsize * channels) {
        vec_f64_push(tns_freqs, 0.0);
    }

    // Prepare LPC coefficients (resize to TNS_MAX_ORDER + 1 per channel)
    vec_i64* lpc = vec_i64_new(0);
    for (size_t i = 0; i < lpc_decoded->size && i < (TNS_MAX_ORDER + 1) * channels; i++) {
        vec_i64_push(lpc, lpc_decoded->data[i]);
    }
    // Pad with zeros if needed
    while (lpc->size < (TNS_MAX_ORDER + 1) * channels) {
        vec_i64_push(lpc, 0);
    }

    vec_i64_free(lpc_decoded);
    vec_i64_free(freqs_decoded);

    // 4. TNS synthesis
    vec_f64* freqs = tns_synthesis(tns_freqs, lpc, channels);
    vec_f64_free(tns_freqs);
    vec_i64_free(lpc);

    if (!freqs) return NULL;

    // 5. Inverse DCT
    vec_f64* pcm = vec_f64_new(0);

    for (uint16_t c = 0; c < channels; c++) {
        // Extract channel frequencies
        vec_f64* freqs_chnl = vec_f64_new(0);
        for (size_t i = c; i < freqs->size; i += channels) {
            vec_f64_push(freqs_chnl, freqs->data[i]);
        }

        // Apply IDCT
        vec_f64* pcm_chnl = idct(freqs_chnl);

        // Store results
        for (size_t i = 0; i < pcm_chnl->size; i++) {
            vec_f64_push(pcm, pcm_chnl->data[i]);
        }

        vec_f64_free(freqs_chnl);
        vec_f64_free(pcm_chnl);
    }

    // Rearrange from channel-by-channel to interleaved format
    vec_f64* interleaved = vec_f64_new(pcm->size);
    size_t samples_per_channel = pcm->size / channels;

    for (size_t i = 0; i < samples_per_channel; i++) {
        for (uint16_t c = 0; c < channels; c++) {
            interleaved->data[i * channels + c] = pcm->data[c * samples_per_channel + i];
        }
    }
    interleaved->size = pcm->size;

    vec_f64_free(freqs);
    vec_f64_free(pcm);

    return interleaved;
}