#include "p1tools.h"
#include "../../backend/backend.h"
#include <math.h>
#include <stdlib.h>

const double SPREAD_ALPHA_P1 = 0.8;
const double QUANT_ALPHA = 0.75;
const int MOSLEN_P1 = 27;
const uint32_t MODIFIED_OPUS_SUBBANDS[] = {
    0,     200,   400,   600,   800,   1000,  1200,  1400,
    1600,  2000,  2400,  2800,  3200,  4000,  4800,  5600,
    6800,  8000,  9600,  12000, 15600, 20000, 24000, 28800,
    34400, 40800, 48000, 0xFFFFFFFF
};

static void get_bin_range(size_t len, uint32_t srate, int i, size_t* start, size_t* end) {
    *start = (size_t)round((double)MODIFIED_OPUS_SUBBANDS[i] / (srate / 2.0) * len);
    *end = (size_t)round((double)MODIFIED_OPUS_SUBBANDS[i + 1] / (srate / 2.0) * len);
    if (*start > len) *start = len;
    if (*end > len) *end = len;
}

vec_f64* mask_thres_mos(const vec_f64* freqs, uint32_t srate, double loss_level, double alpha) {
    vec_f64* thres = vec_f64_new(MOSLEN_P1);
    if (!thres) return NULL;

    for (int i = 0; i < MOSLEN_P1; i++) {
        size_t start, end;
        get_bin_range(freqs->size, srate, i, &start, &end);
        if (start >= end) {
            vec_f64_push(thres, 0.0);
            continue;
        }

        double sum_sq = 0.0;
        for (size_t j = start; j < end; j++) {
            sum_sq += freqs->data[j] * freqs->data[j];
        }
        double rms = sqrt(sum_sq / (end - start));
        double f = (MODIFIED_OPUS_SUBBANDS[i] + MODIFIED_OPUS_SUBBANDS[i + 1]) / 2.0;
        double ath = pow(10.0, (3.64 * pow(f / 1000.0, -0.8) - 6.5 * exp(-0.6 * pow(f / 1000.0 - 3.3, 2)) + 1e-3 * pow(f / 1000.0, 4)) / 20.0);
        double sfq = pow(rms, alpha);
        vec_f64_push(thres, fmax(sfq, fmin(ath, 1.0)) * loss_level);
    }
    return thres;
}

vec_f64* mapping_from_opus(const vec_f64* thres, size_t freq_len, uint32_t srate) {
    vec_f64* output = vec_f64_new(freq_len);
    if (!output) return NULL;

    for (int i = 0; i < MOSLEN_P1 - 1; i++) {
        size_t start, end;
        get_bin_range(freq_len, srate, i, &start, &end);
        size_t num = end - start;
        if (num == 0) continue;

        vec_f64* spaced = linspace(thres->data[i], thres->data[i + 1], num, false);
        if (!spaced) {
            vec_f64_free(output);
            return NULL;
        }
        for (size_t j = 0; j < num; j++) {
            if (start + j < freq_len) {
                output->data[start + j] = spaced->data[j];
            }
        }
        output->size = freq_len;
        vec_f64_free(spaced);
    }
    return output;
}

int64_t quant(double x) { return (int64_t)(x > 0 ? 1 : -1) * pow(fabs(x), QUANT_ALPHA); }
double dequant(double y) { return (y > 0 ? 1 : -1) * pow(fabs(y), 1.0 / QUANT_ALPHA); }

