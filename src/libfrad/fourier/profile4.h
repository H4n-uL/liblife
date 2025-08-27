#ifndef PROFILE4_H
#define PROFILE4_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "../common.h"

// Bit depth table
extern const uint16_t PROFILE4_DEPTHS[];
extern const size_t PROFILE4_DEPTHS_COUNT;

// Profile 4 functions (lossless, no transform)
encoded_packet* profile4_analogue(const double* pcm, size_t pcm_len, uint16_t bit_depth,
                                 uint16_t channels, uint32_t srate, bool little_endian);
vec_f64* profile4_digital(const uint8_t* frad, size_t frad_len, uint16_t bit_depth_index,
                         uint16_t channels, bool little_endian);

#endif