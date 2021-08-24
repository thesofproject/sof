// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Google LLC. All rights reserved.
//
// Author: Pin-chih Lin <johnylin@google.com>

#include <sof/audio/drc/drc_math.h>
#include <sof/math/numbers.h>

#if DRC_HIFI3

#include <xtensa/tie/xt_hifi3.h>

#define ONE_OVER_SQRT2_Q30 759250112 /* Q_CONVERT_FLOAT(0.70710678118654752f, 30); 1/sqrt(2) */
#define LOG10_FUNC_A5_Q26 75959200 /* Q_CONVERT_FLOAT(1.131880283355712890625f, 26) */ 
#define LOG10_FUNC_A4_Q26 -285795039 /* Q_CONVERT_FLOAT(-4.258677959442138671875f, 26) */
#define LOG10_FUNC_A3_Q26 457435200 /* Q_CONVERT_FLOAT(6.81631565093994140625f, 26) */
#define LOG10_FUNC_A2_Q26 -410610303 /* Q_CONVERT_FLOAT(-6.1185703277587890625f, 26) */
#define LOG10_FUNC_A1_Q26 244982704 /* Q_CONVERT_FLOAT(3.6505267620086669921875f, 26) */
#define LOG10_FUNC_A0_Q26 -81731487 /* Q_CONVERT_FLOAT(-1.217894077301025390625f, 26) */
#define HALF_Q25 16777216 /* Q_CONVERT_FLOAT(0.5, 25) */
#define LOG10_2_Q26 20201782 /* Q_CONVERT_FLOAT(0.301029995663981195214f, 26) */
#define NEG_1K_Q21 -2097151999 /* Q_CONVERT_FLOAT(-1000.0f, 21) */
#define LOG_10_Q29 1236190976 /* Q_CONVERT_FLOAT(2.3025850929940457f, 29) */
#define NEG_30_Q26 -2013265919 /* Q_CONVERT_FLOAT(-30.0f, 26) */
#define ASIN_FUNC_A7L_Q30 126897672 /* Q_CONVERT_FLOAT(0.1181826665997505187988281f, 30) */
#define ASIN_FUNC_A5L_Q30 43190596 /* Q_CONVERT_FLOAT(4.0224377065896987915039062e-2f, 30) */
#define ASIN_FUNC_A3L_Q30 184887136 /* Q_CONVERT_FLOAT(0.1721895635128021240234375f, 30) */
#define ASIN_FUNC_A1L_Q30 1073495040 /* Q_CONVERT_FLOAT(0.99977016448974609375f, 30) */
#define ASIN_FUNC_A7H_Q26 948097024 /* Q_CONVERT_FLOAT(14.12774658203125f, 26) */
#define ASIN_FUNC_A5H_Q26 -2024625535 /* Q_CONVERT_FLOAT(-30.1692714691162109375f, 26) */
#define ASIN_FUNC_A3H_Q26 1441234048 /* Q_CONVERT_FLOAT(21.4760608673095703125f, 26) */
#define ASIN_FUNC_A1H_Q26 -261361631 /* Q_CONVERT_FLOAT(-3.894591808319091796875f, 26) */
#define SQRT2_Q30 1518500224 /* Q_CONVERT_FLOAT(1.4142135623730950488f, 30); sqrt(2) */
#define INV_FUNC_A5_Q25 -92027983 /* Q_CONVERT_FLOAT(-2.742647647857666015625f, 25) */
#define INV_FUNC_A4_Q25 470207584 /* Q_CONVERT_FLOAT(14.01327800750732421875f, 25) */
#define INV_FUNC_A3_Q25 -998064895 /* Q_CONVERT_FLOAT(-29.74465179443359375f, 25) */
#define INV_FUNC_A2_Q25 1126492160 /* Q_CONVERT_FLOAT(33.57208251953125f, 25) */
#define INV_FUNC_A1_Q25 -713042175 /* Q_CONVERT_FLOAT(-21.25031280517578125f, 25) */
#define INV_FUNC_A0_Q25 239989712 /* Q_CONVERT_FLOAT(7.152250766754150390625f, 25) */

