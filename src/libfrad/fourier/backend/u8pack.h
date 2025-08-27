#ifndef U8PACK_H
#define U8PACK_H

#include <stdint.h>
#include <stdbool.h>
#include "../../backend/backend.h"

vec_u8* u8pack_pack(const vec_f64* input, uint16_t bit_depth, bool little_endian);
vec_f64* u8pack_unpack(const vec_u8* input, uint16_t bit_depth, bool little_endian);

#endif // U8PACK_H
