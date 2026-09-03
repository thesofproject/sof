// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2025 Intel Corporation. All rights reserved.
//
// These contents may have been developed with support from one or more Intel-operated
// generative artificial intelligence solutions.
//
// Converted from CMock to Ztest
//
// Original test from sof/test/cmocka/src/math/arithmetic/scalar_power.c:
// Author: Shriram Shastry <malladi.sastry@linux.intel.com>

#include <zephyr/ztest.h>
#include <zephyr/logging/log.h>
#include <sof/audio/format.h>
#include <sof/math/power.h>
#include <sof/common.h>
#include <math.h>

LOG_MODULE_REGISTER(test_scalar_power, LOG_LEVEL_INF);

/* Test data tables from MATLAB-generated reference */
#include "power_tables.h"

/* Error tolerance: max error = 0.000034912111005, THD+N = -96.457180359025074 */
#define CMP_TOLERANCE 0.0000150363575813f

/**
 * @brief Test scalar power function with fixed-point arithmetic
 *
 * This test validates the power_int32() function against MATLAB-generated
 * reference values. It tests 64 base values against 6 exponent values,
 * checking that the fixed-point power calculation stays within acceptable
 * tolerance.
 *
 * Base values: Fixed-point Q6.25 format (range -1.0 to 1.0)
 * Exponent values: Fixed-point Q2.29 format
 * Result: Fixed-point Q16.15 format
 */
ZTEST(math_advanced_functions_suite, test_math_arithmetic_power_fixed)
{
	double p;
	int i;
	int j;

	for (i = 0; i < ARRAY_SIZE(b); i++) {
		for (j = 0; j < ARRAY_SIZE(e); j++) {
			p = power_int32(b[i], e[j]);
			float delta = fabsf((float)(power_table[i][j] - (double)p / (1 << 15)));

			zassert_true(delta <= CMP_TOLERANCE,
				     "Power calc error: delta=%f > %f at b[%d]=%d, e[%d]=%d",
				     (double)delta, (double)CMP_TOLERANCE, i, b[i], j, e[j]);
		}
	}
}

ZTEST_SUITE(math_advanced_functions_suite, NULL, NULL, NULL, NULL, NULL);
