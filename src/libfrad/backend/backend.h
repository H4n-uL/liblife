#ifndef BACKEND_H
#define BACKEND_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// Dynamic vector for uint8_t
typedef struct {
    uint8_t* data;
    size_t size;
    size_t capacity;
} vec_u8;

vec_u8* vec_u8_new(size_t capacity);
void vec_u8_free(vec_u8* vec);
void vec_u8_push(vec_u8* vec, uint8_t value);

// Dynamic vector for double
typedef struct {
    double* data;
    size_t size;
    size_t capacity;
} vec_f64;

vec_f64* vec_f64_new(size_t capacity);
void vec_f64_free(vec_f64* vec);
void vec_f64_push(vec_f64* vec, double value);

// Dynamic vector for int64_t
typedef struct {
    int64_t* data;
    size_t size;
    size_t capacity;
} vec_i64;

vec_i64* vec_i64_new(size_t capacity);
void vec_i64_free(vec_i64* vec);
void vec_i64_push(vec_i64* vec, int64_t value);

vec_f64* linspace(double start, double end, size_t num, bool endpoint);

// Pattern finding in vec_u8
int vec_u8_find_pattern(vec_u8* haystack, const uint8_t* needle, size_t needle_len);

// Split front - removes and returns first n bytes from vec_u8
vec_u8* vec_u8_split_front(vec_u8* vec, size_t n);

// Split front - removes and returns first n elements from vec_f64
vec_f64* vec_f64_split_front(vec_f64* vec, size_t n);

// Prepend data to vec_f64
void vec_f64_prepend(vec_f64* dst, vec_f64* src);

// Hanning window for overlap
vec_f64* hanning_in_overlap(size_t len);

#endif // BACKEND_H
