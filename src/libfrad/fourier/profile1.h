#ifndef PROFILE1_H
#define PROFILE1_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "../common.h"

// Bit depth table
extern const uint16_t PROFILE1_DEPTHS[];
extern const size_t PROFILE1_DEPTHS_COUNT;

// Profile 1 functions (lossy with psychoacoustic masking)
encoded_packet* profile1_analogue(const double* pcm, size_t pcm_len, uint16_t bit_depth,
                                 uint16_t channels, uint32_t srate, double loss_level, bool little_endian);
vec_f64* profile1_digital(const uint8_t* frad, size_t frad_len, uint16_t bit_depth_index,
                         uint16_t channels, uint32_t srate, uint32_t fsize, bool little_endian);

#endif