/**
 * @file int_fallback.c
 * @brief Portable C fallback for integer square root.
 *
 * Uses the Quake II fast inverse square root algorithm followed by one
 * Newton-Raphson iteration for a good initial approximation.
 *
 * Bit reinterpretation is done via `memcpy` to comply with strict-aliasing
 * rules (avoids undefined behaviour from direct pointer type-punning).
 */

// F9S - FORTRAN 95 Subset Compiler
// Copyright (C) 2026 Debaditya Malakar
// SPDX-License-Identifier: AGPL-3.0-only

#include <stdint.h>
#include <string.h>

#include "int.h"

long long sqrt_int_fallback(long long a) {
    if (a <= 0) return 0;

    float number = (float)a;
    float x2     = number * 0.5f;
    float y      = number;

    int32_t i;
    memcpy(&i, &y, sizeof(i));           /* reinterpret float bits as int32 */
    i = 0x5f3759df - (i >> 1);          /* initial approximation */
    memcpy(&y, &i, sizeof(y));           /* reinterpret int32 bits back to float */

    y = y * (1.5f - (x2 * y * y));      /* one Newton-Raphson iteration */

    float s = number * y;                /* approximate sqrt: 1 / (1/sqrt(n)) */
    if (s < 0.0f) return 0;
    return (long long)(s + 0.5f);
}
