// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Intel Corporation. All rights reserved.
//
// Author: Amery Song <chao.song@intel.com>
//	   Keyon Jie <yang.jie@linux.intel.com>

#include <sof/audio/coefficients/fft/twiddle_3072_32.h>
#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/buffer.h>
#include <sof/audio/format.h>
#include <sof/trace/trace.h>
#include <sof/lib/uuid.h>
#include <sof/common.h>
#include <rtos/alloc.h>
#include <sof/math/fft.h>

#include "fft_32.h"

/* Constants for size 3 DFT */
#define DFT3_COEFR	-1073741824	/* int32(-0.5 * 2^31) */
#define DFT3_COEFI	1859775393	/* int32(sqrt(3) / 2 * 2^31) */
#define DFT3_SCALE	715827883	/* int32(1/3*2^31) */

LOG_MODULE_REGISTER(math_fft, CONFIG_SOF_LOG_LEVEL);
SOF_DEFINE_REG_UUID(math_fft);
DECLARE_TR_CTX(math_fft_tr, SOF_UUID(math_fft_uuid), LOG_LEVEL_INFO);

static int round_up_to_pow2(int n)
{
	int p = 1;

	while (p < n)
		p *= 2;

	return p;
}

static struct fft_plan *fft_plan_common_new(struct processing_module *mod, void *inb,
					    void *outb, uint32_t size, int bits)
{
	struct fft_plan *plan;
	int lim = 1;
	int len = 0;
	const int size_pow2 = round_up_to_pow2(size);

	if (!inb || !outb) {
		comp_cl_err(mod->dev, "NULL input/output buffers.");
		return NULL;
	}

	if (size != size_pow2) {
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
		comp_cl_err(mod->dev, "Illegal word length.");
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

static void fft_plan_init_bit_reverse(uint16_t *bit_reverse_idx, int size, int len)
{
	int i;

	/* set up the bit reverse index */
	for (i = 1; i < size; ++i)
		bit_reverse_idx[i] = (bit_reverse_idx[i >> 1] >> 1) | ((i & 1) << (len - 1));
}

struct fft_plan *mod_fft_plan_new(struct processing_module *mod, void *inb,
				  void *outb, uint32_t size, int bits)
{
	struct fft_plan *plan;

	if (size > FFT_SIZE_MAX || size < FFT_SIZE_MIN) {
		comp_cl_err(mod->dev, "Illegal FFT size %d", size);
		return NULL;
	}

	plan = fft_plan_common_new(mod, inb, outb, size, bits);
	if (!plan)
		return NULL;

	plan->bit_reverse_idx = mod_zalloc(mod,	plan->size * sizeof(uint16_t));
	if (!plan->bit_reverse_idx) {
		comp_cl_err(mod->dev, "Failed to allocate bit reverse table.");
		mod_free(mod, plan);
		return NULL;
	}

	fft_plan_init_bit_reverse(plan->bit_reverse_idx, plan->size, plan->len);
	return plan;
}

struct fft_multi_plan *mod_fft_multi_plan_new(struct processing_module *mod, void *inb,
					      void *outb, uint32_t size, int bits)
{
	struct fft_multi_plan *plan;
	size_t tmp_size;
	const int size_pow2 = round_up_to_pow2(size);
	const int size_div3 = size / 3;
	int i;

	if (!inb || !outb) {
		comp_cl_err(mod->dev, "Null buffers");
		return NULL;
	}

	if (size < FFT_SIZE_MIN) {
		comp_cl_err(mod->dev, "Illegal FFT size %d", size);
		return NULL;
	}

	plan = mod_zalloc(mod, sizeof(struct fft_multi_plan));
	if (!plan)
		return NULL;

	if (size == size_pow2) {
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
	case 16:
		plan->inb16 = inb;
		plan->outb16 = outb;
		break;
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

void mod_fft_plan_free(struct processing_module *mod, struct fft_plan *plan)
{
	if (!plan)
		return;

	mod_free(mod, plan->bit_reverse_idx);
	mod_free(mod, plan);
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

void dft3_32(struct icomplex32 *x_in, struct icomplex32 *y)
{
	const struct icomplex32 c0 = {DFT3_COEFR, -DFT3_COEFI};
	const struct icomplex32 c1 = {DFT3_COEFR,  DFT3_COEFI};
	struct icomplex32 x[3];
	struct icomplex32 p1, p2, sum;
	int i;

	for (i = 0; i < 3; i++) {
		x[i].real = Q_MULTSR_32X32((int64_t)x_in[i].real, DFT3_SCALE, 31, 31, 31);
		x[i].imag = Q_MULTSR_32X32((int64_t)x_in[i].imag, DFT3_SCALE, 31, 31, 31);
	}

	/*
	 *      | 1   1   1 |
	 * c =  | 1  c0  c1 | , x = [ x0 x1 x2 ]
	 *      | 1  c1  c0 |
	 *
	 * y(0) = c(0,0) * x(0) + c(1,0) * x(1) + c(2,0) * x(0)
	 * y(1) = c(0,1) * x(0) + c(1,1) * x(1) + c(2,1) * x(0)
	 * y(2) = c(0,2) * x(0) + c(1,2) * x(1) + c(2,2) * x(0)
	 */

	/* y(0) = 1 * x(0) + 1 * x(1) + 1 * x(2) */
	icomplex32_adds(&x[0], &x[1], &sum);
	icomplex32_adds(&x[2], &sum, &y[0]);

	/* y(1) = 1 * x(0) + c0 * x(1) + c1 * x(2) */
	icomplex32_mul(&c0, &x[1], &p1);
	icomplex32_mul(&c1, &x[2], &p2);
	icomplex32_adds(&p1, &p2, &sum);
	icomplex32_adds(&x[0], &sum, &y[1]);

	/* y(2) = 1 * x(0) + c1 * x(1) + c0 * x(2) */
	icomplex32_mul(&c1, &x[1], &p1);
	icomplex32_mul(&c0, &x[2], &p2);
	icomplex32_adds(&p1, &p2, &sum);
	icomplex32_adds(&x[0], &sum, &y[2]);
}

void fft_multi_execute_32(struct fft_multi_plan *plan, bool ifft)
{
	struct icomplex32 x[FFT_MULTI_COUNT_MAX];
	struct icomplex32 y[FFT_MULTI_COUNT_MAX];
	struct icomplex32 t, c;
	int i, j, k, m;

	/* Handle 2^N FFT */
	if (plan->num_ffts == 1) {
		bzero(plan->outb32, plan->fft_size * sizeof(struct icomplex32));
		fft_execute_32(plan->fft_plan[0], ifft);
		return;
	}

	//FILE *fh1 = fopen("debug_fft_multi_int1.txt", "w");
	//FILE *fh2 = fopen("debug_fft_multi_int2.txt", "w");
	//FILE *fh3 = fopen("debug_fft_multi_twiddle.txt", "w");
	//FILE *fh4 = fopen("debug_fft_multi_dft_out.txt", "w");

	/* convert to complex conjugate for IFFT */
	if (ifft) {
		for (i = 0; i < plan->total_size; i++)
			icomplex32_conj(&plan->inb32[i]);
	}

	/* Copy input buffers */
	k = 0;
	for (i = 0; i < plan->fft_size; i++)
		for (j = 0; j < plan->num_ffts; j++)
			plan->tmp_i32[j][i] = plan->inb32[k++];

	/* Clear output buffers and call individual FFTs*/
	for (j = 0; j < plan->num_ffts; j++) {
		bzero(&plan->tmp_o32[j][0], plan->fft_size * sizeof(struct icomplex32));
		fft_execute_32(plan->fft_plan[j], 0);
	}

	//for (j = 0; j < plan->num_ffts; j++)
	//	for (i = 0; i < plan->fft_size; i++)
	//		fprintf(fh1, "%d %d\n",
	//			plan->tmp_o32[j][i].real, plan->tmp_o32[j][i].imag);

	/* Multiply with twiddle factors */
	m = FFT_MULTI_TWIDDLE_SIZE / 2 / plan->fft_size;
	for (j = 1; j < plan->num_ffts; j++) {
		for (i = 0; i < plan->fft_size; i++) {
			c = plan->tmp_o32[j][i];
			k = j * i * m;
			t.real = multi_twiddle_real_32[k];
			t.imag = multi_twiddle_imag_32[k];
			//fprintf(fh3, "%d %d\n", t.real, t.imag);
			icomplex32_mul(&t, &c, &plan->tmp_o32[j][i]);
		}
	}

	//for (j = 0; j < plan->num_ffts; j++)
	//	for (i = 0; i < plan->fft_size; i++)
	//		fprintf(fh2, "%d %d\n",
	//			plan->tmp_o32[j][i].real, plan->tmp_o32[j][i].imag);

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

	//for (i = 0; i < plan->total_size; i++)
	//	fprintf(fh4, "%d %d\n",  plan->outb32[i].real, plan->outb32[i].imag);

	/* shift back for IFFT */

	/* TODO: Check if time shift method for IFFT is more efficient or more accurate
	 * tmp = 1 / N * fft(X);
	 * x = tmp([1 N:-1:2])
	 */
	if (ifft) {
		/*
		 * no need to divide N as it is already done in the input side
		 * for Q1.31 format. Instead, we need to multiply N to compensate
		 * the shrink we did in the FFT transform.
		 */
		for (i = 0; i < plan->total_size; i++) {
			/* Need to negate imag part to match reference */
			plan->outb32[i].imag = -plan->outb32[i].imag;
			icomplex32_shift(&plan->outb32[i], plan->fft_plan[0]->len,
					 &plan->outb32[i]);
			plan->outb32[i].real = sat_int32((int64_t)plan->outb32[i].real * 3);
			plan->outb32[i].imag = sat_int32((int64_t)plan->outb32[i].imag * 3);
		}
	}

	// fclose(fh1); fclose(fh2); fclose(fh3); fclose(fh4);
}
