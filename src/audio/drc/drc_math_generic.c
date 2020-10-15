// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Google LLC. All rights reserved.
//
// Author: Pin-chih Lin <johnylin@google.com>

#include <sof/audio/drc/drc_math.h>
#include <sof/audio/format.h>
#include <sof/math/decibels.h>
#include <sof/math/numbers.h>
#include <math.h>

/* Add definitions for fixed-point math function usage */
#define DRC_FIXED_DB2LIN
#define DRC_FIXED_LIN2DB
#define DRC_FIXED_LOG10
#define DRC_FIXED_LOGF
#define DRC_FIXED_SIN
#define DRC_FIXED_ASIN
#define DRC_FIXED_POW
#define DRC_FIXED_EXP

#define DRC_PI_FLOAT 3.141592653589793f
#define DRC_PI_OVER_TWO_FLOAT 1.57079632679489661923f
#define DRC_TWO_OVER_PI_FLOAT 0.63661977236758134f
#define DRC_ONE_OVER_SQRT2 0.70710678118654752f

inline float decibels_to_linear(float decibels)
{
#ifdef DRC_FIXED_DB2LIN
/*
 * Input is Q8.24: max 128.0
 * Output is Q12.20: max 2048.0
 */
	return Q_CONVERT_QTOF(db2lin_fixed(Q_CONVERT_FLOAT(decibels, 24)), 20);
#else
	/* 10^(x/20) = e^(x * log(10^(1/20))) */
	return expf(0.1151292546497022f * decibels);
#endif
}

static inline float warp_log10f(float x)
{
#ifdef DRC_FIXED_LOG10
/*
 * Input is Q6.26: max 32.0
 * Output range ~ (-inf, 1.505); regulated to Q6.26: (-32.0, 32.0)
 */
#define q_v 26
#define q_mult(a, b, qa, qb, qy) ((int32_t)Q_MULTSR_32X32((int64_t)a, b, qa, qb, qy))
	/* Coefficients obtained from:
	 * fpminimax(log10(x), 5, [|SG...|], [1/2;sqrt(2)/2], absolute);
	 * max err ~= 6.088e-8
	 */
	const int32_t HALF_Q_V = Q_CONVERT_FLOAT(0.5f, q_v);
	const int32_t ONE_Q_V = Q_CONVERT_FLOAT(1.0f, q_v);
	const int32_t ONE_OVER_SQRT2 = Q_CONVERT_FLOAT(DRC_ONE_OVER_SQRT2, q_v);
	const int32_t A5 = Q_CONVERT_FLOAT(1.131880283355712890625f, q_v);
	const int32_t A4 = Q_CONVERT_FLOAT(-4.258677959442138671875f, q_v);
	const int32_t A3 = Q_CONVERT_FLOAT(6.81631565093994140625f, q_v);
	const int32_t A2 = Q_CONVERT_FLOAT(-6.1185703277587890625f, q_v);
	const int32_t A1 = Q_CONVERT_FLOAT(3.6505267620086669921875f, q_v);
	const int32_t A0 = Q_CONVERT_FLOAT(-1.217894077301025390625f, q_v);
	const int32_t LOG10_2 = Q_CONVERT_FLOAT(0.301029995663981195214f, q_v);
	int32_t x_fixed = Q_CONVERT_FLOAT(x, q_v);
	int32_t e; /* Q31.1 */
	int32_t x2, x4; /* Q2.30 */
	int32_t A5Xx, A3Xx;
	int bit;
	int32_t bitmask = 0x40000000;

	/* frexpf(x, &e) implementation:
	 * The goal is to adjust x into range [0.5, 1.0) with precision q_v by shiftings. In order
	 * words, bit[q_v-1] should be the right-most 1 for x. So first we find the bit location of
	 * the right-most 1 for original x, then calculate e by the distance from q_v and perform
	 * the shifting.
	 */
	for (bit = 31; bit > 0; bit--) {
		if (x_fixed & bitmask)
			break;
		bitmask >>= 1;
	}
	if (bit > q_v)
		x_fixed = Q_SHIFT_RND(x_fixed, bit, q_v);
	else if (bit < q_v)
		x_fixed = Q_SHIFT_LEFT(x_fixed, bit, q_v);
	e = (bit - q_v) << 1; /* Q_CONVERT_FLOAT(bit - q_v, 1) */

	if (x_fixed > ONE_OVER_SQRT2) {
		x_fixed = q_mult(x_fixed, ONE_OVER_SQRT2, q_v, q_v, q_v);
		e += 1; /* Q_CONVERT_FLOAT(0.5, 1); */
	}

	x2 = q_mult(x_fixed, x_fixed, q_v, q_v, 30);
	x4 = q_mult(x2, x2, 30, 30, 30);
	A5Xx = q_mult(A5, x_fixed, q_v, q_v, q_v);
	A3Xx = q_mult(A3, x_fixed, q_v, q_v, q_v);
	return Q_CONVERT_QTOF((q_mult((A5Xx + A4), x4, q_v, 30, q_v)
		+ q_mult((A3Xx + A2), x2, q_v, 30, q_v) + q_mult(A1, x_fixed, q_v, q_v, q_v) + A0
		+ q_mult(e, LOG10_2, 1, q_v, q_v)), q_v);
#undef q_mult
#undef q_v
#else
	return log10f(x);
#endif
}

