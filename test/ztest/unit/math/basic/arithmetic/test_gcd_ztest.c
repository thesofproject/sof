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
 * @brief Test GCD function with typical case
 *
 * Tests that gcd(5083, 391) returns 391
 */
ZTEST(math_arithmetic_suite, test_math_numbers_gcd_for_5083_and_391_equals_391)
{
	int r;

	r = gcd(5083, 391);
	zassert_equal(r, 391, "gcd(5083, 391) should equal 391");
}

/**
 * @brief Test GCD function with small positive numbers
 *
 * Tests that gcd(12, 9) returns 3
 */
ZTEST(math_arithmetic_suite, test_math_numbers_gcd_for_12_and_9_equals_3)
{
	int r;

	r = gcd(12, 9);
	zassert_equal(r, 3, "gcd(12, 9) should equal 3");
}

/**
 * @brief Test GCD function with zero second argument
 *
 * Tests that gcd(5, 0) returns 5
 */
ZTEST(math_arithmetic_suite, test_math_numbers_gcd_for_5_and_0_equals_5)
{
	int r;

	r = gcd(5, 0);
	zassert_equal(r, 5, "gcd(5, 0) should equal 5");
}

/**
 * @brief Test GCD function with zero first argument
 *
 * Tests that gcd(0, 5) returns 5
 */
ZTEST(math_arithmetic_suite, test_math_numbers_gcd_for_0_and_5_equals_5)
{
	int r;

	r = gcd(0, 5);
	zassert_equal(r, 5, "gcd(0, 5) should equal 5");
}

/**
 * @brief Test GCD function with both arguments zero
 *
 * Tests that gcd(0, 0) returns 0
 */
ZTEST(math_arithmetic_suite, test_math_numbers_gcd_for_0_and_0_equals_0)
{
	int r;

	r = gcd(0, 0);
	zassert_equal(r, 0, "gcd(0, 0) should equal 0");
}

/**
 * @brief Test GCD function with negative first argument
 *
 * Tests that gcd(-4, 14) returns 2
 */
ZTEST(math_arithmetic_suite, test_math_numbers_gcd_for_neg_4_and_14_equals_2)
{
	int r;

	r = gcd(-4, 14);
	zassert_equal(r, 2, "gcd(-4, 14) should equal 2");
}

/**
 * @brief Test GCD function with negative second argument
 *
 * Tests that gcd(4, -14) returns 2
 */
ZTEST(math_arithmetic_suite, test_math_numbers_gcd_for_4_and_neg_14_equals_2)
{
	int r;

	r = gcd(4, -14);
	zassert_equal(r, 2, "gcd(4, -14) should equal 2");
}

/**
 * @brief Test GCD function with both arguments negative
 *
 * Tests that gcd(-4, -14) returns 2
 */
ZTEST(math_arithmetic_suite, test_math_numbers_gcd_for_neg_4_and_neg_14_equals_2)
{
	int r;

	r = gcd(-4, -14);
	zassert_equal(r, 2, "gcd(-4, -14) should equal 2");
}

/**
 * @brief Define and initialize the math arithmetic test suite
 */
ZTEST_SUITE(math_arithmetic_suite, NULL, NULL, NULL, NULL, NULL);
