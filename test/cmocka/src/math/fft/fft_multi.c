// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2025 Intel Corporation. All rights reserved.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

#include <sof/audio/format.h>
#include <sof/math/fft.h>
#include "ref_fft_multi_96_32.h"
#include "ref_fft_multi_512_32.h"
#include "ref_fft_multi_768_32.h"
#include "ref_fft_multi_1024_32.h"
#include "ref_fft_multi_1536_32.h"
#include "ref_fft_multi_3072_32.h"

#include "ref_ifft_multi_24_32.h"
#include "ref_ifft_multi_256_32.h"
#include "ref_ifft_multi_1024_32.h"
#include "ref_ifft_multi_1536_32.h"
#include "ref_ifft_multi_3072_32.h"

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

#define FFT_MAX_ERROR_ABS		1050.0		/* about -126 dB */
#define FFT_MAX_ERROR_RMS		35.0		/* about -156 dB */
#define IFFT_MAX_ERROR_ABS		2400000.0	/* about -59 dB */
#define IFFT_MAX_ERROR_RMS		44000.0		/* about -94 dB */

struct processing_module dummy;

static void fft_multi_32_test(const int32_t *in_real, const int32_t *in_imag,
			      const int32_t *ref_real, const int32_t *ref_imag,
			      int num_bins, int num_tests, double max_error_abs,
			      double max_error_rms, bool do_ifft)
{
	struct icomplex32 *x;
	struct icomplex32 *y;
	struct fft_multi_plan *plan;
	double delta;
	double error_rms;
	double delta_max = 0;
	double sum_squares = 0;
	const int32_t *p_in_real = in_real;
	const int32_t *p_in_imag = in_imag;
	const int32_t *p_ref_real = ref_real;
	const int32_t *p_ref_imag = ref_imag;
	int i, j;
	FILE *fh1, *fh2;

	x = malloc(num_bins * sizeof(struct icomplex32));
	if (!x) {
		fprintf(stderr, "Failed to allocate input data buffer.\n");
		assert_true(false);
	}

	y = malloc(num_bins * sizeof(struct icomplex32));
	if (!y) {
		fprintf(stderr, "Failed to allocate output data buffer.\n");
		assert_true(false);
	}

	plan = mod_fft_multi_plan_new(&dummy, x, y, num_bins, 32);
	if (!plan) {
		fprintf(stderr, "Failed to allocate FFT plan.\n");
		assert_true(false);
	}

	fh1 = fopen("debug_fft_multi_in.txt", "w");
	fh2 = fopen("debug_fft_multi_out.txt", "w");

	for (i = 0; i < num_tests; i++) {
		for (j = 0; j < num_bins; j++) {
			x[j].real = *p_in_real++;
			x[j].imag = *p_in_imag++;
			fprintf(fh1, "%d %d\n", x[j].real, x[j].imag);
		}

		fft_multi_execute_32(plan, do_ifft);

		for (j = 0; j < num_bins; j++) {
			fprintf(fh2, "%d %d %d %d\n",
				y[j].real, y[j].imag, *p_ref_real, *p_ref_imag);
			delta = (double)*p_ref_real - (double)y[j].real;
			sum_squares += delta * delta;
			if (delta > delta_max)
				delta_max = delta;
			else if (-delta > delta_max)
				delta_max = -delta;

			delta = (double)*p_ref_imag - (double)y[j].imag;
			sum_squares += delta * delta;
			if (delta > delta_max)
				delta_max = delta;
			else if (-delta > delta_max)
				delta_max = -delta;

			p_ref_real++;
			p_ref_imag++;
		}

	}

	mod_fft_multi_plan_free(&dummy, plan);
	free(y);
	free(x);
	fclose(fh1); fclose(fh2);

	error_rms = sqrt(sum_squares / (double)(2 * num_bins * num_tests));
	printf("Max absolute error = %5.2f (limit %5.2f), error RMS = %5.2f (limit %5.2f)\n",
	       delta_max, max_error_abs, error_rms, max_error_rms);

	assert_true(error_rms < max_error_rms);
	assert_true(delta_max < max_error_abs);
}

