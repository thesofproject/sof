// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2017 Intel Corporation. All rights reserved.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
//         Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>

#include <sof/audio/eq_iir/iir.h>
#include <sof/audio/format.h>
#include <user/eq.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

#if IIR_GENERIC

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

int32_t iir_df2t(struct iir_state_df2t *iir, int32_t x)
{
	int32_t in;
	int32_t out = 0;
	int i;
	int j;
	int d = 0; /* Index to delays */
	int c = 0; /* Index to coefficient a2 */

	/* Bypass is set with number of biquads set to zero. */
	if (!iir->biquads)
		return x;

	/* Coefficients order in coef[] is {a2, a1, b2, b1, b0, shift, gain} */
	in = x;
	for (j = 0; j < iir->biquads; j += iir->biquads_in_series) {
		for (i = 0; i < iir->biquads_in_series; i++) {
			in = iir_df2t_biquad(in, &iir->coef[c], &iir->delay[d]);
			/* Proceed to next biquad coefficients and delay
			 * lines.
			 */
			c += SOF_EQ_IIR_NBIQUAD_DF2T;
			d += IIR_DF2T_NUM_DELAYS;
		}
		/* Output of previous section is in variable in */
		out = sat_int32((int64_t)out + in);
	}
	return out;
}

#endif

