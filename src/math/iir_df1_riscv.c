// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Sound Open Firmware. All rights reserved.

#include <stdint.h>
#include <stddef.h>
#include <errno.h>
#include <sof/audio/format.h>
#include <sof/math/iir_df1.h>
#include <user/eq.h>
#include <sof/common.h>
#include <rtos/symbol.h>

#if SOF_USE_RISCV_SIMD(FILTER)

#define ROUND_BIT(b) ((b) > 0 ? (1ULL << ((b) - 1)) : 0)

static inline int32_t sat_clamp_q31(int64_t x)
{
	if (__builtin_expect(x >= (int64_t)INT32_MIN && x <= (int64_t)INT32_MAX, 1))
		return (int32_t)x;
	return (x > INT32_MAX) ? INT32_MAX : INT32_MIN;
}

/* Series DF1 IIR optimized for RISC-V */
int32_t iir_df1(struct iir_state_df1 *iir, int32_t x)
{
	int32_t in;
	int32_t tmp;
	int64_t out = 0;
	int64_t acc;
	int i;
	int j;
	int d = 0;
	int c = 0;
	const int32_t *coefp = iir->coef;
	int32_t *delay = iir->delay;
	const int nseries = iir->biquads_in_series;

	if (!iir->biquads)
		return x;

	for (j = 0; j < (int)iir->biquads; j += nseries) {
		in = x;
		for (i = 0; i < nseries; i++) {
			/* Load state and coefficients */
			const int32_t a2 = coefp[c];
			const int32_t a1 = coefp[c + 1];
			const int32_t b2 = coefp[c + 2];
			const int32_t b1 = coefp[c + 3];
			const int32_t b0 = coefp[c + 4];
			const int32_t shift_param = coefp[c + 5];
			const int32_t gain = coefp[c + 6];

			const int32_t y_n2 = delay[d];
			const int32_t y_n1 = delay[d + 1];
			const int32_t x_n2 = delay[d + 2];
			const int32_t x_n1 = delay[d + 3];

			/* 5-term MAC accumulator */
			acc = ((int64_t)a2) * y_n2;
			acc += ((int64_t)a1) * y_n1;
			acc += ((int64_t)b2) * x_n2;
			acc += ((int64_t)b1) * x_n1;
			acc += ((int64_t)b0) * in;

			/* Shift Q3.61 to Q3.31 with symmetric rounding */
			acc = (acc + (1LL << 29)) >> 30;
			tmp = sat_clamp_q31(acc);

			/* Update delay line */
			delay[d] = y_n1;
			delay[d + 1] = tmp;
			delay[d + 2] = x_n1;
			delay[d + 3] = in;

			/* Apply biquad gain & shift */
			acc = ((int64_t)gain) * tmp;
			const int shift_bits = 14 + shift_param;
			acc = (acc + (1LL << (shift_bits - 1))) >> shift_bits;
			in = sat_clamp_q31(acc);

			c += SOF_EQ_IIR_NBIQUAD;
			d += IIR_DF1_NUM_STATE;
		}
		out += in;
	}
	return sat_clamp_q31(out);
}
EXPORT_SYMBOL(iir_df1);

/* Specialized 4th order (2 biquads in series) for RISC-V */
int32_t iir_df1_4th(struct iir_state_df1 *iir, int32_t x)
{
	int32_t in = x;
	int32_t tmp;
	int64_t acc;
	const int32_t *coefp = iir->coef;
	int32_t *delay = iir->delay;

	/* Biquad 0 */
	{
		const int32_t a2 = coefp[0];
		const int32_t a1 = coefp[1];
		const int32_t b2 = coefp[2];
		const int32_t b1 = coefp[3];
		const int32_t b0 = coefp[4];
		const int32_t shift_param = coefp[5];
		const int32_t gain = coefp[6];

		const int32_t y_n2 = delay[0];
		const int32_t y_n1 = delay[1];
		const int32_t x_n2 = delay[2];
		const int32_t x_n1 = delay[3];

		acc = ((int64_t)a2) * y_n2;
		acc += ((int64_t)a1) * y_n1;
		acc += ((int64_t)b2) * x_n2;
		acc += ((int64_t)b1) * x_n1;
		acc += ((int64_t)b0) * in;

		acc = (acc + (1LL << 29)) >> 30;
		tmp = sat_clamp_q31(acc);

		delay[0] = y_n1;
		delay[1] = tmp;
		delay[2] = x_n1;
		delay[3] = in;

		acc = ((int64_t)gain) * tmp;
		const int shift_bits = 14 + shift_param;
		acc = (acc + (1LL << (shift_bits - 1))) >> shift_bits;
		in = sat_clamp_q31(acc);
	}

	/* Biquad 1 */
	{
		const int32_t a2 = coefp[7];
		const int32_t a1 = coefp[8];
		const int32_t b2 = coefp[9];
		const int32_t b1 = coefp[10];
		const int32_t b0 = coefp[11];
		const int32_t shift_param = coefp[12];
		const int32_t gain = coefp[13];

		const int32_t y_n2 = delay[4];
		const int32_t y_n1 = delay[5];
		const int32_t x_n2 = delay[6];
		const int32_t x_n1 = delay[7];

		acc = ((int64_t)a2) * y_n2;
		acc += ((int64_t)a1) * y_n1;
		acc += ((int64_t)b2) * x_n2;
		acc += ((int64_t)b1) * x_n1;
		acc += ((int64_t)b0) * in;

		acc = (acc + (1LL << 29)) >> 30;
		tmp = sat_clamp_q31(acc);

		delay[4] = y_n1;
		delay[5] = tmp;
		delay[6] = x_n1;
		delay[7] = in;

		acc = ((int64_t)gain) * tmp;
		const int shift_bits = 14 + shift_param;
		acc = (acc + (1LL << (shift_bits - 1))) >> shift_bits;
		in = sat_clamp_q31(acc);
	}

	return in;
}
EXPORT_SYMBOL(iir_df1_4th);

#endif /* SOF_USE_RISCV_SIMD(FILTER) */

