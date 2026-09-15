// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Sound Open Firmware. All rights reserved.

#include <sof/audio/format.h>
#include <sof/math/decibels.h>
#include <sof/math/exp_fcn.h>
#include <sof/math/numbers.h>
#include <sof/math/trig.h>
#include <sof/common.h>

#include "drc_math.h"

#if SOF_USE_RISCV_SIMD(DRC)

#define ONE_OVER_SQRT2_Q30 759250112 /* Q_CONVERT_FLOAT(0.70710678118654752f, 30); 1/sqrt(2) */
#define SQRT2_Q30          1518500224 /* Q_CONVERT_FLOAT(1.4142135623730950488f, 30); sqrt(2) */
#define LOG10_FUNC_A5_Q26  75959200   /* Q_CONVERT_FLOAT(1.131880283355712890625f, 26) */
#define LOG10_FUNC_A4_Q26 -285795039  /* Q_CONVERT_FLOAT(-4.258677959442138671875f, 26) */
#define LOG10_FUNC_A3_Q26  457435200  /* Q_CONVERT_FLOAT(6.81631565093994140625f, 26) */
#define LOG10_FUNC_A2_Q26 -410610303  /* Q_CONVERT_FLOAT(-6.1185703277587890625f, 26) */
#define LOG10_FUNC_A1_Q26  244982704  /* Q_CONVERT_FLOAT(3.6505267620086669921875f, 26) */
#define LOG10_FUNC_A0_Q26 -81731487   /* Q_CONVERT_FLOAT(-1.217894077301025390625f, 26) */
#define HALF_Q25           16777216   /* Q_CONVERT_FLOAT(0.5, 25) */
#define LOG10_2_Q26        20201782   /* Q_CONVERT_FLOAT(0.301029995663981195214f, 26) */
#define NEG_1K_Q21        -2097151999 /* Q_CONVERT_FLOAT(-1000.0f, 21) */
#define LOG_10_Q29         1236190976 /* Q_CONVERT_FLOAT(2.3025850929940457f, 29) */
#define NEG_30_Q26        -2013265919 /* Q_CONVERT_FLOAT(-30.0f, 26) */
#define DRC_TWENTY_Q26     1342177280 /* Q_CONVERT_FLOAT(20, 26) */

#define ASIN_FUNC_A7L_Q30  126897672  /* Q_CONVERT_FLOAT(0.1181826665997505187988281f, 30) */
#define ASIN_FUNC_A5L_Q30  43190596   /* Q_CONVERT_FLOAT(4.0224377065896987915039062e-2f, 30) */
#define ASIN_FUNC_A3L_Q30  184887136  /* Q_CONVERT_FLOAT(0.1721895635128021240234375f, 30) */
#define ASIN_FUNC_A1L_Q30  1073495040 /* Q_CONVERT_FLOAT(0.99977016448974609375f, 30) */
#define ASIN_FUNC_A7H_Q26  948097024  /* Q_CONVERT_FLOAT(14.12774658203125f, 26) */
#define ASIN_FUNC_A5H_Q26 -2024625535 /* Q_CONVERT_FLOAT(-30.1692714691162109375f, 26) */
#define ASIN_FUNC_A3H_Q26  1441234048 /* Q_CONVERT_FLOAT(21.4760608673095703125f, 26) */
#define ASIN_FUNC_A1H_Q26 -261361631  /* Q_CONVERT_FLOAT(-3.894591808319091796875f, 26) */

#define INV_FUNC_A5_Q25   -92027983   /* Q_CONVERT_FLOAT(-2.742647647857666015625f, 25) */
#define INV_FUNC_A4_Q25    470207584  /* Q_CONVERT_FLOAT(14.01327800750732421875f, 25) */
#define INV_FUNC_A3_Q25   -998064895  /* Q_CONVERT_FLOAT(-29.74465179443359375f, 25) */
#define INV_FUNC_A2_Q25    1126492160 /* Q_CONVERT_FLOAT(33.57208251953125f, 25) */
#define INV_FUNC_A1_Q25   -713042175  /* Q_CONVERT_FLOAT(-21.25031280517578125f, 25) */
#define INV_FUNC_A0_Q25    239989712  /* Q_CONVERT_FLOAT(7.152250766754150390625f, 25) */

static inline int32_t rexp_fixed_riscv(int32_t x, int32_t precision_x, int32_t *e)
{
	int32_t bit = 31 - norm_int32(x);

	*e = bit - precision_x;

	if (bit > 30)
		return (x + (1 << (bit - 31))) >> (bit - 30);
	if (bit < 30)
		return x << (30 - bit);
	return x;
}

