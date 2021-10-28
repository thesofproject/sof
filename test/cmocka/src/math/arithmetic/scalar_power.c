// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Intel Corporation. All rights reserved.
//
// Author: Shriram Shastry <malladi.sastry@linux.intel.com>

#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <math.h>
#include <cmocka.h>

#include <sof/audio/format.h>
#include <sof/math/power.h>
#include <sof/common.h>
#include "power_tables.h"
/* 'Error (max = 0.000034912111005), THD+N  = -96.457180359025074' */
#define CMP_TOLERANCE 0.0000150363575813

static void test_math_arithmetic_power_fixed(void **state)
{
	(void)state;

	double p;
	int i;
	int j;

	for (i = 0; i < ARRAY_SIZE(b); i++) {
		for (j = 0; j < ARRAY_SIZE(e); j++) {
			p = power_int32(b[i], e[j]);
			float diff = fabsf(power_table[i][j] - (double)p / (1 << 15));

			if (diff > CMP_TOLERANCE) {
				printf("%s: diff for %.16f: base = %.16f",
				       __func__, diff, (double)b[i] / (1 << 25));
				printf(", exponent = %.16f, power = %.16f\n",
				       (double)e[j] / (1 << 29), (double)p / (1 << 15));
				assert_true(diff <= CMP_TOLERANCE);
			}
		}
	}
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_math_arithmetic_power_fixed)
	};

	cmocka_set_message_output(CM_OUTPUT_TAP);

	return cmocka_run_group_tests(tests, NULL, NULL);
}
