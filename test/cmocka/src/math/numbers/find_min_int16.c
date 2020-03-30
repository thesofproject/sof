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

static void test_math_numbers_find_min_int16_for_2_equals_2(void **state)
{
	(void)state;

	int16_t vec[] = {2};
	int r = find_min_int16(vec, ARRAY_SIZE(vec));

	assert_int_equal(r, 2);
}

static void test_math_numbers_find_min_int16_for_5_2_3_4_1_equals_1
	(void **state)
{
	(void)state;

	int16_t vec[] = {5, 2, 3, 4, 1};
	int r = find_min_int16(vec, ARRAY_SIZE(vec));

	assert_int_equal(r, 1);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test
			(test_math_numbers_find_min_int16_for_2_equals_2),
		cmocka_unit_test
			(test_math_numbers_find_min_int16_for_5_2_3_4_1_equals_1)
	};

	cmocka_set_message_output(CM_OUTPUT_TAP);

	return cmocka_run_group_tests(tests, NULL, NULL);
}
