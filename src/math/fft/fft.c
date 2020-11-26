// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Intel Corporation. All rights reserved.
//
// Author: Amery Song <chao.song@intel.com>
//	   Keyon Jie <yang.jie@linux.intel.com>

#include <sof/audio/coefficients/fft/twiddle.h>
#include <sof/audio/buffer.h>
#include <sof/audio/format.h>
#include <sof/common.h>
#include <sof/lib/alloc.h>
#include <sof/math/fft.h>

struct fft_plan *fft_plan_new(struct icomplex32 *inb, struct icomplex32 *outb, uint32_t size)
{
	struct fft_plan *plan;
	int lim = 1;
	int len = 0;
	int i;

	if (!inb || !outb)
		return NULL;

	plan = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(struct fft_plan));
	if (!plan)
		return NULL;

	/* calculate the exponent of 2 */
	while (lim < size) {
		lim <<= 1;
		len++;
	}

	plan->size = lim;
	plan->len = len;

	plan->bit_reverse_idx = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM,
					plan->size * sizeof(uint32_t));
	if (!plan->bit_reverse_idx) {
		rfree(plan);
		return NULL;
	}

	/* set up the bit reverse index */
	for (i = 1; i < plan->size; ++i)
		plan->bit_reverse_idx[i] = (plan->bit_reverse_idx[i >> 1] >> 1) |
					   ((i & 1) << (len - 1));

	plan->inb = inb;
	plan->outb = outb;

	return plan;
}

void fft_plan_free(struct fft_plan *plan)
{
	if (!plan)
		return;

	rfree(plan->bit_reverse_idx);
	rfree(plan);
}

/**
 * \brief Execute the Fast Fourier Transform (FFT) or Inverse FFT (IFFT)
 *	  For the configured fft_pan.
 * \param[in] plan - pointer to fft_plan which will be executed.
 * \param[in] ifft - set to 1 for IFFT and 0 for FFT.
 */
void fft_execute(struct fft_plan *plan, bool ifft)
{
	struct icomplex32 tmp1;
	struct icomplex32 tmp2;
	int depth;
	int top;
	int bottom;
	int index;
	int i;
	int j;
	int k;
	int m;
	int n;

	if (!plan || !plan->bit_reverse_idx)
		return;

	/* convert to complex conjugate for ifft */
	if (ifft) {
		for (i = 0; i < plan->size; i++)
			icomplex_conj(&plan->inb[i]);
	}

	/* step 1: re-arrange input in bit reverse order, and shrink the level to avoid overflow */
	for (i = 1; i < plan->size; ++i)
		icomplex_shift(&plan->inb[i], (-1) * plan->len,
			       &plan->outb[plan->bit_reverse_idx[i]]);

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
				tmp1.real = twiddle_real[index];
				tmp1.imag = twiddle_imag[index];
				/* calculate the accumulator: twiddle * bottom */
				icomplex32_mul(&tmp1, &plan->outb[bottom], &tmp2);
				tmp1 = plan->outb[top];
				/* calculate the top output: top = top + accumulate */
				icomplex32_add(&tmp1, &tmp2, &plan->outb[top]);
				/* calculate the bottom output: bottom = top - accumulate */
				icomplex32_sub(&tmp1, &tmp2, &plan->outb[bottom]);
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
		for (i = 0; i < plan->size; i++)
			icomplex_shift(&plan->outb[i], plan->len, &plan->outb[i]);
	}
}
