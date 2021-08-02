// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Intel Corporation. All rights reserved.
//
// Author: Shriram Shastry <malladi.sastry@linux.intel.com>
//

#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <math.h>
#include <cmocka.h>

#include <sof/audio/format.h>
#include <sof/math/trig.h>
#include <sof/common.h>
#include "trig_tables.h"
/* 'Error (max = 0.000000026077032), THD+N  = -157.948952635422842 (dBc)' */
#define CMP_TOLERANCE 0.000000060077032
#define _M_PI		3.14159265358979323846	/* pi */

static void test_math_trig_acos_32b_fixed(void **state)
{
	(void)state;
	double u;
	double v;
	int indx;
	int b_i;

	for (indx = 0; indx < ARRAY_SIZE(degree_table); ++indx) {
		/* convert angle unit degrees to radians */
		/* angleInRadians = pi/180 * angleInDegrees & const Q2.30 format */
		u = (0.017453292519943295 * (double)degree_table[indx] * 0x40000000);
		v = fabs(u);
		/* GitHub macro Q_CONVERT_FLOAT is inaccurate, so replaced with below */
		u = (v >= 0.5) ? floor(u + 0.5) : 0.0;
		b_i = (int)u;

		float r = Q_CONVERT_QTOF(acos_fixed_32b(b_i), 29);
		float diff = fabsf(acos_ref_table[indx] - r);

		if (diff > CMP_TOLERANCE) {
			printf("%s: diff for %.16f deg = %.10f\n", __func__,
			       ((180 / _M_PI) * b_i) / (1 << 30), diff);
		}

		assert_true(diff <= CMP_TOLERANCE);
	}
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_math_trig_acos_32b_fixed)
	};

	cmocka_set_message_output(CM_OUTPUT_TAP);

	return cmocka_run_group_tests(tests, NULL, NULL);
}
