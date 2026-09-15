// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Sound Open Firmware. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>

#include <stdint.h>
#include <stddef.h>
#include <errno.h>
#include <sof/audio/format.h>
#include <sof/audio/arm_simd.h>
#include <sof/math/iir_df2t.h>
#include <user/eq.h>
#include <sof/common.h>
#include <rtos/symbol.h>

#if SOF_USE_ARM_SIMD(FILTER)

int32_t iir_df2t(struct iir_state_df2t *iir, int32_t x)
{
	int32_t in;
	int32_t tmp;
	int64_t acc;
	int64_t out = 0;
	int i;
	int j;
	int d = 0;
	int c = 0;
	const int32_t *coefp = iir->coef;
	int64_t *delay = iir->delay;
	const int nseries = iir->biquads_in_series;

	if (!iir->biquads)
		return x;

	for (j = 0; j < (int)iir->biquads; j += nseries) {
		in = x;
		for (i = 0; i < nseries; i++) {
			const int32_t a2 = coefp[c];
			const int32_t a1 = coefp[c + 1];
			const int32_t b2 = coefp[c + 2];
			const int32_t b1 = coefp[c + 3];
			const int32_t b0 = coefp[c + 4];
			const int32_t shift_param = coefp[c + 5];
			const int32_t gain = coefp[c + 6];

			/* Compute output */
			acc = delay[d];
			acc = arm_smlal(acc, b0, in);
			acc = (acc + (1LL << 29)) >> 30;
			tmp = arm_sat_q31(acc);

			/* Compute first delay */
			acc = delay[d + 1];
			acc = arm_smlal(acc, b1, in);
			acc = arm_smlal(acc, a1, tmp);
			delay[d] = acc;

			/* Compute second delay */
			acc = arm_smlal(0, b2, in);
			acc = arm_smlal(acc, a2, tmp);
			delay[d + 1] = acc;

			/* Apply gain & shift */
			acc = ((int64_t)gain) * tmp;
			const int shift_bits = 14 + shift_param;
			acc = (acc + (1LL << (shift_bits - 1))) >> shift_bits;
			in = arm_sat_q31(acc);

			c += SOF_EQ_IIR_NBIQUAD;
			d += IIR_DF2T_NUM_DELAYS;
		}
		out += in;
	}
	return arm_sat_q31(out);
}
EXPORT_SYMBOL(iir_df2t);

#endif /* SOF_USE_ARM_SIMD(FILTER) */
