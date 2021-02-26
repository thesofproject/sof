// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Google LLC. All rights reserved.
//
// Author: Pin-chih Lin <johnylin@google.com>

#include <sof/audio/drc/drc_math_float.h>
#include <sof/audio/format.h>
#include <sof/math/decibels.h>
#include <sof/math/numbers.h>
#include <sof/math/trig.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define q_mult(a, b, qa, qb, qy) ((int32_t)Q_MULTSR_32X32((int64_t)(a), b, qa, qb, qy))
#define q_multq(a, b, q) ((int32_t)Q_MULTSR_32X32((int64_t)(a), b, q, q, q))

/* Add definitions for fixed-point math function usage */
#define DRC_FIXED_DB2LIN
#define DRC_FIXED_LIN2DB
#define DRC_FIXED_LOG
#define DRC_FIXED_SIN
#define DRC_FIXED_ASIN
#define DRC_FIXED_POW
#define DRC_FIXED_INV
#define DRC_FIXED_EXP

#define DRC_PI_FLOAT 3.141592653589793f
#define DRC_PI_OVER_TWO_FLOAT 1.57079632679489661923f
#define DRC_TWO_OVER_PI_FLOAT 0.63661977236758134f

/*
 * Input depends on precision_x
 * Output range [0.5, 1); regulated to Q2.30
 */
static inline int32_t rexp_fixed(int32_t x, int32_t precision_x, int32_t *e)
{
	int32_t bit = 31 - norm_int32(x);

	*e = bit - precision_x;

	if (bit > 30)
		return Q_SHIFT_RND(x, bit, 30);
	if (bit < 30)
		return Q_SHIFT_LEFT(x, bit, 30);
	return x;
}

/*
 * Input is Q6.26: max 32.0
 * Output range ~ (-inf, 1.505); regulated to Q6.26: (-32.0, 32.0)
 */
static inline int32_t log10_fixed(int32_t x)
{
#define qc 26
	/* Coefficients obtained from:
	 * fpminimax(log10(x), 5, [|SG...|], [1/2;sqrt(2)/2], absolute);
	 * max err ~= 6.088e-8
	 */
	const int32_t ONE_OVER_SQRT2 = Q_CONVERT_FLOAT(0.70710678118654752f, 30); /* 1/sqrt(2) */
	const int32_t A5 = Q_CONVERT_FLOAT(1.131880283355712890625f, qc);
	const int32_t A4 = Q_CONVERT_FLOAT(-4.258677959442138671875f, qc);
	const int32_t A3 = Q_CONVERT_FLOAT(6.81631565093994140625f, qc);
	const int32_t A2 = Q_CONVERT_FLOAT(-6.1185703277587890625f, qc);
	const int32_t A1 = Q_CONVERT_FLOAT(3.6505267620086669921875f, qc);
	const int32_t A0 = Q_CONVERT_FLOAT(-1.217894077301025390625f, qc);
	const int32_t LOG10_2 = Q_CONVERT_FLOAT(0.301029995663981195214f, qc);
	int32_t e;
	int32_t exp; /* Q31.1 */
	int32_t x2, x4; /* Q2.30 */
	int32_t A5Xx, A3Xx;

	x = rexp_fixed(x, 26, &e); /* Q2.30 */
	exp = (int32_t)e << 1; /* Q_CONVERT_FLOAT(e, 1) */

	if (x > ONE_OVER_SQRT2) {
		x = q_mult(x, ONE_OVER_SQRT2, 30, 30, 30);
		exp += 1; /* Q_CONVERT_FLOAT(0.5, 1) */
	}

	x2 = q_mult(x, x, 30, 30, 30);
	x4 = q_mult(x2, x2, 30, 30, 30);
	A5Xx = q_mult(A5, x, qc, 30, qc);
	A3Xx = q_mult(A3, x, qc, 30, qc);
	return q_mult((A5Xx + A4), x4, qc, 30, qc) + q_mult((A3Xx + A2), x2, qc, 30, qc)
		+ q_mult(A1, x, qc, 30, qc) + A0 + q_mult(exp, LOG10_2, 1, qc, qc);
#undef qc
}

float decibels_to_linear(float decibels)
{
#ifdef DRC_FIXED_DB2LIN
	int32_t dec = Q_CONVERT_FLOAT(decibels, 24);
	int32_t lin = db2lin_fixed(dec);
	return Q_CONVERT_QTOF(lin, 20);
#else
	/* 10^(x/20) = e^(x * log(10^(1/20))) */
	return expf(0.1151292546497022f * decibels);
#endif
}

/*
 * Input is Q6.26: max 32.0
 * Output range ~ (-inf, 30.1030); regulated to Q11.21: (-1024.0, 1024.0)
 */