// Exponential Golomb encoding
vec_u8* exp_golomb_encode(const int64_t* data, size_t len) {
    if (!data || len == 0) {
        vec_u8* result = vec_u8_new(1);
        if (result) vec_u8_push(result, 0);
        return result;
    }

    // Find maximum absolute value to determine k
    int64_t dmax = 0;
    for (size_t i = 0; i < len; i++) {
        int64_t abs_val = data[i] < 0 ? -data[i] : data[i];
        if (abs_val > dmax) dmax = abs_val;
    }

    uint8_t k = 0;
    if (dmax > 0) {
        // Use Rust's formula: ceil(log2(dmax))
        k = (uint8_t)ceil(log2((double)dmax));
    }

    // Calculate required size
    size_t total_bits = 8; // for k parameter
    for (size_t i = 0; i < len; i++) {
        int64_t n = data[i];
        int64_t x = (n > 0 ? (n << 1) - 1 : (-n) << 1) + (1LL << k);
        int bits_needed = 0;
        int64_t temp = x;
        while (temp > 0) {
            bits_needed++;
            temp >>= 1;
        }
        if (bits_needed == 0) bits_needed = 1;
        total_bits += (bits_needed << 1) - (k + 1);
    }

    vec_u8* encoded = vec_u8_new((total_bits + 7) / 8);
    if (!encoded) return NULL;

    // Initialize with zeros
    for (size_t i = 0; i < (total_bits + 7) / 8; i++) {
        vec_u8_push(encoded, 0);
    }

    // Store k parameter
    encoded->data[0] = k;

    // Encode each value
    size_t bit_pos = 8;
    for (size_t i = 0; i < len; i++) {
        int64_t n = data[i];
        int64_t x = (n > 0 ? (n << 1) - 1 : (-n) << 1) + (1LL << k);

        // Calculate number of bits
        int x_bits = 0;
        int64_t temp = x;
        while (temp > 0) {
            x_bits++;
            temp >>= 1;
        }
        if (x_bits == 0) x_bits = 1;

        int total_bits = (x_bits << 1) - (k + 1);

        // Write bits
        for (int j = 0; j < total_bits; j++) {
            size_t byte_idx = bit_pos / 8;
            size_t bit_idx = 7 - (bit_pos % 8);

            if (x & (1LL << (total_bits - 1 - j))) {
                encoded->data[byte_idx] |= (1 << bit_idx);
            }
            bit_pos++;
        }
    }

    return encoded;
}

// Exponential Golomb decoding
int64_t* exp_golomb_decode(const vec_u8* data, size_t* out_len) {
    if (!data || !out_len || data->size == 0) {
        if (out_len) *out_len = 0;
        return NULL;
    }

    uint8_t k = data->data[0];
    int64_t kx = 1LL << k;

    // Convert to bit array
    size_t bit_len = (data->size - 1) * 8;
    uint8_t* bits = calloc(bit_len, sizeof(uint8_t));
    if (!bits) {
        *out_len = 0;
        return NULL;
    }

    // Convert bytes to bits
    for (size_t i = 1; i < data->size; i++) {
        uint8_t byte = data->data[i];
        for (int j = 0; j < 8; j++) {
            bits[(i - 1) * 8 + j] = (byte >> (7 - j)) & 1;
        }
    }

    // Decode values
    size_t capacity = 256;
    int64_t* decoded = malloc(capacity * sizeof(int64_t));
    if (!decoded) {
        free(bits);
        *out_len = 0;
        return NULL;
    }

    size_t decoded_count = 0;
    size_t idx = 0;

    while (idx < bit_len) {
        // Find first 1 bit (count leading zeros)
        size_t m = 0;
        while (idx + m < bit_len && bits[idx + m] == 0) {
            m++;
        }

        if (idx + m >= bit_len) break;

        size_t cwlen = (m * 2) + k + 1;
        if (idx + cwlen > bit_len) break;

        // Read the value
        int64_t n = 0;
        for (size_t i = idx + m; i < idx + cwlen && i < bit_len; i++) {
            n = (n << 1) | bits[i];
        }
        n -= kx;

        // Decode the sign
        int64_t value;
        if (n & 1) {
            value = (n + 1) >> 1;
        } else {
            value = -(n >> 1);
        }

        // Grow array if needed
        if (decoded_count >= capacity) {
            capacity *= 2;
            int64_t* new_decoded = realloc(decoded, capacity * sizeof(int64_t));
            if (!new_decoded) {
                free(decoded);
                free(bits);
                *out_len = 0;
                return NULL;
            }
            decoded = new_decoded;
        }

        decoded[decoded_count++] = value;
        idx += cwlen;
    }

    free(bits);
    *out_len = decoded_count;
    return decoded;
}