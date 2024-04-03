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

#include <rtos/symbol.h>

#if SOF_USE_HIFI(NONE, FILTER)

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
	int32_t in;
	int32_t tmp;
	int64_t out = 0;
	int64_t acc;
	int i;
	int j;
	int d = 0; /* Index to state */
	int c = 0; /* Index to coefficient a2 */
	int32_t *coefp = iir->coef;
	int32_t *delay = iir->delay;
	int nseries = iir->biquads_in_series;

	/* Bypass is set with number of biquads set to zero. */
	if (!iir->biquads)
		return x;

	/* Coefficients order in coef[] is {a2, a1, b2, b1, b0, shift, gain} */
	/* Delay order in state[] is {y(n - 2), y(n - 1), x(n - 2), x(n - 1)} */
	for (j = 0; j < iir->biquads; j += nseries) {
		in = x;
		for (i = 0; i < nseries; i++) {
			/* Compute output: Delay is Q3.61
			 * Q2.30 x Q1.31 -> Q3.61
			 * Shift Q3.61 to Q3.31 with rounding, saturate to Q1.31
			 */
			acc = ((int64_t)coefp[c]) * delay[d]; /* a2 * y(n - 2) */
			acc += ((int64_t)coefp[c + 1]) * delay[d + 1]; /* a1 * y(n - 1) */
			acc += ((int64_t)coefp[c + 2]) * delay[d + 2]; /* b2 * x(n - 2) */
			acc += ((int64_t)coefp[c + 3]) * delay[d + 3]; /* b1 * x(n - 1) */
			acc += ((int64_t)coefp[c + 4]) * in; /* b0 * x */
			tmp = (int32_t)sat_int32(Q_SHIFT_RND(acc, 61, 31));

			/* update the delay value */
			delay[d] = delay[d + 1];
			delay[d + 1] = tmp;
			delay[d + 2] = delay[d + 3];
			delay[d + 3] = in;

			/* Apply gain Q2.14 x Q1.31 -> Q3.45 */
			acc = ((int64_t)coefp[c + 6]) * tmp; /* Gain */

			/* Apply biquad output shift right parameter
			 * simultaneously with Q3.45 to Q3.31 conversion. Then
			 * saturate to 32 bits Q1.31 and prepare for next
			 * biquad.
			 */
			acc = Q_SHIFT_RND(acc, 45 + coefp[c + 5], 31);
			in = sat_int32(acc);

			/* Proceed to next biquad coefficients and delay
			 * lines.
			 */
			c += SOF_EQ_IIR_NBIQUAD;
			d += IIR_DF1_NUM_STATE;
		}
		/* Output of previous section is in variable in */
		out += (int64_t)in;
	}
	return sat_int32(out);
}
EXPORT_SYMBOL(iir_df1);

#endif
