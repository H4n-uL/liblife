#include "u8pack.h"
#include "../../backend/bitcvt.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>

static uint16_t f64_to_f16(double value) {
    float f32 = (float)value;
    uint32_t f32_bits;
    memcpy(&f32_bits, &f32, sizeof(uint32_t));

    uint32_t sign = (f32_bits >> 31) & 0x1;
    uint32_t exponent = (f32_bits >> 23) & 0xFF;
    uint32_t mantissa = f32_bits & 0x7FFFFF;

    // Handle special cases
    if (exponent == 0xFF) {
        // Inf or NaN
        uint16_t f16_exp = 0x1F;
        uint16_t f16_mantissa = mantissa ? (mantissa >> 13) | 0x200 : 0;
        return (sign << 15) | (f16_exp << 10) | f16_mantissa;
    }

    // Convert exponent from f32 bias (127) to f16 bias (15)
    int16_t unbiased_exp = exponent - 127;

    if (unbiased_exp < -14) {
        // Too small - becomes zero
        return sign << 15;
    }
    if (unbiased_exp > 15) {
        // Too large - becomes infinity
        return (sign << 15) | (0x1F << 10);
    }

    uint16_t f16_exp = unbiased_exp + 15;
    uint16_t f16_mantissa = mantissa >> 13;

    return (sign << 15) | (f16_exp << 10) | f16_mantissa;
}

static double f16_to_f64(uint16_t f16_bits) {
    uint32_t sign = (f16_bits >> 15) & 0x1;
    uint32_t exponent = (f16_bits >> 10) & 0x1F;
    uint32_t mantissa = f16_bits & 0x3FF;

    // Handle special cases
    if (exponent == 0x1F) {
        // Inf or NaN
        float result = (mantissa == 0) ? INFINITY : NAN;
        return sign ? -result : result;
    }

    if (exponent == 0) {
        if (mantissa == 0) return sign ? -0.0 : 0.0;
        // Denormalized number
        return pow(2, -14) * (mantissa / 1024.0) * (sign ? -1.0 : 1.0);
    }

    // Normal number
    int16_t unbiased_exp = exponent - 15;
    uint32_t f32_exp = unbiased_exp + 127;
    uint32_t f32_mantissa = mantissa << 13;
    uint32_t f32_bits = (sign << 31) | (f32_exp << 23) | f32_mantissa;

    float f32;
    memcpy(&f32, &f32_bits, sizeof(float));
    return (double)f32;
}

// cut_float3s
// Cuts off last bits of floats to make their bit depth to 12, 24, or 48
static vec_u8* cut_float3s(vec_u8* bytes, size_t bits, bool little_endian) {
    size_t size = (bits % 8 == 0) ? bits / 8 : bits;
    size_t skip = little_endian ? size / 3 : 0;

    if (bits % 8 != 0) {
        // Convert to bitstream, cut, convert back
        size_t bit_count;
        bool* bitstream = to_bits(bytes->data, bytes->size, &bit_count);

        // Create result bitstream
        size_t chunk_size = size * 4 / 3;
        size_t result_bit_count = 0;
        bool* cut_bits = (bool*)malloc(bit_count * sizeof(bool));

        for (size_t i = 0; i < bit_count; i += chunk_size) {
            size_t start = i + skip;
            size_t end = (start + size < bit_count) ? start + size : bit_count;
            for (size_t j = start; j < end; j++) {
                cut_bits[result_bit_count++] = bitstream[j];
            }
        }

        size_t byte_count;
        uint8_t* result_bytes = to_bytes(cut_bits, result_bit_count, &byte_count);
        vec_u8* result = vec_u8_new(byte_count);
        for (size_t i = 0; i < byte_count; i++) {
            vec_u8_push(result, result_bytes[i]);
        }

        free(bitstream);
        free(cut_bits);
        free(result_bytes);
        vec_u8_free(bytes);
        return result;
    } else {
        // Work with bytes directly
        vec_u8* result = vec_u8_new(0);
        size_t chunk_size = size * 4 / 3;

        for (size_t i = 0; i < bytes->size; i += chunk_size) {
            size_t start = i + skip;
            size_t end = (start + size < bytes->size) ? start + size : bytes->size;
            for (size_t j = start; j < end; j++) {
                vec_u8_push(result, bytes->data[j]);
            }
        }

        vec_u8_free(bytes);
        return result;
    }
}

