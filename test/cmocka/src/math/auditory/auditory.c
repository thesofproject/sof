// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 Intel Corporation. All rights reserved.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <string.h>
#include <cmocka.h>
#include <math.h>
#include <sof/math/auditory.h>
#include <sof/math/fft.h>
#include "ref_hz_to_mel.h"
#include "ref_mel_filterbank_16_test1.h"
#include "ref_mel_filterbank_32_test1.h"
#include "ref_mel_filterbank_16_test2.h"
#include "ref_mel_filterbank_32_test2.h"
#include "ref_mel_filterbank_16_test3.h"
#include "ref_mel_filterbank_32_test3.h"
#include "ref_mel_filterbank_16_test4.h"
#include "ref_mel_filterbank_32_test4.h"

#define HZ_TO_MEL_MAX_ERROR_ABS  1.5
#define HZ_TO_MEL_MAX_ERROR_RMS  0.5
#define MEL_TO_HZ_MAX_ERROR_ABS  5.0
#define MEL_TO_HZ_MAX_ERROR_RMS  1.5

#define MEL_FB16_MAX_ERROR_ABS  5.0
#define MEL_FB16_MAX_ERROR_RMS  3.0
#define MEL_FB32_MAX_ERROR_ABS  5.0
#define MEL_FB32_MAX_ERROR_RMS  3.0

#undef DEBUGFILES /* Change this to #define to get output data files for debugging */

static void filterbank_16_test(const int16_t *fft_real, const int16_t *fft_imag,
			       const int16_t *ref_mel_log,
			       int num_fft_bins, int num_mel_bins, int norm_slaney,
			       enum psy_mel_log_scale mel_log_type, int shift)
{
	struct psy_mel_filterbank fb;
	struct icomplex16 *fft_buf;
	struct icomplex16 *fft_out;
	float delta;
	float sum_squares = 0;
	float error_rms;
	float delta_max = 0;
	int32_t *power_spectra;
	int16_t *mel_log;
	int i;
	const int half_fft = num_fft_bins / 2 + 1;
	const int fft_size = num_fft_bins * sizeof(struct icomplex16);
	int ret;

	fft_buf = malloc(fft_size);
	if (!fft_buf) {
		fprintf(stderr, "Failed to allocate fft_buf\n");
		exit(EXIT_FAILURE);
	}

	fft_out = malloc(fft_size);
	if (!fft_out) {
		fprintf(stderr, "Failed to allocate fft_out\n");
		goto err_out_alloc;
	}

	mel_log = malloc(num_mel_bins * sizeof(int16_t));
	if (!mel_log) {
		fprintf(stderr, "Failed to allocate output vector\n");
		goto err_mel_alloc;
	}

	fb.samplerate = 16000;
	fb.start_freq = 100;
	fb.end_freq = 7500;
	fb.mel_bins = num_mel_bins;
	fb.slaney_normalize = (norm_slaney > 0);
	fb.mel_log_scale = mel_log_type;
	fb.fft_bins = num_fft_bins;
	fb.half_fft_bins = half_fft;
	fb.scratch_data1 = (int16_t *)fft_buf;
	fb.scratch_data2 = (int16_t *)fft_out;
	fb.scratch_length1 = fft_size / sizeof(int16_t);
	fb.scratch_length2 = fft_size / sizeof(int16_t);
	ret = psy_get_mel_filterbank(&fb);
	if (ret < 0) {
		fprintf(stderr, "Failed Mel filterbank\n");
		goto err_get_filterbank;
	}

	/* Copy input from test vectors */
	for (i = 0; i < half_fft; i++) {
		fft_out[i].real = fft_real[i];
		fft_out[i].imag = fft_imag[i];
	}

	/* Run filterbank */
	power_spectra = (int32_t *)&fft_buf[0];
	psy_apply_mel_filterbank_16(&fb, fft_out, power_spectra, mel_log, shift);

	/* Check */
	for (i = 0; i < num_mel_bins; i++) {
		delta = (float)ref_mel_log[i] - (float)mel_log[i];
		sum_squares += delta * delta;
		if (delta > delta_max)
			delta_max = delta;
		else if (-delta > delta_max)
			delta_max = -delta;
	}

