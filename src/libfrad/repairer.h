#ifndef LIBFRAD_REPAIRER_H
#define LIBFRAD_REPAIRER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "backend/backend.h"
#include "tools/asfh.h"

// Repairer (opaque type)
typedef struct repairer repairer_t;

// Repairer functions
repairer_t* repairer_new(uint8_t data_size, uint8_t check_size);
void repairer_free(repairer_t* rep);

// Processing
vec_u8* repairer_process(repairer_t* rep, const uint8_t* stream, size_t stream_len);
vec_u8* repairer_flush(repairer_t* rep);

// Status
bool repairer_is_empty(repairer_t* rep);
const ASFH* repairer_get_asfh(repairer_t* rep);

#endif