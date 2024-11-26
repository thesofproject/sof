// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022-2024 Intel Corporation.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@intel.com>

#include <stdint.h>
#include <stddef.h>
#include <errno.h>
#include <sof/audio/format.h>
#include <sof/math/iir_df1.h>
#include <user/eq.h>
#include <sof/common.h>

#include <rtos/symbol.h>

#if SOF_USE_MIN_HIFI(5, FILTER)

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
	ae_valignx2 coef_align;
	ae_valignx2 data_r_align;
	ae_valignx2 data_w_align = AE_ZALIGN128();
	ae_f64 acc;
	ae_int32x2 delay_y2y1;
	ae_int32x2 delay_x2x1;
	ae_int32x2 coef_a2a1;
	ae_int32x2 coef_b2b1;
	ae_int32x2 coef_b0;
	ae_int32x2 gain;
	ae_int32x2 shift;
	ae_int32 in;
	ae_int32 out = 0;
	ae_int32x4 *coefp = (ae_int32x4 *)iir->coef;
	ae_int32x4 *delay_r  = (ae_int32x4 *)iir->delay;
	ae_int32x4 *delay_w = delay_r;
	int i;
	int j;
	int nseries = iir->biquads_in_series;

	/* Bypass is set with number of biquads set to zero. */
	if (!iir->biquads)
		return x;

	/* Coefficients order in coef[] is {a2, a1, b2, b1, b0, shift, gain} */
	/* Delay order in state[] is {y(n - 2), y(n - 1), x(n - 2), x(n - 1)} */
	data_r_align = AE_LA128_PP(delay_r);
	for (j = 0; j < iir->biquads; j += nseries) {
		in = x;
		for (i = 0; i < nseries; i++) {
			/* Load data */
			AE_LA32X2X2_IP(delay_y2y1, delay_x2x1, data_r_align, delay_r);

			/* Load coefficients */
			coef_align = AE_LA128_PP(coefp);
			AE_LA32X2X2_IP(coef_a2a1, coef_b2b1, coef_align, coefp);
			AE_L32_IP(coef_b0, (ae_int32 *)coefp, 4);
			AE_L32_IP(shift, (ae_int32 *)coefp, 4);
			AE_L32_IP(gain, (ae_int32 *)coefp, 4);

			acc = AE_MULF32RA_HH(coef_b0, in);		  /* acc = b0 * in */
			AE_MULAAFD32RA_HH_LL(acc, coef_a2a1, delay_y2y1); /* + a2 * y2 + a1 * y1 */
			AE_MULAAFD32RA_HH_LL(acc, coef_b2b1, delay_x2x1); /* + b2 * x2 + b1 * x1 */
			AE_PKSR32(delay_y2y1, acc, 1);		     /* y2 = y1, y1 = acc(q1.31) */
			delay_x2x1 = AE_SEL32_LL(delay_x2x1, in);   /* x2 = x1, x1 = in */

			/* Store data */
			AE_SA32X2X2_IP(delay_y2y1, delay_x2x1, data_w_align, delay_w);

			/* Apply gain */
			acc = AE_MULF32R_LL(gain, delay_y2y1);	/* acc = gain * y1 */
			acc = AE_SLAI64S(acc, 17);		/* Convert to Q17.47 */

			/* Apply biquad output shift right parameter and then
			 * round and saturate to 32 bits Q1.31.
			 */
			acc = AE_SRAA64(acc, shift);
			in = AE_ROUND32F48SSYM(acc);
		}
		/* Output of previous section is in variable in */
		out = AE_F32_ADDS_F32(out, in);
	}

	AE_SA128POS_FP(data_w_align, delay_w);
	return out;
}
EXPORT_SYMBOL(iir_df1);

#endif
