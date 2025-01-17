// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2022-2025 Intel Corporation.
 *
 * Author: Shriram Shastry <malladi.sastry@linux.intel.com>
 *         Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
 *
 */

#include <sof/audio/format.h>
#include <sof/math/exp_fcn.h>
#include <sof/math/numbers.h>
#include <rtos/symbol.h>
#include <sof/common.h>
#include <stdint.h>

#if defined(EXPONENTIAL_GENERIC)

#define SOFM_EXP_ONE_OVER_LOG2_Q30	 1549082005	/* Q2.30 int32(round(1/log(2) * 2^30)) */
#define SOFM_EXP_LOG2_Q31		 1488522236	/* Q1.31 int32(round(log(2) * 2^31)) */
#define SOFM_EXP_FIXED_INPUT_MINUS8	-1073741824	/* Q5.27 int32(-8 * 2^27) */
#define SOFM_EXP_FIXED_INPUT_PLUS8	 1073741823	/* Q5.27 int32(8 * 2^27) */
#define SOFM_EXP_LOG10_DIV20_Q27	 15452387	/* Q5.27 int32(round(log(10)/20*2^27)) */

/* The table contains exponents of value v, where values of v
 * are the 3 bit 2's complement signed values presented by bits
 * of index 0..7.
 *
 * v = [(0:3)/8 (-4:-1)/8];
 * uint32(round(exp(v) * 2^31))
 */
static const uint32_t sofm_exp_3bit_lookup[8] = {
	2147483648, 2433417774, 2757423586, 3124570271,
	1302514674, 1475942488, 1672461947, 1895147668
};

/* Taylor polynomial coefficients for x^3..x^6, calculated
 * uint32(round(1 ./ factorial(3:6) * 2^32))
 */
static const uint32_t sofm_exp_taylor_coeffs[4] = {
	715827883, 178956971, 35791394, 5965232
};

/* function f(x) = e^x
 *
 * Arguments	: int32_t x (Q4.28)
 * input range -8 to 8
 *
 * Return Type	: int32_t (Q13.19)
 * output range 3.3546e-04 to 2981.0
 */
int32_t sofm_exp_approx(int32_t x)
{
	uint32_t taylor_first_2;
	uint32_t exp_a_b_32bit;
	uint32_t taylor_extra;
	uint32_t rnd_one;
	uint32_t b_f32;
	uint32_t b_pow;
	uint32_t exp_a;
	uint32_t exp_b;
	uint32_t term;
	uint32_t b;
	int32_t x_times_one_over_log2;
	int32_t e_times_log2;
	int32_t x_32bit;
	int32_t y_32bit;
	int32_t e;
	int32_t r;
	int shift_value;
	int a;

	/* -------------------------------------------------------------------------
	 * FIRST RANGE REDUCTION ---------------------------------------------------
	 * -------------------------------------------------------------------------
	 */
	x_times_one_over_log2 = Q_MULTSR_32X32((int64_t)x, SOFM_EXP_ONE_OVER_LOG2_Q30, 28, 30, 26);
	e = Q_SHIFT_RND(x_times_one_over_log2, 26, 0);

	/* Q6.31, but we only keep the bottom 31 bits */
	e_times_log2 = (uint32_t)e * SOFM_EXP_LOG2_Q31;

	/* -------------------------------------------------------------------------
	 * SECOND RANGE REDUCTION --------------------------------------------------
	 * y = a + b
	 * -------------------------------------------------------------------------
	 */
	x_32bit = (int32_t)((uint32_t)x << 3);	/* S4.31 */
	y_32bit = x_32bit - e_times_log2;	/* S0.31, in ~[-0.34, +0.34] */
	a = (y_32bit >> 28) & 7;		/* just the 3 top bits of "y" */
	b = y_32bit & 0x0FFFFFFF;		/* bottom 31-3 = 28 bits. format U-3.31 */
	exp_a = sofm_exp_3bit_lookup[a];
	b_f32 = (b << 1) | 0x4;			/* U0.32, align b on 32-bits of fraction */

	/* Taylor approximation : base part + iterations
	 * Base part      : 1 + b + b^2/2!
	 * Iterative part : b^3/3! + b^4/4! + b^5/5! + b^6/6!
	 *                : Term count determined dynamically using e.
	 *
	 * Base part: NOTE: delay adding the "1" in "1 + b + b^2/2" until after we
	 * add the iterative part in. This gives us one more guard bit.
	 * NOTE: u_int32 x u_int32 => {hi, lo}. We only need {hi} for b_pow.
	 */
	b_pow = (uint64_t)b_f32 * b_f32 >> 32;
	taylor_first_2 = b_f32 + (b_pow >> 1); /* 0.32 */
	taylor_extra = 0;
	term = 1;
	if (e < -10)
		goto ITER_END;

	b_pow = (uint64_t)b_f32 * b_pow >> 32;
	term = (uint64_t)b_pow * sofm_exp_taylor_coeffs[0] >> 32;
	taylor_extra += term;
	if (e < -5)
		goto ITER_END;

	b_pow = (uint64_t)b_f32 * b_pow >> 32;
	term = (uint64_t)b_pow * sofm_exp_taylor_coeffs[1] >> 32;
	taylor_extra += term;
	if (e < 0)
		goto ITER_END;

	b_pow = (uint64_t)b_f32 * b_pow >> 32;
	term = (uint64_t)b_pow * sofm_exp_taylor_coeffs[2] >> 32;
	taylor_extra += term;
	if (e < 6)
		goto ITER_END;

	b_pow = (uint64_t)b_f32 * b_pow >> 32;
	term = (uint64_t)b_pow * sofm_exp_taylor_coeffs[3] >> 32;
	taylor_extra += term;

ITER_END:

	/* Implement rounding to 31 fractional bits.. */
	taylor_first_2 = taylor_first_2 + taylor_extra + 1;

	/* Add the missing "1" for the Taylor series "1+b+b^2/2+...." */
	exp_b = ((uint32_t)1 << 31) + (taylor_first_2 >> 1); /* U1.31 */

	/* -------------------------------------------------------------------------
	 * FIRST RECONSTRUCTION ----------------------------------------------------
	 * -------------------------------------------------------------------------
	 */

	/* U1.31 * U1.31 = U2.62 */
	exp_a_b_32bit = (uint64_t)exp_a * exp_b >> 31;

	/* -------------------------------------------------------------------------
	 * SECOND RECONSTRUCTION ---------------------------------------------------
	 * -------------------------------------------------------------------------
	 */

	/* Rounding to nearest */
	shift_value = 12 - e;
	rnd_one = (shift_value > 0 ? (1 << (shift_value - 1)) : 0);
	exp_a_b_32bit += rnd_one;
	r = (int32_t)(exp_a_b_32bit >> shift_value);
	return r;
}
EXPORT_SYMBOL(sofm_exp_approx);