// pad_float3s
// Pads floats to make them readable directly as 16, 32, or 64 bit floats
static vec_u8* pad_float3s(vec_u8* bstr, size_t bits, bool little_endian) {
    if (bits % 8 != 0) {
        // Convert to bitstream, pad, convert back
        size_t bit_count;
        bool* bitstream = to_bits(bstr->data, bstr->size, &bit_count);

        size_t pad_bits_count = bits / 3;
        size_t padded_size = (bit_count / bits) * (bits + pad_bits_count);
        bool* padded = (bool*)calloc(padded_size, sizeof(bool));
        size_t padded_idx = 0;

        for (size_t i = 0; i < bit_count; i += bits) {
            if (i + bits > bit_count) break;

            if (!little_endian) {
                // Add original bits then padding
                for (size_t j = i; j < i + bits; j++) {
                    padded[padded_idx++] = bitstream[j];
                }
                for (size_t j = 0; j < pad_bits_count; j++) {
                    padded[padded_idx++] = false;
                }
            } else {
                // Add padding then original bits
                for (size_t j = 0; j < pad_bits_count; j++) {
                    padded[padded_idx++] = false;
                }
                for (size_t j = i; j < i + bits; j++) {
                    padded[padded_idx++] = bitstream[j];
                }
            }
        }

        size_t byte_count;
        uint8_t* result_bytes = to_bytes(padded, padded_idx, &byte_count);
        vec_u8* result = vec_u8_new(byte_count);
        for (size_t i = 0; i < byte_count; i++) {
            vec_u8_push(result, result_bytes[i]);
        }

        free(bitstream);
        free(padded);
        free(result_bytes);
        return result;
    } else {
        // Work with bytes directly
        vec_u8* result = vec_u8_new(0);
        size_t chunk_size = bits / 8;
        size_t pad_bytes = bits / 24;

        for (size_t i = 0; i < bstr->size; i += chunk_size) {
            if (i + chunk_size > bstr->size) break;

            if (!little_endian) {
                // Add original bytes then padding
                for (size_t j = i; j < i + chunk_size; j++) {
                    vec_u8_push(result, bstr->data[j]);
                }
                for (size_t j = 0; j < pad_bytes; j++) {
                    vec_u8_push(result, 0);
                }
            } else {
                // Add padding then original bytes
                for (size_t j = 0; j < pad_bytes; j++) {
                    vec_u8_push(result, 0);
                }
                for (size_t j = i; j < i + chunk_size; j++) {
                    vec_u8_push(result, bstr->data[j]);
                }
            }
        }

        return result;
    }
}

static vec_u8* pack_f16(const vec_f64* input, bool little_endian) {
    vec_u8* bytes = vec_u8_new(input->size * 2);
    if (!bytes) return NULL;

    for (size_t i = 0; i < input->size; i++) {
        uint16_t f16_bits = f64_to_f16(input->data[i]);

        if (!little_endian) {
            vec_u8_push(bytes, (f16_bits >> 8) & 0xFF);
            vec_u8_push(bytes, f16_bits & 0xFF);
        } else {
            vec_u8_push(bytes, f16_bits & 0xFF);
            vec_u8_push(bytes, (f16_bits >> 8) & 0xFF);
        }
    }
    return bytes;
}

static vec_u8* pack_f32(const vec_f64* input, bool little_endian) {
    vec_u8* bytes = vec_u8_new(input->size * 4);
    if (!bytes) return NULL;

    for (size_t i = 0; i < input->size; i++) {
        float val = (float)input->data[i];
        uint32_t bits;
        memcpy(&bits, &val, 4);

        if (!little_endian) {
            for (int j = 3; j >= 0; j--) {
                vec_u8_push(bytes, (bits >> (j * 8)) & 0xFF);
            }
        } else {
            for (int j = 0; j < 4; j++) {
                vec_u8_push(bytes, (bits >> (j * 8)) & 0xFF);
            }
        }
    }
    return bytes;
}

