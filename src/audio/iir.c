/*
 * Copyright (c) 2017, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the Intel Corporation nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
 *         Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 */

#include <stdint.h>
#include <stddef.h>
#include <errno.h>

#ifdef MODULE_TEST
#include <stdio.h>
#endif

#include <reef/audio/format.h>
#include "iir.h"

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
	int32_t in, tmp;
	int64_t acc;
	int32_t out = 0;
	int i, j;
	int d = 0; /* Index to delays */
	int c = 2; /* Index to coefficient a2 */

	/* Coefficients order in coef[] is {a2, a1, b2, b1, b0, shift, gain} */
	in = x;
	for (j = 0; j < iir->biquads; j += iir->biquads_in_series) {
		for (i = 0; i < iir->biquads_in_series; i++) {
			/* Compute output: Delay is Q3.61
			 * Q2.30 x Q1.31 -> Q3.61
			 * Shift Q3.61 to Q3.31 with rounding
			 */
			acc = ((int64_t) iir->coef[c + 4]) * in + iir->delay[d];
			tmp = (int32_t) Q_SHIFT_RND(acc, 61, 31);

			/* Compute 1st delay */
			acc = iir->delay[d + 1];
			acc += ((int64_t) iir->coef[c + 3]) * in; /* Coef  b1 */
			acc += ((int64_t) iir->coef[c + 1]) * tmp; /* Coef a1 */
			iir->delay[d] = acc;

			/* Compute 2nd delay */
			acc = ((int64_t) iir->coef[c + 2]) * in; /* Coef  b2 */
			acc += ((int64_t) iir->coef[c]) * tmp; /* Coef a2 */
			iir->delay[d + 1] = acc;

			/* Gain, output shift, prepare for next biquad
			 * Q2.14 x Q1.31 -> Q3.45, shift too Q3.31 and saturate
			 */
			acc = ((int64_t) iir->coef[c + 6]) * tmp; /* Gain */
			acc = Q_SHIFT_RND(acc, 45 + iir->coef[c + 5], 31);
			in = sat_int32(acc);
			c += 7; /* Next coefficients section */
			d += 2; /* Next biquad delays */
		}
		/* Output of previous section is in variable in */
		out = sat_int32((int64_t) out + in);
	}
	return out;
}

size_t iir_init_coef_df2t(struct iir_state_df2t *iir, int32_t config[])
{
	iir->mute = 0;
	iir->biquads = (int) config[0];
	iir->biquads_in_series = (int) config[1];
	iir->coef = &config[0]; /* TODO: Could change this to config[2] */
	iir->delay = NULL;

	if ((iir->biquads > IIR_DF2T_BIQUADS_MAX) || (iir->biquads < 1)) {
		iir_reset_df2t(iir);
		return -EINVAL;
	}

	return 2 * iir->biquads * sizeof(int64_t); /* Needed delay line size */
}

void iir_init_delay_df2t(struct iir_state_df2t *iir, int64_t **delay)
{
	iir->delay = *delay; /* Delay line of this IIR */
	*delay += 2 * iir->biquads; /* Point to next IIR delay line start */

}

void iir_mute_df2t(struct iir_state_df2t *iir)
{
	iir->mute = 1;
}

void iir_unmute_df2t(struct iir_state_df2t *iir)
{
	iir->mute = 0;
}

void iir_reset_df2t(struct iir_state_df2t *iir)
{
	iir->mute = 1;
	iir->biquads = 0;
	iir->biquads_in_series = 0;
	iir->coef = NULL;
	/* Note: May need to know the beginning of dynamic allocation after so
	 * omitting setting iir->delay to NULL.
	 */
}
