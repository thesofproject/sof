// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2025 Intel Corporation. All rights reserved.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

#include <sof/audio/format.h>
#include <sof/math/fft.h>
#include "ref_dft3_32.h"

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

#define SOFM_DFT3_MAX_ERROR_ABS		3.1
#define SOFM_DFT3_MAX_ERROR_RMS		1.1
#define DFT_SIZE			3

static void dft3_32_test(const int32_t *in_real, const int32_t *in_imag,
			 const int32_t *ref_real, const int32_t *ref_imag, int num_tests)

{
	struct icomplex32 x[DFT_SIZE];
	struct icomplex32 y[DFT_SIZE];
	double delta;
	double error_rms;
	double delta_max = 0;
	double sum_squares = 0;
	const int32_t *p_in_real = in_real;
	const int32_t *p_in_imag = in_imag;
	const int32_t *p_ref_real = ref_real;
	const int32_t *p_ref_imag = ref_imag;
	int i, j;

	for (i = 0; i < num_tests; i++) {
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

	error_rms = sqrt(sum_squares / (double)(2 * DFT_SIZE * num_tests));
	printf("Max absolute error = %5.2f (max %5.2f), error RMS = %5.2f (max %5.2f)\n",
	       delta_max, SOFM_DFT3_MAX_ERROR_ABS, error_rms, SOFM_DFT3_MAX_ERROR_RMS);

	assert_true(error_rms < SOFM_DFT3_MAX_ERROR_RMS);
	assert_true(delta_max < SOFM_DFT3_MAX_ERROR_ABS);
}

static void dft3_32_test_1(void **state)
{
	(void)state;

	dft3_32_test(input_data_real_q31, input_data_imag_q31,
		     ref_data_real_q31, ref_data_imag_q31,
		     REF_SOFM_DFT3_NUM_TESTS);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(dft3_32_test_1),
	};

	cmocka_set_message_output(CM_OUTPUT_TAP);

	return cmocka_run_group_tests(tests, NULL, NULL);
}
