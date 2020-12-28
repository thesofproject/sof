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

/*
 * These helpers are optimized for FFT calculation only.
 * e.g. _add/sub() assume the output won't be saturate so no check needed,
 * and _mul() assumes Q1.31 * Q1.31 so the output will be shifted to be Q1.31.
 */

static void icomplex32_add(struct icomplex32 *in1, struct icomplex32 *in2, struct icomplex32 *out)
{
	out->real = in1->real + in2->real;
	out->imag = in1->imag + in2->imag;
}

static void icomplex32_sub(struct icomplex32 *in1, struct icomplex32 *in2, struct icomplex32 *out)
{
	out->real = in1->real - in2->real;
	out->imag = in1->imag - in2->imag;
}

static void icomplex32_mul(struct icomplex32 *in1, struct icomplex32 *in2, struct icomplex32 *out)
{
	out->real = ((int64_t)in1->real * in2->real - (int64_t)in1->imag * in2->imag) >> 31;
	out->imag = ((int64_t)in1->real * in2->imag + (int64_t)in1->imag * in2->real) >> 31;
}

/* complex conjugate */
static void icomplex_conj(struct icomplex32 *comp)
{
	comp->imag = SATP_INT32((int64_t)-1 * comp->imag);
}

/* shift a complex n bits, n > 0: left shift, n < 0: right shift */
static void icomplex_shift(struct icomplex32 *input, int32_t n, struct icomplex32 *output)
{
	if (n > 0) {
		/* need saturation handling */
		output->real = SATP_INT32(SATM_INT32((int64_t)input->real << n));
		output->imag = SATP_INT32(SATM_INT32((int64_t)input->imag << n));
	} else {
		output->real = input->real >> -n;
		output->imag = input->imag >> -n;
	}
}

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

	if (!plan->bit_reverse_idx)
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

#ifdef UNIT_TEST
/**
 * \brief Doing Fast Fourier Transform (FFT) for mono real input buffers.
 * \param[in] src - pointer to input buffer.
 * \param[out] dst - pointer to output buffer, FFT output for input buffer.
 * \param[in] size - input buffer sample count.
 */
void fft_real(struct comp_buffer *src, struct comp_buffer *dst, uint32_t size)
{
	struct icomplex32 *inb;
	struct icomplex32 *outb;
	struct fft_plan *plan;
	int i;

	if (src->stream.channels != 1)
		return;

	if (src->stream.size < size * sizeof(int32_t) ||
	    dst->stream.size < size * sizeof(struct icomplex32))
		return;

	inb = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM,
		      size * sizeof(struct icomplex32));
	if (!inb)
		return;
	outb = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM,
		       size * sizeof(struct icomplex32));
	if (!outb)
		goto err_outb;

	plan = fft_plan_new(inb, outb, size);
	if (!plan)
		goto err_plan;

	for (i = 0; i < size; i++) {
		inb[i].real = *((int32_t *)src->stream.addr + i);
		inb[i].imag = 0;
	}

	/* perform a single FFT transform */
	fft_execute(plan, false);

	for (i = 0; i < size; i++) {
		*((int32_t *)dst->stream.addr + 2 * i) = outb[i].real;
		*((int32_t *)dst->stream.addr + 2 * i + 1) = outb[i].imag;
	}

	fft_plan_free(plan);

err_plan:
	rfree(outb);
err_outb:
	rfree(inb);
}

/**
 * \brief Doing Inverse Fast Fourier Transform (IFFT) for mono complex input buffers.
 * \param[in] src - pointer to input buffer.
 * \param[out] dst - pointer to output buffer, FFT output for input buffer.
 * \param[in] size - input buffer sample count.
 */
void ifft_complex(struct comp_buffer *src, struct comp_buffer *dst, uint32_t size)
{
	struct icomplex32 *inb;
	struct icomplex32 *outb;
	struct fft_plan *plan;
	int i;

	if (src->stream.channels != 1)
		return;

	if (src->stream.size < size * sizeof(struct icomplex32) ||
	    dst->stream.size < size * sizeof(struct icomplex32))
		return;

	inb = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM,
		      size * sizeof(struct icomplex32));
	if (!inb)
		return;

	outb = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM,
		       size * sizeof(struct icomplex32));
	if (!outb)
		goto err_outb;

	plan = fft_plan_new(inb, outb, size);
	if (!plan)
		goto err_plan;

	for (i = 0; i < size; i++) {
		inb[i].real = *((int32_t *)src->stream.addr + 2 * i);
		inb[i].imag = *((int32_t *)src->stream.addr + 2 * i + 1);
	}

	/* perform a single IFFT transform */
	fft_execute(plan, true);

	for (i = 0; i < size; i++) {
		*((int32_t *)dst->stream.addr + 2 * i) = outb[i].real;
		*((int32_t *)dst->stream.addr + 2 * i + 1) = outb[i].imag;
	}

	fft_plan_free(plan);

err_plan:
	rfree(outb);
err_outb:
	rfree(inb);
}

/**
 * \brief Doing Fast Fourier Transform (FFT) for 2 real input buffers.
 * \param[in] src - pointer to input buffer (2 channels).
 * \param[out] dst1 - pointer to output buffer, FFT output for input buffer 1.
 * \param[out] dst2 - pointer to output buffer, FFT output for input buffer 2.
 * \param[in] size - input buffer sample count.
 */
void fft_real_2(struct comp_buffer *src, struct comp_buffer *dst1,
		struct comp_buffer *dst2, uint32_t size)
{
	struct icomplex32 *inb;
	struct icomplex32 *outb;
	struct fft_plan *plan;
	int i;

	if (src->stream.channels != 2)
		return;

	if (src->stream.size < size * sizeof(int32_t) * 2 ||
	    dst1->stream.size < size * sizeof(struct icomplex32) ||
	    dst2->stream.size < size * sizeof(struct icomplex32))
		return;

	inb = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM,
		      size * sizeof(struct icomplex32));
	if (!inb)
		return;

	outb = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM,
		       size * sizeof(struct icomplex32));
	if (!outb)
		goto err_outb;

	plan = fft_plan_new(inb, outb, size);
	if (!plan)
		goto err_plan;

	/* generate complex inputs */
	for (i = 0; i < size; i++) {
		inb[i].real = *((int32_t *)src->stream.addr + 2 * i);
		inb[i].imag = *((int32_t *)src->stream.addr + 2 * i + 1);
	}

	/* perform a single FFT transform */
	fft_execute(plan, false);

	/* calculate the outputs */
	*((int32_t *)dst1->stream.addr) = outb[0].real;
	*((int32_t *)dst1->stream.addr + 1) = 0;
	*((int32_t *)dst2->stream.addr) = 0;
	*((int32_t *)dst2->stream.addr + 1) = -outb[0].imag;
	for (i = 1; i < size; i++) {
		*((int32_t *)dst1->stream.addr + 2 * i) =
			(outb[i].real + outb[size - i].real) / 2;
		*((int32_t *)dst1->stream.addr + 2 * i + 1) =
			(outb[i].imag - outb[size - i].imag) / 2;
		*((int32_t *)dst2->stream.addr + 2 * i) =
			(outb[i].imag + outb[size - i].imag) / 2;
		*((int32_t *)dst2->stream.addr + 2 * i + 1) =
			(outb[size - i].real - outb[i].real) / 2;
	}

	fft_plan_free(plan);

err_plan:
	rfree(outb);
err_outb:
	rfree(inb);
}
#endif
