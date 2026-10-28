// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.
//
// These contents may have been developed with support from one or more
// Intel-operated generative artificial intelligence solutions.
//
// Converted from CMock to Ztest
// Original tests from test/cmocka/src/math/matrix/matrix.c:
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

#include <zephyr/ztest.h>

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/math/matrix.h>

#include <stdint.h>
#include <math.h>

/* Reference test vectors shared with the legacy CMocka test. They are
 * #included in place from the CMocka tree (added to the include path in
 * CMakeLists.txt) so the exact same fixed-point data is exercised.
 */
#include "ref_matrix_mult_16_test1.h"
#include "ref_matrix_mult_16_test2.h"
#include "ref_matrix_mult_16_test3.h"
#include "ref_matrix_mult_16_test4.h"

#include "mod_alloc.h"

#define MATRIX_MULT_16_MAX_ERROR_ABS 1.5f
#define MATRIX_MULT_16_MAX_ERROR_RMS 0.5f

static struct processing_module dummy;

/**
 * @brief Multiply two reference matrices and compare the result to a reference.
 * @param[in] a_ref Linear Q(16-a_frac).a_frac data of the left operand.
 * @param[in] b_ref Linear Q(16-b_frac).b_frac data of the right operand.
 * @param[in] c_ref Linear Q(16-c_frac).c_frac data of the expected result.
 * @param[in] elementwise Non-zero for an element-wise multiply.
 * @param[in] a_rows Rows of the left operand.
 * @param[in] a_columns Columns of the left operand.
 * @param[in] b_rows Rows of the right operand.
 * @param[in] b_columns Columns of the right operand.
 * @param[in] c_rows Rows of the result.
 * @param[in] c_columns Columns of the result.
 * @param[in] a_frac Fractional bits of the left operand.
 * @param[in] b_frac Fractional bits of the right operand.
 * @param[in] c_frac Fractional bits of the result.
 */
static void matrix_mult_16_test(const int16_t *a_ref, const int16_t *b_ref, const int16_t *c_ref,
				int elementwise, int a_rows, int a_columns, int b_rows,
				int b_columns, int c_rows, int c_columns, int a_frac, int b_frac,
				int c_frac)
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

	a_matrix = mod_mat_matrix_alloc_16b(&dummy, a_rows, a_columns, a_frac);
	zassert_not_null(a_matrix, "Failed to allocate matrix a.");

	b_matrix = mod_mat_matrix_alloc_16b(&dummy, b_rows, b_columns, b_frac);
	zassert_not_null(b_matrix, "Failed to allocate matrix b.");

	c_matrix = mod_mat_matrix_alloc_16b(&dummy, c_rows, c_columns, c_frac);
	zassert_not_null(c_matrix, "Failed to allocate matrix c.");

	/* Initialize matrices a and b from test vectors and do matrix multiply */
	mat_copy_from_linear_16b(a_matrix, a_ref);
	mat_copy_from_linear_16b(b_matrix, b_ref);
	if (elementwise) {
		mat_multiply_elementwise(a_matrix, b_matrix, c_matrix);
	} else {
		mat_multiply(a_matrix, b_matrix, c_matrix);
	}

	/* Check */
	k = 0;
	for (i = 0; i < c_matrix->rows; i++) {
		for (j = 0; j < c_matrix->columns; j++) {
			x = mat_get_scalar_16b(c_matrix, i, j);
			delta = (float)x - (float)c_ref[k++];
			sum_squares += delta * delta;
			if (delta > delta_max) {
				delta_max = delta;
			} else if (-delta > delta_max) {
				delta_max = -delta;
			}
		}
	}

	error_rms = sqrtf(sum_squares / (float)(c_matrix->rows * c_matrix->columns));

	mod_free(&dummy, a_matrix);
	mod_free(&dummy, b_matrix);
	mod_free(&dummy, c_matrix);

	/* printk() is varargs, so the float arguments are promoted to double
	 * anyway; cast explicitly to keep -Wdouble-promotion quiet.
	 */
	TC_PRINT("Max absolute error = %5.2f (max %5.2f), error RMS = %5.2f (max %5.2f)\n",
		 (double)delta_max, (double)MATRIX_MULT_16_MAX_ERROR_ABS, (double)error_rms,
		 (double)MATRIX_MULT_16_MAX_ERROR_RMS);

	zassert_true(error_rms < MATRIX_MULT_16_MAX_ERROR_RMS,
		     "error RMS %5.2f exceeds limit %5.2f", (double)error_rms,
		     (double)MATRIX_MULT_16_MAX_ERROR_RMS);
	zassert_true(delta_max < MATRIX_MULT_16_MAX_ERROR_ABS,
		     "Max absolute error %5.2f exceeds limit %5.2f", (double)delta_max,
		     (double)MATRIX_MULT_16_MAX_ERROR_ABS);
}

/* Expand the per-test reference vector and geometry names of
 * ref_matrix_mult_16_test<n>.h into a matrix_mult_16_test() call, so that the
 * 13 arguments are spelled out once instead of once per test case.
 */
#define MATRIX_MULT_16_TEST(n)                                                                     \
	matrix_mult_16_test(matrix_mult_16_test##n##_a, matrix_mult_16_test##n##_b,                \
			    matrix_mult_16_test##n##_c, MATRIX_MULT_16_TEST##n##_ELEMENTWISE,      \
			    MATRIX_MULT_16_TEST##n##_A_ROWS, MATRIX_MULT_16_TEST##n##_A_COLUMNS,   \
			    MATRIX_MULT_16_TEST##n##_B_ROWS, MATRIX_MULT_16_TEST##n##_B_COLUMNS,   \
			    MATRIX_MULT_16_TEST##n##_C_ROWS, MATRIX_MULT_16_TEST##n##_C_COLUMNS,   \
			    MATRIX_MULT_16_TEST##n##_A_QXY_Y, MATRIX_MULT_16_TEST##n##_B_QXY_Y,    \
			    MATRIX_MULT_16_TEST##n##_C_QXY_Y)

/**
 * @brief Verify a 3x4 by 4x5 multiply, Q1.15 operands and a Q2.14 result
 */
ZTEST(math_matrix_suite, test_matrix_mult_16_test1)
{
	MATRIX_MULT_16_TEST(1);
}

/**
 * @brief Verify a 6x7 by 7x8 multiply of integers, no fractional bits
 */
ZTEST(math_matrix_suite, test_matrix_mult_16_test2)
{
	MATRIX_MULT_16_TEST(2);
}

/**
 * @brief Verify an element-wise 5x6 multiply, Q1.15 operands and result
 */
ZTEST(math_matrix_suite, test_matrix_mult_16_test3)
{
	MATRIX_MULT_16_TEST(3);
}

/**
 * @brief Verify an outer product, 6x1 by 1x8, Q1.15 operands and result
 */
ZTEST(math_matrix_suite, test_matrix_mult_16_test4)
{
	MATRIX_MULT_16_TEST(4);
}

/**
 * @brief Define and initialize the matrix operations test suite
 *
 * mod_alloc_test_after() checks that each test case freed the matrices it
 * allocated through the mod_alloc() API.
 */
ZTEST_SUITE(math_matrix_suite, NULL, NULL, NULL, mod_alloc_test_after, NULL);
