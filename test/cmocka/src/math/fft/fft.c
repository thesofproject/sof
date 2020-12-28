// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Intel Corporation. All rights reserved.
//
// Author: Keyon Jie <yang.jie@linux.intel.com>

#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <math.h>
#include <cmocka.h>
#include <stdbool.h>

#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/audio/format.h>
#include <sof/math/fft.h>

#include "input.h"

/* SNR DB threshold in Q8.24 */
#define FFT_DB_TH	30.0

#define SINE_HZ	1000

/**
 * \brief Doing Fast Fourier Transform (FFT) for mono real input buffers.
 * \param[in] src - pointer to input buffer.
 * \param[out] dst - pointer to output buffer, FFT output for input buffer.
 * \param[in] size - input buffer sample count.
 */
static void fft_real(struct comp_buffer *src, struct comp_buffer *dst, uint32_t size)
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
static void ifft_complex(struct comp_buffer *src, struct comp_buffer *dst, uint32_t size)
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
static void fft_real_2(struct comp_buffer *src, struct comp_buffer *dst1,
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

static void test_math_fft_256(void **state)
{
	uint32_t fft_size = 256;
	int i;
	int r = 0;
	int64_t peak = 0;
	struct sof_ipc_buffer test_buf_desc = {
		.size = 256 * 4 * 2,
	};
	struct comp_buffer *source = buffer_new(&test_buf_desc);
	struct comp_buffer *sink = buffer_new(&test_buf_desc);
	struct icomplex32 *out = (struct icomplex32 *)sink->stream.addr;

	(void)state;

	for (i = 0; i < fft_size; i++)
		*((int32_t *)source->stream.addr + i) = input_samples[i];

	source->stream.channels = 1;
	/* do fft transform */
	fft_real(source, sink, fft_size);

	/* looking for the peak */
	for (i = 0; i < fft_size / 2; i++) {
		if ((int64_t)out[i].real * out[i].real +
		    (int64_t)out[i].imag * out[i].imag > peak) {
			peak = (int64_t)out[i].real * out[i].real +
			       (int64_t)out[i].imag * out[i].imag;
			r = i;
		}
	}

	i = (SINE_HZ * fft_size) / 48000;
	printf("%s: peak at point %d\n", __func__, r);

	/* the peak should be in range i +/-1 */
	assert_in_range(r, i - 1, i + 1);
}

static void test_math_fft_1024(void **state)
{
	uint32_t fft_size = 1024;
	int i;
	int r = 0;
	int64_t peak = 0;
	struct sof_ipc_buffer test_buf_desc = {
		.size = 1024 * 4 * 2,
	};
	struct comp_buffer *source = buffer_new(&test_buf_desc);
	struct comp_buffer *sink = buffer_new(&test_buf_desc);
	struct icomplex32 *out = (struct icomplex32 *)sink->stream.addr;

	(void)state;

	for (i = 0; i < fft_size; i++)
		*((int32_t *)source->stream.addr + i) = input_samples[i];

	source->stream.channels = 1;
	/* do fft transform */
	fft_real(source, sink, fft_size);

	/* looking for the peak */
	for (i = 0; i < fft_size / 2; i++) {
		if ((int64_t)out[i].real * out[i].real +
		    (int64_t)out[i].imag * out[i].imag > peak) {
			peak = (int64_t)out[i].real * out[i].real +
			       (int64_t)out[i].imag * out[i].imag;
			r = i;
		}
	}

	i = (SINE_HZ * fft_size) / 48000;
	printf("%s: peak at point %d\n", __func__, r);

	/* the peak should be in range i +/-1 */
	assert_in_range(r, i - 1, i + 1);
}

static void test_math_fft_1024_ifft(void **state)
{
	uint32_t fft_size = 1024;
	int i;
	int64_t signal = 0;
	int64_t noise = 0;
	int r = 0;
	float db;
	struct sof_ipc_buffer test_buf_desc = {
		.size = 1024 * 4 * 2,
	};
	struct comp_buffer *source = buffer_new(&test_buf_desc);
	struct comp_buffer *intm = buffer_new(&test_buf_desc);
	struct comp_buffer *sink = buffer_new(&test_buf_desc);
	struct icomplex32 *out = (struct icomplex32 *)sink->stream.addr;

	(void)state;

	for (i = 0; i < fft_size; i++)
		*((int32_t *)source->stream.addr + i) = input_samples[i];

	source->stream.channels = 1;
	/* do fft transform */
	fft_real(source, intm, fft_size);

	intm->stream.channels = 1;
	/* do ifft transform */
	ifft_complex(intm, sink, fft_size);

	/* calculate signal and noise */
	for (i = 0; i < fft_size; i++) {
		signal += (int64_t)(input_samples[i] / 32) * (input_samples[i] / 32);
		noise += (int64_t)((out[i].real - input_samples[i]) / 32) *
			 ((out[i].real - input_samples[i]) / 32) +
			 (int64_t)(out[i].imag / 32) * (out[i].imag / 32);
	}

	/* db in Q8.24 */
	db = 10 * log10((float)signal / noise);

	printf("%s: signal: 0x%llx noise: 0x%llx db: %f\n", __func__,
	       (uint64_t)signal, (uint64_t)noise, db);

	if (db < FFT_DB_TH)
		r = 1;

	assert_int_equal(r, 0);
}

static void test_math_fft_512_2ch(void **state)
{
	uint32_t fft_size = 512;
	int i;
	int r = 0;
	int64_t peak = 0;
	struct sof_ipc_buffer test_buf_desc = {
		.size = 512 * 4 * 2,
	};
	struct comp_buffer *source = buffer_new(&test_buf_desc);
	struct comp_buffer *sink1 = buffer_new(&test_buf_desc);
	struct comp_buffer *sink2 = buffer_new(&test_buf_desc);
	struct icomplex32 *out1 = (struct icomplex32 *)sink1->stream.addr;
	struct icomplex32 *out2 = (struct icomplex32 *)sink2->stream.addr;

	(void)state;

	for (i = 0; i < fft_size; i++) {
		*((int32_t *)source->stream.addr + 2 * i) = input_samples[i];
		*((int32_t *)source->stream.addr + 2 * i + 1) = input_samples[fft_size + i];
	}

	source->stream.channels = 2;
	/* do fft transform */
	fft_real_2(source, sink1, sink2, fft_size);

	/* looking for the peak for channel 0 */
	for (i = 0; i < fft_size / 2; i++) {
		if ((int64_t)out1[i].real * out1[i].real +
		    (int64_t)out1[i].imag * out1[i].imag > peak) {
			peak = (int64_t)out1[i].real * out1[i].real +
			       (int64_t)out1[i].imag * out1[i].imag;
			r = i;
		}
	}

	i = (SINE_HZ * fft_size) / 48000;
	printf("%s: peak for channel 0 at point %d\n", __func__, r);

	/* the peak should be in range i +/-1 */
	assert_in_range(r, i - 1, i + 1);

	/* looking for the peak for channel 1 */
	for (i = 0; i < fft_size / 2; i++) {
		if ((int64_t)out2[i].real * out2[i].real +
		    (int64_t)out2[i].imag * out2[i].imag > peak) {
			peak = (int64_t)out2[i].real * out2[i].real +
			       (int64_t)out2[i].imag * out2[i].imag;
			r = i;
		}
	}

	i = (SINE_HZ * fft_size) / 48000;
	printf("%s: peak for channel 1 at point %d\n", __func__, r);

	/* the peak should be in range i +/-1 */
	assert_in_range(r, i - 1, i + 1);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_math_fft_256),
		cmocka_unit_test(test_math_fft_1024),
		cmocka_unit_test(test_math_fft_1024_ifft),
		cmocka_unit_test(test_math_fft_512_2ch),
	};

	cmocka_set_message_output(CM_OUTPUT_TAP);

	return cmocka_run_group_tests(tests, NULL, NULL);
}
