// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2025 Intel Corporation.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

#include <sof/audio/coefficients/fft/twiddle_3072_32.h>
#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/format.h>
#include <sof/math/icomplex32.h>
#include <sof/trace/trace.h>
#include <sof/lib/uuid.h>
#include <sof/common.h>
#include <rtos/alloc.h>
#include <sof/math/fft.h>
#include "fft_common.h"

LOG_MODULE_REGISTER(math_fft_multi, CONFIG_SOF_LOG_LEVEL);
SOF_DEFINE_REG_UUID(math_fft_multi);
DECLARE_TR_CTX(math_fft_multi_tr, SOF_UUID(math_fft_multi_uuid), LOG_LEVEL_INFO);

struct fft_multi_plan *mod_fft_multi_plan_new(struct processing_module *mod, void *inb,
					      void *outb, uint32_t size, int bits)
{
	struct fft_multi_plan *plan;
	size_t tmp_size;
	const int size_div3 = size / 3;
	int i;

	if (!inb || !outb) {
		comp_cl_err(mod->dev, "Null buffers");
		return NULL;
	}

	if (size < FFT_SIZE_MIN) {
		comp_cl_err(mod->dev, "Invalid FFT size %d", size);
		return NULL;
	}

	plan = mod_zalloc(mod, sizeof(struct fft_multi_plan));
	if (!plan)
		return NULL;

	if (is_power_of_2(size)) {
		plan->num_ffts = 1;
	} else if (size_div3 * 3 == size) {
		plan->num_ffts = 3;
	} else {
		comp_cl_err(mod->dev, "Not supported FFT size %d", size);
		goto err;
	}

	/* Allocate common bit reverse table for all FFT plans */
	plan->total_size = size;
	plan->fft_size = size / plan->num_ffts;
	if (plan->fft_size > FFT_SIZE_MAX) {
		comp_cl_err(mod->dev, "Requested size %d FFT is too large", size);
		goto err;
	}

	plan->bit_reverse_idx = mod_zalloc(mod, plan->fft_size * sizeof(uint16_t));
	if (!plan->bit_reverse_idx) {
		comp_cl_err(mod->dev, "Failed to allocate FFT plan");
		goto err;
	}

	switch (bits) {
	case 32:
		plan->inb32 = inb;
		plan->outb32 = outb;

		if (plan->num_ffts > 1) {
			/* Allocate input/output buffers for FFTs */
			tmp_size = 2 * plan->num_ffts * plan->fft_size * sizeof(struct icomplex32);
			plan->tmp_i32[0] = mod_balloc(mod, tmp_size);
			if (!plan->tmp_i32[0]) {
				comp_cl_err(mod->dev, "Failed to allocate FFT buffers");
				goto err_free_bit_reverse;
			}

			/* Set up buffers */
			plan->tmp_o32[0] = plan->tmp_i32[0] + plan->fft_size;
			for (i = 1; i < plan->num_ffts; i++) {
				plan->tmp_i32[i] = plan->tmp_o32[i - 1] + plan->fft_size;
				plan->tmp_o32[i] = plan->tmp_i32[i] + plan->fft_size;
			}
		} else {
			plan->tmp_i32[0] = inb;
			plan->tmp_o32[0] = outb;
		}

		for (i = 0; i < plan->num_ffts; i++) {
			plan->fft_plan[i] = fft_plan_common_new(mod,
								plan->tmp_i32[i],
								plan->tmp_o32[i],
								plan->fft_size, 32);
			if (!plan->fft_plan[i])
				goto err_free_buffer;

			plan->fft_plan[i]->bit_reverse_idx = plan->bit_reverse_idx;
		}
		break;
	default:
		comp_cl_err(mod->dev, "Not supported word length %d", bits);
		goto err;
	}

	/* Set up common bit index reverse table */
	fft_plan_init_bit_reverse(plan->bit_reverse_idx, plan->fft_plan[0]->size,
				  plan->fft_plan[0]->len);
	return plan;

err_free_buffer:
	mod_free(mod, plan->tmp_i32[0]);

err_free_bit_reverse:
	mod_free(mod, plan->bit_reverse_idx);

err:
	mod_free(mod, plan);
	return NULL;
}

void mod_fft_multi_plan_free(struct processing_module *mod, struct fft_multi_plan *plan)
{
	int i;

	if (!plan)
		return;

	for (i = 0; i < plan->num_ffts; i++)
		mod_free(mod, plan->fft_plan[i]);

	/* If single FFT, the internal buffers were not allocated. */
	if (plan->num_ffts > 1)
		mod_free(mod, plan->tmp_i32[0]);

	mod_free(mod, plan->bit_reverse_idx);
	mod_free(mod, plan);
}
