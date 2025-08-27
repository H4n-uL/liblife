// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025 HaמuL

#ifndef DCT_CORE_H
#define DCT_CORE_H

#include <stddef.h>
#include "../../backend/backend.h"

vec_f64* dct(const vec_f64* input);
vec_f64* idct(const vec_f64* input);

#endif // DCT_CORE_H
