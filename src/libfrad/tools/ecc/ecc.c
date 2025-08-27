#include "ecc.h"
#include "reedsolo.h"
#include <stdlib.h>
#include <string.h>

// Encode data with Reed-Solomon ECC
vec_u8* ecc_encode(const vec_u8* data, uint8_t ratio[2]) {
    if (!data || !ratio) return NULL;

    size_t data_size = ratio[0];
    size_t parity_size = ratio[1];

    // Create RS codec
    RSCodec* rs = rs_codec_new_default(data_size, parity_size);
    if (!rs) return NULL;

    // Calculate result size and create result vector
    vec_u8* result = vec_u8_new(0);
    if (!result) {
        rs_codec_free(rs);
        return NULL;
    }

    // Process data in chunks
    for (size_t i = 0; i < data->size; i += data_size) {
        // Determine chunk size
        size_t chunk_size = (i + data_size > data->size) ? (data->size - i) : data_size;

        // Encode the chunk
        size_t encoded_len;
        uint8_t* encoded = rs_encode(rs, data->data + i, chunk_size, &encoded_len);

        if (!encoded) {
            vec_u8_free(result);
            rs_codec_free(rs);
            return NULL;
        }

        // Append encoded chunk to result
        for (size_t j = 0; j < encoded_len; j++) {
            vec_u8_push(result, encoded[j]);
        }

        free(encoded);
    }

    rs_codec_free(rs);
    return result;
}

// Decode data and correct errors with Reed-Solomon ECC
vec_u8* ecc_decode(const vec_u8* data, uint8_t ratio[2], bool repair) {
    if (!data || !ratio) return NULL;

    size_t data_size = ratio[0];
    size_t parity_size = ratio[1];
    size_t block_size = data_size + parity_size;

    // Create RS codec
    RSCodec* rs = rs_codec_new_default(data_size, parity_size);
    if (!rs) return NULL;

    vec_u8* result = vec_u8_new(data->size);
    if (!result) {
        rs_codec_free(rs);
        return NULL;
    }

    // Process each block
    for (size_t i = 0; i < data->size; i += block_size) {
        size_t chunk_size = (i + block_size > data->size) ? (data->size - i) : block_size;

        if (repair) {
            if (chunk_size == block_size) {
                // Full block - try to decode and repair
                RSError error;
                size_t decoded_len;
                uint8_t* decoded = rs_decode(rs, data->data + i, chunk_size, NULL, 0, &decoded_len, &error);

                if (decoded && error == RS_SUCCESS) {
                    // Copy decoded data
                    for (size_t j = 0; j < decoded_len; j++) {
                        vec_u8_push(result, decoded[j]);
                    }
                    free(decoded);
                } else {
                    // If decoding fails, fill with zeros
                    for (size_t j = 0; j < data_size; j++) {
                        vec_u8_push(result, 0);
                    }
                    if (decoded) free(decoded);
                }
            } else {
                // Incomplete block - can't repair, just strip parity if present
                size_t copy_len = (chunk_size >= parity_size) ? (chunk_size - parity_size) : chunk_size;
                for (size_t j = 0; j < copy_len; j++) {
                    vec_u8_push(result, data->data[i + j]);
                }
            }
        } else {
            // Just strip parity bytes without repair
            // For incomplete blocks, copy all data except parity bytes
            size_t copy_len;
            if (chunk_size >= parity_size) {
                // Remove parity_size bytes from the chunk
                copy_len = chunk_size - parity_size;
            } else {
                // Chunk is smaller than parity size, copy all
                copy_len = chunk_size;
            }

            for (size_t j = 0; j < copy_len; j++) {
                vec_u8_push(result, data->data[i + j]);
            }
        }
    }

    rs_codec_free(rs);
    return result;
}