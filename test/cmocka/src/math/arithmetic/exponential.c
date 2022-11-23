// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 *
 * Author: Shriram Shastry <malladi.sastry@linux.intel.com>
 */

#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <math.h>
#include <cmocka.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>

#include <sof/math/exp_fcn.h>
#include <sof/audio/format.h>
#include <rtos/string.h>
#include <sof/common.h>

#define ULP_TOLERANCE 5.60032793
#define ULP_SCALE 0.0000999999999749

static void gen_exp_testvector(double a, double b, double *r, int32_t *b_i);

/* testvector in Q4.28, -5 to +5 */
/*
 * Arguments	:double a
 *		 double b
 *		 double *r
 *		 int32_t *b_i
 * Return Type	:void
 */
static void gen_exp_testvector(double a, double b, double *r, int32_t *b_i)
{
	double a2;
	double b2;

	*r = a + rand() % (int32_t)(b - a + 1);
	a2 = *r * 268435456;
	b2 = fabs(a2);
	if (b2 < 4503599627370496LL)
		a2 = floor(a2 + 0.5);

	if (a2 < 2147483648LL)
		*b_i = (a2 >= -2147483648LL) ? a2 : INT32_MIN;
	else
		*b_i = INT32_MAX;
}

static void test_math_arithmetic_exponential_fixed(void **state)
{
	(void)state;

	int32_t accum;
	int i;
	double a_i;
	double max_ulp;
	double a_tmp = -5.0123456789;
	double b_tmp =  5.0123456789;
	double r;
	int32_t b_i;

	srand((unsigned int)time(NULL));
	for (i = 0; i < 256; i++) {
		gen_exp_testvector(a_tmp, b_tmp, &r, &b_i);
		a_i = (double)b_i / (1 << 28);
		accum = sofm_exp_int32(b_i);
		max_ulp = fabs(exp(a_i) - (double)accum / (1 << 23)) / ULP_SCALE;

		if (max_ulp > ULP_TOLERANCE) {
			printf("%s: ULP for %.16f: value = %.16f, Exp = %.16f\n", __func__,
			       max_ulp, (double)b_i / (1 << 28), (double)accum / (1 << 23));
			assert_true(max_ulp <= ULP_TOLERANCE);
		}
	}
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_math_arithmetic_exponential_fixed)
	};

	cmocka_set_message_output(CM_OUTPUT_TAP);

	return cmocka_run_group_tests(tests, NULL, NULL);
}
