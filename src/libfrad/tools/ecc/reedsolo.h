// SPDX-License-Identifier: Unlicense
// Ported from https://github.com/tomerfiliba/reedsolomon
// Copyright - Tomer Filiba

#ifndef LIBFRAD_TOOLS_ECC_REEDSOLO_H
#define LIBFRAD_TOOLS_ECC_REEDSOLO_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// Error codes
typedef enum {
    RS_SUCCESS = 0,
    RS_ERROR_DIVIDE_BY_ZERO,
    RS_ERROR_MESSAGE_TOO_LONG,
    RS_ERROR_TOO_MANY_ERASURES,
    RS_ERROR_TOO_MANY_ERRORS,
    RS_ERROR_LOCATION_FAILURE,
    RS_ERROR_CORRECTION_FAILURE,
    RS_ERROR_MEMORY_ALLOCATION
} RSError;

// Reed-Solomon codec structure
typedef struct {
    size_t data_size;
    size_t parity_size;
    uint8_t fcr;
    uint16_t prim;
    uint8_t generator;
    uint32_t c_exp;
    uint8_t gf_log[256];
    uint8_t gf_exp[512];
    uint8_t* polynomial;
    size_t polynomial_size;
} RSCodec;

// Main API functions
RSCodec* rs_codec_new(size_t data_size, size_t parity_size, uint8_t fcr,
                      uint16_t prim, uint8_t generator, uint32_t c_exp);
RSCodec* rs_codec_new_default(size_t data_size, size_t parity_size);
void rs_codec_free(RSCodec* codec);

// Encode data with Reed-Solomon error correction
// Returns encoded data or NULL on error
// Caller must free the returned buffer
uint8_t* rs_encode(const RSCodec* codec, const uint8_t* data, size_t data_len, size_t* out_len);

// Decode data and correct errors
// Returns decoded data or NULL on error
// Caller must free the returned buffer
uint8_t* rs_decode(const RSCodec* codec, const uint8_t* data, size_t data_len,
                   const size_t* erase_pos, size_t erase_count, size_t* out_len, RSError* error);

#endif // LIBFRAD_TOOLS_ECC_REEDSOLO_H