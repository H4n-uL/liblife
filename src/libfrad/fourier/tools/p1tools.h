#ifndef P1TOOLS_H
#define P1TOOLS_H

#include <stdint.h>
#include <stdbool.h>
#include "../../backend/backend.h"

#define MOSLEN 21
#define SPREAD_ALPHA 0.5

vec_f64* mask_thres_mos(const vec_f64* freqs, uint32_t srate, double loss_level, double alpha);
vec_f64* mapping_from_opus(const vec_f64* thres, size_t freq_len, uint32_t srate);
int64_t quant(double x);
double dequant(double x);
vec_u8* exp_golomb_encode(const int64_t* data, size_t len);
int64_t* exp_golomb_decode(const vec_u8* data, size_t* out_len);

#endif // P1TOOLS_H
