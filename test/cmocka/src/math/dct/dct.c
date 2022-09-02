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
#include <sof/math/dct.h>
#include <sof/math/matrix.h>
#include "ref_dct_matrix_16_test1.h"
#include "ref_dct_matrix_16_test2.h"

#define MATRIX_MULT_16_MAX_ERROR_ABS  2.5
#define MATRIX_MULT_16_MAX_ERROR_RMS  1.1

static void dct_matrix_16_test(const int16_t *ref, int num_in, int num_out,
			       enum dct_type type, bool ortho)
{
	struct dct_plan_16 dct;
	double delta;
	double sum_squares = 0;
	double error_rms;
	double delta_max = 0;
	int16_t x;
	int rows;
	int columns;
	int ret;
	int i, j, k;

	/* Calculate DCT matrix */
	dct.num_in = num_in;
	dct.num_out = num_out;
	dct.type = type;
	dct.ortho = ortho;
	ret = dct_initialize_16(&dct);
	if (ret) {
		fprintf(stderr, "Failed to initialize DCT.\n");
		exit(EXIT_FAILURE);
	}

	/* Check */
	rows = dct.matrix->rows;
	columns = dct.matrix->columns;
	k = 0;
	for (i = 0; i < rows; i++) {
		for (j = 0; j < columns; j++) {
			x = mat_get_scalar_16b(dct.matrix, i, j);
			delta = (double)x - (double)ref[k++];
			sum_squares += delta * delta;
			if (delta > delta_max)
				delta_max = delta;
			else if (-delta > delta_max)
				delta_max = -delta;
		}
	}

	error_rms = sqrt(sum_squares / (double)(rows * columns));
	printf("Max absolute error = %5.2f (max %5.2f), error RMS = %5.2f (max %5.2f)\n",
	       delta_max, MATRIX_MULT_16_MAX_ERROR_ABS, error_rms, MATRIX_MULT_16_MAX_ERROR_RMS);

	assert_true(error_rms < MATRIX_MULT_16_MAX_ERROR_RMS);
	assert_true(delta_max < MATRIX_MULT_16_MAX_ERROR_ABS);
}

static void test_dct_matrix_16_test1(void **state)
{
	(void)state;

	dct_matrix_16_test(dct_matrix_16_test1_matrix,
			   DCT_MATRIX_16_TEST1_NUM_IN,
			   DCT_MATRIX_16_TEST1_NUM_OUT,
			   DCT_MATRIX_16_TEST1_TYPE,
			   DCT_MATRIX_16_TEST1_ORTHO);
}

static void test_dct_matrix_16_test2(void **state)
{
	(void)state;

	dct_matrix_16_test(dct_matrix_16_test2_matrix,
			   DCT_MATRIX_16_TEST2_NUM_IN,
			   DCT_MATRIX_16_TEST2_NUM_OUT,
			   DCT_MATRIX_16_TEST2_TYPE,
			   DCT_MATRIX_16_TEST2_ORTHO);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_dct_matrix_16_test1),
		cmocka_unit_test(test_dct_matrix_16_test2),
	};

	cmocka_set_message_output(CM_OUTPUT_TAP);

	return cmocka_run_group_tests(tests, NULL, NULL);
}
