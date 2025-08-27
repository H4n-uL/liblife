#ifndef LIBFRAD_DECODER_H
#define LIBFRAD_DECODER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "backend/backend.h"
#include "tools/asfh.h"

// Decode result
typedef struct {
    vec_f64* pcm;
    uint16_t channels;
    uint32_t srate;
    size_t frames;
    bool crit;
} decode_result_t;

// Decoder (opaque type)
typedef struct decoder decoder_t;

// Decoder functions
decoder_t* decoder_new(bool fix_error);
void decoder_free(decoder_t* dec);

// Processing
decode_result_t* decoder_process(decoder_t* dec, const uint8_t* stream, size_t stream_len);
decode_result_t* decoder_flush(decoder_t* dec);
void decode_result_free(decode_result_t* result);

// Status
bool decoder_is_empty(decoder_t* dec);
const ASFH* decoder_get_asfh(decoder_t* dec);

#endif