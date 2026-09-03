// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2024 Intel Corporation.

#include <math.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <cmocka.h>

#include <drc/drc_math.h>

#define TEST_POINTS 500

#define ABS_DELTA_TOLERANCE_T1 1.0e-9
#define REL_DELTA_TOLERANCE_T1 1.4e-4
#define ABS_DELTA_TOLERANCE_T2 7.4e-7
#define REL_DELTA_TOLERANCE_T2 7.5e-6
#define ABS_DELTA_TOLERANCE_T3 3.8e-4
#define REL_DELTA_TOLERANCE_T3 1.9e-6

static int32_t drc_inv_ref(int32_t x, int32_t precision_x, int32_t precision_y)
{
	double xf, yf, ys;

	xf = (double)x / ((int64_t)1 << precision_x);
	yf = 1 / xf;
	ys = round(yf * ((int64_t)1 << precision_y));
	if (ys > INT32_MAX)
		ys = INT32_MAX;

	if (ys < INT32_MIN)
		ys = INT32_MIN;

	return (int32_t)ys;
}

static void drc_math_inv_fixed_test_helper(int32_t x_start, int32_t x_end,
					   int32_t x_step, int q_in, int q_out,
					   double abs_delta_tolerance, double rel_delta_tolerance)
{
	double fx, fy1, fy2;
	double abs_delta, rel_delta;
	int64_t x1;
	int32_t x, y1, y2;
	int32_t differences = 0;
	double abs_delta_max = 0;
	double rel_delta_max = 0;

	printf("%s: Testing q_in = %d q_out = %d in = %d:%d:%d\n", __func__,
		q_in, q_out, x_start, x_step, x_end);

	x1 = x_start;
	while (x1 <= x_end) {
		x = (int32_t)x1;
		y1 = drc_inv_ref(x, q_in, q_out);
		y2 = drc_inv_fixed(x, q_in, q_out);
		if (y1 != y2)
			differences++;

		fx = (double)x / (1 << q_in);
		fy1 = (double)y1 / (1 << q_out);
		fy2 = (double)y2 / (1 << q_out);
		abs_delta = fabs(fy1 - fy2);
		rel_delta = abs_delta / fy1;
		if (rel_delta > rel_delta_max)
			rel_delta_max = rel_delta;

		if (abs_delta > abs_delta_max)
			abs_delta_max = abs_delta;

		if (rel_delta > rel_delta_tolerance) {
			printf("%s: Relative error %g exceeds limit %g, input %g output %g ref %g.\n",
			       __func__, rel_delta, rel_delta_tolerance, fx, fy2, fy1);
			assert_true(false);
		}

		if (abs_delta > abs_delta_tolerance) {
			printf("%s: Absolute error %g exceeds limit %g, input %g output %g ref %g.\n",
			       __func__, abs_delta, abs_delta_tolerance, fx, fy2, fy1);
			assert_true(false);
		}

		x1 += x_step;
	}

	printf("%s: bit exact differences count = %d\n", __func__, differences);
	printf("%s: Absolute max error was %.6e.\n", __func__, abs_delta_max);
	printf("%s: Relative max error was %.6e.\n", __func__, rel_delta_max);
}

static void test_function_drc_inv_fixed_q12_q30(void **state)
{
	(void)state;

	int32_t x_start = 1;
	int32_t x_end = INT32_MAX;
	int32_t x_step = (int32_t)(((int64_t)x_end - (int64_t)x_start) / (TEST_POINTS - 1));
	int q_in = 12;
	int q_out = 30;

	drc_math_inv_fixed_test_helper(x_start, x_end, x_step, q_in, q_out,
				       ABS_DELTA_TOLERANCE_T1, REL_DELTA_TOLERANCE_T1);
}

static void test_function_drc_inv_fixed_q22_q26(void **state)
{
	(void)state;

	int32_t x_start = 1;
	int32_t x_end = INT32_MAX;
	int32_t x_step = (int32_t)(((int64_t)x_end - (int64_t)x_start) / TEST_POINTS);
	int q_in = 22;
	int q_out = 26;

	drc_math_inv_fixed_test_helper(x_start, x_end, x_step, q_in, q_out,
				       ABS_DELTA_TOLERANCE_T2, REL_DELTA_TOLERANCE_T2);
}

static void test_function_drc_inv_fixed_q31_q20(void **state)
{
	(void)state;

	int32_t x_start = 1;
	int32_t x_end = INT32_MAX;
	int32_t x_step = (int32_t)(((int64_t)x_end - (int64_t)x_start) / TEST_POINTS);
	int q_in = 31;
	int q_out = 20;

	drc_math_inv_fixed_test_helper(x_start, x_end, x_step, q_in, q_out,
				       ABS_DELTA_TOLERANCE_T3, REL_DELTA_TOLERANCE_T3);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_function_drc_inv_fixed_q12_q30),
		cmocka_unit_test(test_function_drc_inv_fixed_q22_q26),
		cmocka_unit_test(test_function_drc_inv_fixed_q31_q20),
	};

	cmocka_set_message_output(CM_OUTPUT_TAP);

	return cmocka_run_group_tests(tests, NULL, NULL);
}