inline float linear_to_decibels(float linear)
{
	/* For negative or zero, just return a very small dB value. */
	if (linear <= 0)
		return -1000;
	if (isbadf(linear))
		return linear;
#ifdef DRC_FIXED_LIN2DB
/*
 * Input is Q6.26: max 32.0
 * Output range ~ (-inf, 30.1030); regulated to Q11.21: (-1024.0, 1024.0)
 */
	int32_t log10_linear = Q_CONVERT_FLOAT(warp_log10f(linear), 26);

	return Q_CONVERT_QTOF(Q_MULTSR_32X32((int64_t)20, log10_linear, 0, 26, 21), 21);
#else
	/* 20 * log10(x) = 20 / log(10) * log(x) */
	return 8.6858896380650366f * logf(linear);
#endif
}

inline float warp_logf(float x)
{
#ifdef DRC_FIXED_LOGF
/*
 * Input is Q6.26: max 32.0
 * Output range ~ (-inf, 3.4657); regulated to Q6.26: (-32.0, 32.0)
 */
	/* log(x) = log(10) * log10(x) */
	const int32_t LOG10 = Q_CONVERT_FLOAT(2.3025850929940457f, 29);
	int32_t log10_x = Q_CONVERT_FLOAT(warp_log10f(x), 26);

	return Q_CONVERT_QTOF(Q_MULTSR_32X32((int64_t)LOG10, log10_x, 29, 26, 26), 26);
#else
	return logf(x);
#endif
}

