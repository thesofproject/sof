// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.
//
// Converted from CMock to Ztest
// Original tests from test/cmocka/src/math/fft/dft3.c:
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

#include <zephyr/ztest.h>

#include <sof/audio/format.h>
#include <sof/math/icomplex32.h>
#include <sof/math/fft.h>

/* Reference test vectors, generated with ref_dft3.m */
#include "ref_dft3_32.h"

#include <stdint.h>
#include <stddef.h>
#include <math.h>

#define SOFM_DFT3_MAX_ERROR_ABS 3.1
#define SOFM_DFT3_MAX_ERROR_RMS 1.1
#define DFT_SIZE 3

BUILD_ASSERT(ARRAY_SIZE(input_data_real_q31) == ARRAY_SIZE(input_data_imag_q31));
BUILD_ASSERT(ARRAY_SIZE(ref_data_real_q31) == ARRAY_SIZE(ref_data_imag_q31));
BUILD_ASSERT(ARRAY_SIZE(input_data_real_q31) == ARRAY_SIZE(ref_data_real_q31));
BUILD_ASSERT(ARRAY_SIZE(input_data_real_q31) == DFT_SIZE * REF_SOFM_DFT3_NUM_TESTS);

/**
 * @brief Run the 3-point DFT against its Q1.31 reference vector.
 *
 * Feeds each of the REF_SOFM_DFT3_NUM_TESTS blocks of DFT_SIZE complex
 * samples through dft3_32() and accumulates the absolute and RMS error
 * against the pre-computed reference, preserving the fixed-point semantics
 * of the original CMocka test.
 */
ZTEST(math_dft3_suite, test_dft3_32)
{
	struct icomplex32 x[DFT_SIZE];
	struct icomplex32 y[DFT_SIZE];
	double delta;
	double error_rms;
	double delta_max = 0;
	double sum_squares = 0;
	const int32_t *p_in_real = input_data_real_q31;
	const int32_t *p_in_imag = input_data_imag_q31;
	const int32_t *p_ref_real = ref_data_real_q31;
	const int32_t *p_ref_imag = ref_data_imag_q31;
	int i, j;

	for (i = 0; i < REF_SOFM_DFT3_NUM_TESTS; i++) {
		for (j = 0; j < DFT_SIZE; j++) {
			x[j].real = *p_in_real++;
			x[j].imag = *p_in_imag++;
		}

		dft3_32(x, y);

		for (j = 0; j < DFT_SIZE; j++) {
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

	error_rms = sqrt(sum_squares / (double)(2 * DFT_SIZE * REF_SOFM_DFT3_NUM_TESTS));
	printk("Max absolute error = %5.2f (limit %5.2f), error RMS = %5.2f (limit %5.2f)\n",
	       (double)delta_max, (double)SOFM_DFT3_MAX_ERROR_ABS, (double)error_rms,
	       (double)SOFM_DFT3_MAX_ERROR_RMS);

	zassert_true(error_rms < SOFM_DFT3_MAX_ERROR_RMS,
		     "error RMS %5.2f exceeds limit %5.2f", error_rms, SOFM_DFT3_MAX_ERROR_RMS);
	zassert_true(delta_max < SOFM_DFT3_MAX_ERROR_ABS,
		     "Max absolute error %5.2f exceeds limit %5.2f", delta_max,
		     SOFM_DFT3_MAX_ERROR_ABS);
}

ZTEST_SUITE(math_dft3_suite, NULL, NULL, NULL, NULL, NULL);
