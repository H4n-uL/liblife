#include "p2tools.h"
#include "../backend/signal.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>

// Constants from Rust code
#define TNS_MAX_ORDER 12
#define L 1.5
#define QUANT_CONST 3.0

// Calculate autocorrelation
static vec_f64* calc_autocorr(const vec_f64* freq) {
    vec_f64* result = correlate_full(freq, freq);
    if (!result) return NULL;

    // Take second half of correlation result
    vec_f64* autocorr = vec_f64_new(freq->size);
    if (!autocorr) {
        vec_f64_free(result);
        return NULL;
    }

    size_t start = freq->size - 1;
    for (size_t i = 0; i < freq->size; i++) {
        vec_f64_push(autocorr, result->data[start + i]);
    }

    vec_f64_free(result);
    return autocorr;
}

// Levinson-Durbin algorithm for LPC calculation
static vec_f64* levinson_durbin(const vec_f64* autocorr) {
    size_t p = (autocorr->size < TNS_MAX_ORDER) ? autocorr->size : TNS_MAX_ORDER;

    vec_f64* lpc = vec_f64_new(p);
    if (!lpc) return NULL;

    double* a = (double*)calloc(p + 1, sizeof(double));
    double* a_prev = (double*)calloc(p + 1, sizeof(double));
    if (!a || !a_prev) {
        free(a);
        free(a_prev);
        vec_f64_free(lpc);
        return NULL;
    }

    double e = autocorr->data[0];

    for (size_t i = 1; i <= p; i++) {
        double k = autocorr->data[i];
        for (size_t j = 1; j < i; j++) {
            k -= a_prev[j] * autocorr->data[i - j];
        }
        k /= e;

        a[i] = k;
        for (size_t j = 1; j < i; j++) {
            a[j] = a_prev[j] - k * a_prev[i - j];
        }

        e *= (1.0 - k * k);

        // Copy a to a_prev
        memcpy(a_prev, a, (p + 1) * sizeof(double));
    }

    // Copy final LPC coefficients
    for (size_t i = 1; i <= p; i++) {
        vec_f64_push(lpc, a[i]);
    }

    free(a);
    free(a_prev);
    return lpc;
}

// Quantise LPC coefficients
static vec_i64* quantise_lpc(const vec_f64* lpc) {
    vec_i64* lpcq = vec_i64_new(lpc->size);
    if (!lpcq) return NULL;

    for (size_t i = 0; i < lpc->size; i++) {
        double val = lpc->data[i];
        double absval = fabs(val);

        // Apply non-linear quantization
        double q_val = L * absval / (1.0 - absval);

        // Round and apply sign
        int64_t quantized = (int64_t)round(q_val * QUANT_CONST);
        if (val < 0) quantized = -quantized;

        vec_i64_push(lpcq, quantized);
    }

    return lpcq;
}

// Dequantise LPC coefficients
static vec_f64* dequantise_lpc(const vec_i64* lpcq) {
    vec_f64* lpc = vec_f64_new(lpcq->size);
    if (!lpc) return NULL;

    for (size_t i = 0; i < lpcq->size; i++) {
        int64_t q_val = lpcq->data[i];
        double absq = fabs((double)q_val / QUANT_CONST);

        // Apply inverse non-linear quantization
        double val = absq / (L + absq);
        if (q_val < 0) val = -val;

        vec_f64_push(lpc, val);
    }

    return lpc;
}

// Calculate prediction gain
static double predgain(const vec_f64* orig, const vec_f64* prc) {
    if (orig->size != prc->size) return 0.0;

    double orig_energy = 0.0;
    double residual_energy = 0.0;

    for (size_t i = 0; i < orig->size; i++) {
        orig_energy += orig->data[i] * orig->data[i];
        double diff = orig->data[i] - prc->data[i];
        residual_energy += diff * diff;
    }

    if (residual_energy < DBL_EPSILON) {
        return 1000.0; // High gain for perfect prediction
    }

    return 10.0 * log10(orig_energy / residual_energy);
}

