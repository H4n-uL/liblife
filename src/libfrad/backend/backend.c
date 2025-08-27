#include "backend.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

// vec_u8 implementation
vec_u8* vec_u8_new(size_t capacity) {
    vec_u8* vec = (vec_u8*)malloc(sizeof(vec_u8));
    if (!vec) return NULL;
    vec->capacity = capacity > 0 ? capacity : 16;
    vec->size = 0;
    vec->data = (uint8_t*)malloc(vec->capacity * sizeof(uint8_t));
    if (!vec->data) {
        free(vec);
        return NULL;
    }
    return vec;
}

void vec_u8_free(vec_u8* vec) {
    if (vec) {
        if (vec->data) free(vec->data);
        free(vec);
    }
}

void vec_u8_push(vec_u8* vec, uint8_t value) {
    if (vec->size >= vec->capacity) {
        vec->capacity *= 2;
        uint8_t* new_data = (uint8_t*)realloc(vec->data, vec->capacity * sizeof(uint8_t));
        if (!new_data) return; // Handle allocation failure
        vec->data = new_data;
    }
    vec->data[vec->size++] = value;
}

// vec_f64 implementation
vec_f64* vec_f64_new(size_t capacity) {
    vec_f64* vec = (vec_f64*)malloc(sizeof(vec_f64));
    if (!vec) return NULL;
    vec->capacity = capacity > 0 ? capacity : 16;
    vec->size = 0;
    vec->data = (double*)malloc(vec->capacity * sizeof(double));
    if (!vec->data) {
        free(vec);
        return NULL;
    }
    return vec;
}

void vec_f64_free(vec_f64* vec) {
    if (vec) {
        if (vec->data) free(vec->data);
        free(vec);
    }
}

void vec_f64_push(vec_f64* vec, double value) {
    if (vec->size >= vec->capacity) {
        vec->capacity *= 2;
        double* new_data = (double*)realloc(vec->data, vec->capacity * sizeof(double));
        if (!new_data) return; // Handle allocation failure
        vec->data = new_data;
    }
    vec->data[vec->size++] = value;
}

// vec_i64 functions
vec_i64* vec_i64_new(size_t capacity) {
    vec_i64* vec = (vec_i64*)malloc(sizeof(vec_i64));
    if (!vec) return NULL;

    vec->data = capacity > 0 ? (int64_t*)malloc(capacity * sizeof(int64_t)) : NULL;
    vec->size = 0;
    vec->capacity = capacity;
    return vec;
}

void vec_i64_free(vec_i64* vec) {
    if (vec) {
        free(vec->data);
        free(vec);
    }
}

void vec_i64_push(vec_i64* vec, int64_t value) {
    if (!vec) return;

    if (vec->size >= vec->capacity) {
        size_t new_capacity = vec->capacity == 0 ? 1 : vec->capacity * 2;
        int64_t* new_data = (int64_t*)realloc(vec->data, new_capacity * sizeof(int64_t));
        if (!new_data) return;
        vec->data = new_data;
        vec->capacity = new_capacity;
    }
    vec->data[vec->size++] = value;
}

vec_f64* linspace(double start, double end, size_t num, bool endpoint) {
    vec_f64* vec = vec_f64_new(num);
    if (!vec) return NULL;

    double step = (end - start) / (endpoint ? (num - 1) : num);
    for (size_t i = 0; i < num; i++) {
        vec_f64_push(vec, start + i * step);
    }
    return vec;
}

// Pattern finding in vec_u8 - returns index of first occurrence, -1 if not found
int vec_u8_find_pattern(vec_u8* haystack, const uint8_t* needle, size_t needle_len) {
    if (!haystack || !needle || needle_len == 0 || haystack->size < needle_len) {
        return -1;
    }

    for (size_t i = 0; i <= haystack->size - needle_len; i++) {
        bool match = true;
        for (size_t j = 0; j < needle_len; j++) {
            if (haystack->data[i + j] != needle[j]) {
                match = false;
                break;
            }
        }
        if (match) {
            return (int)i;
        }
    }
    return -1;
}

// Split front - removes and returns first n bytes from vec_u8
vec_u8* vec_u8_split_front(vec_u8* vec, size_t n) {
    if (!vec || n == 0) return vec_u8_new(0);

    // Limit n to actual size
    if (n > vec->size) n = vec->size;

    // Create result with first n bytes
    vec_u8* result = vec_u8_new(n);
    for (size_t i = 0; i < n; i++) {
        vec_u8_push(result, vec->data[i]);
    }

    // Remove first n bytes from original vector
    memmove(vec->data, vec->data + n, vec->size - n);
    vec->size -= n;

    return result;
}

// Split front - removes and returns first n elements from vec_f64
vec_f64* vec_f64_split_front(vec_f64* vec, size_t n) {
    if (!vec || n == 0) return vec_f64_new(0);

    // Limit n to actual size
    if (n > vec->size) n = vec->size;

    // Create result with first n elements
    vec_f64* result = vec_f64_new(n);
    for (size_t i = 0; i < n; i++) {
        vec_f64_push(result, vec->data[i]);
    }

    // Remove first n elements from original vector
    memmove(vec->data, vec->data + n, (vec->size - n) * sizeof(double));
    vec->size -= n;

    return result;
}

// Prepend data from src to dst
void vec_f64_prepend(vec_f64* dst, vec_f64* src) {
    if (!dst || !src || src->size == 0) return;

    // Ensure dst has enough capacity
    size_t new_size = dst->size + src->size;
    if (new_size > dst->capacity) {
        dst->capacity = new_size * 2;
        double* new_data = (double*)realloc(dst->data, dst->capacity * sizeof(double));
        if (!new_data) return;
        dst->data = new_data;
    }

    // Move existing data to make room at the front
    memmove(dst->data + src->size, dst->data, dst->size * sizeof(double));

    // Copy src data to front
    memcpy(dst->data, src->data, src->size * sizeof(double));
    dst->size = new_size;
}

// Generate Hanning window for overlap (Rust-compatible optimized version)
vec_f64* hanning_in_overlap(size_t olap_len) {
    if (olap_len == 0) return vec_f64_new(0);

    vec_f64* window = vec_f64_new(olap_len);

    // First half: reverse of (1.0 - hanning)
    size_t mid_point = ((olap_len + 1) >> 1) + 1;
    size_t first_half_len = olap_len - mid_point + 1;

    // Generate the second half first (for easier reversal)
    double* temp = (double*)malloc(first_half_len * sizeof(double));
    for (size_t i = mid_point; i <= olap_len; i++) {
        double val = 0.5 * (1.0 - cos(M_PI * (double)i / ((double)olap_len + 1.0)));
        temp[i - mid_point] = val;
    }

    // Add reversed (1.0 - x) values for first part
    for (int i = first_half_len - 1; i >= 0; i--) {
        vec_f64_push(window, 1.0 - temp[i]);
    }

    // Add middle value if odd length
    if (olap_len & 1) {
        vec_f64_push(window, 0.5);
    }

    // Add the second half values
    for (size_t i = 0; i < first_half_len; i++) {
        vec_f64_push(window, temp[i]);
    }

    free(temp);
    return window;
}

