#ifndef LIBFRAD_TOOLS_ECC_H
#define LIBFRAD_TOOLS_ECC_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "../../backend/backend.h"

// Encode data with Reed-Solomon ECC
// Parameters: Data, ECC ratio (data_size, parity_size)
// Returns: Encoded data
vec_u8* ecc_encode(const vec_u8* data, uint8_t ratio[2]);

// Decode data and correct errors with Reed-Solomon ECC
// Parameters: Data, ECC ratio (data_size, parity_size), repair flag
// Returns: Decoded data
vec_u8* ecc_decode(const vec_u8* data, uint8_t ratio[2], bool repair);

#endif // LIBFRAD_TOOLS_ECC_H