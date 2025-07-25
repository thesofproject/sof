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

/**
 * @brief Test norm_int32 function with zero value
 *
 * Tests that norm_int32(0) returns 31 (number of leading zeros in 32-bit zero)
 */
ZTEST(math_arithmetic_suite, test_math_numbers_norm_int32_for_0_equals_31)
{
	int r = norm_int32(0);

	zassert_equal(r, 31, "norm_int32(0) should return 31");
}

/**
 * @brief Test norm_int32 function with small positive value
 *
 * Tests that norm_int32(35) returns 25 (number of leading zeros)
 */
ZTEST(math_arithmetic_suite, test_math_numbers_norm_int32_for_35_equals_25)
{
	int r = norm_int32(35);

	zassert_equal(r, 25, "norm_int32(35) should return 25");
}

/**
 * @brief Test norm_int32 function with maximum positive value
 *
 * Tests that norm_int32(INT32_MAX) returns 0 (no leading zeros in max int32)
 */
ZTEST(math_arithmetic_suite, test_math_numbers_norm_int32_for_int32_max_equals_0)
{
	int r = norm_int32(INT32_MAX);

	zassert_equal(r, 0, "norm_int32(INT32_MAX) should return 0");
}
