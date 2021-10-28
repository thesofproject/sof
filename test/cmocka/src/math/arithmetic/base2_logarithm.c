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
#include <sof/math/log.h>
#include <sof/common.h>
#include "log2_tables.h"
/* 'Error[max] = 0.0000236785999981,THD(-dBc) = -92.5128795787487235' */
#define CMP_TOLERANCE 0.0000236785999981

static void test_math_arithmetic_base2log_fixed(void **state)
{
	(void)state;

	double u;
	double v;
	int b_i;
	int i;

	for (i = 0; i < ARRAY_SIZE(log2_lookup_table); i++) {
		u = ((double)vector_table[i] / (1 << 1));
		v = fabs(u);
		/* GitHub macro Q_CONVERT_FLOAT is inaccurate, so replaced with below */
		u = (v >= 0.5) ? floor(u + 0.5) : 0.0;
		b_i = (int)u;

		float y = Q_CONVERT_QTOF(base2_logarithm(b_i), 16);
		float diff = fabs(log2_lookup_table[i] - y);

		if (diff > CMP_TOLERANCE) {
			printf("%s: diff for %.16f: value = %.16f, log2 = %.16f\n", __func__, diff,
			       (double)vector_table[i] / (1 << 1), (double)y);
		}

	assert_true(diff <= CMP_TOLERANCE);
	}
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_math_arithmetic_base2log_fixed)
	};

	cmocka_set_message_output(CM_OUTPUT_TAP);

	return cmocka_run_group_tests(tests, NULL, NULL);
}
