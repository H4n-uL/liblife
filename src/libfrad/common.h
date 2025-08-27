#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <stddef.h>
#include "backend/backend.h"

typedef struct {
    vec_u8* data;
    uint16_t bit_depth_index;
    uint16_t channels;
    uint32_t sample_rate;
} encoded_packet;

extern const uint8_t SIGNATURE[];
extern const uint8_t FRM_SIGN[];

uint16_t crc16_ansi(uint16_t crc, const uint8_t* data, size_t len);
uint32_t frad_crc32(uint32_t crc, const uint8_t* data, size_t len);

#endif // COMMON_H
