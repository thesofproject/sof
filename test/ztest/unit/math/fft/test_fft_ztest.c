// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.
//
// These contents may have been developed with support from one or more
// Intel-operated generative artificial intelligence solutions.
//
// Converted from CMock to Ztest
// Original tests from test/cmocka/src/math/fft/fft.c:
// Author: Keyon Jie <yang.jie@linux.intel.com>

#include <zephyr/ztest.h>

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <math.h>
#include <stdbool.h>

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/audio/format.h>
#include <sof/math/icomplex16.h>
#include <sof/math/icomplex32.h>
#include <sof/math/fft.h>

#include "mod_alloc.h"

/* Reference test vectors shared with the legacy CMocka test. They are
 * #included in place from the CMocka tree (added to the include path in
 * CMakeLists.txt) so the exact same fixed-point data is exercised.
 */
#include "input.h"

/* SNR DB threshold */
#define FFT_DB_TH_16 27.0
#define FFT_DB_TH    95.0
#define SINE_HZ      1000

/* For rectangular window frequency matched sine wave test */
#define TWO_PI          6.28318530717959
#define SINE_SCALE_S16  32767.0
#define SINE_SCALE_S32  2147483647.0
#define SINE_FS         48000.0
#define SINE_BASE       (SINE_FS / 256.0) /* One perid exactly 256 samples, 187.5 Hz */
#define SINE_FREQ       (4.0 * SINE_BASE) /* Four periods, 1125 Hz */
#define MIN_SNR_256_16  49.0
#define MIN_SNR_512_16  42.0
#define MIN_SNR_1024_16 38.0
#define MIN_SNR_256     132.0
#define MIN_SNR_512     125.0
#define MIN_SNR_1024    119.0

static struct processing_module dummy;

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

	if (audio_stream_get_channels(&src->stream) != 1) {
		return;
	}

	if (src->stream.size < size * sizeof(int32_t) ||
	    dst->stream.size < size * sizeof(struct icomplex32)) {
		return;
	}

	inb = rzalloc(SOF_MEM_FLAG_USER, size * sizeof(struct icomplex32));
	if (!inb) {
		return;
	}
	outb = rzalloc(SOF_MEM_FLAG_USER, size * sizeof(struct icomplex32));
	if (!outb) {
		goto err_outb;
	}

	plan = mod_fft_plan_new(&dummy, inb, outb, size, 32);
	if (!plan) {
		goto err_plan;
	}

	for (i = 0; i < size; i++) {
		inb[i].real = *((int32_t *)src->stream.addr + i);
		inb[i].imag = 0;
	}

	/* perform a single FFT transform */
	fft_execute_32(plan, false);

	for (i = 0; i < size; i++) {
		*((int32_t *)dst->stream.addr + 2 * i) = outb[i].real;
		*((int32_t *)dst->stream.addr + 2 * i + 1) = outb[i].imag;
	}

	mod_fft_plan_free(&dummy, plan);

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

	if (audio_stream_get_channels(&src->stream) != 1) {
		return;
	}

	if (src->stream.size < size * sizeof(struct icomplex32) ||
	    dst->stream.size < size * sizeof(struct icomplex32)) {
		return;
	}

	inb = rzalloc(SOF_MEM_FLAG_USER, size * sizeof(struct icomplex32));
	if (!inb) {
		return;
	}

	outb = rzalloc(SOF_MEM_FLAG_USER, size * sizeof(struct icomplex32));
	if (!outb) {
		goto err_outb;
	}

	plan = mod_fft_plan_new(&dummy, inb, outb, size, 32);
	if (!plan) {
		goto err_plan;
	}

	for (i = 0; i < size; i++) {
		inb[i].real = *((int32_t *)src->stream.addr + 2 * i);
		inb[i].imag = *((int32_t *)src->stream.addr + 2 * i + 1);
	}

	/* perform a single IFFT transform */
	fft_execute_32(plan, true);

	for (i = 0; i < size; i++) {
		*((int32_t *)dst->stream.addr + 2 * i) = outb[i].real;
		*((int32_t *)dst->stream.addr + 2 * i + 1) = outb[i].imag;
	}

	mod_fft_plan_free(&dummy, plan);

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
static void fft_real_2(struct comp_buffer *src, struct comp_buffer *dst1, struct comp_buffer *dst2,
		       uint32_t size)
{
	struct icomplex32 *inb;
	struct icomplex32 *outb;
	struct fft_plan *plan;
	int i;

	if (audio_stream_get_channels(&src->stream) != 2) {
		return;
	}

	if (src->stream.size < size * sizeof(int32_t) * 2 ||
	    dst1->stream.size < size * sizeof(struct icomplex32) ||
	    dst2->stream.size < size * sizeof(struct icomplex32)) {
		return;
	}

	inb = rzalloc(SOF_MEM_FLAG_USER, size * sizeof(struct icomplex32));
	if (!inb) {
		return;
	}

	outb = rzalloc(SOF_MEM_FLAG_USER, size * sizeof(struct icomplex32));
	if (!outb) {
		goto err_outb;
	}

	plan = mod_fft_plan_new(&dummy, inb, outb, size, 32);
	if (!plan) {
		goto err_plan;
	}

	/* generate complex inputs */
	for (i = 0; i < size; i++) {
		inb[i].real = *((int32_t *)src->stream.addr + 2 * i);
		inb[i].imag = *((int32_t *)src->stream.addr + 2 * i + 1);
	}

	/* perform a single FFT transform */
	fft_execute_32(plan, false);

	/* calculate the outputs */
	*((int32_t *)dst1->stream.addr) = outb[0].real;
	*((int32_t *)dst1->stream.addr + 1) = 0;
	*((int32_t *)dst2->stream.addr) = 0;
	*((int32_t *)dst2->stream.addr + 1) = -outb[0].imag;
	for (i = 1; i < size; i++) {
		*((int32_t *)dst1->stream.addr + 2 * i) = (outb[i].real + outb[size - i].real) / 2;
		*((int32_t *)dst1->stream.addr + 2 * i + 1) =
			(outb[i].imag - outb[size - i].imag) / 2;
		*((int32_t *)dst2->stream.addr + 2 * i) = (outb[i].imag + outb[size - i].imag) / 2;
		*((int32_t *)dst2->stream.addr + 2 * i + 1) =
			(outb[size - i].real - outb[i].real) / 2;
	}

	mod_fft_plan_free(&dummy, plan);

err_plan:
	rfree(outb);
err_outb:
	rfree(inb);
}

