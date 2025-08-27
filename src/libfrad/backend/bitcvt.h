#ifndef BITCVT_H
#define BITCVT_H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

bool* to_bits(const uint8_t* bytes, size_t byte_count, size_t* bit_count);
uint8_t* to_bytes(const bool* bitstream, size_t bit_count, size_t* byte_count);

#endif