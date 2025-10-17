// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Intel Corporation. All rights reserved.
//
// Author: Amery Song <chao.song@intel.com>
//	   Keyon Jie <yang.jie@linux.intel.com>

#include <sof/audio/format.h>
#include <sof/trace/trace.h>
#include <sof/lib/uuid.h>
#include <sof/common.h>
#include <rtos/alloc.h>
#include <sof/math/fft.h>

SOF_DEFINE_REG_UUID(math_fft);
DECLARE_TR_CTX(math_fft_tr, SOF_UUID(math_fft_uuid), LOG_LEVEL_INFO);

struct fft_plan *fft_plan_new(void *inb, void *outb, uint32_t size, int bits)
{
	struct fft_plan *plan;
	int lim = 1;
	int len = 0;
	int i;

	if (!inb || !outb) {
		tr_err(&math_fft_tr, "Input or output buffer is NULL");
		return NULL;
	}

	if (size > FFT_SIZE_MAX) {
		tr_err(&math_fft_tr, "Illegal FFT size %d, max is %d.", size, FFT_SIZE_MAX);
		return NULL;
	}

	plan = rzalloc(SOF_MEM_FLAG_USER, sizeof(struct fft_plan));
	if (!plan) {
		tr_err(&math_fft_tr, "Memory allocation failed.");
		return NULL;
	}

	switch (bits) {
	case 16:
		plan->inb16 = inb;
		plan->outb16 = outb;
		break;
	case 32:
		plan->inb32 = inb;
		plan->outb32 = outb;
		break;
	default:
		tr_err(&math_fft_tr, "Illegal number of bits %d", bits);
		rfree(plan);
		return NULL;
	}

	/* calculate the exponent of 2 */
	while (lim < size) {
		lim <<= 1;
		len++;
	}

	plan->size = lim;
	plan->len = len;

	plan->bit_reverse_idx = rzalloc(SOF_MEM_FLAG_USER,
					plan->size * sizeof(uint16_t));
	if (!plan->bit_reverse_idx) {
		rfree(plan);
		return NULL;
	}

	/* set up the bit reverse index */
	for (i = 1; i < plan->size; ++i)
		plan->bit_reverse_idx[i] = (plan->bit_reverse_idx[i >> 1] >> 1) |
					   ((i & 1) << (len - 1));

	return plan;
}

void fft_plan_free(struct fft_plan *plan)
{
	if (!plan)
		return;

	rfree(plan->bit_reverse_idx);
	rfree(plan);
}
