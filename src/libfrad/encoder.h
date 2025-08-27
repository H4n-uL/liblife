#ifndef LIBFRAD_ENCODER_H
#define LIBFRAD_ENCODER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "backend/backend.h"
#include "../tools/pcmproc.h"
#include "tools/asfh.h"

// Encoder parameters
typedef struct {
    uint8_t profile;
    uint32_t srate;
    uint16_t channels;
    uint16_t bit_depth;
    uint32_t frame_size;
} encoder_params_t;

// Encode result
typedef struct {
    vec_u8* data;
    size_t samples;
} encode_result_t;

// Encoder (opaque type)
typedef struct encoder encoder_t;

// Encoder functions
encoder_t* encoder_new(encoder_params_t* params);
void encoder_free(encoder_t* enc);

// Processing (now takes f64 samples directly)
encode_result_t* encoder_process(encoder_t* enc, const double* samples, size_t sample_count);
encode_result_t* encoder_flush(encoder_t* enc);
void encode_result_free(encode_result_t* result);

// Setters and getters
const char* encoder_set_profile(encoder_t* enc, encoder_params_t* params);
uint8_t encoder_get_profile(encoder_t* enc);
uint16_t encoder_get_channels(encoder_t* enc);
uint32_t encoder_get_srate(encoder_t* enc);
uint32_t encoder_get_frame_size(encoder_t* enc);
uint16_t encoder_get_bit_depth(encoder_t* enc);

// Non-critical setters
void encoder_set_ecc(encoder_t* enc, bool ecc, uint8_t data_size, uint8_t check_size);
void encoder_set_little_endian(encoder_t* enc, bool little_endian);
void encoder_set_loss_level(encoder_t* enc, double loss_level);
void encoder_set_overlap_ratio(encoder_t* enc, uint16_t overlap_ratio);

#endif