inline float warp_sinf(float x)
{
#ifdef DRC_FIXED_SIN
/*
 * Input is Q2.30: (-2.0, 2.0)
 * Output range: [-1.0, 1.0]; regulated to Q2.30: (-2.0, 2.0)
 */
#define q_v 30
#define q_multv(a, b) ((int32_t)Q_MULTSR_32X32((int64_t)a, b, q_v, q_v, q_v))
	/* Coefficients obtained from:
	 * fpminimax(sin(x*pi/2), [|1,3,5,7|], [|SG...|], [-1e-30;1], absolute)
	 * max err ~= 5.901e-7
	 */
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

inline float warp_asinf(float x)
{
#ifdef DRC_FIXED_ASIN
/*
 * Input is Q2.30: (-2.0, 2.0)
 * Output range: [-1.0, 1.0]; regulated to Q2.30: (-2.0, 2.0)
 */
#define q_vl 30
#define q_vh 26
#define q_multv(a, b, q) ((int32_t)Q_MULTSR_32X32((int64_t)a, b, q, q, q))
	/* Coefficients obtained from:
	 * If x <= 1/sqrt(2), then
	 *   fpminimax(asin(x), [|1,3,5,7|], [|SG...|], [-1e-30;1/sqrt(2)], absolute)
	 *   max err ~= 1.89936e-5
	 * Else then
	 *   fpminimax(asin(x), [|1,3,5,7|], [|SG...|], [1/sqrt(2);1], absolute)
	 *   max err ~= 3.085226e-2
	 */
	const int32_t TWO_OVER_PI = Q_CONVERT_FLOAT(DRC_TWO_OVER_PI_FLOAT, q_vl);
	const int32_t ONE_OVER_SQRT2 = Q_CONVERT_FLOAT(DRC_ONE_OVER_SQRT2, q_vl);
	const int32_t A7L = Q_CONVERT_FLOAT(0.1181826665997505187988281f, q_vl);
	const int32_t A5L = Q_CONVERT_FLOAT(4.0224377065896987915039062e-2f, q_vl);
	const int32_t A3L = Q_CONVERT_FLOAT(0.1721895635128021240234375f, q_vl);
	const int32_t A1L = Q_CONVERT_FLOAT(0.99977016448974609375f, q_vl);

	const int32_t A7H = Q_CONVERT_FLOAT(14.12774658203125f, q_vh);
	const int32_t A5H = Q_CONVERT_FLOAT(-30.1692714691162109375f, q_vh);
	const int32_t A3H = Q_CONVERT_FLOAT(21.4760608673095703125f, q_vh);
	const int32_t A1H = Q_CONVERT_FLOAT(-3.894591808319091796875f, q_vh);

	int32_t A7, A5, A3, A1, q_v;
	int32_t x2, x4;
	int32_t A3Xx2, A7Xx2, asinx;

	int32_t x_fixed = Q_CONVERT_FLOAT(x, q_vl);

	if (ABS(x_fixed) <= ONE_OVER_SQRT2) {
		A7 = A7L;
		A5 = A5L;
		A3 = A3L;
		A1 = A1L;
		q_v = q_vl;
	} else {
		A7 = A7H;
		A5 = A5H;
		A3 = A3H;
		A1 = A1H;
		q_v = q_vh;
		x_fixed = Q_SHIFT_RND(x_fixed, q_vl, q_vh);
	}

	x2 = q_multv(x_fixed, x_fixed, q_v);
	x4 = q_multv(x2, x2, q_v);

	A3Xx2 = q_multv(A3, x2, q_v);
	A7Xx2 = q_multv(A7, x2, q_v);

	asinx = q_multv(x_fixed, (q_multv(x4, (A7Xx2 + A5), q_v) + A3Xx2 + A1), q_v);
	return Q_CONVERT_QTOF(Q_MULTSR_32X32((int64_t)asinx, TWO_OVER_PI, q_v, q_vl, q_vl), q_vl);
#undef q_multv
#undef q_vh
#undef q_vl
#else
	return asinf(x) * DRC_TWO_OVER_PI_FLOAT;
#endif
}

inline float warp_powf(float x, float y)
{
#ifdef DRC_FIXED_POW
/*
 * Input x is Q6.26: (-32.0, 32.0)
 *       y is Q2.30: (-2.0, 2.0)
 * Output is Q12.20: max 2048.0
 */
	/* x^y = expf(y * logf(x)) */
	int32_t y_fixed = Q_CONVERT_FLOAT(y, 30);
	int32_t logx_fixed = Q_CONVERT_FLOAT(warp_logf(x), 26);

	return Q_CONVERT_QTOF(exp_fixed(Q_MULTSR_32X32((int64_t)y_fixed, logx_fixed, 30, 26, 27)),
			      20);
#else
	return powf(x, y);
#endif
}

inline float knee_expf(float input)
{
#ifdef DRC_FIXED_EXP
/*
 * Input is Q5.27: max 16.0
 * Output is Q12.20: max 2048.0
 */
	return Q_CONVERT_QTOF(exp_fixed(Q_CONVERT_FLOAT(input, 27)), 20);
#else
	return expf(input);
#endif
}

inline int isbadf(float x)
{
	return x != 0 && !isnormal(x);
}
