// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 Intel Corporation. All rights reserved.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

#include <sof/audio/format.h>
#include <sof/math/decibels.h>
#include <sof/math/log.h>
#include <sof/math/trig.h>
#include <sof/math/window.h>
#include <errno.h>
#include <stdint.h>

#define WIN_ONE_Q15 INT16_MAX
#define WIN_ONE_Q31 INT32_MAX
#define WIN_05_Q31 Q_CONVERT_FLOAT(0.5, 31)

#define WIN_TWO_PI_Q28 Q_CONVERT_FLOAT(6.2831853072, 28)

#define WIN_085_Q31 Q_CONVERT_FLOAT(0.85, 31)
#define WIN_LOG_2POW31_Q26 Q_CONVERT_FLOAT(21.4875625974, 26)

/* Exact
 * #define WIN_HAMMING_A0_Q30 Q_CONVERT_FLOAT(25.0 / 46.0, 30)
 * #define WIN_HAMMING_A1_Q30 Q_CONVERT_FLOAT(1 - 25.0 / 46.0, 30)
 */

/* Common approximations to match e.g. Octave */
#define WIN_HAMMING_A0_Q30 Q_CONVERT_FLOAT(0.54, 30)
#define WIN_HAMMING_A1_Q30 Q_CONVERT_FLOAT(0.46, 30)

/**
 * \brief Return rectangular window, simply values of one
 * \param[in,out]  win  Output vector with coefficients
 * \param[in]  length  Length of coefficients vector
 */
void win_rectangular_16b(int16_t *win, int length)
{
	int i;

	for (i = 0; i < length; i++)
		win[i] = WIN_ONE_Q15;
}

/**
 * \brief Calculate Blackman window function, reference
 * https://en.wikipedia.org/wiki/Window_function#Blackman_window

 * \param[in,out]  win  Output vector with coefficients
 * \param[in]  length  Length of coefficients vector
 * \param[in]  a0      Parameter for window shape, use e.g. 0.42 as Q1.15
 */
void win_blackman_16b(int16_t win[], int length, int16_t a0)
{
	const int32_t a1 = Q_CONVERT_FLOAT(0.5, 31);
	int32_t inv_length;
	int32_t val;
	int32_t a;
	int16_t alpha;
	int32_t a2;
	int32_t c1;
	int32_t c2;
	int n;

	alpha = WIN_ONE_Q15 - 2 * a0; /* Q1.15 */
	a2 = alpha << 15; /* Divided by 2 in Q1.31 */
	a = WIN_TWO_PI_Q28 / (length - 1); /* Q4.28 */
	inv_length = WIN_ONE_Q31 / length;

	for (n = 0; n < length; n++) {
		c1 = cos_fixed_32b(a * n);
		c2 = cos_fixed_32b(2 * n * Q_MULTSR_32X32((int64_t)a, inv_length, 28, 31, 28));
		val = a0 - Q_MULTSR_32X32((int64_t)a1, c1, 31, 31, 15) +
			Q_MULTSR_32X32((int64_t)a2, c2, 31, 31, 15);
		win[n] = sat_int16(val);
	}
}

void win_hamming_16b(int16_t win[], int length)
{
	int32_t val;
	int32_t a;
	int n;

	a = WIN_TWO_PI_Q28 / (length - 1); /* Q4.28 */
	for (n = 0; n < length; n++) {
		/* Calculate 0.54 - 0.46 * cos(a * n) */
		val = cos_fixed_32b(a * n); /* Q4.28 -> Q1.31 */
		val = Q_MULTSR_32X32((int64_t)val, WIN_HAMMING_A1_Q30, 31, 30, 30); /* Q1.30 */
		val = WIN_HAMMING_A0_Q30 - val;

		/* Convert to Q1.15 */
		win[n] = sat_int16(Q_SHIFT_RND(val, 30, 15));
	}
}

void win_povey_16b(int16_t win[], int length)
{
	int32_t cos_an;
	int32_t x1;
	int32_t x2;
	int32_t x3;
	int32_t x4;
	int32_t a;
	int n;

	a = WIN_TWO_PI_Q28 / (length - 1); /* Q4.28 */
	for (n = 0; n < length; n++) {
		/* Calculate 0.5 - 0.5 * cos(a * n) */
		cos_an = cos_fixed_32b(a * n); /* Q4.28 -> Q1.31 */
		x1 = WIN_05_Q31 - (cos_an >> 1); /* Q1.31 */

		/* Calculate x^0.85 as exp(0.85 * log(x)) */
		x2 = (int32_t)(ln_int32((uint32_t)x1) >> 1) - WIN_LOG_2POW31_Q26;
		x3 = sat_int32(Q_MULTSR_32X32((int64_t)x2, WIN_085_Q31, 26, 31, 27)); /* Q5.27 */
		x4 = exp_fixed(x3); /* Q5.27 -> Q12.20 */

		/* Convert to Q1.15 */
		win[n] = sat_int16(Q_SHIFT_RND(x4, 20, 15));
	}
}
