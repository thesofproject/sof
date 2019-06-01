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

static void test_math_numbers_find_equal_int16_for_5_123_5_10_123_500_123_n_123_equals_1_4_and_6
	(void **state)
{
	(void)state;

	int16_t r[4];
	int16_t vec[] = {5, 123, 5, 10, 123, 500, 123};
	int16_t template[] = {1, 4, 6};

	int r_num = find_equal_int16(r, vec, 123, 7, 4);

	assert_int_equal(r_num, 3);
	assert_memory_equal(r, template, sizeof(int16_t) * 3);
}

static void test_math_numbers_find_equal_int16_for_1_2_3_4_5_n_0_equals_nothing
	(void **state)
{
	(void)state;

	int16_t r[4];
	int16_t vec[] = {1, 2, 3, 4, 5};

	int r_num = find_equal_int16(r, vec, 0, 5, 4);

	assert_int_equal(r_num, 0);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test
			(test_math_numbers_find_equal_int16_for_5_123_5_10_123_500_123_n_123_equals_1_4_and_6),
		cmocka_unit_test
			(test_math_numbers_find_equal_int16_for_1_2_3_4_5_n_0_equals_nothing)
	};

	cmocka_set_message_output(CM_OUTPUT_TAP);

	return cmocka_run_group_tests(tests, NULL, NULL);
}
