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
 * @brief Test find_equal_int16 function with array containing matches
 *
 * Tests that find_equal_int16 correctly finds all indices where value 123
 * appears in the array [5, 123, 5, 10, 123, 500, 123]
 */
ZTEST(math_arithmetic_suite, test_math_numbers_find_equal_int16_matches)
{
	int16_t r[4];
	int16_t vec[] = {5, 123, 5, 10, 123, 500, 123};
	int16_t template[] = {1, 4, 6};

	int r_num = find_equal_int16(r, vec, 123, 7, 4);

	zassert_equal(r_num, 3, "Should find 3 occurrences of 123");
	zassert_mem_equal(r, template, sizeof(int16_t) * 3,
			  "Result indices should match expected template");
}

/**
 * @brief Test find_equal_int16 function with no matches
 *
 * Tests that find_equal_int16 returns 0 when searching for value 0
 * in array [1, 2, 3, 4, 5] where it doesn't exist
 */
ZTEST(math_arithmetic_suite, test_math_numbers_find_equal_int16_no_matches)
{
	int16_t r[4];
	int16_t vec[] = {1, 2, 3, 4, 5};

	int r_num = find_equal_int16(r, vec, 0, 5, 4);

	zassert_equal(r_num, 0, "Should find 0 occurrences of 0");
}
