// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.
//
// These contents may have been developed with support from one or more
// Intel-operated generative artificial intelligence solutions.
//
// Converted from CMock to Ztest
// Original tests from test/cmocka/src/math/fft/fft_multi.c:
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

#include <zephyr/ztest.h>

#include <rtos/alloc.h>
#include <sof/audio/format.h>
#include <sof/audio/module_adapter/module/generic.h>
#include <sof/math/icomplex16.h>
#include <sof/math/icomplex32.h>
#include <sof/math/fft.h>

#include "mod_alloc.h"

/* Reference test vectors shared with the legacy CMocka test. They are
 * #included in place from the CMocka tree (added to the include path in
 * CMakeLists.txt) so the exact same fixed-point data is exercised.
 */
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
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>

#define FFT_MAX_ERROR_ABS  1050.0    /* about -126 dB */
#define FFT_MAX_ERROR_RMS  35.0      /* about -156 dB */
#define IFFT_MAX_ERROR_ABS 2400000.0 /* about -59 dB */
#define IFFT_MAX_ERROR_RMS 44000.0   /* about -94 dB */

static struct processing_module dummy;

/**
 * @brief Run a multi-size FFT/IFFT test against a reference vector.
 *
 * Feeds each of the num_tests blocks of num_bins complex samples through
 * the FFT-multi plan and accumulates the absolute and RMS error against
 * the pre-computed reference, preserving the fixed-point semantics of the
 * original CMocka test (the debug .txt file dumps of the legacy test are
 * dropped as they were unused diagnostics, not part of the test itself).
 */
static void fft_multi_32_test(const int32_t *in_real, const int32_t *in_imag,
			      const int32_t *ref_real, const int32_t *ref_imag, int num_bins,
			      int num_tests, double max_error_abs, double max_error_rms,
			      bool do_ifft)
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

	x = rzalloc(SOF_MEM_FLAG_USER, num_bins * sizeof(struct icomplex32));
	zassert_not_null(x, "Failed to allocate input data buffer.");

	y = rzalloc(SOF_MEM_FLAG_USER, num_bins * sizeof(struct icomplex32));
	zassert_not_null(y, "Failed to allocate output data buffer.");

	plan = mod_fft_multi_plan_new(&dummy, x, y, num_bins, 32);
	zassert_not_null(plan, "Failed to allocate FFT plan.");

	for (i = 0; i < num_tests; i++) {
		for (j = 0; j < num_bins; j++) {
			x[j].real = *p_in_real++;
			x[j].imag = *p_in_imag++;
		}

		fft_multi_execute_32(plan, do_ifft);

		for (j = 0; j < num_bins; j++) {
			delta = (double)*p_ref_real - (double)y[j].real;
			sum_squares += delta * delta;
			if (delta > delta_max) {
				delta_max = delta;
			} else if (-delta > delta_max) {
				delta_max = -delta;
			}

			delta = (double)*p_ref_imag - (double)y[j].imag;
			sum_squares += delta * delta;
			if (delta > delta_max) {
				delta_max = delta;
			} else if (-delta > delta_max) {
				delta_max = -delta;
			}

			p_ref_real++;
			p_ref_imag++;
		}
	}

	mod_fft_multi_plan_free(&dummy, plan);
	rfree(y);
	rfree(x);

	error_rms = sqrt(sum_squares / (double)(2 * num_bins * num_tests));
	printf("Max absolute error = %5.2f (limit %5.2f), error RMS = %5.2f (limit %5.2f)\n",
	       delta_max, max_error_abs, error_rms, max_error_rms);

	zassert_true(error_rms < max_error_rms, "error RMS %5.2f exceeds limit %5.2f", error_rms,
		     max_error_rms);
	zassert_true(delta_max < max_error_abs, "Max absolute error %5.2f exceeds limit %5.2f",
		     delta_max, max_error_abs);
}

/**
 * @brief Verify a 96-bin FFT against its reference vector
 */
ZTEST(math_fft_multi_suite, test_fft_multi_96)
{
	fft_multi_32_test(fft_in_real_96_q31, fft_in_imag_96_q31, fft_ref_real_96_q31,
			  fft_ref_imag_96_q31, 96, REF_SOFM_FFT_MULTI_96_NUM_TESTS,
			  FFT_MAX_ERROR_ABS, FFT_MAX_ERROR_RMS, false);
}

/**
 * @brief Verify a 512-bin FFT against its reference vector
 */
ZTEST(math_fft_multi_suite, test_fft_multi_512)
{
	fft_multi_32_test(fft_in_real_512_q31, fft_in_imag_512_q31, fft_ref_real_512_q31,
			  fft_ref_imag_512_q31, 512, REF_SOFM_FFT_MULTI_512_NUM_TESTS,
			  FFT_MAX_ERROR_ABS, FFT_MAX_ERROR_RMS, false);
}

/**
 * @brief Verify a 768-bin FFT against its reference vector
 */
ZTEST(math_fft_multi_suite, test_fft_multi_768)
{
	fft_multi_32_test(fft_in_real_768_q31, fft_in_imag_768_q31, fft_ref_real_768_q31,
			  fft_ref_imag_768_q31, 768, REF_SOFM_FFT_MULTI_768_NUM_TESTS,
			  FFT_MAX_ERROR_ABS, FFT_MAX_ERROR_RMS, false);
}

