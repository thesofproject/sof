// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2025-2026 Intel Corporation.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

/**
 * @file fft_multi_hifi3.c
 * @brief HiFi3 optimized multi-radix FFT functions.
 *
 * This file provides HiFi3 optimized implementations of dft3_32() and
 * fft_multi_execute_32() using Xtensa HiFi3 intrinsics. These replace
 * the generic versions when HiFi3 or HiFi4 hardware is available.
 *
 * Key optimizations over the generic versions:
 * - Packed {real, imag} processing using ae_int32x2 registers
 * - 64-bit MAC accumulation for fused complex multiply-add
 * - Saturating 32-bit arithmetic via AE_ADD32S/AE_SLAA32S
 * - Vectorized conjugate, shift, and scale in the IFFT path
 */

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/format.h>
#include <sof/math/icomplex32.h>
#include <sof/common.h>
#include <sof/math/fft.h>
#include <string.h>

/* Twiddle factor tables defined in fft_multi.c via twiddle_3072_32.h */
#define FFT_MULTI_TWIDDLE_SIZE 2048
extern const int32_t multi_twiddle_real_32[];
extern const int32_t multi_twiddle_imag_32[];

#ifdef FFT_HIFI3

#include <xtensa/tie/xt_hifi3.h>

/** @brief Q1.31 constant -0.5 */
#define DFT3_COEFR -1073741824
/** @brief Q1.31 constant sqrt(3)/2 */
#define DFT3_COEFI 1859775393
/** @brief Q1.31 constant 1/3 */
#define DFT3_SCALE 715827883

/**
 * dft3_32() - Compute 3-point DFT of Q1.31 complex data (HiFi3).
 * @param x_in Pointer to 3 input complex samples in Q1.31.
 * @param y Pointer to 3 output complex samples in Q1.31.
 *
 * Computes the DFT matrix-vector product:
 *
 *      | 1   1   1 |
 * Y =  | 1  c0  c1 | * X / 3
 *      | 1  c1  c0 |
 *
 * where c0 = exp(-j*2*pi/3) and c1 = exp(+j*2*pi/3).
 * Input is prescaled by 1/3 to prevent overflow.
 *
 * The two complex multiplies for each output bin are fused
 * into a single 64-bit accumulator pair, avoiding intermediate
 * rounding and saving instructions.
 */
void dft3_32(struct icomplex32 *x_in, struct icomplex32 *y)
{
	ae_int32x2 *p_in = (ae_int32x2 *)x_in;
	ae_int32x2 *p_out = (ae_int32x2 *)y;
	ae_int32x2 x0, x1, x2;
	ae_int32x2 c0, c1;
	ae_int32x2 scale;
	ae_int32x2 sum, result;
	ae_int64 re, im;

	/*
	 * Set up DFT3 twiddle factors as packed {H=real, L=imag}.
	 * c0 = {-0.5, -sqrt(3)/2}
	 * c1 = {-0.5, +sqrt(3)/2}
	 */
	scale = AE_MOVDA32(DFT3_COEFR);
	c0 = AE_SEL32_LH(scale, AE_MOVDA32(-DFT3_COEFI));
	c1 = AE_SEL32_LH(scale, AE_MOVDA32(DFT3_COEFI));

	/* Scale factor 1/3, broadcast to both H and L */
	scale = AE_MOVDA32(DFT3_SCALE);

	/* Load input samples as packed {real, imag} */
	x0 = AE_L32X2_I(p_in, 0);
	x1 = AE_L32X2_I(p_in, 1 * sizeof(ae_int32x2));
	x2 = AE_L32X2_I(p_in, 2 * sizeof(ae_int32x2));

	/* Scale all inputs by 1/3 to prevent overflow */
	x0 = AE_MULFP32X2RS(x0, scale);
	x1 = AE_MULFP32X2RS(x1, scale);
	x2 = AE_MULFP32X2RS(x2, scale);

	/* y[0] = x[0] + x[1] + x[2] */
	sum = AE_ADD32S(x0, x1);
	AE_S32X2_I(AE_ADD32S(sum, x2), p_out, 0);

	/*
	 * y[1] = x[0] + c0 * x[1] + c1 * x[2]
	 *
	 * Fuse two complex multiplies and their sum into a single
	 * 64-bit accumulator pair to avoid intermediate rounding.
	 *
	 * Real part: c0.re*x1.re - c0.im*x1.im + c1.re*x2.re - c1.im*x2.im
	 * Imag part: c0.re*x1.im + c0.im*x1.re + c1.re*x2.im + c1.im*x2.re
	 */
	re = AE_MULF32S_HH(c0, x1); /* c0.re * x1.re */
	AE_MULSF32S_LL(re, c0, x1); /* -= c0.im * x1.im */
	AE_MULAF32S_HH(re, c1, x2); /* += c1.re * x2.re */
	AE_MULSF32S_LL(re, c1, x2); /* -= c1.im * x2.im */

	im = AE_MULF32S_HL(c0, x1); /* c0.re * x1.im */
	AE_MULAF32S_LH(im, c0, x1); /* += c0.im * x1.re */
	AE_MULAF32S_HL(im, c1, x2); /* += c1.re * x2.im */
	AE_MULAF32S_LH(im, c1, x2); /* += c1.im * x2.re */

	result = AE_ROUND32X2F64SSYM(re, im);
	AE_S32X2_I(AE_ADD32S(x0, result), p_out, sizeof(ae_int32x2));

	/*
	 * y[2] = x[0] + c1 * x[1] + c0 * x[2]
	 *
	 * Same structure as y[1] but with swapped coefficients.
	 */
	re = AE_MULF32S_HH(c1, x1); /* c1.re * x1.re */
	AE_MULSF32S_LL(re, c1, x1); /* -= c1.im * x1.im */
	AE_MULAF32S_HH(re, c0, x2); /* += c0.re * x2.re */
	AE_MULSF32S_LL(re, c0, x2); /* -= c0.im * x2.im */

	im = AE_MULF32S_HL(c1, x1); /* c1.re * x1.im */
	AE_MULAF32S_LH(im, c1, x1); /* += c1.im * x1.re */
	AE_MULAF32S_HL(im, c0, x2); /* += c0.re * x2.im */
	AE_MULAF32S_LH(im, c0, x2); /* += c0.im * x2.re */

	result = AE_ROUND32X2F64SSYM(re, im);
	AE_S32X2_I(AE_ADD32S(x0, result), p_out, 2 * sizeof(ae_int32x2));
}