static int power_peak_index_32(struct icomplex32 *out, int fft_size)
{
	int64_t p;
	int64_t peak = 0;
	int i;
	int r = 0;

	/* looking for the peak */
	for (i = 0; i < fft_size / 2; i++) {
		p = (int64_t)out[i].real * out[i].real + (int64_t)out[i].imag * out[i].imag;
		if (p > peak) {
			peak = p;
			r = i;
		}
	}

	return r;
}

static double integrate_power_32(struct icomplex32 *s, int start_idx, int end_idx)
{
	double energy = 0;
	int i;

	for (i = start_idx; i <= end_idx; i++) {
		energy += (double)s[i].real * s[i].real + (double)s[i].imag * s[i].imag;
	}

	return energy;
}

static void get_sine_32(int32_t *in, double sine_freq, double fs, int fft_size)
{
	double c = TWO_PI * sine_freq / fs;
	double w;
	int i;

	for (i = 0; i < fft_size; i++) {
		w = c * i;
		in[i] = (int32_t)round(SINE_SCALE_S32 * sin(w));
	}
}

/**
 * @brief Verify the 32-bit FFT SNR and peak bin for a 256-point real sine wave
 */
ZTEST(math_fft_suite, test_math_fft_256)
{
	struct sof_ipc_buffer test_buf_desc = {
		.size = 256 * 2 * sizeof(int32_t),
	};
	struct comp_buffer *source = buffer_new(NULL, &test_buf_desc, BUFFER_USAGE_NOT_SHARED);
	struct comp_buffer *sink = buffer_new(NULL, &test_buf_desc, BUFFER_USAGE_NOT_SHARED);
	struct icomplex32 *out = (struct icomplex32 *)sink->stream.addr;
	int32_t *in = (int32_t *)source->stream.addr;
	int fft_size = 256;
	int r;
	int i;
	double signal;
	double noise;
	double snr;

	/* create sine wave */
	get_sine_32(in, SINE_FREQ, SINE_FS, fft_size);
	audio_stream_set_channels(&source->stream, 1);

	/* do fft transform */
	fft_real(source, sink, fft_size);

	/* find peak */
	r = power_peak_index_32(out, fft_size);
	i = (int)round((SINE_FREQ * fft_size) / SINE_FS);
	printf("%s: peak at point %d\n", __func__, r);

	/* the peak should be in range i +/-1 */
	zassert_between_inclusive(r, i - 1, i + 1);

	/* the min. SNR should be met */
	noise = integrate_power_32(out, 0, i - 2);
	signal = integrate_power_32(out, i - 1, i + 1);
	noise += integrate_power_32(out, i + 2, fft_size / 2 - 1);
	snr = 10 * log10(signal / noise);
	printf("%s: SNR %5.2f dB\n", __func__, snr);
	zassert_true(snr >= MIN_SNR_256, "FFT 256 SNR %5.2f below minimum %5.2f", snr, MIN_SNR_256);

	buffer_free(source);
	buffer_free(sink);
}

