/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2020 Google LLC. All rights reserved.
 *
 * Author: Pin-chih Lin <johnylin@google.com>
 */
#ifndef __SOF_AUDIO_DRC_DRC_MATH_H__
#define __SOF_AUDIO_DRC_DRC_MATH_H__

#include <sof/audio/format.h>
#include <sof/math/decibels.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>

/* Add definitions for fixed-point math function usage */
#define DRC_FIXED_DB2LIN
#define DRC_FIXED_SINF
#define DRC_FIXED_EXP

#define DRC_PI_FLOAT 3.141592653589793f
#define DRC_PI_OVER_TWO_FLOAT 1.57079632679489661923f
#define DRC_TWO_OVER_PI_FLOAT 0.63661977236758134f
#define DRC_NEG_TWO_DB 0.7943282347242815f /* -2dB = 10^(-2/20) */

static inline float decibels_to_linear(float decibels)
{
#ifdef DRC_FIXED_DB2LIN
	return Q_CONVERT_QTOF(db2lin_fixed(Q_CONVERT_FLOAT(decibels, 24)), 20);
#else
	/* 10^(x/20) = e^(x * log(10^(1/20))) */
	return expf(0.1151292546497022f * decibels);
#endif
}

static inline float linear_to_decibels(float linear)
{
	/* For negative or zero, just return a very small dB value. */
	if (linear <= 0)
		return -1000;

	/* 20 * log10(x) = 20 / log(10) * log(x) */
	return 8.6858896380650366f * logf(linear);
}

static inline float warp_sinf(float x)
{
#ifdef DRC_FIXED_SINF
#define q_v 30
#define q_multv(a, b) ((int32_t)Q_MULTSR_32X32((int64_t)a, b, q_v, q_v, q_v))
	const int32_t A7 = Q_CONVERT_FLOAT(-4.3330336920917034149169921875e-3f, q_v);
	const int32_t A5 = Q_CONVERT_FLOAT(7.9434238374233245849609375e-2f, q_v);
	const int32_t A3 = Q_CONVERT_FLOAT(-0.645892798900604248046875f, q_v);
	const int32_t A1 = Q_CONVERT_FLOAT(1.5707910060882568359375f, q_v);
	int32_t x_fixed = Q_CONVERT_FLOAT(x, q_v);
	int32_t x2 = q_multv(x_fixed, x_fixed);
	int32_t x4 = q_multv(x2, x2);

	int32_t A3Xx2 = q_multv(A3, x2);
	int32_t A7Xx2 = q_multv(A7, x2);

	return Q_CONVERT_QTOF(q_multv(x_fixed, (q_multv(x4, (A7Xx2 + A5)) + A3Xx2 + A1)), q_v);
#undef q_multv
#undef q_v
#else
	return sinf(DRC_PI_OVER_TWO_FLOAT * x);
#endif
}

static inline float warp_asinf(float x)
{
	return asinf(x) * DRC_TWO_OVER_PI_FLOAT;
}

static inline float knee_expf(float input)
{
#ifdef DRC_FIXED_EXP
	const int32_t A = Q_CONVERT_FLOAT(8.685889638065044f, 27);
	int32_t i_fixed = Q_CONVERT_FLOAT(input, 20);
	return Q_CONVERT_QTOF(db2lin_fixed(Q_MULTSR_32X32((int64_t)A, i_fixed, 27, 20, 24)), 20);
#else
	return expf(input);
#endif
}

static inline int isbadf(float x)
{
	return x != 0 && !isnormal(x);
}

#endif //  __SOF_AUDIO_DRC_DRC_MATH_H__
