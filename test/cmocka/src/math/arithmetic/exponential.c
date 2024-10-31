// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 *
 * Author: Shriram Shastry <malladi.sastry@linux.intel.com>
 */

#include <stdbool.h>
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
#define NUMTESTSAMPLES 256

#define NUMTESTSAMPLES_MIDDLE_TEST2 100
#define NUMTESTSAMPLES_SMALL_TEST2 100
#define NUMTESTSAMPLES_LARGE_TEST2 100
#define ABS_DELTA_TOLERANCE_TEST1 1.2e-2
#define REL_DELTA_TOLERANCE_TEST1 2.8e-4
#define ABS_DELTA_TOLERANCE_TEST2 1.7e-6
#define REL_DELTA_TOLERANCE_TEST2 3.7e-2
#define ABS_DELTA_TOLERANCE_TEST3 1.2e-1
#define REL_DELTA_TOLERANCE_TEST3 7.7e-5
#define SOFM_EXP_FIXED_ARG_MIN -11.5
#define SOFM_EXP_FIXED_ARG_MAX 7.6245

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

static void test_function_sofm_exp_int32(void **state)
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
	for (i = 0; i < NUMTESTSAMPLES; i++) {
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

static void gen_exp_testvector_linspace_q27(double a, double b, int step_count,
					    int point, int32_t *value)
{
	double fstep = (b - a) / (step_count - 1);
	double fvalue = a + fstep * point;

	*value = (int32_t)round(fvalue * 134217728.0);
}

static double ref_exp(double value)
{
	if (value < SOFM_EXP_FIXED_ARG_MIN)
		return 0.0;
	else if (value > SOFM_EXP_FIXED_ARG_MAX)
		return 2048.0;
	else
		return exp(value);
}

static void test_exp_with_input_value(int32_t ivalue, int32_t *iexp_value,
				      double *abs_delta_max, double *rel_delta_max,
				      double abs_delta_tolerance, double rel_delta_tolerance)
{
	double fvalue, fexp_value, ref_exp_value;
	double rel_delta, abs_delta;

	*iexp_value = sofm_exp_fixed(ivalue);
	fvalue = (double)ivalue / (1 << 27); /* Q5.27 */
	fexp_value = (double)*iexp_value / (1 << 20); /* Q12.20 */
	/* printf("val = %10.6f, exp = %12.6f\n", fvalue, fexp_value); */

	ref_exp_value = ref_exp(fvalue);
	abs_delta = fabs(ref_exp_value - fexp_value);
	rel_delta = abs_delta / ref_exp_value;
	if (abs_delta > *abs_delta_max)
		*abs_delta_max = abs_delta;

	if (rel_delta > *rel_delta_max)
		*rel_delta_max = rel_delta;

	if (abs_delta > abs_delta_tolerance) {
		printf("%s: Absolute error %g exceeds limit %g, input %g output %g.\n",
		       __func__, abs_delta, abs_delta_tolerance, fvalue, fexp_value);
		assert_true(false);
	}

	if (rel_delta > rel_delta_tolerance) {
		printf("%s: Relative error %g exceeds limit %g, input %g output %g.\n",
		       __func__, rel_delta, rel_delta_tolerance, fvalue, fexp_value);
		assert_true(false);
	}
}

static void test_function_sofm_exp_fixed(void **state)
{
	(void)state;

	double rel_delta_max, abs_delta_max;
	int32_t ivalue, iexp_value;
	int i;

	rel_delta_max = 0;
	abs_delta_max = 0;
	for (i = 0; i < NUMTESTSAMPLES_MIDDLE_TEST2; i++) {
		gen_exp_testvector_linspace_q27(-5, 5, NUMTESTSAMPLES_MIDDLE_TEST2, i, &ivalue);
		test_exp_with_input_value(ivalue, &iexp_value, &abs_delta_max, &rel_delta_max,
					  ABS_DELTA_TOLERANCE_TEST1, REL_DELTA_TOLERANCE_TEST1);

	}

	printf("%s: Absolute max error was %.6e (middle).\n", __func__, abs_delta_max);
	printf("%s: Relative max error was %.6e (middle).\n", __func__, rel_delta_max);

	rel_delta_max = 0;
	abs_delta_max = 0;
	for (i = 0; i < NUMTESTSAMPLES_SMALL_TEST2; i++) {
		gen_exp_testvector_linspace_q27(-16, -5, NUMTESTSAMPLES_SMALL_TEST2, i, &ivalue);
		test_exp_with_input_value(ivalue, &iexp_value, &abs_delta_max, &rel_delta_max,
					  ABS_DELTA_TOLERANCE_TEST2, REL_DELTA_TOLERANCE_TEST2);
	}

	printf("%s: Absolute max error was %.6e (small).\n", __func__, abs_delta_max);
	printf("%s: Relative max error was %.6e (small).\n", __func__, rel_delta_max);

	rel_delta_max = 0;
	abs_delta_max = 0;
	for (i = 0; i < NUMTESTSAMPLES_LARGE_TEST2; i++) {
		gen_exp_testvector_linspace_q27(5, 15.9999, NUMTESTSAMPLES_LARGE_TEST2, i, &ivalue);
		test_exp_with_input_value(ivalue, &iexp_value, &abs_delta_max, &rel_delta_max,
					  ABS_DELTA_TOLERANCE_TEST3, REL_DELTA_TOLERANCE_TEST3);
	}

	printf("%s: Absolute max error was %.6e (large).\n", __func__, abs_delta_max);
	printf("%s: Relative max error was %.6e (large).\n", __func__, rel_delta_max);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_function_sofm_exp_int32),
		cmocka_unit_test(test_function_sofm_exp_fixed),
	};

	cmocka_set_message_output(CM_OUTPUT_TAP);

	return cmocka_run_group_tests(tests, NULL, NULL);
}