/**
 * @brief Verify the 32-bit FFT SNR and peak bin for a 512-point real sine wave
 */
ZTEST(math_fft_suite, test_math_fft_512)
{
	struct sof_ipc_buffer test_buf_desc = {
		.size = 512 * 2 * sizeof(int32_t),
	};
	struct comp_buffer *source = buffer_new(NULL, &test_buf_desc, BUFFER_USAGE_NOT_SHARED);
	struct comp_buffer *sink = buffer_new(NULL, &test_buf_desc, BUFFER_USAGE_NOT_SHARED);
	struct icomplex32 *out = (struct icomplex32 *)sink->stream.addr;
	int32_t *in = (int32_t *)source->stream.addr;
	int fft_size = 512;
	int r;
	int i;
	double signal;
	double noise;
	double snr;

	/* create sine wave */
	get_sine_32(in, SINE_FREQ, SINE_FS, fft_size);
	audio_stream_set_channels(&source->stream, 1);

	/* do fft transform */
	fft_real(source, sink, fft_size);

	/* find peak */
	r = power_peak_index_32(out, fft_size);
	i = (int)round((SINE_FREQ * fft_size) / SINE_FS);
	printf("%s: peak at point %d\n", __func__, r);

	/* the peak should be in range i +/-1 */
	zassert_between_inclusive(r, i - 1, i + 1);

	/* the min. SNR should be met */
	noise = integrate_power_32(out, 0, i - 2);
	signal = integrate_power_32(out, i - 1, i + 1);
	noise += integrate_power_32(out, i + 2, fft_size / 2 - 1);
	snr = 10 * log10(signal / noise);
	printf("%s: SNR %5.2f dB\n", __func__, snr);
	zassert_true(snr >= MIN_SNR_512, "FFT 512 SNR %5.2f below minimum %5.2f", snr, MIN_SNR_512);

	buffer_free(source);
	buffer_free(sink);
}

/**
 * @brief Verify the 32-bit FFT SNR and peak bin for a 1024-point real sine wave
 */
ZTEST(math_fft_suite, test_math_fft_1024)
{
	struct sof_ipc_buffer test_buf_desc = {
		.size = 1024 * 2 * sizeof(int32_t),
	};
	struct comp_buffer *source = buffer_new(NULL, &test_buf_desc, BUFFER_USAGE_NOT_SHARED);
	struct comp_buffer *sink = buffer_new(NULL, &test_buf_desc, BUFFER_USAGE_NOT_SHARED);
	struct icomplex32 *out = (struct icomplex32 *)sink->stream.addr;
	int32_t *in = (int32_t *)source->stream.addr;
	int fft_size = 1024;
	int r;
	int i;
	double signal;
	double noise;
	double snr;

	/* create sine wave */
	get_sine_32(in, SINE_FREQ, SINE_FS, fft_size);
	audio_stream_set_channels(&source->stream, 1);

	/* do fft transform */
	fft_real(source, sink, fft_size);

	/* find peak */
	r = power_peak_index_32(out, fft_size);
	i = (int)round((SINE_FREQ * fft_size) / SINE_FS);
	printf("%s: peak at point %d\n", __func__, r);

	/* the peak should be in range i +/-1 */
	zassert_between_inclusive(r, i - 1, i + 1);

	/* the min. SNR should be met */
	noise = integrate_power_32(out, 0, i - 2);
	signal = integrate_power_32(out, i - 1, i + 1);
	noise += integrate_power_32(out, i + 2, fft_size / 2 - 1);
	snr = 10 * log10(signal / noise);
	printf("%s: SNR %5.2f dB\n", __func__, snr);
	zassert_true(snr >= MIN_SNR_1024, "FFT 1024 SNR %5.2f below minimum %5.2f", snr,
		     MIN_SNR_1024);

	buffer_free(source);
	buffer_free(sink);
}

