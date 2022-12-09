// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 Intel Corporation. All rights reserved.
//
// Author: Andrula Song <andrula.song@intel.com>

#include <sof/audio/format.h>
#include <sof/common.h>
#include <sof/math/fft.h>

#ifdef FFT_HIFI3
#include <sof/audio/coefficients/fft/twiddle_16.h>
#include <xtensa/tie/xt_hifi3.h>

/**
 * \brief Execute the 16-bits Fast Fourier Transform (FFT) or Inverse FFT (IFFT)
 *	  For the configured fft_pan.
 * \param[in] plan - pointer to fft_plan which will be executed.
 * \param[in] ifft - set to 1 for IFFT and 0 for FFT.
 */
void fft_execute_16(struct fft_plan *plan, bool ifft)
{
	struct icomplex16 *outb;
	ae_int16 *in;
	ae_int16x4 sample;
	ae_int32x2 res1, res2, res;
	ae_p16x2s *outs16;
	ae_int24x2 temp1, temp2;
	ae_int16 *out;
	ae_int16x4 *in16x4;
	ae_int16x4 *out16x4;
	ae_valign inu = AE_ZALIGN64();
	ae_valign outu = AE_ZALIGN64();
	int depth, top, bottom, index;
	int i, j, k, m, n;
	int size = plan->size;
	int len = plan->len;

	if (!plan || !plan->bit_reverse_idx)
		return;

	outb = plan->outb16;
	if (!plan->inb16 || !outb)
		return;

	/* convert to complex conjugate for ifft */
	if (ifft) {
		in = (ae_int16 *)&plan->inb16->imag;
		for (i = 0; i < size; i++) {
			AE_L16_IP(sample, in, 0);
			sample = AE_NEG16S(sample);
			AE_S16_0_IP(sample, in, sizeof(struct icomplex16));
		}
	}

	/* step 1: re-arrange input in bit reverse order, and shrink the level to avoid overflow */
	in = (ae_int16 *)&plan->inb16[1];
	for (i = 1; i < size ; ++i) {
		out = (ae_int16 *)&outb[plan->bit_reverse_idx[i]];
		AE_L16_IP(sample, in, 2);
		sample = AE_SRAA16RS(sample, len);
		AE_S16_0_IP(sample, out, 2);

		AE_L16_IP(sample, in, 2);
		sample = AE_SRAA16RS(sample, len);
		AE_S16_0_IP(sample, out, 2);
	}

	/* step 2: loop to do FFT transform in smaller size */
	for (depth = 1; depth <= plan->len; ++depth) {
		m = 1 << depth;
		n = m >> 1;
		i = FFT_SIZE_MAX >> depth;

		/* doing FFT transforms in size m */
		for (k = 0; k < plan->size; k += m) {
			/* doing one FFT transform for size m */
			for (j = 0; j < n; ++j) {
				index = i * j;
				top = k + j;
				bottom = top + n;
				/* store twiddle and bottom as Q9.23*/
				temp1 = AE_CVTP24A16X2_LL(outb[bottom].real, outb[bottom].imag);
				temp2 = AE_CVTP24A16X2_LL(twiddle_real_16[index],
							  twiddle_imag_16[index]);
				/* calculate the accumulator: twiddle * bottom */
				res = AE_MULFC24RA(temp1, temp2);
				/* saturate and round the result to 16bit and put it in
				 * the middle element of res.
				 */
				res2 = AE_SRAI32R(res, 8);
				res2 = AE_SLAI32S(res2, 8);
				/* store top format Q9.23*/
				res1 = AE_CVTP24A16X2_LL(outb[top].real, outb[top].imag);
				/* calculate the top output: top = top + accumulate */
				res = AE_ADD24S(res1, res2);
				outs16 = (ae_p16x2s *)&outb[top];
				/* store the middle 16bit into outb[top]*/
				AE_S16X2M_I(res, outs16, 0);
				/* calculate the bottom output: bottom = top - accumulate */
				outs16 = (ae_p16x2s *)&outb[bottom];
				res = AE_SUB24S(res1, res2);
				AE_S16X2M_I(res, outs16, 0);
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
		in16x4 = (ae_int16x4 *)plan->outb16;
		out16x4 = (ae_int16x4 *)plan->outb16;
		n = size >> 1;
		inu = AE_LA64_PP(in16x4);
		/* shift 2 samples per loop */
		for (i = 0; i < n; i++) {
			AE_LA16X4_IP(sample, inu, in16x4);
			sample = AE_SLAA16S(sample, len);
			AE_SA16X4_IP(sample, outu, out16x4);
		}
		AE_SA64POS_FP(outu, out16x4);
		/* if size is odd, shift the real & imag part respectively */
		if (size & 1) {
			in = (ae_int16 *)in16x4;
			out = (ae_int16 *)out16x4;
			AE_L16_IP(sample, in, 2);
			sample = AE_SLAA16S(sample, len);
			AE_S16_0_IP(sample, out, 2);
			AE_L16_IP(sample, in, 2);
			sample = AE_SLAA16S(sample, len);
			AE_S16_0_IP(sample, out, 2);
		}
	}
}
#endif
