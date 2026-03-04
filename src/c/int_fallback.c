// F9S - FORTRAN 95 Subset Compiler
// Copyright (C) 2026 Debaditya Malakar
// SPDX-License-Identifier: AGPL-3.0-only

#include <stdint.h>
#include <string.h>
#include "int.h"

/*
 * Integer sqrt via Quake II fast inverse square root.
 * Uses memcpy for float<->int32 bit reinterpretation to comply with
 * strict-aliasing rules (avoids UB from direct pointer type-punning).
 */
long long sqrt_int_fallback(long long a) {
	if (a <= 0) return 0;
	float number = (float)a;
	float x2 = number * 0.5f;
	float y = number;

	int32_t i;
	memcpy(&i, &y, sizeof(i));      /* reinterpret float bits as int32 */
	i = 0x5f3759df - (i >> 1);      /* initial approximation */
	memcpy(&y, &i, sizeof(y));      /* reinterpret int32 bits back to float */

	const float threehalfs = 1.5f;
	y = y * (threehalfs - (x2 * y * y)); /* one Newton-Raphson iteration */

	float s = number * y; /* approximate sqrt: 1 / (1/sqrt(n)) */
	if (s < 0.0f) return 0;
	return (long long)(s + 0.5f);
}
