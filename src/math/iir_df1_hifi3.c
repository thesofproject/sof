// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 Intel Corporation. All rights reserved.
//
// Author: Andrula Song <andrula.song@intel.com>

#include <stdint.h>
#include <stddef.h>
#include <errno.h>
#include <sof/audio/format.h>
#include <sof/math/iir_df1.h>
#include <user/eq.h>
#include <sof/common.h>

#include <rtos/symbol.h>

#if SOF_USE_HIFI(3, FILTER)

/*
 * Direct form I second order filter block (biquad)
 *
 *              +----+                            +---+    +-------+
 * X(z) ---o--->| b0 |---> + --+-------------o--->| g |--->| shift |---> Y(z)
 *         |    +----+     ^   ^             |    +---+    +-------+
 *         |               |   |             |
 *     +------+            |   |          +------+
 *     | z^-1 |            |   |          | z^-1 |
 *     +------+            |   |          +------+
 *         |    +----+     |   |     +----+   |
 *         o--->| b1 |---> +   + <---| a1 |---o
 *         |    +----+     ^   ^     +----+   |
 *         |               |   |              |
 *     +------+            |   |          +------+
 *     | z^-1 |            |   |          | z^-1 |
 *     +------+            |   |          +------+
 *         |               ^   ^              |
 *         |    +----+     |   |     +----+   |
 *         o--->| b2 |---> +   +<--- | a2 |---o
 *              +----+               +----+
 *
 * y[n] = b0 * x[n] + b1 * x[n-1] + b2 * x[n-2] + a1 * y[n-1] + a2 * y[n-2]
 * the a1 a2 has been negated during calculation
 */

/* Series DF1 IIR */

/* 32 bit data, 32 bit coefficients and 32 bit state variables */

int32_t iir_df1(struct iir_state_df1 *iir, int32_t x)
{
	ae_int64 acc;
	ae_valign coef_align;
	ae_int32x2 coef_a2a1;
	ae_int32x2 coef_b2b1;
	ae_int32x2 coef_b0;
	ae_int32x2 gain;
	ae_int32x2 shift;
	ae_int32x2 delay_y2y1;
	ae_int32x2 delay_x2x1;
	ae_int32 in;
	ae_int32 tmp;
	ae_int32x2 *coefp;
	ae_int32x2 *delayp;
	ae_int32 out = 0;
	int32_t *delay_update;
	int i;
	int j;
	int nseries = iir->biquads_in_series;

	/* Bypass is set with number of biquads set to zero. */
	if (!iir->biquads)
		return x;

	/* Coefficients order in coef[] is {a2, a1, b2, b1, b0, shift, gain} */
	coefp = (ae_int32x2 *)&iir->coef[0];
	delayp = (ae_int32x2 *)&iir->delay[0];
	for (j = 0; j < iir->biquads; j += nseries) {
		/* the first for loop is for parallel EQs, and they have the same input */
		in = x;
		for (i = 0; i < nseries; i++) {
			/* Compute output: Delay is kept Q17.47 while multiply
			 * instruction gives Q2.30 x Q1.31 -> Q18.46. Need to
			 * shift delay line values right by one for same align
			 * as MAC. Store to delay line need to be shifted left
			 * by one similarly.
			 */
			coef_align = AE_LA64_PP(coefp);
			AE_LA32X2_IP(coef_a2a1, coef_align, coefp);
			AE_LA32X2_IP(coef_b2b1, coef_align, coefp);
			AE_L32_IP(coef_b0, (ae_int32 *)coefp, 4);
			AE_L32_IP(shift, (ae_int32 *)coefp, 4);
			AE_L32_IP(gain, (ae_int32 *)coefp, 4);

			AE_L32X2_IP(delay_y2y1, delayp, 8);
			AE_L32X2_IP(delay_x2x1, delayp, 8);

			acc = AE_MULF32R_HH(coef_a2a1, delay_y2y1); /* a2 * y(n - 2) */
			AE_MULAF32R_LL(acc, coef_a2a1, delay_y2y1); /* a1 * y(n - 1) */
			AE_MULAF32R_HH(acc, coef_b2b1, delay_x2x1); /* b2 * x(n - 2) */
			AE_MULAF32R_LL(acc, coef_b2b1, delay_x2x1); /* b1 * x(n - 1) */
			AE_MULAF32R_HH(acc, coef_b0, in); /*  b0 * x  */
			acc = AE_SLAI64S(acc, 1); /* Convert to Q17.47 */
			tmp = AE_ROUND32F48SSYM(acc); /* Round to Q1.31 */

			/* update the state value */
			delay_update = (int32_t *)delayp - 4;
			delay_update[0] = delay_update[1];
			delay_update[1] = tmp;
			delay_update[2] = delay_update[3];
			delay_update[3] = in;

			/* Apply gain Q18.14 x Q1.31 -> Q34.30 */
			acc = AE_MULF32R_HH(gain, tmp); /* Gain */
			acc = AE_SLAI64S(acc, 17); /* Convert to Q17.47 */

			/* Apply biquad output shift right parameter and then
			 * round and saturate to 32 bits Q1.31.
			 */
			acc = AE_SRAA64(acc, shift);
			in = AE_ROUND32F48SSYM(acc);
		}
		/* Output of previous section is in variable in */
		out = AE_F32_ADDS_F32(out, in);
	}
	return out;
}
EXPORT_SYMBOL(iir_df1);

