// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 Intel Corporation. All rights reserved.
//
// Author: Andrula Song <andrula.song@intel.com>

#include <sof/audio/format.h>
#include <sof/common.h>
#include <rtos/alloc.h>
#include <sof/math/fft.h>

#ifdef FFT_HIFI3
#include <sof/audio/coefficients/fft/twiddle_32.h>
#include <xtensa/tie/xt_hifi3.h>

void fft_execute_32(struct fft_plan *plan, bool ifft)
{
	ae_int64 res, res1;
	ae_int32x2 sample;
	ae_int32x2 sample1;
	ae_int32x2 sample2;
	ae_int32x2 tw;
	ae_int32x2 *inx = (ae_int32x2 *)plan->inb32;
	ae_int32x2 *outx = (ae_int32x2 *)plan->outb32;
	ae_int32x2 *top_ptr;
	ae_int32x2 *bot_ptr;
	uint16_t *idx = &plan->bit_reverse_idx[0];
	const int32_t *tw_r;
	const int32_t *tw_i;
	int depth, i;
	int j, k, m, n;
	int size = plan->size;
	int len = plan->len;

	if (!plan || !plan->bit_reverse_idx)
		return;

	if (!plan->inb32 || !plan->outb32)
		return;

	/* step 1: re-arrange input in bit reverse order, and shrink the level to avoid overflow */
	if (ifft) {
		/* convert to complex conjugate for ifft */
		for (i = 0; i < size; ++i) {
			AE_L32X2_IP(sample, inx, sizeof(ae_int32x2));
			sample = AE_SRAA32S(sample, len);
			sample1 = AE_NEG32S(sample);
			sample = AE_SEL32_HL(sample, sample1);
			AE_S32X2_X(sample, outx, idx[i] * sizeof(ae_int32x2));
		}
	} else {
		for (i = 0; i < size; ++i) {
			AE_L32X2_IP(sample, inx, sizeof(ae_int32x2));
			sample = AE_SRAA32S(sample, len);
			AE_S32X2_X(sample, outx, idx[i] * sizeof(ae_int32x2));
		}
	}

	/*
	 * Step 2a: First FFT stage (depth=1, m=2, n=1).
	 * All butterflies use twiddle factor W^0 = 1+0j,
	 * so the complex multiply is skipped entirely.
	 */
	top_ptr = outx;
	bot_ptr = outx + 1;
	for (k = 0; k < size; k += 2) {
		sample1 = AE_L32X2_I(top_ptr, 0);
		sample2 = AE_L32X2_I(bot_ptr, 0);
		sample = AE_ADD32S(sample1, sample2);
		AE_S32X2_I(sample, top_ptr, 0);
		sample = AE_SUB32S(sample1, sample2);
		AE_S32X2_I(sample, bot_ptr, 0);
		top_ptr += 2;
		bot_ptr += 2;
	}

	/* Step 2b: Remaining FFT stages (depth >= 2) */
	for (depth = 2; depth <= len; ++depth) {
		m = 1 << depth;
		n = m >> 1;
		i = FFT_SIZE_MAX >> depth;

		top_ptr = outx;
		bot_ptr = outx + n;

		/* doing FFT transforms in size m */
		for (k = 0; k < size; k += m) {
			/*
			 * j=0: twiddle factor W^0 = 1+0j,
			 * butterfly without complex multiply.
			 */
			sample1 = AE_L32X2_I(top_ptr, 0);
			sample = AE_L32X2_I(bot_ptr, 0);
			sample2 = AE_ADD32S(sample1, sample);
			AE_S32X2_I(sample2, top_ptr, 0);
			sample2 = AE_SUB32S(sample1, sample);
			AE_S32X2_I(sample2, bot_ptr, 0);
			top_ptr++;
			bot_ptr++;

			/* j=1..n-1: full butterfly with twiddle multiply */
			tw_r = &twiddle_real_32[i];
			tw_i = &twiddle_imag_32[i];
			for (j = 1; j < n; ++j) {
				/* load and combine twiddle factor {real, imag} into tw */
				tw = AE_MOVDA32X2(tw_r[0], tw_i[0]);

				/* calculate the accumulator: twiddle * bottom */
				sample2 = AE_L32X2_I(bot_ptr, 0);
				res = AE_MULF32S_HH(tw, sample2);
				AE_MULSF32S_LL(res, tw, sample2);
				res1 = AE_MULF32S_HL(tw, sample2);
				AE_MULAF32S_LH(res1, tw, sample2);
				sample = AE_ROUND32X2F64SSYM(res, res1);
				sample1 = AE_L32X2_I(top_ptr, 0);

				/* calculate the top output: top = top + accumulate */
				sample2 = AE_ADD32S(sample1, sample);
				AE_S32X2_I(sample2, top_ptr, 0);

				/* calculate the bottom output: bottom = top - accumulate */
				sample2 = AE_SUB32S(sample1, sample);
				AE_S32X2_I(sample2, bot_ptr, 0);

				top_ptr++;
				bot_ptr++;
				tw_r += i;
				tw_i += i;
			}
			/* advance pointers past current group's bottom half */
			top_ptr += n;
			bot_ptr += n;
		}
	}

	/* shift back for ifft */
	if (ifft) {
		/*
		 * no need to divide N as it is already done in the input side
		 * for Q1.31 format. Instead, we need to multiply N to compensate
		 * the shrink we did in the FFT transform. Also make complex
		 * conjugate by negating the imaginary part.
		 */
		inx = outx;
		for (i = 0; i < size; ++i) {
			AE_L32X2_IP(sample, inx, sizeof(ae_int32x2));
			sample = AE_SLAA32S(sample, len);
			sample1 = AE_NEG32S(sample);
			sample = AE_SEL32_HL(sample, sample1);
			AE_S32X2_IP(sample, outx, sizeof(ae_int32x2));
		}
	}
}
#endif
