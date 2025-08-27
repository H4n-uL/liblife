#include "bitcvt.h"
#include <string.h>

bool* to_bits(const uint8_t* bytes, size_t byte_count, size_t* bit_count) {
    if (!bytes || byte_count == 0) {
        if (bit_count) *bit_count = 0;
        return NULL;
    }

    *bit_count = byte_count * 8;
    bool* bits = (bool*)malloc(*bit_count * sizeof(bool));
    if (!bits) {
        *bit_count = 0;
        return NULL;
    }

    for (size_t i = 0; i < byte_count; i++) {
        uint8_t byte = bytes[i];
        for (size_t j = 0; j < 8; j++) {
            bits[i * 8 + j] = (byte >> (7 - j)) & 1;
        }
    }

    return bits;
}

uint8_t* to_bytes(const bool* bitstream, size_t bit_count, size_t* byte_count) {
    if (!bitstream || bit_count == 0) {
        if (byte_count) *byte_count = 0;
        return NULL;
    }

    // Calculate number of bytes needed (padding with zeros if necessary)
    *byte_count = (bit_count + 7) / 8;
    uint8_t* bytes = (uint8_t*)calloc(*byte_count, sizeof(uint8_t));
    if (!bytes) {
        *byte_count = 0;
        return NULL;
    }

    for (size_t i = 0; i < bit_count; i++) {
        if (bitstream[i]) {
            size_t byte_idx = i / 8;
            size_t bit_idx = 7 - (i % 8);
            bytes[byte_idx] |= (1 << bit_idx);
        }
    }

    return bytes;
}