/**
 * @brief Verify a 1024-bin FFT against its reference vector
 */
ZTEST(math_fft_multi_suite, test_fft_multi_1024)
{
	fft_multi_32_test(fft_in_real_1024_q31, fft_in_imag_1024_q31, fft_ref_real_1024_q31,
			  fft_ref_imag_1024_q31, 1024, REF_SOFM_FFT_MULTI_1024_NUM_TESTS,
			  FFT_MAX_ERROR_ABS, FFT_MAX_ERROR_RMS, false);
}

/**
 * @brief Verify a 1536-bin FFT against its reference vector
 */
ZTEST(math_fft_multi_suite, test_fft_multi_1536)
{
	fft_multi_32_test(fft_in_real_1536_q31, fft_in_imag_1536_q31, fft_ref_real_1536_q31,
			  fft_ref_imag_1536_q31, 1536, REF_SOFM_FFT_MULTI_1536_NUM_TESTS,
			  FFT_MAX_ERROR_ABS, FFT_MAX_ERROR_RMS, false);
}

/**
 * @brief Verify a 3072-bin FFT against its reference vector, twice, matching
 * the two invocations exercised by the original CMocka test.
 */
ZTEST(math_fft_multi_suite, test_fft_multi_3072)
{
	fft_multi_32_test(fft_in_real_3072_q31, fft_in_imag_3072_q31, fft_ref_real_3072_q31,
			  fft_ref_imag_3072_q31, 3072, REF_SOFM_FFT_MULTI_3072_NUM_TESTS,
			  FFT_MAX_ERROR_ABS, FFT_MAX_ERROR_RMS, false);
	fft_multi_32_test(fft_in_real_3072_q31, fft_in_imag_3072_q31, fft_ref_real_3072_q31,
			  fft_ref_imag_3072_q31, 3072, REF_SOFM_FFT_MULTI_3072_NUM_TESTS,
			  FFT_MAX_ERROR_ABS, FFT_MAX_ERROR_RMS, false);
}

/**
 * @brief Verify a 24-bin IFFT against its reference vector
 */
ZTEST(math_fft_multi_suite, test_ifft_multi_24)
{
	fft_multi_32_test(ifft_in_real_24_q31, ifft_in_imag_24_q31, ifft_ref_real_24_q31,
			  ifft_ref_imag_24_q31, 24, REF_SOFM_IFFT_MULTI_24_NUM_TESTS,
			  IFFT_MAX_ERROR_ABS, IFFT_MAX_ERROR_RMS, true);
}

/**
 * @brief Verify a 256-bin IFFT against its reference vector
 */
ZTEST(math_fft_multi_suite, test_ifft_multi_256)
{
	fft_multi_32_test(ifft_in_real_256_q31, ifft_in_imag_256_q31, ifft_ref_real_256_q31,
			  ifft_ref_imag_256_q31, 256, REF_SOFM_IFFT_MULTI_256_NUM_TESTS,
			  IFFT_MAX_ERROR_ABS, IFFT_MAX_ERROR_RMS, true);
}

/**
 * @brief Verify a 1024-bin IFFT against its reference vector
 */
ZTEST(math_fft_multi_suite, test_ifft_multi_1024)
{
	fft_multi_32_test(ifft_in_real_1024_q31, ifft_in_imag_1024_q31, ifft_ref_real_1024_q31,
			  ifft_ref_imag_1024_q31, 1024, REF_SOFM_IFFT_MULTI_1024_NUM_TESTS,
			  IFFT_MAX_ERROR_ABS, IFFT_MAX_ERROR_RMS, true);
}

/**
 * @brief Verify a 1536-bin IFFT against its reference vector
 */
ZTEST(math_fft_multi_suite, test_ifft_multi_1536)
{
	fft_multi_32_test(ifft_in_real_1536_q31, ifft_in_imag_1536_q31, ifft_ref_real_1536_q31,
			  ifft_ref_imag_1536_q31, 1536, REF_SOFM_IFFT_MULTI_1536_NUM_TESTS,
			  IFFT_MAX_ERROR_ABS, IFFT_MAX_ERROR_RMS, true);
}

/**
 * @brief Verify a 3072-bin IFFT against its reference vector
 */
ZTEST(math_fft_multi_suite, test_ifft_multi_3072)
{
	fft_multi_32_test(ifft_in_real_3072_q31, ifft_in_imag_3072_q31, ifft_ref_real_3072_q31,
			  ifft_ref_imag_3072_q31, 3072, REF_SOFM_IFFT_MULTI_3072_NUM_TESTS,
			  IFFT_MAX_ERROR_ABS, IFFT_MAX_ERROR_RMS, true);
}

/**
 * @brief Define and initialize the FFT-multi test suite
 *
 * mod_alloc_test_after() checks that each test case freed the FFT plans it
 * allocated through the mod_alloc() API.
 */
ZTEST_SUITE(math_fft_multi_suite, NULL, NULL, NULL, mod_alloc_test_after, NULL);