int32_t iir_df1_4th(struct iir_state_df1 *iir, int32_t x)
{
	ae_int64 acc;
	ae_valign coef_align;
	ae_int32x2 coef_a2a1;
	ae_int32x2 coef_b2b1;
	ae_int32x2 coef_b0;
	ae_int32x2 gain;
	ae_int32x2 shift;
	ae_int32x2 delay_y2y1;
	ae_int32x2 delay_x2x1;
	ae_int32 in;
	ae_int32 tmp;
	ae_int32x2 *coefp;
	ae_int32x2 *delayp;
	int32_t *delay_update;
	int i;

	/* Coefficients order in coef[] is {a2, a1, b2, b1, b0, shift, gain} */
	coefp = (ae_int32x2 *)&iir->coef[0];
	delayp = (ae_int32x2 *)&iir->delay[0];
	in = x;
	for (i = 0; i < SOF_IIR_DF1_4TH_NUM_BIQUADS; i++) {
		/* Compute output: Delay is kept Q17.47 while multiply
		 * instruction gives Q2.30 x Q1.31 -> Q18.46. Need to
		 * shift delay line values right by one for same align
		 * as MAC. Store to delay line need to be shifted left
		 * by one similarly.
		 */
		coef_align = AE_LA64_PP(coefp);
		AE_LA32X2_IP(coef_a2a1, coef_align, coefp);
		AE_LA32X2_IP(coef_b2b1, coef_align, coefp);
		AE_L32_IP(coef_b0, (ae_int32 *)coefp, 4);
		AE_L32_IP(shift, (ae_int32 *)coefp, 4);
		AE_L32_IP(gain, (ae_int32 *)coefp, 4);

		AE_L32X2_IP(delay_y2y1, delayp, 8);
		AE_L32X2_IP(delay_x2x1, delayp, 8);

		acc = AE_MULF32R_HH(coef_a2a1, delay_y2y1); /* a2 * y(n - 2) */
		AE_MULAF32R_LL(acc, coef_a2a1, delay_y2y1); /* a1 * y(n - 1) */
		AE_MULAF32R_HH(acc, coef_b2b1, delay_x2x1); /* b2 * x(n - 2) */
		AE_MULAF32R_LL(acc, coef_b2b1, delay_x2x1); /* b1 * x(n - 1) */
		AE_MULAF32R_HH(acc, coef_b0, in); /*  b0 * x  */
		acc = AE_SLAI64S(acc, 1); /* Convert to Q17.47 */
		tmp = AE_ROUND32F48SSYM(acc); /* Round to Q1.31 */

		/* update the state value */
		delay_update = (int32_t *)delayp - 4;
		delay_update[0] = delay_update[1];
		delay_update[1] = tmp;
		delay_update[2] = delay_update[3];
		delay_update[3] = in;

		/* Apply gain Q18.14 x Q1.31 -> Q34.30 */
		acc = AE_MULF32R_HH(gain, tmp); /* Gain */
		acc = AE_SLAI64S(acc, 17); /* Convert to Q17.47 */

		/* Apply biquad output shift right parameter and then
		 * round and saturate to 32 bits Q1.31.
		 */
		acc = AE_SRAA64(acc, shift);
		in = AE_ROUND32F48SSYM(acc);
	}
	return in;
}
EXPORT_SYMBOL(iir_df1_4th);

#endif