// TNS Analysis
void tns_analysis(const vec_f64* freqs, size_t channels,
                   vec_f64** tns_freqs_out, vec_i64** lpcqs_out) {
    *tns_freqs_out = NULL;
    *lpcqs_out = NULL;

    if (!freqs || channels == 0) return;

    size_t csize = freqs->size / channels;
    vec_f64* tns_freqs = vec_f64_new(freqs->size);
    vec_i64* all_lpcqs = vec_i64_new(0);

    if (!tns_freqs || !all_lpcqs) {
        vec_f64_free(tns_freqs);
        vec_i64_free(all_lpcqs);
        return;
    }

    for (size_t c = 0; c < channels; c++) {
        // Extract channel data
        vec_f64* chan_data = vec_f64_new(csize);
        if (!chan_data) {
            vec_f64_free(tns_freqs);
            vec_i64_free(all_lpcqs);
            return;
        }

        for (size_t i = 0; i < csize; i++) {
            vec_f64_push(chan_data, freqs->data[c * csize + i]);
        }

        // Calculate autocorrelation
        vec_f64* autocorr = calc_autocorr(chan_data);
        if (!autocorr) {
            vec_f64_free(chan_data);
            vec_f64_free(tns_freqs);
            vec_i64_free(all_lpcqs);
            return;
        }

        // Calculate LPC coefficients
        vec_f64* lpc = levinson_durbin(autocorr);
        vec_f64_free(autocorr);

        if (!lpc) {
            vec_f64_free(chan_data);
            vec_f64_free(tns_freqs);
            vec_i64_free(all_lpcqs);
            return;
        }

        // Apply TNS filter
        vec_f64* a_coeffs = vec_f64_new(lpc->size + 1);
        vec_f64* b_coeffs = vec_f64_new(1);

        if (!a_coeffs || !b_coeffs) {
            vec_f64_free(a_coeffs);
            vec_f64_free(b_coeffs);
            vec_f64_free(lpc);
            vec_f64_free(chan_data);
            vec_f64_free(tns_freqs);
            vec_i64_free(all_lpcqs);
            return;
        }

        vec_f64_push(a_coeffs, 1.0);
        for (size_t i = 0; i < lpc->size; i++) {
            vec_f64_push(a_coeffs, -lpc->data[i]);
        }
        vec_f64_push(b_coeffs, 1.0);

        vec_f64* tns_chan = impulse_filt(b_coeffs, a_coeffs, chan_data);

        vec_f64_free(a_coeffs);
        vec_f64_free(b_coeffs);

        if (!tns_chan) {
            vec_f64_free(lpc);
            vec_f64_free(chan_data);
            vec_f64_free(tns_freqs);
            vec_i64_free(all_lpcqs);
            return;
        }

        // Calculate prediction gain
        double gain = predgain(chan_data, tns_chan);

        // Use TNS if gain is significant
        if (gain > 5.0) {
            // Use TNS filtered data
            for (size_t i = 0; i < csize; i++) {
                vec_f64_push(tns_freqs, tns_chan->data[i]);
            }

            // Quantise and store LPC coefficients
            vec_i64* lpcq = quantise_lpc(lpc);
            if (lpcq) {
                for (size_t i = 0; i < lpcq->size; i++) {
                    vec_i64_push(all_lpcqs, lpcq->data[i]);
                }
                vec_i64_free(lpcq);
            }
        } else {
            // Use original data, no TNS
            for (size_t i = 0; i < csize; i++) {
                vec_f64_push(tns_freqs, chan_data->data[i]);
            }
            // Push zeros for LPC coefficients
            for (size_t i = 0; i < TNS_MAX_ORDER && i < csize; i++) {
                vec_i64_push(all_lpcqs, 0);
            }
        }

        vec_f64_free(lpc);
        vec_f64_free(tns_chan);
        vec_f64_free(chan_data);
    }

    *tns_freqs_out = tns_freqs;
    *lpcqs_out = all_lpcqs;
}

// TNS Synthesis
vec_f64* tns_synthesis(const vec_f64* tns_freqs, const vec_i64* lpcqs, size_t channels) {
    if (!tns_freqs || !lpcqs || channels == 0) return NULL;

    size_t csize = tns_freqs->size / channels;
    size_t lpc_per_channel = lpcqs->size / channels;

    vec_f64* freqs = vec_f64_new(tns_freqs->size);
    if (!freqs) return NULL;

    for (size_t c = 0; c < channels; c++) {
        // Extract channel LPC quantised coefficients
        vec_i64* chan_lpcq = vec_i64_new(lpc_per_channel);
        if (!chan_lpcq) {
            vec_f64_free(freqs);
            return NULL;
        }

        for (size_t i = 0; i < lpc_per_channel; i++) {
            vec_i64_push(chan_lpcq, lpcqs->data[c * lpc_per_channel + i]);
        }

        // Check if TNS was applied (non-zero LPC coefficients)
        bool has_tns = false;
        for (size_t i = 0; i < chan_lpcq->size; i++) {
            if (chan_lpcq->data[i] != 0) {
                has_tns = true;
                break;
            }
        }

        if (has_tns) {
            // Dequantise LPC coefficients
            vec_f64* lpc = dequantise_lpc(chan_lpcq);
            vec_i64_free(chan_lpcq);

            if (!lpc) {
                vec_f64_free(freqs);
                return NULL;
            }

            // Extract channel TNS data
            vec_f64* chan_tns = vec_f64_new(csize);
            if (!chan_tns) {
                vec_f64_free(lpc);
                vec_f64_free(freqs);
                return NULL;
            }

            for (size_t i = 0; i < csize; i++) {
                vec_f64_push(chan_tns, tns_freqs->data[c * csize + i]);
            }

            // Apply inverse TNS filter
            vec_f64* a_coeffs = vec_f64_new(1);
            vec_f64* b_coeffs = vec_f64_new(lpc->size + 1);

            if (!a_coeffs || !b_coeffs) {
                vec_f64_free(a_coeffs);
                vec_f64_free(b_coeffs);
                vec_f64_free(lpc);
                vec_f64_free(chan_tns);
                vec_f64_free(freqs);
                return NULL;
            }

            vec_f64_push(a_coeffs, 1.0);
            vec_f64_push(b_coeffs, 1.0);
            for (size_t i = 0; i < lpc->size; i++) {
                vec_f64_push(b_coeffs, -lpc->data[i]);
            }

            vec_f64* chan_freq = impulse_filt(b_coeffs, a_coeffs, chan_tns);

            vec_f64_free(a_coeffs);
            vec_f64_free(b_coeffs);
            vec_f64_free(lpc);
            vec_f64_free(chan_tns);

            if (!chan_freq) {
                vec_f64_free(freqs);
                return NULL;
            }

            // Copy synthesized data
            for (size_t i = 0; i < csize; i++) {
                vec_f64_push(freqs, chan_freq->data[i]);
            }

            vec_f64_free(chan_freq);
        } else {
            // No TNS, copy original data
            vec_i64_free(chan_lpcq);
            for (size_t i = 0; i < csize; i++) {
                vec_f64_push(freqs, tns_freqs->data[c * csize + i]);
            }
        }
    }

    return freqs;
}