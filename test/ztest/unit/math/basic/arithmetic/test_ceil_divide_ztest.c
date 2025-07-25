// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2025 Intel Corporation. All rights reserved.
//
// These contents may have been developed with support from one or more Intel-operated
// generative artificial intelligence solutions.
//
// Converted from CMock to Ztest

#include <zephyr/ztest.h>
#include <sof/math/numbers.h>
#include <math.h>

/**
 * @brief Test ceil_divide function with various parameter combinations
 *
 * Tests ceil_divide against reference ceilf implementation with multiple
 * positive and negative integer combinations
 */
ZTEST(math_arithmetic_suite, test_math_numbers_ceil_divide)
{
	int params[8] = {
		-1000,
		300,
		123,
		-10,
		1337,
		-6,
		999,
		-2
	};

	int i, j;

	for (i = 0; i < ARRAY_SIZE(params); ++i) {
		for (j = 0; j < ARRAY_SIZE(params); ++j) {
			int ref = ceilf((float)params[i] / (float)params[j]);
			int r = ceil_divide(params[i], params[j]);

			zassert_equal(r, ref,
				      "ceil_divide(%d, %d) = %d, expected %d",
				       params[i], params[j], r, ref);
		}
	}
}