/**
 * @brief Verify a 1024-point FFT followed by an IFFT reconstructs the signal
 */
ZTEST(math_fft_suite, test_math_fft_1024_ifft)
{
	struct sof_ipc_buffer test_buf_desc = {
		.size = 1024 * 4 * 2,
	};
	struct comp_buffer *source = buffer_new(NULL, &test_buf_desc, BUFFER_USAGE_NOT_SHARED);
	struct comp_buffer *intm = buffer_new(NULL, &test_buf_desc, BUFFER_USAGE_NOT_SHARED);
	struct comp_buffer *sink = buffer_new(NULL, &test_buf_desc, BUFFER_USAGE_NOT_SHARED);
	struct icomplex32 *out = (struct icomplex32 *)sink->stream.addr;
	double db;
	int64_t signal = 0;
	int64_t noise = 0;
	uint32_t fft_size = 1024;
	int32_t *in = (int32_t *)source->stream.addr;
	int i;

	get_sine_32(in, SINE_FREQ, SINE_FS, fft_size);
	audio_stream_set_channels(&source->stream, 1);

	/* do fft transform */
	fft_real(source, intm, fft_size);

	audio_stream_set_channels(&intm->stream, 1);
	/* do ifft transform */
	ifft_complex(intm, sink, fft_size);

	/* calculate signal and noise */
	for (i = 0; i < fft_size; i++) {
		signal += (int64_t)(in[i] / 32) * (in[i] / 32);
		noise += (int64_t)((out[i].real - in[i]) / 32) * ((out[i].real - in[i]) / 32) +
			 (int64_t)(out[i].imag / 32) * (out[i].imag / 32);
	}

	db = 10 * log10((double)signal / noise);
	printf("%s: SNR: %6.2f dB\n", __func__, db);
	zassert_true(db >= FFT_DB_TH, "FFT/IFFT SNR %6.2f below minimum %6.2f", db, FFT_DB_TH);

	buffer_free(source);
	buffer_free(intm);
	buffer_free(sink);
}

/**
 * @brief Verify a 512-point 2-channel real FFT finds the expected peak on
 * both channels
 */
ZTEST(math_fft_suite, test_math_fft_512_2ch)
{
	struct sof_ipc_buffer test_buf_desc = {
		.size = 512 * 4 * 2,
	};
	struct comp_buffer *source = buffer_new(NULL, &test_buf_desc, BUFFER_USAGE_NOT_SHARED);
	struct comp_buffer *sink1 = buffer_new(NULL, &test_buf_desc, BUFFER_USAGE_NOT_SHARED);
	struct comp_buffer *sink2 = buffer_new(NULL, &test_buf_desc, BUFFER_USAGE_NOT_SHARED);
	struct icomplex32 *out1 = (struct icomplex32 *)sink1->stream.addr;
	struct icomplex32 *out2 = (struct icomplex32 *)sink2->stream.addr;
	uint32_t fft_size = 512;
	int i;
	int r;

	for (i = 0; i < fft_size; i++) {
		*((int32_t *)source->stream.addr + 2 * i) = input_samples[i];
		*((int32_t *)source->stream.addr + 2 * i + 1) = input_samples[fft_size + i];
	}

	audio_stream_set_channels(&source->stream, 2);
	/* do fft transform */
	fft_real_2(source, sink1, sink2, fft_size);

	/* looking for the peak for channel 0 */
	r = power_peak_index_32(out1, fft_size);
	i = (SINE_HZ * fft_size) / 48000;
	printf("%s: peak for channel 0 at point %d\n", __func__, r);

	/* the peak should be in range i +/-1 */
	zassert_between_inclusive(r, i - 1, i + 1);

	/* looking for the peak for channel 1 */
	r = power_peak_index_32(out2, fft_size);
	i = (SINE_HZ * fft_size) / 48000;
	printf("%s: peak for channel 1 at point %d\n", __func__, r);

	/* the peak should be in range i +/-1 */
	zassert_between_inclusive(r, i - 1, i + 1);

	buffer_free(source);
	buffer_free(sink1);
	buffer_free(sink2);
}

