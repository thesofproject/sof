// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Author: Marcin Maka <marcin.maka@linux.intel.com>
//         Janusz Jankowski <janusz.jankowski@linux.intel.com>

#include <sof/math/numbers.h>

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>

static void test_math_numbers_gcd_for_5083_and_391_equals_391(void **state)
{
	int r;

	(void) state;

	r = gcd(5083, 391);
	assert_int_equal(r, 391);
}

static void test_math_numbers_gcd_for_12_and_9_equals_3(void **state)
{
	int r;

	(void) state;

	r = gcd(12, 9);
	assert_int_equal(r, 3);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test
			(test_math_numbers_gcd_for_5083_and_391_equals_391),
		cmocka_unit_test(test_math_numbers_gcd_for_12_and_9_equals_3),
	};

	cmocka_set_message_output(CM_OUTPUT_TAP);

	return cmocka_run_group_tests(tests, NULL, NULL);
}