float linear_to_decibels(float linear)
{
	/* For negative or zero, just return a very small dB value. */
	if (linear <= 0)
		return -1000;

#ifdef DRC_FIXED_LIN2DB
	int32_t lin = Q_CONVERT_FLOAT(linear, 26);
	int32_t log10_linear = log10_fixed(lin); /* Q6.26 */
	int32_t dec = q_mult(20, log10_linear, 0, 26, 21);
	return Q_CONVERT_QTOF(dec, 21);
#else
	/* 20 * log10(x) = 20 / log(10) * log(x) */
	return 8.6858896380650366f * logf(linear);
#endif
}

/*
 * Input is Q6.26: max 32.0
 * Output range ~ (-inf, 3.4657); regulated to Q6.26: (-32.0, 32.0)
 */
inline float warp_logf(float x)
{
	if (x <= 0)
		return -30.0f;

#ifdef DRC_FIXED_LOG
	const int32_t LOG10 = Q_CONVERT_FLOAT(2.3025850929940457f, 29);
	int32_t log10_x;

	int32_t xf = Q_CONVERT_FLOAT(x, 26);
	/* log(x) = log(10) * log10(x) */
	log10_x = log10_fixed(xf); /* Q6.26 */
	int32_t logv = q_mult(LOG10, log10_x, 29, 26, 26);
	return Q_CONVERT_QTOF(logv, 26);
#else
	return logf(x);
#endif
}

/*
 * Input is Q2.30: (-2.0, 2.0)
 * Output range: (-1.0, 1.0); regulated to Q1.31: (-1.0, 1.0)
 */
inline float warp_sinf(float x)
{
#ifdef DRC_FIXED_SIN
	const int32_t PI_OVER_TWO = Q_CONVERT_FLOAT(1.57079632679489661923f, 30);

	/* input range of sin_fixed() is non-negative */
	int32_t xf = Q_CONVERT_FLOAT(x, 30);
	int32_t abs_sin_val = sin_fixed(q_mult(ABS(xf), PI_OVER_TWO, 30, 30, 28));
	int32_t sinv = SGN(x) < 0 ? -abs_sin_val : abs_sin_val;
	return Q_CONVERT_QTOF(sinv, 31);
#else
	return sinf(DRC_PI_OVER_TWO_FLOAT * x);
#endif
}

/*
 * Input is Q2.30; valid range: [-1.0, 1.0]
 * Output range: [-1.0, 1.0]; regulated to Q2.30: (-2.0, 2.0)
 */
inline float warp_asinf(float x)
{
#ifdef DRC_FIXED_ASIN
#define qcl 30
#define qch 26
	/* Coefficients obtained from:
	 * If x <= 1/sqrt(2), then
	 *   fpminimax(asin(x), [|1,3,5,7|], [|SG...|], [-1e-30;1/sqrt(2)], absolute)
	 *   max err ~= 1.89936e-5
	 * Else then
	 *   fpminimax(asin(x), [|1,3,5,7|], [|SG...|], [1/sqrt(2);1], absolute)
	 *   max err ~= 3.085226e-2
	 */
	const int32_t TWO_OVER_PI = Q_CONVERT_FLOAT(0.63661977236758134f, qcl); /* 2/pi */
	const int32_t ONE_OVER_SQRT2 = Q_CONVERT_FLOAT(0.70710678118654752f, qcl); /* 1/sqrt(2) */
	const int32_t A7L = Q_CONVERT_FLOAT(0.1181826665997505187988281f, qcl);
	const int32_t A5L = Q_CONVERT_FLOAT(4.0224377065896987915039062e-2f, qcl);
	const int32_t A3L = Q_CONVERT_FLOAT(0.1721895635128021240234375f, qcl);
	const int32_t A1L = Q_CONVERT_FLOAT(0.99977016448974609375f, qcl);

	const int32_t A7H = Q_CONVERT_FLOAT(14.12774658203125f, qch);
	const int32_t A5H = Q_CONVERT_FLOAT(-30.1692714691162109375f, qch);
	const int32_t A3H = Q_CONVERT_FLOAT(21.4760608673095703125f, qch);
	const int32_t A1H = Q_CONVERT_FLOAT(-3.894591808319091796875f, qch);

	int32_t A7, A5, A3, A1, qc;
	int32_t x2, x4;
	int32_t A3Xx2, A7Xx2, asinx;

	int32_t xf = Q_CONVERT_FLOAT(x, 30);

	if (ABS(xf) <= ONE_OVER_SQRT2) {
		A7 = A7L;
		A5 = A5L;
		A3 = A3L;
		A1 = A1L;
		qc = qcl;
	} else {
		A7 = A7H;
		A5 = A5H;
		A3 = A3H;
		A1 = A1H;
		qc = qch;
		xf = Q_SHIFT_RND(xf, qcl, qch); /* Q6.26 */
	}

	x2 = q_multq(xf, xf, qc);
	x4 = q_multq(x2, x2, qc);

	A3Xx2 = q_multq(A3, x2, qc);
	A7Xx2 = q_multq(A7, x2, qc);

	asinx = q_multq(xf, (q_multq(x4, (A7Xx2 + A5), qc) + A3Xx2 + A1), qc);
	int32_t asinv = q_mult(asinx, TWO_OVER_PI, qc, qcl, 30);
	return Q_CONVERT_QTOF(asinv, 30);
#undef qch
#undef qcl
#else
	return asinf(x) * DRC_TWO_OVER_PI_FLOAT;
#endif
}

