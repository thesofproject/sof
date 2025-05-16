// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Google LLC. All rights reserved.
//
// Author: Pin-chih Lin <johnylin@google.com>

#include <sof/math/numbers.h>
#include <sof/common.h>
#include "drc_math.h"

#if SOF_USE_MIN_HIFI(3, DRC)

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
#define INV_FUNC_ONE_Q30 1073741824 /* Q_CONVERT_FLOAT(1, 30) */
#define SHIFT_IDX_QX30_QY30_QZ30 1 /* drc_get_lshift(30, 30, 30) */
#define SHIFT_IDX_QX26_QY30_QZ26 1 /* drc_get_lshift(26, 30, 26) */
#define SHIFT_IDX_QX25_QY30_QZ25 1 /* drc_get_lshift(25, 30, 25) */
#define SHIFT_IDX_QX29_QY26_QZ26 2 /* drc_get_lshift(29, 26, 26) */
#define SHIFT_IDX_QX25_QY26_QZ26 6 /* drc_get_lshift(25, 26, 26) */
#define DRC_TWENTY_Q26 1342177280   /* Q_CONVERT_FLOAT(20, 26) */

#define DRC_PACK_INT32X2_TO_UINT64(a, b) (((uint64_t)((int64_t)(a) & 0xffffffff) << 32) | \
						((uint64_t)((int64_t)(b) & 0xffffffff)))

static const uint64_t drc_inv_func_coefficients[] = {
	DRC_PACK_INT32X2_TO_UINT64(INV_FUNC_A2_Q25, INV_FUNC_A5_Q25),
	DRC_PACK_INT32X2_TO_UINT64(INV_FUNC_A1_Q25, INV_FUNC_A4_Q25),
	DRC_PACK_INT32X2_TO_UINT64(INV_FUNC_A0_Q25, INV_FUNC_A3_Q25)
};

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
	int bit = 31 - AE_NSAZ32_L(x);
	int32_t e = bit - 26;
	ae_f32 exp; /* Q7.25 */
	ae_f32 acc; /* Q6.26 */
	ae_f32 tmp; /* Q6.26 */
	ae_f64 tmp64;

	/*Output range [0.5, 1); regulated to Q2.30 */
	x = AE_SRAA32(x, bit - 30);
	exp = e << 25; /* Q_CONVERT_FLOAT(e, 25) */

	if ((int32_t)x > (int32_t)ONE_OVER_SQRT2_Q30) {
		tmp64 = AE_MULF32R_LL(x, ONE_OVER_SQRT2_Q30);
		tmp64 = AE_SLAI64S(tmp64, SHIFT_IDX_QX30_QY30_QZ30);
		x = AE_ROUND32F48SSYM(tmp64);
		exp = AE_ADD32(exp, HALF_Q25);
	}

	tmp64 = AE_MULF32R_LL(LOG10_FUNC_A5_Q26, x);
	tmp64 = AE_SLAI64S(tmp64, SHIFT_IDX_QX26_QY30_QZ26);
	acc = AE_ROUND32F48SSYM(tmp64);
	acc = AE_ADD32(acc, LOG10_FUNC_A4_Q26);
	tmp64 = AE_MULF32R_LL(acc, x);
	tmp64 = AE_SLAI64S(tmp64, SHIFT_IDX_QX26_QY30_QZ26);
	acc = AE_ROUND32F48SSYM(tmp64);
	acc = AE_ADD32(acc, LOG10_FUNC_A3_Q26);
	tmp64 = AE_MULF32R_LL(acc, x);
	tmp64 = AE_SLAI64S(tmp64, SHIFT_IDX_QX26_QY30_QZ26);
	acc = AE_ROUND32F48SSYM(tmp64);
	acc = AE_ADD32(acc, LOG10_FUNC_A2_Q26);
	tmp64 = AE_MULF32R_LL(acc, x);
	tmp64 = AE_SLAI64S(tmp64, SHIFT_IDX_QX26_QY30_QZ26);
	acc = AE_ROUND32F48SSYM(tmp64);
	acc = AE_ADD32(acc, LOG10_FUNC_A1_Q26);
	tmp64 = AE_MULF32R_LL(acc, x);
	tmp64 = AE_SLAI64S(tmp64, SHIFT_IDX_QX26_QY30_QZ26);
	acc = AE_ROUND32F48SSYM(tmp64);
	acc = AE_ADD32(acc, LOG10_FUNC_A0_Q26);
	tmp64 = AE_MULF32R_LL(exp, LOG10_2_Q26);
	tmp64 = AE_SLAI64S(tmp64, SHIFT_IDX_QX25_QY26_QZ26);
	tmp = AE_ROUND32F48SSYM(tmp64);
	acc = AE_ADD32(acc, tmp);

	return acc;
}