static void fft_multi_32_test_1(void **state)
{
	(void)state;

	/* Test FFT */
	fft_multi_32_test(fft_in_real_96_q31, fft_in_imag_96_q31,
			  fft_ref_real_96_q31, fft_ref_imag_96_q31,
			  96, REF_SOFM_FFT_MULTI_96_NUM_TESTS,
			  FFT_MAX_ERROR_ABS, FFT_MAX_ERROR_RMS, false);
	fft_multi_32_test(fft_in_real_512_q31, fft_in_imag_512_q31,
			  fft_ref_real_512_q31, fft_ref_imag_512_q31,
			  512, REF_SOFM_FFT_MULTI_512_NUM_TESTS,
			  FFT_MAX_ERROR_ABS, FFT_MAX_ERROR_RMS, false);
	fft_multi_32_test(fft_in_real_768_q31, fft_in_imag_768_q31,
			  fft_ref_real_768_q31, fft_ref_imag_768_q31,
			  768, REF_SOFM_FFT_MULTI_768_NUM_TESTS,
			  FFT_MAX_ERROR_ABS, FFT_MAX_ERROR_RMS, false);
	fft_multi_32_test(fft_in_real_1024_q31, fft_in_imag_1024_q31,
			  fft_ref_real_1024_q31, fft_ref_imag_1024_q31,
			  1024, REF_SOFM_FFT_MULTI_1024_NUM_TESTS,
			  FFT_MAX_ERROR_ABS, FFT_MAX_ERROR_RMS, false);
	fft_multi_32_test(fft_in_real_1536_q31, fft_in_imag_1536_q31,
			  fft_ref_real_1536_q31, fft_ref_imag_1536_q31,
			  1536, REF_SOFM_FFT_MULTI_1536_NUM_TESTS,
			  FFT_MAX_ERROR_ABS, FFT_MAX_ERROR_RMS, false);
	fft_multi_32_test(fft_in_real_3072_q31, fft_in_imag_3072_q31,
			  fft_ref_real_3072_q31, fft_ref_imag_3072_q31,
			  3072, REF_SOFM_FFT_MULTI_3072_NUM_TESTS,
			  FFT_MAX_ERROR_ABS, FFT_MAX_ERROR_RMS, false);
	fft_multi_32_test(fft_in_real_3072_q31, fft_in_imag_3072_q31,
			  fft_ref_real_3072_q31, fft_ref_imag_3072_q31,
			  3072, REF_SOFM_FFT_MULTI_3072_NUM_TESTS,
			  FFT_MAX_ERROR_ABS, FFT_MAX_ERROR_RMS, false);

	/* Test IFFT */
	fft_multi_32_test(ifft_in_real_24_q31, ifft_in_imag_24_q31,
			  ifft_ref_real_24_q31, ifft_ref_imag_24_q31,
			  24, REF_SOFM_IFFT_MULTI_24_NUM_TESTS,
			  IFFT_MAX_ERROR_ABS, IFFT_MAX_ERROR_RMS, true);
	fft_multi_32_test(ifft_in_real_256_q31, ifft_in_imag_256_q31,
			  ifft_ref_real_256_q31, ifft_ref_imag_256_q31,
			  256, REF_SOFM_IFFT_MULTI_256_NUM_TESTS,
			  IFFT_MAX_ERROR_ABS, IFFT_MAX_ERROR_RMS, true);
	fft_multi_32_test(ifft_in_real_1024_q31, ifft_in_imag_1024_q31,
			  ifft_ref_real_1024_q31, ifft_ref_imag_1024_q31,
			  1024, REF_SOFM_IFFT_MULTI_1024_NUM_TESTS,
			  IFFT_MAX_ERROR_ABS, IFFT_MAX_ERROR_RMS, true);
	fft_multi_32_test(ifft_in_real_1536_q31, ifft_in_imag_1536_q31,
			  ifft_ref_real_1536_q31, ifft_ref_imag_1536_q31,
			  1536, REF_SOFM_IFFT_MULTI_1536_NUM_TESTS,
			  IFFT_MAX_ERROR_ABS, IFFT_MAX_ERROR_RMS, true);
	fft_multi_32_test(ifft_in_real_3072_q31, ifft_in_imag_3072_q31,
			  ifft_ref_real_3072_q31, ifft_ref_imag_3072_q31,
			  3072, REF_SOFM_IFFT_MULTI_3072_NUM_TESTS,
			  IFFT_MAX_ERROR_ABS, IFFT_MAX_ERROR_RMS, true);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(fft_multi_32_test_1),
	};

	cmocka_set_message_output(CM_OUTPUT_TAP);

	return cmocka_run_group_tests(tests, NULL, NULL);
}
