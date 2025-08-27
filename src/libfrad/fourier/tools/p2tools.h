#ifndef P2TOOLS_H
#define P2TOOLS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "../../backend/backend.h"

// TNS analysis and synthesis functions
void tns_analysis(const vec_f64* freqs, size_t channels,
                   vec_f64** tns_freqs_out, vec_i64** lpcqs_out);
vec_f64* tns_synthesis(const vec_f64* tns_freqs, const vec_i64* lpcqs, size_t channels);

#endif