/*
 * Input is Q6.26: max 32.0
 * Output range ~ (-inf, 30.1030); regulated to Q11.21: (-1024.0, 1024.0)
 */
int32_t drc_lin2db_fixed(int32_t linear)
{
	ae_f32 log10_linear;
	ae_f64 tmp;
	ae_f32 y;

	/* For negative or zero, just return a very small dB value. */
	if (linear <= 0)
		return NEG_1K_Q21;

	log10_linear = log10_fixed(linear); /* Q6.26 */

	tmp = AE_MULF32R_LL(DRC_TWENTY_Q26, log10_linear);
	/*Don't need to do AE_SLAA64S since drc_get_lshift(26, 26, 21) = 0 */
	y = AE_ROUND32F48SSYM(tmp);
	return AE_MOVAD32_L(y);
}

/*
 * Input is Q6.26: max 32.0
 * Output range ~ (-inf, 3.4657); regulated to Q6.26: (-32.0, 32.0)
 */
int32_t drc_log_fixed(int32_t x)
{
	ae_f32 log10_x;
	ae_f64 tmp;
	ae_f32 y;

	if (x <= 0)
		return NEG_30_Q26;

	/* log(x) = log(10) * log10(x) */
	log10_x = log10_fixed(x); /* Q6.26 */
	tmp = AE_MULF32R_LL(LOG_10_Q29, log10_x);
	tmp = AE_SLAI64S(tmp, SHIFT_IDX_QX29_QY26_QZ26);
	y = AE_ROUND32F48SSYM(tmp);
	return AE_MOVAD32_L(y);
	}

#ifndef DRC_USE_CORDIC_ASIN
/*
 * Input is Q2.30; valid range: [-1.0, 1.0]
 * Output range: [-1.0, 1.0]; regulated to Q2.30: (-2.0, 2.0)
 */
int32_t drc_asin_fixed(int32_t x)
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
	int32_t x_abs = AE_MOVAD32_L(AE_ABS32S(x));
	ae_f32 in2; /* Q2.30 */
	ae_f32 A7, A5, A3, A1;
	int32_t qc;
	ae_f32 acc;
	ae_f64 tmp;

	tmp = AE_MULF32R_LL(x, x);
	tmp = AE_SLAI64S(tmp, SHIFT_IDX_QX30_QY30_QZ30);
	in2 = AE_ROUND32F48SSYM(tmp);

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
	tmp = AE_MULF32R_LL(A7, in2);
	tmp = AE_SLAA64S(tmp, lshift);
	acc = AE_ROUND32F48SSYM(tmp);
	acc = AE_ADD32(acc, A5);
	tmp = AE_MULF32R_LL(acc, in2);
	tmp = AE_SLAA64S(tmp, lshift);
	acc = AE_ROUND32F48SSYM(tmp);
	acc = AE_ADD32(acc, A3);
	tmp = AE_MULF32R_LL(acc, in2);
	tmp = AE_SLAA64S(tmp, lshift);
	acc = AE_ROUND32F48SSYM(tmp);
	acc = AE_ADD32(acc, A1);
	tmp = AE_MULF32R_LL(acc, x);
	tmp = AE_SLAA64S(tmp, lshift);
	acc = AE_ROUND32F48SSYM(tmp);
	lshift = drc_get_lshift(qc, 30, 30);
	tmp = AE_MULF32R_LL(acc, TWO_OVER_PI_Q30);
	tmp = AE_SLAA64S(tmp, lshift);
	acc = AE_ROUND32F48SSYM(tmp);
	return AE_MOVAD32_L(acc);
}
#endif /* !DRC_USE_CORDIC_ASIN */

