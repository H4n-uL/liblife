#ifndef PROFILE0_H
#define PROFILE0_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "../common.h"

// Bit depth table
extern const uint16_t PROFILE0_DEPTHS[];
extern const size_t PROFILE0_DEPTHS_COUNT;

// Profile 0 functions
encoded_packet* profile0_analogue(const double* pcm, size_t pcm_len, uint16_t bit_depth,
                                 uint16_t channels, uint32_t srate, bool little_endian);
vec_f64* profile0_digital(const uint8_t* frad, size_t frad_len, uint16_t bit_depth_index,
                         uint16_t channels, bool little_endian);

#endif