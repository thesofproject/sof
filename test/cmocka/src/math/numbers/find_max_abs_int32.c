// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Author: Slawomir Blauciak <slawomir.blauciak@linux.intel.com>

#include <sof/math/numbers.h>
#include <sof/common.h>

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>

static void test_math_numbers_find_max_abs_int32_for_neg100_99_98_50_equals_100
	(void **state)
{
	(void)state;

	int32_t vec[] = {-100, 99, 98, 50};
	int r = find_max_abs_int32(vec, ARRAY_SIZE(vec));

	assert_int_equal(r, 100);
}

static void test_math_numbers_find_max_abs_int32_for_neg100_99_98_50_101_equals_101
	(void **state)
{
	(void)state;

	int32_t vec[] = {-100, 99, 98, 50, 101};
	int r = find_max_abs_int32(vec, ARRAY_SIZE(vec));

	assert_int_equal(r, 101);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test
			(test_math_numbers_find_max_abs_int32_for_neg100_99_98_50_equals_100),
		cmocka_unit_test
			(test_math_numbers_find_max_abs_int32_for_neg100_99_98_50_101_equals_101)
	};

	cmocka_set_message_output(CM_OUTPUT_TAP);

	return cmocka_run_group_tests(tests, NULL, NULL);
}
