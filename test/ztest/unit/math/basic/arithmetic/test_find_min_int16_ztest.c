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
 * @brief Test find_min_int16 function with single element array
 *
 * Tests that find_min_int16 returns the single element when array has one item
 */
ZTEST(math_arithmetic_suite, test_math_numbers_find_min_int16_for_2_equals_2)
{
	int16_t vec[] = {2};
	int r = find_min_int16(vec, ARRAY_SIZE(vec));

	zassert_equal(r, 2, "find_min_int16([2]) should return 2");
}

/**
 * @brief Test find_min_int16 function with multiple elements
 *
 * Tests that find_min_int16 correctly finds minimum value in array [5, 2, 3, 4, 1]
 */
ZTEST(math_arithmetic_suite, test_math_numbers_find_min_int16_for_5_2_3_4_1_equals_1)
{
	int16_t vec[] = {5, 2, 3, 4, 1};
	int r = find_min_int16(vec, ARRAY_SIZE(vec));

	zassert_equal(r, 1, "find_min_int16([5, 2, 3, 4, 1]) should return 1");
}
