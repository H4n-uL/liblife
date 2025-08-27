#ifndef SIGNAL_H
#define SIGNAL_H

#include <stddef.h>
#include "../../backend/backend.h"

// Signal processing functions
vec_f64* impulse_filt(const vec_f64* b, const vec_f64* a, const vec_f64* input);
vec_f64* correlate_full(const vec_f64* x, const vec_f64* y);

#endif