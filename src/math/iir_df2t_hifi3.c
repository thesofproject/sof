// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2017 Intel Corporation. All rights reserved.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
//         Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>

#include <stdint.h>
#include <stddef.h>
#include <errno.h>
#include <sof/audio/format.h>
#include <sof/math/iir_df2t.h>
#include <user/eq.h>

#if SOF_USE_MIN_HIFI(3, FILTER)

#include <xtensa/tie/xt_hifi3.h>

/*
 * Direct form II transposed second order filter block (biquad)
 *
 *              +----+                         +---+    +-------+
 * X(z) ---o--->| b0 |---> + -------------o--->| g |--->| shift |---> Y(z)
 *         |    +----+     ^              |    +---+    +-------+
 *         |               |              |
 *         |            +------+          |
 *         |            | z^-1 |          |
 *         |            +------+          |
 *         |               ^              |
 *         |    +----+     |     +----+   |
 *         o--->| b1 |---> + <---| a1 |---o
 *         |    +----+     ^     +----+   |
 *         |               |              |
 *         |            +------+          |
 *         |            | z^-1 |          |
 *         |            +------+          |
 *         |               ^              |
 *         |    +----+     |     +----+   |
 *         o--->| b2 |---> + <---| a2 |---+
 *              +----+           +----+
 *
 */

/* Series DF2T IIR */

/* 32 bit data, 32 bit coefficients and 64 bit state variables */

int32_t iir_df2t(struct iir_state_df2t *iir, int32_t x)
{
	ae_f64 acc;
	ae_valign align;
	ae_f32x2 coef_a2a1;
	ae_f32x2 coef_b2b1;
	ae_f32x2 coef_b0shift;
	ae_f32x2 gain;
	ae_f32 in;
	ae_f32 tmp;
	ae_f32x2 *coefp;
	ae_f64 *delayp;
	ae_f32 out = 0;
	int i;
	int j;
	int shift;
	int nseries = iir->biquads_in_series;

	/* Bypass is set with number of biquads set to zero. */
	if (!iir->biquads)
		return x;

	/* Coefficients order in coef[] is {a2, a1, b2, b1, b0, shift, gain} */
	coefp = (ae_f32x2 *)&iir->coef[0];
	delayp = (ae_f64 *)&iir->delay[0];
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
			align = AE_LA64_PP(coefp);
			AE_LA32X2_IP(coef_a2a1, align, coefp);
			AE_LA32X2_IP(coef_b2b1, align, coefp);
			AE_LA32X2_IP(coef_b0shift, align, coefp);
			AE_LA32X2_IP(gain, align, coefp);

			acc = AE_SRAI64(*delayp, 1); /* Convert d0 to Q18.46 */
			delayp++; /* Point to d1 */
			AE_MULAF32R_HH(acc, coef_b0shift, in); /* Coef b0 */
			acc = AE_SLAI64S(acc, 1); /* Convert to Q17.47 */
			tmp = AE_ROUND32F48SSYM(acc); /* Round to Q1.31 */

			/* Compute 1st delay d0 */
			acc = AE_SRAI64(*delayp, 1); /* Convert d1 to Q18.46 */
			delayp--; /* Point to d0 */
			AE_MULAF32R_LL(acc, coef_b2b1, in); /* Coef b1 */
			AE_MULAF32R_LL(acc, coef_a2a1, tmp); /* Coef a1 */
			acc = AE_SLAI64S(acc, 1); /* Convert to Q17.47 */
			*delayp = acc; /* Store d0 */
			delayp++; /* Point to d1 */

			/* Compute delay d1 */
			acc = AE_MULF32R_HH(coef_b2b1, in); /* Coef b2 */
			AE_MULAF32R_HH(acc, coef_a2a1, tmp); /* Coef a2 */
			acc = AE_SLAI64S(acc, 1); /* Convert to Q17.47 */
			*delayp = acc; /* Store d1 */

			/* Apply gain Q18.14 x Q1.31 -> Q34.30 */
			acc = AE_MULF32R_HH(gain, tmp); /* Gain */
			acc = AE_SLAI64S(acc, 17); /* Convert to Q17.47 */

			/* Apply biquad output shift right parameter and then
			 * round and saturate to 32 bits Q1.31.
			 */
			shift = AE_SEL32_LL(coef_b0shift, coef_b0shift);
			acc = AE_SRAA64(acc, shift);
			in = AE_ROUND32F48SSYM(acc);

			/* Proceed to next biquad coefficients and delay
			 * lines. The coefp needs rewind by one int32_t
			 * due to odd number of words in coefficient block.
			 */
			delayp++;
			coefp = (ae_f32x2 *)((int32_t *)coefp - 1);
		}
		/* Output of previous section is in variable in */
		out = AE_F32_ADDS_F32(out, in);
	}
	return out;
}

#endif
