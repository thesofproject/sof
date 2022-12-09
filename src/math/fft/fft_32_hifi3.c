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
	struct icomplex32 tmp1;
	ae_int32x2 *inx;
	ae_int32x2 *out;
	ae_int32x2 *outtop;
	ae_int32x2 *outbottom;
	ae_int32x2 *outx;
	ae_int32x2 sample;
	ae_int32x2 sample1;
	ae_int32x2 sample2;
	ae_int64 res, res1;
	int depth, top, bottom, index;
	int i, j, k, m, n;
	ae_int32 *in;
	ae_valign inu = AE_ZALIGN64();
	ae_valign outu = AE_ZALIGN64();
	int size = plan->size;
	int len = plan->len;

	if (!plan || !plan->bit_reverse_idx)
		return;

	if (!plan->inb32 || !plan->outb32)
		return;

	inx = (ae_int32x2 *)plan->inb32 + 1;
	outx = (ae_int32x2 *)plan->outb32;

	/* convert to complex conjugate for ifft */
	if (ifft) {
		in = (ae_int32 *)&plan->inb32->imag;
		for (i = 0; i < size; i++) {
			AE_L32_IP(sample, in, 0);
			sample = AE_NEG32S(sample);
			AE_S32_L_IP(sample, in, sizeof(struct icomplex32));
		}
	}

	/* step 1: re-arrange input in bit reverse order, and shrink the level to avoid overflow */
	inu = AE_LA64_PP(inx);
	for (i = 1; i < size; ++i) {
		AE_LA32X2_IP(sample, inu, inx);
		sample = AE_SRAA32S(sample, len);
		out = &outx[plan->bit_reverse_idx[i]];
		AE_SA32X2_IP(sample, outu, out);
	}
	AE_SA64POS_FP(outu, out);

	/* step 2: loop to do FFT transform in smaller size */
	for (depth = 1; depth <= len; ++depth) {
		m = 1 << depth;
		n = m >> 1;
		i = FFT_SIZE_MAX >> depth;

		/* doing FFT transforms in size m */
		for (k = 0; k < size; k += m) {
			/* doing one FFT transform for size m */
			for (j = 0; j < n; ++j) {
				index = i * j;
				top = k + j;
				bottom = top + n;
				tmp1.real = twiddle_real_32[index];
				tmp1.imag = twiddle_imag_32[index];
				inx = (ae_int32x2 *)&tmp1;
				AE_LA32X2_IP(sample1, inu, inx);
				/* calculate the accumulator: twiddle * bottom */
				sample2 = outx[bottom];
				res = AE_MULF32S_HH(sample1, sample2);
				AE_MULSF32S_LL(res, sample1, sample2);
				res1 = AE_MULF32S_HL(sample1, sample2);
				AE_MULAF32S_LH(res1, sample1, sample2);
				sample = AE_ROUND32X2F64SSYM(res, res1);

				sample1 = outx[top];
				/* calculate the top output: top = top + accumulate */
				sample2 = AE_ADD32S(sample1, sample);
				outtop = outx + top;
				AE_SA32X2_IP(sample2, outu, outtop);
				/* calculate the bottom output: bottom = top - accumulate */
				sample2 = AE_SUB32S(sample1, sample);
				outbottom = outx + bottom;
				AE_SA32X2_IP(sample2, outu, outbottom);
			}
		}
	}

	/* shift back for ifft */
	if (ifft) {
		/*
		 * no need to divide N as it is already done in the input side
		 * for Q1.31 format. Instead, we need to multiply N to compensate
		 * the shrink we did in the FFT transform.
		 */
		inx = outx;
		inu = AE_LA64_PP(inx);
		for (i = 0; i < size; ++i) {
			AE_LA32X2_IP(sample, inu, inx);
			sample = AE_SLAA32S(sample, len);
			AE_SA32X2_IP(sample, outu, outx);
		}
		AE_SA64POS_FP(outu, outx);
	}
}
#endif
