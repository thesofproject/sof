// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

/**
 * \file math/atan2.c
 * \brief Four-quadrant arctangent function using polynomial approximation
 */

#include <rtos/symbol.h>
#include <sof/audio/format.h>
#include <sof/math/trig.h>
#include <stdint.h>

/*
 * Degree-9 Remez minimax polynomial for atan(z) in first octant, z in [0, 1]:
 *
 *   atan(z) = z * (C0 + z^2 * (C1 + z^2 * (C2 + z^2 * (C3 + z^2 * C4))))
 *
 * Coefficients are in Q3.29 format, computed via Remez exchange algorithm
 * to minimize the maximum approximation error over [0, 1].
 * Maximum approximation error is ~0.0011 degrees (1.94e-5 radians).
 */
#define SOFM_ATAN2_C0 536890772	 /* Q3.29, +1.000036992319 */
#define SOFM_ATAN2_C1 -178298711 /* Q3.29, -0.332107229582 */
#define SOFM_ATAN2_C2 99794340	 /* Q3.29, +0.185881443393 */
#define SOFM_ATAN2_C3 -49499604	 /* Q3.29, -0.092200197922 */
#define SOFM_ATAN2_C4 12781063	 /* Q3.29, +0.023806584771 */

/**
 * sofm_atan2_32b() - Compute four-quadrant arctangent of y/x
 * @param y Imaginary component in Q1.31 format
 * @param x Real component in Q1.31 format
 * @return Angle in Q3.29 radians, range [-pi, +pi]
 *
 * Uses degree-9 Remez minimax polynomial for the core atan(z) computation
 * where z = min(|y|,|x|) / max(|y|,|x|) is reduced to [0, 1] range.
 * Octant and quadrant corrections extend the result to full [-pi, +pi].
 */
int32_t sofm_atan2_32b(int32_t y, int32_t x)
{
	int32_t abs_x;
	int32_t abs_y;
	int32_t num;
	int32_t den;
	int32_t z;
	int32_t z2;
	int32_t acc;
	int32_t angle;
	int swap;

	/* Return zero for origin */
	if (x == 0 && y == 0)
		return 0;

	/* Take absolute values, handle INT32_MIN overflow */
	abs_x = (x > 0) ? x : ((x == INT32_MIN) ? INT32_MAX : -x);
	abs_y = (y > 0) ? y : ((y == INT32_MIN) ? INT32_MAX : -y);

	/* Reduce to first octant: z = min/max ensures z in [0, 1] */
	if (abs_y <= abs_x) {
		num = abs_y;
		den = abs_x;
		swap = 0;
	} else {
		num = abs_x;
		den = abs_y;
		swap = 1;
	}

	/* z = num / den in Q1.31, den is always > 0 here */
	z = sat_int32(((int64_t)num << 31) / den);

	/*
	 * Horner evaluation of degree-9 Remez minimax polynomial:
	 *   atan(z) = z * (C0 + z^2 * (C1 + z^2 * (C2 + z^2 * (C3 + z^2 * C4))))
	 *
	 * z is Q1.31, coefficients are Q3.29.
	 * Multiplications: Q1.31 * Q3.29 = Q4.60, >> 31 -> Q3.29.
	 */

	/* z^2: Q1.31 * Q1.31 = Q2.62, >> 31 -> Q1.31 */
	z2 = (int32_t)(((int64_t)z * z) >> 31);

	/* Innermost: C3 + z^2 * C4 */
	acc = (int32_t)(((int64_t)z2 * SOFM_ATAN2_C4) >> 31) + SOFM_ATAN2_C3;

	/* C2 + z^2 * (C3 + z^2 * C4) */
	acc = (int32_t)(((int64_t)z2 * acc) >> 31) + SOFM_ATAN2_C2;

	/* C1 + z^2 * (C2 + z^2 * (C3 + z^2 * C4)) */
	acc = (int32_t)(((int64_t)z2 * acc) >> 31) + SOFM_ATAN2_C1;

	/* C0 + z^2 * (C1 + z^2 * (C2 + z^2 * (C3 + z^2 * C4))) */
	acc = (int32_t)(((int64_t)z2 * acc) >> 31) + SOFM_ATAN2_C0;

	/* angle = z * acc: Q1.31 * Q3.29 = Q4.60, >> 31 -> Q3.29 */
	angle = (int32_t)(((int64_t)z * acc) >> 31);

	/* If |y| > |x|, use identity: atan(y/x) = pi/2 - atan(x/y) */
	if (swap)
		angle = PI_DIV2_Q3_29 - angle;

	/* Map to correct quadrant based on signs of x and y */
	if (x < 0)
		angle = (y >= 0) ? PI_Q3_29 - angle : -(PI_Q3_29 - angle);
	else if (y < 0)
		angle = -angle;

	return angle;
}
EXPORT_SYMBOL(sofm_atan2_32b);