/**
 * \brief Doing Fast Fourier Transform (FFT) for mono real input buffers.
 * \param[in] src - pointer to input buffer.
 * \param[out] dst - pointer to output buffer, FFT output for input buffer.
 * \param[in] size - input buffer sample count.
 */
static void fft_real_16(struct comp_buffer *src, struct comp_buffer *dst, uint32_t size)
{
	struct icomplex16 *inb;
	struct icomplex16 *outb;
	struct fft_plan *plan;
	int i;

	if (audio_stream_get_channels(&src->stream) != 1) {
		return;
	}

	if (src->stream.size < size * sizeof(int16_t) ||
	    dst->stream.size < size * sizeof(struct icomplex16)) {
		return;
	}

	inb = rzalloc(SOF_MEM_FLAG_USER, size * sizeof(struct icomplex16));
	if (!inb) {
		return;
	}
	outb = rzalloc(SOF_MEM_FLAG_USER, size * sizeof(struct icomplex16));
	if (!outb) {
		goto err_outb;
	}

	plan = mod_fft_plan_new(&dummy, inb, outb, size, 16);
	if (!plan) {
		goto err_plan;
	}

	for (i = 0; i < size; i++) {
		inb[i].real = *((int16_t *)src->stream.addr + i);
		inb[i].imag = 0;
	}

	/* perform a single FFT transform */
	fft_execute_16(plan, false);

	for (i = 0; i < size; i++) {
		*((int16_t *)dst->stream.addr + 2 * i) = outb[i].real;
		*((int16_t *)dst->stream.addr + 2 * i + 1) = outb[i].imag;
	}

	mod_fft_plan_free(&dummy, plan);

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
static void ifft_complex_16(struct comp_buffer *src, struct comp_buffer *dst, uint32_t size)
{
	struct icomplex16 *inb;
	struct icomplex16 *outb;
	struct fft_plan *plan;
	int i;

	if (audio_stream_get_channels(&src->stream) != 1) {
		return;
	}

	if (src->stream.size < size * sizeof(struct icomplex16) ||
	    dst->stream.size < size * sizeof(struct icomplex16)) {
		return;
	}

	inb = rzalloc(SOF_MEM_FLAG_USER, size * sizeof(struct icomplex16));
	if (!inb) {
		return;
	}

	outb = rzalloc(SOF_MEM_FLAG_USER, size * sizeof(struct icomplex16));
	if (!outb) {
		goto err_outb;
	}

	plan = mod_fft_plan_new(&dummy, inb, outb, size, 16);
	if (!plan) {
		goto err_plan;
	}

	for (i = 0; i < size; i++) {
		inb[i].real = *((int16_t *)src->stream.addr + 2 * i);
		inb[i].imag = *((int16_t *)src->stream.addr + 2 * i + 1);
	}

	/* perform a single IFFT transform */
	fft_execute_16(plan, true);

	for (i = 0; i < size; i++) {
		*((int16_t *)dst->stream.addr + 2 * i) = outb[i].real;
		*((int16_t *)dst->stream.addr + 2 * i + 1) = outb[i].imag;
	}

	mod_fft_plan_free(&dummy, plan);

err_plan:
	rfree(outb);
err_outb:
	rfree(inb);
}

static int power_peak_index_16(struct icomplex16 *out, int fft_size)
{
	int32_t p;
	int32_t peak = 0;
	int i;
	int r = 0;

	/* looking for the peak */
	for (i = 0; i < fft_size / 2; i++) {
		p = (int32_t)out[i].real * out[i].real + (int32_t)out[i].imag * out[i].imag;
		if (p > peak) {
			peak = p;
			r = i;
		}
	}

	return r;
}

static double integrate_power_16(struct icomplex16 *s, int start_idx, int end_idx)
{
	double energy = 0;
	int i;

	for (i = start_idx; i <= end_idx; i++) {
		energy += (double)s[i].real * s[i].real + (double)s[i].imag * s[i].imag;
	}

	return energy;
}

static void get_sine_16(int16_t *in, double sine_freq, double fs, int fft_size)
{
	double c = TWO_PI * sine_freq / fs;
	double w;
	int i;

	for (i = 0; i < fft_size; i++) {
		w = c * i;
		in[i] = (int16_t)round(SINE_SCALE_S16 * sin(w));
	}
}

/**
 * @brief Verify the 16-bit FFT SNR and peak bin for a 256-point real sine wave
 */
ZTEST(math_fft_suite, test_math_fft_256_16)
{
	struct sof_ipc_buffer test_buf_desc = {
		.size = 256 * 2 * sizeof(int16_t),
	};
	struct comp_buffer *source = buffer_new(NULL, &test_buf_desc, BUFFER_USAGE_NOT_SHARED);
	struct comp_buffer *sink = buffer_new(NULL, &test_buf_desc, BUFFER_USAGE_NOT_SHARED);
	struct icomplex16 *out = (struct icomplex16 *)sink->stream.addr;
	int16_t *in = (int16_t *)source->stream.addr;
	int fft_size = 256;
	int r;
	int i;
	double signal;
	double noise;
	double snr;

	/* create sine wave */
	get_sine_16(in, SINE_FREQ, SINE_FS, fft_size);
	audio_stream_set_channels(&source->stream, 1);

	/* do fft transform */
	fft_real_16(source, sink, fft_size);

	/* find peak */
	r = power_peak_index_16(out, fft_size);
	i = (int)round((SINE_FREQ * fft_size) / SINE_FS);
	printf("%s: peak at point %d\n", __func__, r);

	/* the peak should be in range i +/-1 */
	zassert_between_inclusive(r, i - 1, i + 1);

	/* the min. SNR should be met */
	noise = integrate_power_16(out, 0, i - 2);
	signal = integrate_power_16(out, i - 1, i + 1);
	noise += integrate_power_16(out, i + 2, fft_size / 2 - 1);
	snr = 10 * log10(signal / noise);
	printf("%s: SNR %5.2f dB\n", __func__, snr);
	zassert_true(snr >= MIN_SNR_256_16, "FFT16 256 SNR %5.2f below minimum %5.2f", snr,
		     MIN_SNR_256_16);

	buffer_free(source);
	buffer_free(sink);
}

/**
 * @brief Verify the 16-bit FFT SNR and peak bin for a 512-point real sine wave
 */
ZTEST(math_fft_suite, test_math_fft_512_16)
{
	struct sof_ipc_buffer test_buf_desc = {
		.size = 512 * 2 * sizeof(int16_t),
	};
	struct comp_buffer *source = buffer_new(NULL, &test_buf_desc, BUFFER_USAGE_NOT_SHARED);
	struct comp_buffer *sink = buffer_new(NULL, &test_buf_desc, BUFFER_USAGE_NOT_SHARED);
	struct icomplex16 *out = (struct icomplex16 *)sink->stream.addr;
	int16_t *in = (int16_t *)source->stream.addr;
	int fft_size = 512;
	int r;
	int i;
	double signal;
	double noise;
	double snr;

	/* create sine wave */
	get_sine_16(in, SINE_FREQ, SINE_FS, fft_size);
	audio_stream_set_channels(&source->stream, 1);

	/* do fft transform */
	fft_real_16(source, sink, fft_size);

	/* find peak */
	r = power_peak_index_16(out, fft_size);
	i = (int)round((SINE_FREQ * fft_size) / SINE_FS);
	printf("%s: peak at point %d\n", __func__, r);

	/* the peak should be in range i +/-1 */
	zassert_between_inclusive(r, i - 1, i + 1);

	/* the min. SNR should be met */
	noise = integrate_power_16(out, 0, i - 2);
	signal = integrate_power_16(out, i - 1, i + 1);
	noise += integrate_power_16(out, i + 2, fft_size / 2 - 1);
	snr = 10 * log10(signal / noise);
	printf("%s: SNR %5.2f dB\n", __func__, snr);
	zassert_true(snr >= MIN_SNR_512_16, "FFT16 512 SNR %5.2f below minimum %5.2f", snr,
		     MIN_SNR_512_16);

	buffer_free(source);
	buffer_free(sink);
}

/**
 * @brief Verify the 16-bit FFT SNR and peak bin for a 1024-point real sine wave
 */
ZTEST(math_fft_suite, test_math_fft_1024_16)
{
	struct sof_ipc_buffer test_buf_desc = {
		.size = 1024 * 2 * sizeof(int16_t),
	};
	struct comp_buffer *source = buffer_new(NULL, &test_buf_desc, BUFFER_USAGE_NOT_SHARED);
	struct comp_buffer *sink = buffer_new(NULL, &test_buf_desc, BUFFER_USAGE_NOT_SHARED);
	struct icomplex16 *out = (struct icomplex16 *)sink->stream.addr;
	int16_t *in = (int16_t *)source->stream.addr;
	int fft_size = 1024;
	int r;
	int i;
	double signal;
	double noise;
	double snr;

	/* create sine wave */
	get_sine_16(in, SINE_FREQ, SINE_FS, fft_size);
	audio_stream_set_channels(&source->stream, 1);

	/* do fft transform */
	fft_real_16(source, sink, fft_size);

	/* find peak */
	r = power_peak_index_16(out, fft_size);
	i = (int)round((SINE_FREQ * fft_size) / SINE_FS);
	printf("%s: peak at point %d\n", __func__, r);

	/* the peak should be in range i +/-1 */
	zassert_between_inclusive(r, i - 1, i + 1);

	/* the min. SNR should be met */
	noise = integrate_power_16(out, 0, i - 2);
	signal = integrate_power_16(out, i - 1, i + 1);
	noise += integrate_power_16(out, i + 2, fft_size / 2 - 1);
	snr = 10 * log10(signal / noise);
	printf("%s: SNR %5.2f dB\n", __func__, snr);
	zassert_true(snr >= MIN_SNR_1024_16, "FFT16 1024 SNR %5.2f below minimum %5.2f", snr,
		     MIN_SNR_1024_16);

	buffer_free(source);
	buffer_free(sink);
}

/**
 * @brief Verify a 1024-point 16-bit FFT followed by an IFFT reconstructs the signal
 */
ZTEST(math_fft_suite, test_math_fft_1024_ifft_16)
{
	struct sof_ipc_buffer test_buf_desc = {
		.size = 1024 * 2 * sizeof(int16_t),
	};
	struct comp_buffer *source = buffer_new(NULL, &test_buf_desc, BUFFER_USAGE_NOT_SHARED);
	struct comp_buffer *intm = buffer_new(NULL, &test_buf_desc, BUFFER_USAGE_NOT_SHARED);
	struct comp_buffer *sink = buffer_new(NULL, &test_buf_desc, BUFFER_USAGE_NOT_SHARED);
	struct icomplex16 *out = (struct icomplex16 *)sink->stream.addr;
	double db;
	int64_t signal = 0;
	int64_t noise = 0;
	uint32_t fft_size = 1024;
	int16_t *in = (int16_t *)source->stream.addr;
	int i;

	get_sine_16(in, SINE_FREQ, SINE_FS, fft_size);
	audio_stream_set_channels(&source->stream, 1);

	/* do fft transform */
	fft_real_16(source, intm, fft_size);

	audio_stream_set_channels(&intm->stream, 1);
	/* do ifft transform */
	ifft_complex_16(intm, sink, fft_size);

	/* calculate signal and noise */
	for (i = 0; i < fft_size; i++) {
		signal += (int64_t)in[i] * in[i];
		noise += (int64_t)(out[i].real - in[i]) * (out[i].real - in[i]) +
			 (int64_t)out[i].imag * out[i].imag;
	}

	db = 10 * log10((double)signal / noise);
	printf("%s: SNR: %6.2f dB\n", __func__, db);
	zassert_true(db >= FFT_DB_TH_16, "FFT16/IFFT16 SNR %6.2f below minimum %6.2f", db,
		     FFT_DB_TH_16);

	buffer_free(source);
	buffer_free(intm);
	buffer_free(sink);
}

/**
 * @brief Define and initialize the FFT/IFFT test suite
 *
 * mod_alloc_test_after() checks that each test case freed the FFT plans it
 * allocated through the mod_alloc() API.
 */
ZTEST_SUITE(math_fft_suite, NULL, NULL, NULL, mod_alloc_test_after, NULL);
