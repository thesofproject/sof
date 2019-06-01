// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Author: Slawomir Blauciak <slawomir.blauciak@linux.intel.com>

#include <sof/math/numbers.h>

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>

static void test_math_numbers_norm_int32_for_0_equals_31(void **state)
{
	(void)state;

	int r = norm_int32(0);

	assert_int_equal(r, 31);
}

static void test_math_numbers_norm_int32_for_35_equals_10(void **state)
{
	(void)state;

	int r = norm_int32(35);

	assert_int_equal(r, 25);
}

static void test_math_numbers_norm_int32_for_2147483647_equals_0(void **state)
{
	(void)state;

	int r = norm_int32(2147483647);

	assert_int_equal(r, 0);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_math_numbers_norm_int32_for_0_equals_31),
		cmocka_unit_test
			(test_math_numbers_norm_int32_for_35_equals_10),
		cmocka_unit_test
			(test_math_numbers_norm_int32_for_2147483647_equals_0)
	};

	cmocka_set_message_output(CM_OUTPUT_TAP);

	return cmocka_run_group_tests(tests, NULL, NULL);
}
