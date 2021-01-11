// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Google LLC. All rights reserved.
//
// Author: Pin-chih Lin <johnylin@google.com>

#include <sof/audio/drc/drc_math.h>
#include <sof/audio/format.h>
#include <sof/math/decibels.h>
#include <sof/math/numbers.h>
#include <sof/math/trig.h>

#define q_mult(a, b, qa, qb, qy) ((int32_t)Q_MULTSR_32X32((int64_t)(a), b, qa, qb, qy))
#define q_multq(a, b, q) ((int32_t)Q_MULTSR_32X32((int64_t)(a), b, q, q, q))

/*
 * Input depends on precision_x
 * Output range [0.5, 1); regulated to Q2.30
 */
static inline int32_t rexp_fixed(int32_t x, int precision_x, int *e)
{
	int bit = 31 - norm_int32(x);

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
	int e;
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

/*
 * Input is Q6.26: max 32.0
 * Output range ~ (-inf, 30.1030); regulated to Q11.21: (-1024.0, 1024.0)
 */
inline int32_t drc_lin2db_fixed(int32_t linear)
{
	int32_t log10_linear;

	/* For negative or zero, just return a very small dB value. */
	if (linear <= 0)
		return Q_CONVERT_FLOAT(-1000.0f, 21);

	log10_linear = log10_fixed(linear); /* Q6.26 */
	return q_mult(20, log10_linear, 0, 26, 21);
}

/*
 * Input is Q6.26: max 32.0
 * Output range ~ (-inf, 3.4657); regulated to Q6.26: (-32.0, 32.0)
 */
inline int32_t drc_log_fixed(int32_t x)
{
	const int32_t LOG10 = Q_CONVERT_FLOAT(2.3025850929940457f, 29);
	int32_t log10_x;

	if (x <= 0)
		return Q_CONVERT_FLOAT(-30.0f, 26);

	/* log(x) = log(10) * log10(x) */
	log10_x = log10_fixed(x); /* Q6.26 */
	return q_mult(LOG10, log10_x, 29, 26, 26);
}

/*
 * Input is Q2.30: (-2.0, 2.0)
 * Output range: (-1.0, 1.0); regulated to Q1.31: (-1.0, 1.0)
 */
inline int32_t drc_sin_fixed(int32_t x)
{
	const int32_t PI_OVER_TWO = Q_CONVERT_FLOAT(1.57079632679489661923f, 30);

	/* input range of sin_fixed() is non-negative */
	int32_t abs_sin_val = sin_fixed(q_mult(ABS(x), PI_OVER_TWO, 30, 30, 28));
	return SGN(x) < 0 ? -abs_sin_val : abs_sin_val;
}

/*
 * Input is Q2.30: (-2.0, 2.0)
 * Output range: [-1.0, 1.0]; regulated to Q2.30: (-2.0, 2.0)
 */
inline int32_t drc_asin_fixed(int32_t x)
{
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

	if (ABS(x) <= ONE_OVER_SQRT2) {
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
		x = Q_SHIFT_RND(x, qcl, qch); /* Q6.26 */
	}

	x2 = q_multq(x, x, qc);
	x4 = q_multq(x2, x2, qc);

	A3Xx2 = q_multq(A3, x2, qc);
	A7Xx2 = q_multq(A7, x2, qc);

	asinx = q_multq(x, (q_multq(x4, (A7Xx2 + A5), qc) + A3Xx2 + A1), qc);
	return q_mult(asinx, TWO_OVER_PI, qc, qcl, 30);
#undef qch
#undef qcl
}

/*
 * Input x is Q6.26: (-32.0, 32.0)
 *       y is Q2.30: (-2.0, 2.0)
 * Output is Q12.20: max 2048.0
 */
inline int32_t drc_pow_fixed(int32_t x, int32_t y)
{
	/* x^y = expf(y * log(x)) */
	return exp_fixed(q_mult(y, drc_log_fixed(x), 30, 26, 27));
}

/*
 * Input depends on precision_x
 * Output depends on precision_y
 */
inline int32_t drc_inv_fixed(int32_t x, int precision_x, int precision_y)
{
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
	int e;
	int sqrt2_extracted = 0;
	int32_t x2, x4; /* Q2.30 */
	int32_t A5Xx, A3Xx;
	int32_t inv;

	x = rexp_fixed(x, precision_x, &e); /* Q2.30 */

	if (x < ONE_OVER_SQRT2) {
		x = q_mult(x, SQRT2, 30, 30, 30);
		sqrt2_extracted = 1;
	}

	x2 = q_mult(x, x, 30, 30, 30);
	x4 = q_mult(x2, x2, 30, 30, 30);
	A5Xx = q_mult(A5, x, qc, 30, qc);
	A3Xx = q_mult(A3, x, qc, 30, qc);
	inv = q_mult((A5Xx + A4), x4, qc, 30, qc) + q_mult((A3Xx + A2), x2, qc, 30, qc)
		+ q_mult(A1, x, qc, 30, qc) + A0;

	if (sqrt2_extracted)
		inv = q_mult(inv, SQRT2, qc, 30, qc);

	e += qc;
	if (e > precision_y)
		return Q_SHIFT_RND(inv, e, precision_y);
	if (e < precision_y)
		return Q_SHIFT_LEFT(inv, e, precision_y);
	return inv;
#undef qc
}

#undef q_multq
#undef q_mult