	error_rms = sqrt(sum_squares / (float)num_mel_bins);
	printf("Max absolute error = %5.2f (max %5.2f), error RMS = %5.2f (max %5.2f)\n",
	       delta_max, MEL_FB16_MAX_ERROR_ABS, error_rms, MEL_FB16_MAX_ERROR_RMS);

#ifdef DEBUGFILES
	FILE *fh = fopen("mel_filterbank_16.txt", "w");

	for (i = 0; i < num_mel_bins; i++)
		fprintf(fh, "%d %d\n", ref_mel_log[i], mel_log[i]);

	fclose(fh);
#endif

	assert_true(error_rms < MEL_FB16_MAX_ERROR_RMS);
	assert_true(delta_max < MEL_FB16_MAX_ERROR_ABS);
	return;

err_get_filterbank:
	free(mel_log);

err_mel_alloc:
	free(fft_buf);

err_out_alloc:
	free(fft_out);
	exit(EXIT_FAILURE);
}

static void filterbank_32_test(const int32_t *fft_real, const int32_t *fft_imag,
			       const int16_t *ref_mel_log,
			       int num_fft_bins, int num_mel_bins, int norm_slaney,
			       enum psy_mel_log_scale mel_log_type, int shift)
{
	struct psy_mel_filterbank fb;
	struct icomplex32 *fft_buf;
	struct icomplex32 *fft_out;
	float delta;
	float sum_squares = 0;
	float error_rms;
	float delta_max = 0;
	int32_t *power_spectra;
	int16_t *mel_log;
	int i;
	const int half_fft = num_fft_bins / 2 + 1;
	const int fft_size = num_fft_bins * sizeof(struct icomplex32);
	int ret;

	fft_buf = malloc(fft_size);
	if (!fft_buf) {
		fprintf(stderr, "Failed to allocate fft_buf\n");
		exit(EXIT_FAILURE);
	}

	fft_out = malloc(fft_size);
	if (!fft_out) {
		fprintf(stderr, "Failed to allocate fft_out\n");
		goto err_out_alloc;
	}

	mel_log = malloc(MEL_FILTERBANK_32_TEST1_NUM_MEL_BINS * sizeof(int16_t));
	if (!mel_log) {
		fprintf(stderr, "Failed to allocate output vector\n");
		goto err_mel_alloc;
	}

	fb.samplerate = 16000;
	fb.start_freq = 100;
	fb.end_freq = 7500;
	fb.mel_bins = num_mel_bins;
	fb.slaney_normalize = (norm_slaney > 0);
	fb.mel_log_scale = mel_log_type;
	fb.fft_bins = num_fft_bins;
	fb.half_fft_bins = half_fft;
	fb.scratch_data1 = (int16_t *)fft_buf;
	fb.scratch_data2 = (int16_t *)fft_out;
	fb.scratch_length1 = fft_size / sizeof(int16_t);
	fb.scratch_length2 = fft_size / sizeof(int16_t);
	ret = psy_get_mel_filterbank(&fb);
	if (ret < 0) {
		fprintf(stderr, "Failed Mel filterbank\n");
		goto err_get_filterbank;
	}

	/* Copy input from test vectors */
	for (i = 0; i < half_fft; i++) {
		fft_out[i].real = fft_real[i];
		fft_out[i].imag = fft_imag[i];
	}

	/* Run filterbank */
	power_spectra = (int32_t *)&fft_buf[0];
	psy_apply_mel_filterbank_32(&fb, fft_out, power_spectra, mel_log, shift);

	/* Check */
	for (i = 0; i < num_mel_bins; i++) {
		delta = (float)ref_mel_log[i] - (float)mel_log[i];
		sum_squares += delta * delta;
		if (delta > delta_max)
			delta_max = delta;
		else if (-delta > delta_max)
			delta_max = -delta;
	}

	error_rms = sqrt(sum_squares / (float)num_mel_bins);
	printf("Max absolute error = %5.2f (max %5.2f), error RMS = %5.2f (max %5.2f)\n",
	       delta_max, MEL_FB32_MAX_ERROR_ABS, error_rms, MEL_FB32_MAX_ERROR_RMS);

#ifdef DEBUGFILES
	FILE *fh = fopen("mel_filterbank_32.txt", "w");

	for (i = 0; i < num_mel_bins; i++)
		fprintf(fh, "%d %d\n", ref_mel_log[i], mel_log[i]);

