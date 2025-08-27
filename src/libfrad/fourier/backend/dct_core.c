#include "dct_core.h"
#include "pocketfft.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static vec_f64* dct2_core(const vec_f64* x, double fct) {
    size_t n = x->size;
    size_t n2 = 2 * n;

    // Allocate complex array for FFT (interleaved real/imag)
    double* beta = (double*)calloc(n2 * 2, sizeof(double));
    if (!beta) return NULL;

    // Create beta: x concatenated with reversed x
    for (size_t i = 0; i < n; i++) {
        beta[i * 2] = x->data[i];      // real part
        beta[i * 2 + 1] = 0.0;          // imag part
        beta[(n + i) * 2] = x->data[n - 1 - i];  // real part
        beta[(n + i) * 2 + 1] = 0.0;             // imag part
    }

    // Create FFT plan and perform forward FFT
    cfft_plan plan = make_cfft_plan(n2);
    if (!plan) {
        free(beta);
        return NULL;
    }

    cfft_forward(plan, beta, fct);

    // Create output vector
    vec_f64* output = vec_f64_new(n);
    if (!output) {
        destroy_cfft_plan(plan);
        free(beta);
        return NULL;
    }

    // Extract DCT coefficients with phase correction
    for (size_t k = 0; k < n; k++) {
        double phase = -M_PI * k / (2.0 * n);
        double cos_phase = cos(phase);
        double sin_phase = sin(phase);
        double real = beta[k * 2] * cos_phase - beta[k * 2 + 1] * sin_phase;
        vec_f64_push(output, real);
    }

    destroy_cfft_plan(plan);
    free(beta);

    return output;
}

// DCT-III using FFT (matches Rust dct3_core)
static vec_f64* dct3_core(const vec_f64* x, double fct) {
    size_t n = x->size;
    size_t n2 = 2 * n;

    // Allocate complex array for FFT (interleaved real/imag)
    double* beta = (double*)calloc(n2 * 2, sizeof(double));
    if (!beta) return NULL;

    // Create beta with phase-shifted input
    for (size_t i = 0; i < n; i++) {
        double phase = -M_PI * i / (2.0 * n);
        beta[i * 2] = x->data[i] * cos(phase);
        beta[i * 2 + 1] = x->data[i] * sin(phase);
    }

    // Middle element is zero
    beta[n * 2] = 0.0;
    beta[n * 2 + 1] = 0.0;

    // Mirror with conjugation
    for (size_t i = 1; i < n; i++) {
        double phase = -M_PI * i / (2.0 * n);
        beta[(n + i) * 2] = x->data[n - i] * cos(phase);
        beta[(n + i) * 2 + 1] = -x->data[n - i] * sin(phase);  // Conjugate
    }

    // Create FFT plan and perform forward FFT
    cfft_plan plan = make_cfft_plan(n2);
    if (!plan) {
        free(beta);
        return NULL;
    }

    cfft_forward(plan, beta, fct);

    // Create output vector (take real parts)
    vec_f64* output = vec_f64_new(n);
    if (!output) {
        destroy_cfft_plan(plan);
        free(beta);
        return NULL;
    }

    for (size_t k = 0; k < n; k++) {
        vec_f64_push(output, beta[k * 2]);  // real part
    }

    destroy_cfft_plan(plan);
    free(beta);

    return output;
}

// Public DCT function (DCT-II)
vec_f64* dct(const vec_f64* input) {
    if (!input || input->size == 0) return NULL;
    return dct2_core(input, 1.0 / (2.0 * input->size));
}

// Public IDCT function (DCT-III)
vec_f64* idct(const vec_f64* input) {
    if (!input || input->size == 0) return NULL;
    return dct3_core(input, 1.0);
}