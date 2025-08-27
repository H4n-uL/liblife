#include "profile4.h"
#include "backend/u8pack.h"
#include <stdlib.h>
#include <math.h>

const uint16_t PROFILE4_DEPTHS[] = {12, 16, 24, 32, 48, 64, 0, 0};
const size_t PROFILE4_DEPTHS_COUNT = 6;

encoded_packet* profile4_analogue(const double* pcm, size_t pcm_len, uint16_t bit_depth,
                                 uint16_t channels, uint32_t srate, bool little_endian) {
    if (bit_depth == 0) bit_depth = 16;

    // Find max absolute value
    double max_abs = 0.0;
    for (size_t i = 0; i < pcm_len; i++) {
        double abs_val = fabs(pcm[i]);
        if (abs_val > max_abs) {
            max_abs = abs_val;
        }
    }

    // Find appropriate bit depth index
    int bit_depth_index = -1;
    for (size_t i = 0; i < PROFILE4_DEPTHS_COUNT; i++) {
        if (PROFILE4_DEPTHS[i] >= bit_depth) {
            bit_depth_index = i;
            break;
        }
    }
    if (bit_depth_index == -1) {
        return NULL; // Overflow
    }

    // Create vec_f64 from pcm data
    vec_f64* pcm_vec = vec_f64_new(pcm_len);
    if (!pcm_vec) return NULL;

    for (size_t i = 0; i < pcm_len; i++) {
        vec_f64_push(pcm_vec, pcm[i]);
    }

    // Pack the data
    vec_u8* frad = u8pack_pack(pcm_vec, PROFILE4_DEPTHS[bit_depth_index], little_endian);
    vec_f64_free(pcm_vec);
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

vec_f64* profile4_digital(const uint8_t* frad, size_t frad_len, uint16_t bit_depth_index,
                         uint16_t channels, bool little_endian) {
    (void)channels; // Not used in profile4

    if (bit_depth_index >= PROFILE4_DEPTHS_COUNT) {
        return NULL;
    }

    // Create vec_u8 wrapper for frad data
    vec_u8* frad_vec = vec_u8_new(frad_len);
    if (!frad_vec) return NULL;

    for (size_t i = 0; i < frad_len; i++) {
        vec_u8_push(frad_vec, frad[i]);
    }

    // Unpack the data
    vec_f64* pcm = u8pack_unpack(frad_vec, PROFILE4_DEPTHS[bit_depth_index], little_endian);
    vec_u8_free(frad_vec);

    return pcm;
}