static inline int32_t log10_fixed(int32_t x)
{
	int32_t e;
	int32_t exp;
	int32_t acc;
	const int32_t lshift = drc_get_lshift(26, 30, 26);

	x = rexp_fixed_riscv(x, 26, &e);
	exp = e << 25;

	if (x > ONE_OVER_SQRT2_Q30) {
		x = drc_mult_lshift(x, ONE_OVER_SQRT2_Q30, drc_get_lshift(30, 30, 30));
		exp += HALF_Q25;
	}

	/* Horner's rule: A0 + x*(A1 + x*(A2 + x*(A3 + x*(A4 + x*A5)))) */
	acc = drc_mult_lshift(LOG10_FUNC_A5_Q26, x, lshift) + LOG10_FUNC_A4_Q26;
	acc = drc_mult_lshift(acc, x, lshift) + LOG10_FUNC_A3_Q26;
	acc = drc_mult_lshift(acc, x, lshift) + LOG10_FUNC_A2_Q26;
	acc = drc_mult_lshift(acc, x, lshift) + LOG10_FUNC_A1_Q26;
	acc = drc_mult_lshift(acc, x, lshift) + LOG10_FUNC_A0_Q26;

	acc += drc_mult_lshift(exp, LOG10_2_Q26, drc_get_lshift(25, 26, 26));
	return acc;
}

int32_t drc_lin2db_fixed(int32_t linear)
{
	int32_t log10_linear;

	if (linear <= 0)
		return NEG_1K_Q21;

	log10_linear = log10_fixed(linear);
	return drc_mult_lshift(DRC_TWENTY_Q26, log10_linear, drc_get_lshift(26, 26, 21));
}

int32_t drc_log_fixed(int32_t x)
{
	int32_t log10_x;

	if (x <= 0)
		return NEG_30_Q26;

	log10_x = log10_fixed(x);
	return drc_mult_lshift(LOG_10_Q29, log10_x, drc_get_lshift(29, 26, 26));
}

#ifndef DRC_USE_CORDIC_ASIN
int32_t drc_asin_fixed(int32_t x)
{
	int32_t lshift;
	int32_t x_abs = x < 0 ? -x : x;
	int32_t in2;
	int32_t A7, A5, A3, A1;
	int32_t qc;
	int32_t acc;

	in2 = drc_mult_lshift(x, x, drc_get_lshift(30, 30, 30));

	if (x_abs <= ONE_OVER_SQRT2_Q30) {
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
	acc = drc_mult_lshift(A7, in2, lshift) + A5;
	acc = drc_mult_lshift(acc, in2, lshift) + A3;
	acc = drc_mult_lshift(acc, in2, lshift) + A1;
	acc = drc_mult_lshift(acc, x, lshift);

	return drc_mult_lshift(acc, TWO_OVER_PI_Q30, drc_get_lshift(qc, 30, 30));
}
#endif /* !DRC_USE_CORDIC_ASIN */

int32_t drc_inv_fixed(int32_t x, int32_t precision_x, int32_t precision_y)
{
	int32_t e;
	int32_t precision_inv;
	int32_t sqrt2_extracted = 0;
	int32_t acc;
	const int32_t lshift = drc_get_lshift(25, 30, 25);

	x = rexp_fixed_riscv(x, precision_x, &e);

	if ((x < 0 ? -x : x) < ONE_OVER_SQRT2_Q30) {
		x = drc_mult_lshift(x, SQRT2_Q30, drc_get_lshift(30, 30, 30));
		sqrt2_extracted = 1;
	}

	/* Horner's rule for reciprocal */
	acc = drc_mult_lshift(INV_FUNC_A5_Q25, x, lshift) + INV_FUNC_A4_Q25;
	acc = drc_mult_lshift(acc, x, lshift) + INV_FUNC_A3_Q25;
	acc = drc_mult_lshift(acc, x, lshift) + INV_FUNC_A2_Q25;
	acc = drc_mult_lshift(acc, x, lshift) + INV_FUNC_A1_Q25;
	acc = drc_mult_lshift(acc, x, lshift) + INV_FUNC_A0_Q25;

	if (sqrt2_extracted)
		acc = drc_mult_lshift(acc, SQRT2_Q30, lshift);

	precision_inv = e + 25;
	if (precision_inv > precision_y) {
		int shift = precision_inv - precision_y;
		return (acc + (1 << (shift - 1))) >> shift;
	}
	if (precision_inv < precision_y) {
		int shift = precision_y - precision_inv;
		int64_t val = ((int64_t)acc) << shift;
		if (val > INT32_MAX)
			return INT32_MAX;
		if (val < INT32_MIN)
			return INT32_MIN;
		return (int32_t)val;
	}
	return acc;
}

#endif /* SOF_USE_RISCV_SIMD(DRC) */