/**
 * fft_multi_execute_32() - Execute multi-radix FFT/IFFT (HiFi3).
 * @param plan Pointer to multi-FFT plan.
 * @param ifft False for FFT, true for IFFT.
 *
 * Performs a composite FFT of size N = num_ffts * fft_size (e.g. 3 * 512 = 1536).
 * For power-of-two sizes the call is forwarded to fft_execute_32().
 *
 * HiFi3 optimizations vs. the generic path:
 * - IFFT conjugate uses packed AE_SEL32_HL / AE_NEG32S
 * - Twiddle multiply uses fused 64-bit MAC (no intermediate rounding)
 * - IFFT shift-back combines negate, shift, and *3 scale in HiFi3 ops
 */
void fft_multi_execute_32(struct fft_multi_plan *plan, bool ifft)
{
	struct icomplex32 x[FFT_MULTI_COUNT_MAX];
	struct icomplex32 y[FFT_MULTI_COUNT_MAX];
	ae_int32x2 *p_src;
	ae_int32x2 *p_dst;
	ae_int32x2 sample, sample_neg, tw, data;
	ae_int64 re, im;
	int i, j, k, m;

	/* Handle 2^N FFT */
	if (plan->num_ffts == 1) {
		memset(plan->outb32, 0, plan->fft_size * sizeof(struct icomplex32));
		fft_execute_32(plan->fft_plan[0], ifft);
		return;
	}

	/* Convert to complex conjugate for IFFT */
	if (ifft) {
		p_src = (ae_int32x2 *)plan->inb32;
		for (i = 0; i < plan->total_size; i++) {
			AE_L32X2_IP(sample, p_src, 0);
			sample_neg = AE_NEG32S(sample);
			sample = AE_SEL32_HL(sample, sample_neg);
			AE_S32X2_IP(sample, p_src, sizeof(ae_int32x2));
		}
	}

	/* Copy input buffers (interleaved -> per-FFT) */
	k = 0;
	for (i = 0; i < plan->fft_size; i++)
		for (j = 0; j < plan->num_ffts; j++)
			plan->tmp_i32[j][i] = plan->inb32[k++];

	/* Clear output buffers and call individual FFTs */
	for (j = 0; j < plan->num_ffts; j++) {
		memset(&plan->tmp_o32[j][0], 0, plan->fft_size * sizeof(struct icomplex32));
		fft_execute_32(plan->fft_plan[j], 0);
	}

	/* Multiply with twiddle factors using HiFi3 complex multiply */
	m = FFT_MULTI_TWIDDLE_SIZE / 2 / plan->fft_size;
	for (j = 1; j < plan->num_ffts; j++) {
		p_dst = (ae_int32x2 *)plan->tmp_o32[j];
		for (i = 0; i < plan->fft_size; i++) {
			k = j * i * m;
			/*
			 * Build twiddle as packed {H=real, L=imag}.
			 * Use AE_SEL32_LH to pack two scalar loads.
			 */
			tw = AE_SEL32_LH(AE_MOVDA32(multi_twiddle_real_32[k]),
					 AE_MOVDA32(multi_twiddle_imag_32[k]));

			data = AE_L32X2_I(p_dst, 0);

			/* Complex multiply: tw * data */
			re = AE_MULF32S_HH(tw, data);
			AE_MULSF32S_LL(re, tw, data);
			im = AE_MULF32S_HL(tw, data);
			AE_MULAF32S_LH(im, tw, data);

			AE_S32X2_IP(AE_ROUND32X2F64SSYM(re, im),
				    p_dst, sizeof(ae_int32x2));
		}
	}

	/* DFT of size 3 */
	j = plan->fft_size;
	k = 2 * plan->fft_size;
	for (i = 0; i < plan->fft_size; i++) {
		x[0] = plan->tmp_o32[0][i];
		x[1] = plan->tmp_o32[1][i];
		x[2] = plan->tmp_o32[2][i];
		dft3_32(x, y);
		plan->outb32[i] = y[0];
		plan->outb32[i + j] = y[1];
		plan->outb32[i + k] = y[2];
	}

	/* Shift back for IFFT */
	if (ifft) {
		int len = plan->fft_plan[0]->len;

		p_dst = (ae_int32x2 *)plan->outb32;
		for (i = 0; i < plan->total_size; i++) {
			AE_L32X2_IP(sample, p_dst, 0);

			/* Negate imag part to match reference */
			sample_neg = AE_NEG32S(sample);
			sample = AE_SEL32_HL(sample, sample_neg);

			/* Shift left by FFT length to compensate shrink */
			sample = AE_SLAA32S(sample, len);

			/* Integer multiply by 3 (num_ffts) with saturation */
			data = AE_ADD32S(sample, sample);
			sample = AE_ADD32S(data, sample);

			AE_S32X2_IP(sample, p_dst, sizeof(ae_int32x2));
		}
	}
}

#endif /* FFT_HIFI3 */
