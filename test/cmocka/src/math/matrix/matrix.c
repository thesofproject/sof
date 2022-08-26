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
#include <sof/math/matrix.h>
#include "ref_matrix_mult_16_test1.h"
#include "ref_matrix_mult_16_test2.h"
#include "ref_matrix_mult_16_test3.h"
#include "ref_matrix_mult_16_test4.h"

#define MATRIX_MULT_16_MAX_ERROR_ABS  1.5
#define MATRIX_MULT_16_MAX_ERROR_RMS  0.5

static void matrix_mult_16_test(const int16_t *a_ref, const int16_t *b_ref, const int16_t *c_ref,
				int elementwise, int a_rows, int a_columns,
				int b_rows, int b_columns, int c_rows, int c_columns,
				int a_frac, int b_frac, int c_frac)
{
	struct mat_matrix_16b *a_matrix;
	struct mat_matrix_16b *b_matrix;
	struct mat_matrix_16b *c_matrix;
	float delta;
	float sum_squares = 0;
	float error_rms;
	float delta_max = 0;
	int16_t x;
	int i, j, k;

	a_matrix = mat_matrix_alloc_16b(a_rows, a_columns, a_frac);
	if (!a_matrix)
		exit(EXIT_FAILURE);

	b_matrix = mat_matrix_alloc_16b(b_rows, b_columns, b_frac);
	if (!b_matrix) {
		free(a_matrix);
		exit(EXIT_FAILURE);
	}

	c_matrix = mat_matrix_alloc_16b(c_rows, c_columns, c_frac);
	if (!c_matrix)  {
		free(a_matrix);
		free(b_matrix);
		exit(EXIT_FAILURE);
	}

	/* Initialize matrices a and b from test vectors and do matrix multiply */
	mat_copy_from_linear_16b(a_matrix, a_ref);
	mat_copy_from_linear_16b(b_matrix, b_ref);
	if (elementwise)
		mat_multiply_elementwise(a_matrix, b_matrix, c_matrix);
	else
		mat_multiply(a_matrix, b_matrix, c_matrix);

	/* Check */
	k = 0;
	for (i = 0; i < c_matrix->rows; i++) {
		for (j = 0; j < c_matrix->columns; j++) {
			x = mat_get_scalar_16b(c_matrix, i, j);
			delta = (float)x - (float)c_ref[k++];
			sum_squares += delta * delta;
			if (delta > delta_max)
				delta_max = delta;
			else if (-delta > delta_max)
				delta_max = -delta;
		}
	}

	error_rms = sqrt(sum_squares / (float)(c_matrix->rows * c_matrix->columns));
	printf("Max absolute error = %5.2f (max %5.2f), error RMS = %5.2f (max %5.2f)\n",
	       delta_max, MATRIX_MULT_16_MAX_ERROR_ABS, error_rms, MATRIX_MULT_16_MAX_ERROR_RMS);

	assert_true(error_rms < MATRIX_MULT_16_MAX_ERROR_RMS);
	assert_true(delta_max < MATRIX_MULT_16_MAX_ERROR_ABS);
}

static void test_matrix_mult_16_test1(void **state)
{
	(void)state;

	matrix_mult_16_test(matrix_mult_16_test1_a,
			    matrix_mult_16_test1_b,
			    matrix_mult_16_test1_c,
			    MATRIX_MULT_16_TEST1_ELEMENTWISE,
			    MATRIX_MULT_16_TEST1_A_ROWS,
			    MATRIX_MULT_16_TEST1_A_COLUMNS,
			    MATRIX_MULT_16_TEST1_B_ROWS,
			    MATRIX_MULT_16_TEST1_B_COLUMNS,
			    MATRIX_MULT_16_TEST1_C_ROWS,
			    MATRIX_MULT_16_TEST1_C_COLUMNS,
			    MATRIX_MULT_16_TEST1_A_QXY_Y,
			    MATRIX_MULT_16_TEST1_B_QXY_Y,
			    MATRIX_MULT_16_TEST1_C_QXY_Y);
}

static void test_matrix_mult_16_test2(void **state)
{
	(void)state;

	matrix_mult_16_test(matrix_mult_16_test2_a,
			    matrix_mult_16_test2_b,
			    matrix_mult_16_test2_c,
			    MATRIX_MULT_16_TEST2_ELEMENTWISE,
			    MATRIX_MULT_16_TEST2_A_ROWS,
			    MATRIX_MULT_16_TEST2_A_COLUMNS,
			    MATRIX_MULT_16_TEST2_B_ROWS,
			    MATRIX_MULT_16_TEST2_B_COLUMNS,
			    MATRIX_MULT_16_TEST2_C_ROWS,
			    MATRIX_MULT_16_TEST2_C_COLUMNS,
			    MATRIX_MULT_16_TEST2_A_QXY_Y,
			    MATRIX_MULT_16_TEST2_B_QXY_Y,
			    MATRIX_MULT_16_TEST2_C_QXY_Y);
}

static void test_matrix_mult_16_test3(void **state)
{
	(void)state;

	matrix_mult_16_test(matrix_mult_16_test3_a,
			    matrix_mult_16_test3_b,
			    matrix_mult_16_test3_c,
			    MATRIX_MULT_16_TEST3_ELEMENTWISE,
			    MATRIX_MULT_16_TEST3_A_ROWS,
			    MATRIX_MULT_16_TEST3_A_COLUMNS,
			    MATRIX_MULT_16_TEST3_B_ROWS,
			    MATRIX_MULT_16_TEST3_B_COLUMNS,
			    MATRIX_MULT_16_TEST3_C_ROWS,
			    MATRIX_MULT_16_TEST3_C_COLUMNS,
			    MATRIX_MULT_16_TEST3_A_QXY_Y,
			    MATRIX_MULT_16_TEST3_B_QXY_Y,
			    MATRIX_MULT_16_TEST3_C_QXY_Y);
}

static void test_matrix_mult_16_test4(void **state)
{
	(void)state;

	matrix_mult_16_test(matrix_mult_16_test4_a,
			    matrix_mult_16_test4_b,
			    matrix_mult_16_test4_c,
			    MATRIX_MULT_16_TEST4_ELEMENTWISE,
			    MATRIX_MULT_16_TEST4_A_ROWS,
			    MATRIX_MULT_16_TEST4_A_COLUMNS,
			    MATRIX_MULT_16_TEST4_B_ROWS,
			    MATRIX_MULT_16_TEST4_B_COLUMNS,
			    MATRIX_MULT_16_TEST4_C_ROWS,
			    MATRIX_MULT_16_TEST4_C_COLUMNS,
			    MATRIX_MULT_16_TEST4_A_QXY_Y,
			    MATRIX_MULT_16_TEST4_B_QXY_Y,
			    MATRIX_MULT_16_TEST4_C_QXY_Y);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_matrix_mult_16_test1),
		cmocka_unit_test(test_matrix_mult_16_test2),
		cmocka_unit_test(test_matrix_mult_16_test3),
		cmocka_unit_test(test_matrix_mult_16_test4),
	};

	cmocka_set_message_output(CM_OUTPUT_TAP);

	return cmocka_run_group_tests(tests, NULL, NULL);
}
