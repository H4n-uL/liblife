#include "profile0.h"
#include "backend/dct_core.h"
#include "backend/u8pack.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

const uint16_t PROFILE0_DEPTHS[] = {12, 16, 24, 32, 48, 64, 0, 0};
const size_t PROFILE0_DEPTHS_COUNT = 6;

encoded_packet* profile0_analogue(const double* pcm, size_t pcm_len, uint16_t bit_depth,
                                 uint16_t channels, uint32_t srate, bool little_endian) {
    if (bit_depth == 0) bit_depth = 16;

    vec_f64* freqs = vec_f64_new(pcm_len);
    if (!freqs) return NULL;

    // Process each channel separately and interleave results (matching Rust)
    for (uint16_t c = 0; c < channels; c++) {
        // Extract channel data
        vec_f64* pcm_chnl = vec_f64_new(0);
        if (!pcm_chnl) {
            vec_f64_free(freqs);
            return NULL;
        }
        for (size_t i = c; i < pcm_len; i += channels) {
            vec_f64_push(pcm_chnl, pcm[i]);
        }

        // Apply DCT to channel
        vec_f64* dct_result = dct(pcm_chnl);
        vec_f64_free(pcm_chnl);
        if (!dct_result) {
            vec_f64_free(freqs);
            return NULL;
        }

        // Interleave DCT results directly (matching Rust pattern)
        for (size_t i = 0; i < dct_result->size; i++) {
            freqs->data[i * channels + c] = dct_result->data[i];
        }
        vec_f64_free(dct_result);
    }
    freqs->size = pcm_len;

    double max_abs = 0.0;
    for (size_t i = 0; i < freqs->size; i++) {
        double abs_val = fabs(freqs->data[i]);
        if (abs_val > max_abs) {
            max_abs = abs_val;
        }
    }

    int bit_depth_index = -1;
    for (size_t i = 0; i < PROFILE0_DEPTHS_COUNT; i++) {
        if (PROFILE0_DEPTHS[i] >= bit_depth) {
            bit_depth_index = i;
            break;
        }
    }
    if (bit_depth_index == -1) {
        // Overflow
        vec_f64_free(freqs);
        return NULL;
    }

    vec_u8* frad = u8pack_pack(freqs, PROFILE0_DEPTHS[bit_depth_index], little_endian);
    vec_f64_free(freqs);
    if (!frad) return NULL;

    encoded_packet* packet = (encoded_packet*)malloc(sizeof(encoded_packet));
    if (!packet) {
        vec_u8_free(frad);
        return NULL;
    }

    packet->data = frad;
    packet->bit_depth_index = bit_depth_index;
    packet->channels = channels;
    packet->sample_rate = srate;

    return packet;
}

vec_f64* profile0_digital(const uint8_t* frad, size_t frad_len, uint16_t bit_depth_index,
                         uint16_t channels, bool little_endian) {
    if (bit_depth_index >= PROFILE0_DEPTHS_COUNT) {
        return NULL;
    }

    // Create vec_u8 wrapper for frad data
    vec_u8* frad_vec = vec_u8_new(frad_len);
    if (!frad_vec) return NULL;

    for (size_t i = 0; i < frad_len; i++) {
        vec_u8_push(frad_vec, frad[i]);
    }

    // Unpack the data
    vec_f64* freqs = u8pack_unpack(frad_vec, PROFILE0_DEPTHS[bit_depth_index], little_endian);
    vec_u8_free(frad_vec);
    if (!freqs) return NULL;

    // Create output PCM vector
    vec_f64* pcm = vec_f64_new(freqs->size);
    if (!pcm) {
        vec_f64_free(freqs);
        return NULL;
    }

    // Process each channel with IDCT and interleave directly (matching Rust)
    for (uint16_t c = 0; c < channels; c++) {
        // Extract channel frequencies
        vec_f64* freqs_chnl = vec_f64_new(0);
        if (!freqs_chnl) {
            vec_f64_free(freqs);
            vec_f64_free(pcm);
            return NULL;
        }

        for (size_t i = c; i < freqs->size; i += channels) {
            vec_f64_push(freqs_chnl, freqs->data[i]);
        }

        // Apply IDCT to channel
        vec_f64* idct_result = idct(freqs_chnl);
        vec_f64_free(freqs_chnl);
        if (!idct_result) {
            vec_f64_free(freqs);
            vec_f64_free(pcm);
            return NULL;
        }

        // Interleave IDCT results directly (matching Rust pattern)
        for (size_t i = 0; i < idct_result->size; i++) {
            pcm->data[i * channels + c] = idct_result->data[i];
        }
        vec_f64_free(idct_result);
    }
    pcm->size = freqs->size;

    vec_f64_free(freqs);

    return pcm;
}