/*
 * Input depends on precision_x
 * Output depends on precision_y
 */
int32_t drc_inv_fixed(int32_t x, int32_t precision_x, int32_t precision_y)
{
	/* Coefficients obtained from:
	 * fpminimax(1/x, 5, [|SG...|], [sqrt(2)/2;1], absolute);
	 * max err ~= 1.00388e-6
	 */
	__aligned(8) ae_f32 coef[2];
	ae_f64 tmp;
	ae_f32x2 in;
	ae_f32x2 p1p0;
	ae_f32 acc;
	ae_f32x2 *coefp;
	int32_t in_abs;
	int shift_input;
	int shift_output;
	int sqrt2_extracted = 0;

	/* Output range [0.5, 1); regulated to Q2.30
	 *
	 * Note: input and output shift code was originally as more self-documenting
	 *   bit = 31 - AE_NSAZ32_L(x); input_shift = bit - 30;
	 *   e = bit - precision_x; precision_inv = e + 25;
	 *   output_shift = precision_y - precision_inv;
	 */

	shift_input = 1 - AE_NSAZ32_L(x);
	shift_output = precision_y + precision_x - shift_input - 55;
	in = AE_SRAA32(x, shift_input);
	in_abs = AE_MOVAD32_L(AE_ABS32S(in));

	if (in_abs < ONE_OVER_SQRT2_Q30) {
		tmp = AE_MULF32R_LL(in, SQRT2_Q30);
		tmp = AE_SLAI64S(tmp, SHIFT_IDX_QX30_QY30_QZ30);
		in = AE_ROUND32F48SSYM(tmp);
		sqrt2_extracted = 1;
	}

	/* calculate p00(x) = a1 + a2 * x, p11(x) = a4 + a5 * x
	 *
	 * In Q25 coef * Q30 in 32 bit AE multiply the result is Q24 (25 + 30 + 1 - 32),
	 * so every multiply is shifted left by one to keep results as Q25.
	 */
	coefp = (ae_f32x2 *)drc_inv_func_coefficients;
	p1p0 = AE_SLAI32S(AE_MULFP32X2RS(*coefp, in), SHIFT_IDX_QX25_QY30_QZ25);
	coefp++;
	p1p0 = AE_ADD32S(p1p0, *coefp);

	/* calculate p0(x) = a0 + p00(x) * x, p1(x) = a3 + p11(x) * x */
	p1p0 = AE_SLAI32S(AE_MULFP32X2RS(p1p0, in), SHIFT_IDX_QX25_QY30_QZ25);
	coefp++;
	p1p0 = AE_ADD32S(p1p0, *coefp);

	/* calculate p1(x) * x^3
	 *
	 * Q30 * Q30 AE multiply gives Q61. shifting it low 32 bits as Q30 would
	 * need right shift by 31. For Q17,47 AE round instead it is shifted 16 bits
	 * less, so shift right by 15.
	 */
	acc = AE_ROUND32F48SASYM(AE_SRAA64(AE_MULF32S_HH(in, in), 15));
	acc = AE_ROUND32F48SASYM(AE_SRAA64(AE_MULF32S_HH(acc, in), 15));
	coef[1] = INV_FUNC_ONE_Q30;
	coef[0] = acc;
	p1p0 = AE_SLAI32S(AE_MULFP32X2RS(p1p0, *(ae_int32x2 *)coef), SHIFT_IDX_QX25_QY30_QZ25);

	/* calculate p(x) = p0(x) + p1(x) * x^3
	 * p1p0.l holds p0(x) after multiply with one
	 * p1p0.h holds p1(x) * x^3
	 */
	acc = AE_MOVAD32_L(AE_ADD32_HL_LH(p1p0, p1p0));

	if (sqrt2_extracted) {
		tmp = AE_MULF32R_LL(SQRT2_Q30, acc);
		tmp = AE_SLAI64S(tmp, SHIFT_IDX_QX25_QY30_QZ25);
		acc = AE_ROUND32F48SSYM(tmp);
	}

	acc = AE_SLAA32S(acc, shift_output);
	return AE_MOVAD32_L(acc);
}

#endif /* DRC_HIFI_3/4 */