/*
 * Input depends on precision_x
 * Output range [0.5, 1); regulated to Q2.30
 */
static inline ae_f32 rexp_fixed(ae_f32 x, int32_t precision_x, int32_t *e)
{
	int32_t bit = 31 - AE_NSAZ32_L(x);

	*e = bit - precision_x;

	return AE_SRAA32(x, bit - 30);
}

/*
 * Input is Q6.26: max 32.0
 * Output range ~ (-inf, 1.505); regulated to Q6.26: (-32.0, 32.0)
 */
static inline ae_f32 log10_fixed(ae_f32 x)
{
	/* Coefficients obtained from:
	 * fpminimax(log10(x), 5, [|SG...|], [1/2;sqrt(2)/2], absolute);
	 * max err ~= 6.088e-8
	 */
	int32_t e;
	int32_t lshift;
	ae_f32 exp; /* Q7.25 */
	ae_f32 acc; /* Q6.26 */
	ae_f32 tmp; /* Q6.26 */

	x = rexp_fixed(x, 26, &e); /* Q2.30 */
	exp = e << 25; /* Q_CONVERT_FLOAT(e, 25) */

	if ((int32_t)x > (int32_t)ONE_OVER_SQRT2_Q30) {
		lshift = drc_get_lshift(30, 30, 30);
		x = drc_mult_lshift(x, ONE_OVER_SQRT2_Q30, lshift);
		exp = AE_ADD32(exp, HALF_Q25);
	}

	lshift = drc_get_lshift(26, 30, 26);
	acc = drc_mult_lshift(LOG10_FUNC_A5_Q26, x, lshift);
	acc = AE_ADD32(acc, LOG10_FUNC_A4_Q26);
	acc = drc_mult_lshift(acc, x, lshift);
	acc = AE_ADD32(acc, LOG10_FUNC_A3_Q26);
	acc = drc_mult_lshift(acc, x, lshift);
	acc = AE_ADD32(acc, LOG10_FUNC_A2_Q26);
	acc = drc_mult_lshift(acc, x, lshift);
	acc = AE_ADD32(acc, LOG10_FUNC_A1_Q26);
	acc = drc_mult_lshift(acc, x, lshift);
	acc = AE_ADD32(acc, LOG10_FUNC_A0_Q26);

	lshift = drc_get_lshift(25, 26, 26);
	tmp = drc_mult_lshift(exp, LOG10_2_Q26, lshift);
	acc = AE_ADD32(acc, tmp);

	return acc;
}

/*
 * Input is Q6.26: max 32.0
 * Output range ~ (-inf, 30.1030); regulated to Q11.21: (-1024.0, 1024.0)
 */
inline int32_t drc_lin2db_fixed(int32_t linear)
{
	ae_f32 log10_linear;

	/* For negative or zero, just return a very small dB value. */
	if (linear <= 0)
		return NEG_1K_Q21;

	log10_linear = log10_fixed(linear); /* Q6.26 */
	return drc_mult_lshift(20 << 26, log10_linear, drc_get_lshift(26, 26, 21));
}

/*
 * Input is Q6.26: max 32.0
 * Output range ~ (-inf, 3.4657); regulated to Q6.26: (-32.0, 32.0)
 */
inline int32_t drc_log_fixed(int32_t x)
{
	ae_f32 log10_x;

	if (x <= 0)
		return NEG_30_Q26;

	/* log(x) = log(10) * log10(x) */
	log10_x = log10_fixed(x); /* Q6.26 */
	return drc_mult_lshift(LOG_10_Q29, log10_x, drc_get_lshift(29, 26, 26));
}

#ifndef DRC_USE_CORDIC_ASIN
/*
 * Input is Q2.30; valid range: [-1.0, 1.0]
 * Output range: [-1.0, 1.0]; regulated to Q2.30: (-2.0, 2.0)
 */
