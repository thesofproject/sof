// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Author: Slawomir Blauciak <slawomir.blauciak@linux.intel.com>

#include <sof/math/numbers.h>

#include <stdio.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <math.h>
#include <stdint.h>
#include <cmocka.h>

static void test_math_numbers_ceil_divide(void **state)
{
	(void)state;

	int params[8] = {
		-1000,
		300,
		123,
		-10,
		1337,
		-6,
		999,
		-2
	};

	int i, j;

	for (i = 0; i < 8; ++i) {
		for (j = 0; j < 8; ++j) {
			int ref = ceilf((float)params[i] / (float)params[j]);
			int r = ceil_divide(params[i], params[j]);

			if (r != ref) {
				printf("%s: %d / %d = %d (ref: %d)\n", __func__,
				       params[i], params[j], r, ref);
			}

			assert_int_equal(r, ref);
		}
	}

}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_math_numbers_ceil_divide)
	};

	cmocka_set_message_output(CM_OUTPUT_TAP);

	return cmocka_run_group_tests(tests, NULL, NULL);
}
