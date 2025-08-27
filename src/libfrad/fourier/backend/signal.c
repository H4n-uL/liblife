#include "signal.h"
#include "../../backend/backend.h"
#include "pocketfft.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

vec_f64* impulse_filt(const vec_f64* b, const vec_f64* a, const vec_f64* input) {
    vec_f64* output = vec_f64_new(input->size);
    if (!output) return NULL;

    vec_f64* x_hist = vec_f64_new(b->size);
    vec_f64* y_hist = vec_f64_new(a->size - 1);
    if (!x_hist || !y_hist) {
        vec_f64_free(output);
        vec_f64_free(x_hist);
        vec_f64_free(y_hist);
        return NULL;
    }

    // Initialize history buffers with zeros
    for (size_t i = 0; i < b->size; i++) {
        vec_f64_push(x_hist, 0.0);
    }
    for (size_t i = 0; i < a->size - 1; i++) {
        vec_f64_push(y_hist, 0.0);
    }

    for (size_t i = 0; i < input->size; i++) {
        // Shift x_hist
        for (size_t j = x_hist->size - 1; j > 0; j--) {
            x_hist->data[j] = x_hist->data[j - 1];
        }
        x_hist->data[0] = input->data[i];

        // Calculate output
        double y = b->data[0] * x_hist->data[0];
        for (size_t j = 1; j < b->size; j++) {
            y += b->data[j] * x_hist->data[j];
        }
        for (size_t j = 0; j < a->size - 1; j++) {
            y -= a->data[j + 1] * y_hist->data[j];
        }

        // Shift y_hist
        for (size_t j = y_hist->size - 1; j > 0; j--) {
            y_hist->data[j] = y_hist->data[j - 1];
        }
        if (y_hist->size > 0) {
            y_hist->data[0] = y;
        }

        vec_f64_push(output, y);
    }

    vec_f64_free(x_hist);
    vec_f64_free(y_hist);
    return output;
}

// Helper function to get next power of two
static size_t next_power_of_two(size_t n) {
    size_t power = 1;
    while (power < n) {
        power <<= 1;
    }
    return power;
}

vec_f64* correlate_full(const vec_f64* x, const vec_f64* y) {
    size_t n = x->size + y->size - 1;
    size_t size = next_power_of_two(n);

    // Allocate complex arrays (interleaved real/imag)
    double* x_fft = (double*)calloc(size * 2, sizeof(double));
    double* y_fft = (double*)calloc(size * 2, sizeof(double));
    if (!x_fft || !y_fft) {
        free(x_fft);
        free(y_fft);
        return NULL;
    }

    // Copy x to complex array (zero-padded)
    for (size_t i = 0; i < x->size; i++) {
        x_fft[i * 2] = x->data[i];
        x_fft[i * 2 + 1] = 0.0;
    }

    // Copy y to complex array (reversed and zero-padded)
    for (size_t i = 0; i < y->size; i++) {
        y_fft[i * 2] = y->data[y->size - 1 - i];
        y_fft[i * 2 + 1] = 0.0;
    }

    // Create FFT plan
    cfft_plan plan = make_cfft_plan(size);
    if (!plan) {
        free(x_fft);
        free(y_fft);
        return NULL;
    }

    // Forward FFT
    cfft_forward(plan, x_fft, 1.0);
    cfft_forward(plan, y_fft, 1.0);

    // Multiply in frequency domain
    double* z = (double*)calloc(size * 2, sizeof(double));
    if (!z) {
        destroy_cfft_plan(plan);
        free(x_fft);
        free(y_fft);
        return NULL;
    }

    for (size_t i = 0; i < size; i++) {
        // Complex multiplication: (a + bi) * (c + di) = (ac - bd) + (ad + bc)i
        z[i * 2] = x_fft[i * 2] * y_fft[i * 2] - x_fft[i * 2 + 1] * y_fft[i * 2 + 1];
        z[i * 2 + 1] = x_fft[i * 2] * y_fft[i * 2 + 1] + x_fft[i * 2 + 1] * y_fft[i * 2];
    }

    // Inverse FFT
    cfft_backward(plan, z, 1.0);

    // Create output vector
    vec_f64* output = vec_f64_new(n);
    if (!output) {
        destroy_cfft_plan(plan);
        free(x_fft);
        free(y_fft);
        free(z);
        return NULL;
    }

    // Copy real part of result (normalized)
    for (size_t i = 0; i < n; i++) {
        vec_f64_push(output, z[i * 2] / (double)size);
    }

    // Cleanup
    destroy_cfft_plan(plan);
    free(x_fft);
    free(y_fft);
    free(z);

    return output;
}