static vec_u8* pack_f64(const vec_f64* input, bool little_endian) {
    vec_u8* bytes = vec_u8_new(input->size * 8);
    if (!bytes) return NULL;

    for (size_t i = 0; i < input->size; i++) {
        uint64_t bits;
        memcpy(&bits, &input->data[i], 8);

        if (!little_endian) {
            for (int j = 7; j >= 0; j--) {
                vec_u8_push(bytes, (bits >> (j * 8)) & 0xFF);
            }
        } else {
            for (int j = 0; j < 8; j++) {
                vec_u8_push(bytes, (bits >> (j * 8)) & 0xFF);
            }
        }
    }
    return bytes;
}

vec_u8* u8pack_pack(const vec_f64* input, uint16_t bits, bool little_endian) {
    size_t bits_size = bits;
    if (bits % 8 != 0) little_endian = false;

    vec_u8* bytes;
    switch (bits) {
        case 12:
        case 16:
            bytes = pack_f16(input, little_endian);
            break;
        case 24:
        case 32:
            bytes = pack_f32(input, little_endian);
            break;
        case 48:
        case 64:
            bytes = pack_f64(input, little_endian);
            break;
        default:
            return NULL; // Invalid bit depth
    }

    if (bits % 3 == 0) {
        return cut_float3s(bytes, bits_size, little_endian);
    }
    return bytes;
}

static vec_f64* unpack_f16(const vec_u8* input, bool little_endian) {
    vec_f64* output = vec_f64_new(input->size / 2);
    if (!output) return NULL;

    for (size_t i = 0; i < input->size; i += 2) {
        uint16_t bits;

        if (!little_endian) {
            bits = (input->data[i] << 8) | input->data[i + 1];
        } else {
            bits = (input->data[i + 1] << 8) | input->data[i];
        }

        vec_f64_push(output, f16_to_f64(bits));
    }
    return output;
}

static vec_f64* unpack_f32(const vec_u8* input, bool little_endian) {
    vec_f64* output = vec_f64_new(input->size / 4);
    if (!output) return NULL;

    for (size_t i = 0; i < input->size; i += 4) {
        uint32_t bits = 0;

        if (!little_endian) {
            for (int j = 0; j < 4; j++) {
                bits = (bits << 8) | input->data[i + j];
            }
        } else {
            for (int j = 3; j >= 0; j--) {
                bits = (bits << 8) | input->data[i + j];
            }
        }

        float val;
        memcpy(&val, &bits, 4);
        vec_f64_push(output, (double)val);
    }
    return output;
}

static vec_f64* unpack_f64(const vec_u8* input, bool little_endian) {
    vec_f64* output = vec_f64_new(input->size / 8);
    if (!output) return NULL;

    for (size_t i = 0; i < input->size; i += 8) {
        uint64_t bits = 0;

        if (!little_endian) {
            for (int j = 0; j < 8; j++) {
                bits = (bits << 8) | input->data[i + j];
            }
        } else {
            for (int j = 7; j >= 0; j--) {
                bits = (bits << 8) | input->data[i + j];
            }
        }

        double val;
        memcpy(&val, &bits, 8);
        vec_f64_push(output, val);
    }
    return output;
}

vec_f64* u8pack_unpack(const vec_u8* input, uint16_t bits, bool little_endian) {
    size_t bits_size = bits;
    if (bits % 8 != 0) little_endian = false;

    // Create a mutable copy if we need to pad
    vec_u8* work_input = NULL;
    const vec_u8* input_to_use = input;

    if (bits % 3 == 0) {
        // Create a copy for padding
        vec_u8* input_copy = vec_u8_new(input->size);
        for (size_t i = 0; i < input->size; i++) {
            vec_u8_push(input_copy, input->data[i]);
        }
        work_input = pad_float3s(input_copy, bits_size, little_endian);
        input_to_use = work_input;
    }

    vec_f64* result;
    switch (bits) {
        case 12:
        case 16:
            result = unpack_f16(input_to_use, little_endian);
            break;
        case 24:
        case 32:
            result = unpack_f32(input_to_use, little_endian);
            break;
        case 48:
        case 64:
            result = unpack_f64(input_to_use, little_endian);
            break;
        default:
            result = NULL; // Invalid bit depth
    }

    if (work_input) {
        vec_u8_free(work_input);
    }

    return result;
}