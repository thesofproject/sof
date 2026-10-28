// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.
//
// These contents may have been developed with support from one or more
// Intel-operated generative artificial intelligence solutions.
//
// Converted from CMock to Ztest
// Original tests from test/cmocka/src/math/dct/dct.c:
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

#include <zephyr/ztest.h>

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/math/dct.h>
#include <sof/math/matrix.h>

#include <stdbool.h>
#include <stdint.h>
#include <math.h>

/* Reference DCT matrices shared with the legacy CMocka test. They are
 * #included in place from the CMocka tree (added to the include path in
 * CMakeLists.txt) so the exact same fixed-point data is exercised.
 */
#include "ref_dct_matrix_16_test1.h"
#include "ref_dct_matrix_16_test2.h"

#include "mod_alloc.h"

#define DCT_MATRIX_16_MAX_ERROR_ABS 2.5
#define DCT_MATRIX_16_MAX_ERROR_RMS 1.1

static struct processing_module dummy;

/**
 * @brief Build a 16-bit DCT matrix and compare it to a reference matrix.
 * @param[in] ref Expected matrix coefficients in row major order.
 * @param[in] num_in Number of DCT input values.
 * @param[in] num_out Number of DCT output values.
 * @param[in] type DCT type to generate.
 * @param[in] ortho True to generate an orthogonal matrix.
 */
static void dct_matrix_16_test(const int16_t *ref, int num_in, int num_out, enum dct_type type,
			       bool ortho)
{
	struct dct_plan_16 dct;
	double delta;
	double sum_squares = 0;
	double error_rms;
	double delta_max = 0;
	int16_t x;
	int rows;
	int columns;
	int i, j, k;

	/* Calculate DCT matrix */
	dct.num_in = num_in;
	dct.num_out = num_out;
	dct.type = type;
	dct.ortho = ortho;
	zassert_ok(mod_dct_initialize_16(&dummy, &dct), "Failed to initialize DCT.");

	/* Check */
	rows = dct.matrix->rows;
	columns = dct.matrix->columns;
	k = 0;
	for (i = 0; i < rows; i++) {
		for (j = 0; j < columns; j++) {
			x = mat_get_scalar_16b(dct.matrix, i, j);
			delta = (double)x - (double)ref[k++];
			sum_squares += delta * delta;
			if (delta > delta_max) {
				delta_max = delta;
			} else if (-delta > delta_max) {
				delta_max = -delta;
			}
		}
	}

	error_rms = sqrt(sum_squares / (double)(rows * columns));

	mod_dct_free_16(&dummy, &dct);

	TC_PRINT("Max absolute error = %5.2f (max %5.2f), error RMS = %5.2f (max %5.2f)\n",
		 delta_max, DCT_MATRIX_16_MAX_ERROR_ABS, error_rms, DCT_MATRIX_16_MAX_ERROR_RMS);

	zassert_true(error_rms < DCT_MATRIX_16_MAX_ERROR_RMS, "error RMS %5.2f exceeds limit %5.2f",
		     error_rms, DCT_MATRIX_16_MAX_ERROR_RMS);
	zassert_true(delta_max < DCT_MATRIX_16_MAX_ERROR_ABS,
		     "Max absolute error %5.2f exceeds limit %5.2f", delta_max,
		     DCT_MATRIX_16_MAX_ERROR_ABS);
}

/* Expand the reference matrix and plan parameter names of
 * ref_dct_matrix_16_test<n>.h into a dct_matrix_16_test() call, so that the
 * five arguments are spelled out once instead of once per test case.
 */
#define DCT_MATRIX_16_TEST(n)                                                                      \
	dct_matrix_16_test(dct_matrix_16_test##n##_matrix, DCT_MATRIX_16_TEST##n##_NUM_IN,         \
			   DCT_MATRIX_16_TEST##n##_NUM_OUT, DCT_MATRIX_16_TEST##n##_TYPE,          \
			   DCT_MATRIX_16_TEST##n##_ORTHO)

/**
 * @brief Verify the 16-bit DCT matrix against reference matrix 1
 */
ZTEST(math_dct_suite, test_dct_matrix_16_test1)
{
	DCT_MATRIX_16_TEST(1);
}

/**
 * @brief Verify the 16-bit DCT matrix against reference matrix 2
 */
ZTEST(math_dct_suite, test_dct_matrix_16_test2)
{
	DCT_MATRIX_16_TEST(2);
}

/**
 * @brief Define and initialize the discrete cosine transform test suite
 *
 * mod_alloc_test_after() checks that each test case freed the DCT plan it
 * allocated through the mod_alloc() API.
 */
ZTEST_SUITE(math_dct_suite, NULL, NULL, NULL, mod_alloc_test_after, NULL);