	fclose(fh);
#endif

	assert_true(error_rms < MEL_FB32_MAX_ERROR_RMS);
	assert_true(delta_max < MEL_FB32_MAX_ERROR_ABS);
	return;

err_get_filterbank:
	free(mel_log);

err_mel_alloc:
	free(fft_buf);

err_out_alloc:
	free(fft_out);
	exit(EXIT_FAILURE);
}

static void test_hz_to_mel(void **state)
{
	float error_rms;
	float delta_max = 0.0;
	float delta;
	float sum_squares = 0.0;
	int16_t test_mel;
	int i;

	(void)state;

	for (i = 0; i < HZ_TO_MEL_NPOINTS; i++) {
		test_mel = psy_hz_to_mel(ref_hz[i]);
		delta = (float)ref_mel[i] - (float)test_mel;
		sum_squares += delta * delta;
		if (delta > delta_max)
			delta_max = delta;
		else if (-delta > delta_max)
			delta_max = -delta;
	}

	error_rms = sqrt(sum_squares / (float)HZ_TO_MEL_NPOINTS);
	printf("Max absolute error = %5.2f (max %5.2f), error RMS = %5.2f (max %5.2f)\n",
	       delta_max, HZ_TO_MEL_MAX_ERROR_ABS, error_rms, HZ_TO_MEL_MAX_ERROR_RMS);

	assert_true(error_rms < HZ_TO_MEL_MAX_ERROR_RMS);
	assert_true(delta_max < HZ_TO_MEL_MAX_ERROR_ABS);
}

static void test_mel_to_hz(void **state)
{
	float error_rms;
	float delta_max = 0.0;
	float delta;
	float sum_squares = 0.0;
	int16_t test_hz;
	int i;

	(void)state;

	for (i = 0; i < HZ_TO_MEL_NPOINTS; i++) {
		test_hz = psy_mel_to_hz(ref_mel[i]);
		delta = (float)ref_revhz[i] - (float)test_hz;
		sum_squares += delta * delta;
		if (delta > delta_max)
			delta_max = delta;
		else if (-delta > delta_max)
			delta_max = -delta;
	}

	error_rms = sqrt(sum_squares / (float)HZ_TO_MEL_NPOINTS);
	printf("Max absolute error = %5.2f (max %5.2f), error RMS = %5.2f (max %5.2f)\n",
	       delta_max, MEL_TO_HZ_MAX_ERROR_ABS, error_rms, MEL_TO_HZ_MAX_ERROR_RMS);

	assert_true(error_rms < MEL_TO_HZ_MAX_ERROR_RMS);
	assert_true(delta_max < MEL_TO_HZ_MAX_ERROR_ABS);
}

static void test_mel_filterbank_16_test1(void **state)
{
	(void)state;

	filterbank_16_test(mel_filterbank_16_test1_real,
			   mel_filterbank_16_test1_imag,
			   mel_filterbank_16_test1_mel_log,
			   MEL_FILTERBANK_16_TEST1_FFT_SIZE,
			   MEL_FILTERBANK_16_TEST1_NUM_MEL_BINS,
			   MEL_FILTERBANK_16_TEST1_NORM_SLANEY,
			   MEL_FILTERBANK_16_TEST1_MEL_LOG,
			   MEL_FILTERBANK_16_TEST1_SHIFT);
}

static void test_mel_filterbank_32_test1(void **state)
{
	(void)state;

	filterbank_32_test(mel_filterbank_32_test1_real,
			   mel_filterbank_32_test1_imag,
			   mel_filterbank_32_test1_mel_log,
			   MEL_FILTERBANK_32_TEST1_FFT_SIZE,
			   MEL_FILTERBANK_32_TEST1_NUM_MEL_BINS,
			   MEL_FILTERBANK_32_TEST1_NORM_SLANEY,
			   MEL_FILTERBANK_32_TEST1_MEL_LOG,
			   MEL_FILTERBANK_32_TEST1_SHIFT);
}