/*
 * Input x is Q6.26; valid range: (0.0, 32.0); x <= 0 is not supported
 *       y is Q2.30: (-2.0, 2.0)
 * Output is Q12.20: max 2048.0
 */
inline float warp_powf(float x, float y)
{
	/* Negative or zero input x is not supported, just return 0. */
	if (x <= 0)
		return 0;

#ifdef DRC_FIXED_POW
	int32_t yf = Q_CONVERT_FLOAT(y, 30);
	/* x^y = expf(y * log(x)) */
	int32_t logxf = Q_CONVERT_FLOAT(warp_logf(x), 26);
	int32_t powv = exp_fixed(q_mult(yf, logxf, 30, 26, 27));
	return Q_CONVERT_QTOF(powv, 20);
#else
	return powf(x, y);
#endif
}

/*
 * Input depends on precision_x
 * Output depends on precision_y
 */
inline float warp_inv(float x, int32_t precision_x, int32_t precision_y)
{
#ifdef DRC_FIXED_INV
#define qc 25
	/* Coefficients obtained from:
	 * fpminimax(1/x, 5, [|SG...|], [sqrt(2)/2;1], absolute);
	 * max err ~= 1.00388e-6
	 */
	const int32_t ONE_OVER_SQRT2 = Q_CONVERT_FLOAT(0.70710678118654752f, 30); /* 1/sqrt(2) */
	const int32_t SQRT2 = Q_CONVERT_FLOAT(1.4142135623730950488f, 30); /* sqrt(2) */
	const int32_t A5 = Q_CONVERT_FLOAT(-2.742647647857666015625f, qc);
	const int32_t A4 = Q_CONVERT_FLOAT(14.01327800750732421875f, qc);
	const int32_t A3 = Q_CONVERT_FLOAT(-29.74465179443359375f, qc);
	const int32_t A2 = Q_CONVERT_FLOAT(33.57208251953125f, qc);
	const int32_t A1 = Q_CONVERT_FLOAT(-21.25031280517578125f, qc);
	const int32_t A0 = Q_CONVERT_FLOAT(7.152250766754150390625f, qc);
	int32_t e;
	int32_t precision_inv;
	int32_t sqrt2_extracted = 0;
	int32_t x2, x4; /* Q2.30 */
	int32_t A5Xx, A3Xx;
	int32_t inv;

	int32_t xf = Q_CONVERT_FLOAT(x, precision_x);

	if (x >= 1.0 && precision_x == 31) {
		fprintf(stderr, "warp_inv(1.0) occurred!!\n");
		return 1.0;
	}

	if (xf == 0) {
		fprintf(stderr, "warp_inv(0) occurred!!\n");
		return 0;
	}

	xf = rexp_fixed(xf, precision_x, &e); /* Q2.30 */

	if (ABS(xf) < ONE_OVER_SQRT2) {
		xf = q_mult(xf, SQRT2, 30, 30, 30);
		sqrt2_extracted = 1;
	}

	x2 = q_mult(xf, xf, 30, 30, 30);
	x4 = q_mult(x2, x2, 30, 30, 30);
	A5Xx = q_mult(A5, xf, qc, 30, qc);
	A3Xx = q_mult(A3, xf, qc, 30, qc);
	inv = q_mult((A5Xx + A4), x4, qc, 30, qc) + q_mult((A3Xx + A2), x2, qc, 30, qc)
		+ q_mult(A1, xf, qc, 30, qc) + A0;

	if (sqrt2_extracted)
		inv = q_mult(inv, SQRT2, qc, 30, qc);

	precision_inv = e + qc;
	if (precision_inv > precision_y)
		inv = Q_SHIFT_RND(inv, precision_inv, precision_y);
	if (precision_inv < precision_y)
		inv = Q_SHIFT_LEFT(inv, precision_inv, precision_y);
	return Q_CONVERT_QTOF(inv, precision_y);
#undef qc
#else
	return 1.0 / x;
#endif
}

inline float knee_expf(float input)
{
#ifdef DRC_FIXED_EXP
	int32_t inputf = Q_CONVERT_FLOAT(input, 27);
	int32_t expv = exp_fixed(inputf); /* Q12.20 */
	return Q_CONVERT_QTOF(expv, 20);
#else
	return expf(input);
#endif
}

#undef q_multq
#undef q_mult
