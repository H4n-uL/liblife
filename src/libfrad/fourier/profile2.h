#ifndef PROFILE2_H
#define PROFILE2_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "../common.h"

// Bit depth table
extern const uint16_t PROFILE2_DEPTHS[];
extern const size_t PROFILE2_DEPTHS_COUNT;

// Profile 2 functions (TNS - Temporal Noise Shaping)
encoded_packet* profile2_analogue(const double* pcm, size_t pcm_len, uint16_t bit_depth,
                                 uint16_t channels, uint32_t srate, bool little_endian);
vec_f64* profile2_digital(const uint8_t* frad, size_t frad_len, uint16_t bit_depth_index,
                         uint16_t channels, uint32_t srate, uint32_t fsize, bool little_endian);

#endif