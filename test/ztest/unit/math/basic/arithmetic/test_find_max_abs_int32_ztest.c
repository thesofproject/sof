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
#include <sof/common.h>

/**
 * @brief Test find_max_abs_int32 function with negative maximum absolute value
 *
 * Tests that find_max_abs_int32 correctly finds maximum absolute value
 * in array [-100, 99, 98, 50] where -100 has the largest absolute value
 */
ZTEST(math_arithmetic_suite, test_math_numbers_find_max_abs_int32_negative_max)
{
	int32_t vec[] = {-100, 99, 98, 50};
	int32_t r = find_max_abs_int32(vec, ARRAY_SIZE(vec));

	zassert_equal(r, 100, "find_max_abs_int32([-100, 99, 98, 50]) should return 100");
}

/**
 * @brief Test find_max_abs_int32 function with positive maximum absolute value
 *
 * Tests that find_max_abs_int32 correctly finds maximum absolute value
 * in array [-100, 99, 98, 50, 101] where 101 has the largest absolute value
 */
ZTEST(math_arithmetic_suite, test_math_numbers_find_max_abs_int32_positive_max)
{
	int32_t vec[] = {-100, 99, 98, 50, 101};
	int32_t r = find_max_abs_int32(vec, ARRAY_SIZE(vec));

	zassert_equal(r, 101, "find_max_abs_int32([-100, 99, 98, 50, 101]) should return 101");
}
