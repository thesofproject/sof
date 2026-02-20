// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020-2025 Intel Corporation.
//
// Author: Amery Song <chao.song@intel.com>
//	   Keyon Jie <yang.jie@linux.intel.com>

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/buffer.h>
#include <sof/audio/format.h>
#include <sof/trace/trace.h>
#include <sof/lib/uuid.h>
#include <sof/common.h>
#include <rtos/alloc.h>
#include <sof/math/fft.h>

#include "fft_common.h"
#include "fft_32.h"
#include "coef/twiddle_32.h"
#include "coef/twiddle_16.h"

LOG_MODULE_REGISTER(math_fft, CONFIG_SOF_LOG_LEVEL);
SOF_DEFINE_REG_UUID(math_fft);
DECLARE_TR_CTX(math_fft_tr, SOF_UUID(math_fft_uuid), LOG_LEVEL_INFO);

struct fft_plan *fft_plan_common_new(struct processing_module *mod, void *inb,
				     void *outb, uint32_t size, int bits)
{
	struct fft_plan *plan;
	int lim = 1;
	int len = 0;

	if (!inb || !outb) {
		comp_cl_err(mod->dev, "NULL input/output buffers.");
		return NULL;
	}

	if (!is_power_of_2(size)) {
		comp_cl_err(mod->dev, "The FFT size must be a power of two.");
		return NULL;
	}

	plan = mod_zalloc(mod, sizeof(struct fft_plan));
	if (!plan) {
		comp_cl_err(mod->dev, "Failed to allocate FFT plan.");
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
		comp_cl_err(mod->dev, "Invalid word length.");
		return NULL;
	}

	/* calculate the exponent of 2 */
	while (lim < size) {
		lim <<= 1;
		len++;
	}

	plan->size = lim;
	plan->len = len;
	return plan;
}

void fft_plan_init_bit_reverse(uint16_t *bit_reverse_idx, int size, int len)
{
	int i;

	/* Set up the bit reverse index. The array will contain the value of
	 * the index with the bits order reversed. Index can be skipped.
	 */
	for (i = 1; i < size; ++i)
		bit_reverse_idx[i] = (bit_reverse_idx[i >> 1] >> 1) | ((i & 1) << (len - 1));
}

void *fft_plan_allocate_twiddle(struct processing_module *mod, int size, int bits)
{
	int twiddle_size = ((bits == 32) ? sizeof(int32_t) : sizeof(int16_t)) * 2 * size;

	return mod_alloc_align(mod, twiddle_size, 2 * sizeof(int32_t));
}

void fft_plan_init_twiddle(void *twiddle, int size, int bits)
{
	int32_t *twiddle32;
	int16_t *twiddle16;
	int k = FFT_SIZE_MAX / size;
	int i, j;

	if (bits == 32) {
		twiddle32 = twiddle;
		for (i = 0; i < size; i++) {
			j = i * k;
			*twiddle32++ = twiddle_real_32[j];
			*twiddle32++ = twiddle_imag_32[j];
		}
	} else {
		twiddle16 = twiddle;
		for (i = 0; i < size; i++) {
			j = i * k;
			*twiddle16++ = twiddle_real_16[j];
			*twiddle16++ = twiddle_imag_16[j];
		}
	}
}

struct fft_plan *mod_fft_plan_new(struct processing_module *mod, void *inb,
				  void *outb, uint32_t size, int bits)
{
	struct fft_plan *plan;

	if (size > FFT_SIZE_MAX || size < FFT_SIZE_MIN) {
		comp_cl_err(mod->dev, "Invalid FFT size %d", size);
		return NULL;
	}

	plan = fft_plan_common_new(mod, inb, outb, size, bits);
	if (!plan)
		return NULL;

	plan->bit_reverse_idx = mod_zalloc(mod,	plan->size * sizeof(uint16_t));
	if (!plan->bit_reverse_idx) {
		comp_cl_err(mod->dev, "Failed to allocate bit reverse table.");
		goto err;
	}

	fft_plan_init_bit_reverse(plan->bit_reverse_idx, plan->size, plan->len);

	/* Allocate memory for packed twiddle factors */
	plan->twiddle = fft_plan_allocate_twiddle(mod, size, bits);
	if (!plan->twiddle) {
		comp_cl_err(mod->dev, "Failed to allocate twiddle factors.");
		goto err_free_bit_reverse;
	}

	/* Pack twiddle factors from sparse real and image to complex pairs */
	fft_plan_init_twiddle(plan->twiddle, size, bits);

	return plan;

err_free_bit_reverse:
	mod_free(mod, plan->bit_reverse_idx);

err:
	mod_free(mod, plan);
	return NULL;
}

void mod_fft_plan_free(struct processing_module *mod, struct fft_plan *plan)
{
	if (!plan)
		return;

	mod_free(mod, plan->bit_reverse_idx);
	mod_free(mod, plan);
}