/* Fixed point exponent function for approximate range -16 .. 7.6
 * that corresponds to decibels range -120 .. +66 dB.
 *
 * The functions uses rule exp(x) = exp(x/2) * exp(x/2) to reduce
 * the input argument for the exponent function.
 *
 * Input  is Q5.27, -16.0 .. +16.0, but note the input range limitation
 * Output is Q12.20, 0.0 .. +2048.0
 */

int32_t sofm_exp_fixed(int32_t x)
{
	int32_t x0, y0, y1;

	if (x > SOFM_EXP_FIXED_INPUT_MAX)
		return INT32_MAX;

	if (x < SOFM_EXP_FIXED_INPUT_MINUS8 || x > SOFM_EXP_FIXED_INPUT_PLUS8) {
		/* Divide by 2, convert Q27 to Q28 is x as such */
		y0 = sofm_exp_approx(x);
		y1 = Q_MULTSR_32X32((int64_t)y0, y0, 19, 19, 20);
		return y1;
	}

	x0 = sat_int32((int64_t)x << 1);
	y0 = sofm_exp_approx(x0);
	return sat_int32((int64_t)y0 << 1);
}
EXPORT_SYMBOL(sofm_exp_fixed);

/* Decibels to linear conversion: The function uses exp() to calculate
 * the linear value. The argument is multiplied by log(10)/20 to
 * calculate equivalent of 10^(db/20).
 *
 * The error in conversion is less than 0.1 dB for -89..+66 dB range. Do not
 * use the code for argument less than -100 dB. The code simply returns zero
 * as linear value for such very small value.
 *
 * Input is Q8.24 (max 128.0)
 * output is Q12.20 (max 2048.0)
 */

int32_t sofm_db2lin_fixed(int32_t db)
{
	int32_t arg;

	if (db > SOFM_DB2LIN_INPUT_MAX)
		return INT32_MAX;

	/* Q8.24 x Q5.27, result needs to be Q5.27 */
	arg = (int32_t)Q_MULTSR_32X32((int64_t)db, SOFM_EXP_LOG10_DIV20_Q27, 24, 27, 27);
	return sofm_exp_fixed(arg);
}
EXPORT_SYMBOL(sofm_db2lin_fixed);

#endif /* EXPONENTIAL_GENERIC */
