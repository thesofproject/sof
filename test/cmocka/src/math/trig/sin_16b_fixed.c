// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
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
#include <sof/math/trig.h>
#include "trig_tables.h"
/* 'Error (max = 0.000061), THD+N  = -91.502670' */
#define CMP_TOLERANCE	0.000065
#define _M_PI		3.14159265358979323846	/* pi */

static void test_math_trig_sin_fixed(void **state)
{
	(void)state;

	int theta;

	for (theta = 0; theta < 360; ++theta) {
		double rad = _M_PI * (theta / 180.0);
		int32_t rad_q28 = Q_CONVERT_FLOAT(rad, 28);

		float r = Q_CONVERT_QTOF(sin_fixed_16b(rad_q28), 15);
		float diff = fabsf(sin_ref_table[theta] - r);

		if (diff > CMP_TOLERANCE) {
			printf("%s: diff for %d deg = %.10f\n", __func__,
			       theta, diff);
		}

		assert_true(diff <= CMP_TOLERANCE);
	}
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_math_trig_sin_fixed)
	};

	cmocka_set_message_output(CM_OUTPUT_TAP);

	return cmocka_run_group_tests(tests, NULL, NULL);
}