inline int32_t drc_asin_fixed(int32_t x)
{
	/* Coefficients obtained from:
	 * If x <= 1/sqrt(2), then
	 *   fpminimax(asin(x), [|1,3,5,7|], [|SG...|], [-1e-30;1/sqrt(2)], absolute)
	 *   max err ~= 1.89936e-5
	 * Else then
	 *   fpminimax(asin(x), [|1,3,5,7|], [|SG...|], [1/sqrt(2);1], absolute)
	 *   max err ~= 3.085226e-2
	 */
	int32_t lshift;
	ae_f32 in = x; /* Q2.30 */
	ae_f32 in2; /* Q2.30 */
	ae_f32 A7, A5, A3, A1;
	int32_t qc;
	ae_f32 acc;

	lshift = drc_get_lshift(30, 30, 30);
	in2 = drc_mult_lshift(in, in, lshift);

	if (ABS((int32_t)in) <= ONE_OVER_SQRT2_Q30) {
		A7 = ASIN_FUNC_A7L_Q30;
		A5 = ASIN_FUNC_A5L_Q30;
		A3 = ASIN_FUNC_A3L_Q30;
		A1 = ASIN_FUNC_A1L_Q30;
		qc = 30;
	} else {
		A7 = ASIN_FUNC_A7H_Q26;
		A5 = ASIN_FUNC_A5H_Q26;
		A3 = ASIN_FUNC_A3H_Q26;
		A1 = ASIN_FUNC_A1H_Q26;
		qc = 26;
	}

	lshift = drc_get_lshift(qc, 30, qc);
	acc = drc_mult_lshift(A7, in2, lshift);
	acc = AE_ADD32(acc, A5);
	acc = drc_mult_lshift(acc, in2, lshift);
	acc = AE_ADD32(acc, A3);
	acc = drc_mult_lshift(acc, in2, lshift);
	acc = AE_ADD32(acc, A1);
	acc = drc_mult_lshift(acc, in, lshift);
	lshift = drc_get_lshift(qc, 30, 30);
	return drc_mult_lshift(acc, TWO_OVER_PI_Q30, lshift);
}
#endif /* !DRC_USE_CORDIC_ASIN */

/*
 * Input depends on precision_x
 * Output depends on precision_y
 */
inline int32_t drc_inv_fixed(int32_t x, int32_t precision_x, int32_t precision_y)
{
	/* Coefficients obtained from:
	 * fpminimax(1/x, 5, [|SG...|], [sqrt(2)/2;1], absolute);
	 * max err ~= 1.00388e-6
	 */
	ae_f32 in;
	int32_t lshift;
	int32_t e;
	int32_t precision_inv;
	int32_t sqrt2_extracted = 0;
	ae_f32 acc;

	in = rexp_fixed(x, precision_x, &e); /* Q2.30 */

	if (ABS((int32_t)in) < ONE_OVER_SQRT2_Q30) {
		lshift = drc_get_lshift(30, 30, 30);
		in = drc_mult_lshift(in, SQRT2_Q30, lshift);
		sqrt2_extracted = 1;
	}

	lshift = drc_get_lshift(25, 30, 25);
	acc = drc_mult_lshift(INV_FUNC_A5_Q25, in, lshift);
	acc = AE_ADD32(acc, INV_FUNC_A4_Q25);
	acc = drc_mult_lshift(acc, in, lshift);
	acc = AE_ADD32(acc, INV_FUNC_A3_Q25);
	acc = drc_mult_lshift(acc, in, lshift);
	acc = AE_ADD32(acc, INV_FUNC_A2_Q25);
	acc = drc_mult_lshift(acc, in, lshift);
	acc = AE_ADD32(acc, INV_FUNC_A1_Q25);
	acc = drc_mult_lshift(acc, in, lshift);
	acc = AE_ADD32(acc, INV_FUNC_A0_Q25);

	if (sqrt2_extracted)
		acc = drc_mult_lshift(acc, SQRT2_Q30, lshift);

	precision_inv = e + 25;
	acc = AE_SLAA32S(acc, precision_y - precision_inv);
	return acc;
}

#endif /* DRC_HIFI3 */