static void test_mel_filterbank_16_test2(void **state)
{
	(void)state;

	filterbank_16_test(mel_filterbank_16_test2_real,
			   mel_filterbank_16_test2_imag,
			   mel_filterbank_16_test2_mel_log,
			   MEL_FILTERBANK_16_TEST2_FFT_SIZE,
			   MEL_FILTERBANK_16_TEST2_NUM_MEL_BINS,
			   MEL_FILTERBANK_16_TEST2_NORM_SLANEY,
			   MEL_FILTERBANK_16_TEST2_MEL_LOG,
			   MEL_FILTERBANK_16_TEST2_SHIFT);
}

static void test_mel_filterbank_32_test2(void **state)
{
	(void)state;

	filterbank_32_test(mel_filterbank_32_test2_real,
			   mel_filterbank_32_test2_imag,
			   mel_filterbank_32_test2_mel_log,
			   MEL_FILTERBANK_32_TEST2_FFT_SIZE,
			   MEL_FILTERBANK_32_TEST2_NUM_MEL_BINS,
			   MEL_FILTERBANK_32_TEST2_NORM_SLANEY,
			   MEL_FILTERBANK_32_TEST2_MEL_LOG,
			   MEL_FILTERBANK_32_TEST2_SHIFT);
}

static void test_mel_filterbank_16_test3(void **state)
{
	(void)state;

	filterbank_16_test(mel_filterbank_16_test3_real,
			   mel_filterbank_16_test3_imag,
			   mel_filterbank_16_test3_mel_log,
			   MEL_FILTERBANK_16_TEST3_FFT_SIZE,
			   MEL_FILTERBANK_16_TEST3_NUM_MEL_BINS,
			   MEL_FILTERBANK_16_TEST3_NORM_SLANEY,
			   MEL_FILTERBANK_16_TEST3_MEL_LOG,
			   MEL_FILTERBANK_16_TEST3_SHIFT);
}

static void test_mel_filterbank_32_test3(void **state)
{
	(void)state;

	filterbank_32_test(mel_filterbank_32_test3_real,
			   mel_filterbank_32_test3_imag,
			   mel_filterbank_32_test3_mel_log,
			   MEL_FILTERBANK_32_TEST3_FFT_SIZE,
			   MEL_FILTERBANK_32_TEST3_NUM_MEL_BINS,
			   MEL_FILTERBANK_32_TEST3_NORM_SLANEY,
			   MEL_FILTERBANK_32_TEST3_MEL_LOG,
			   MEL_FILTERBANK_32_TEST3_SHIFT);
}

static void test_mel_filterbank_16_test4(void **state)
{
	(void)state;

	filterbank_16_test(mel_filterbank_16_test4_real,
			   mel_filterbank_16_test4_imag,
			   mel_filterbank_16_test4_mel_log,
			   MEL_FILTERBANK_16_TEST4_FFT_SIZE,
			   MEL_FILTERBANK_16_TEST4_NUM_MEL_BINS,
			   MEL_FILTERBANK_16_TEST4_NORM_SLANEY,
			   MEL_FILTERBANK_16_TEST4_MEL_LOG,
			   MEL_FILTERBANK_16_TEST4_SHIFT);
}

static void test_mel_filterbank_32_test4(void **state)
{
	(void)state;

	filterbank_32_test(mel_filterbank_32_test4_real,
			   mel_filterbank_32_test4_imag,
			   mel_filterbank_32_test4_mel_log,
			   MEL_FILTERBANK_32_TEST4_FFT_SIZE,
			   MEL_FILTERBANK_32_TEST4_NUM_MEL_BINS,
			   MEL_FILTERBANK_32_TEST4_NORM_SLANEY,
			   MEL_FILTERBANK_32_TEST4_MEL_LOG,
			   MEL_FILTERBANK_32_TEST4_SHIFT);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_mel_filterbank_16_test1),
		cmocka_unit_test(test_mel_filterbank_32_test1),
		cmocka_unit_test(test_mel_filterbank_16_test2),
		cmocka_unit_test(test_mel_filterbank_32_test2),
		cmocka_unit_test(test_mel_filterbank_16_test3),
		cmocka_unit_test(test_mel_filterbank_32_test3),
		cmocka_unit_test(test_mel_filterbank_16_test4),
		cmocka_unit_test(test_mel_filterbank_32_test4),
		cmocka_unit_test(test_hz_to_mel),
		cmocka_unit_test(test_mel_to_hz),
	};

	cmocka_set_message_output(CM_OUTPUT_TAP);

	return cmocka_run_group_tests(tests, NULL, NULL);
}
