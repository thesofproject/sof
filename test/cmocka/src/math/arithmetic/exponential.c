// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2022-2025 Intel Corporation.
 *
 * Author: Shriram Shastry <malladi.sastry@linux.intel.com>
 *         Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
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

#define ULP_TOLERANCE		1.0
#define ULP_SCALE		1.9073e-06 /* For exp() output Q13.19, 1 / 2^19 */
#define NUMTESTSAMPLES 256

#define NUMTESTSAMPLES_TEST2 100
#define ABS_DELTA_TOLERANCE_TEST2 2.0e-6
#define REL_DELTA_TOLERANCE_TEST2 1000.0 /* rel. error is large with values near zero */
#define NUMTESTSAMPLES_TEST3 100
#define ABS_DELTA_TOLERANCE_TEST3 2.0e-6
#define REL_DELTA_TOLERANCE_TEST3 10.0e-2
#define SOFM_EXP_FIXED_ARG_MIN -11.5
#define SOFM_EXP_FIXED_ARG_MAX 7.6245

#define NUMTESTSAMPLES_TEST4 100
#define ABS_DELTA_TOLERANCE_TEST4 2.5e-5
#define REL_DELTA_TOLERANCE_TEST4 1000.0 /* rel. error is large with values near zero */

/**
 * Saturates input to 32 bits
 * @param	x	Input value
 * @return		Saturated output value
 */
static int32_t saturate32(int64_t x)
{
	if (x < INT32_MIN)
		return INT32_MIN;
	else if (x > INT32_MAX)
		return INT32_MAX;

	return x;
}

/**
 * Generates linearly spaced values for a vector with end points and number points in
 * desired fractional Q-format for 32 bit integer. If the test values exceed int32_t
 * range, the values are saturated to INT32_MIN to INT32_MAX range.
 *
 * @param a		First value of test vector
 * @param b		Last value of test vector
 * @param step_count	Number of values in vector
 * @param point		Calculate n-th point of vector 0 .. step_count - 1
 * @param qformat	Number of fractional bits y in Qx.y format
 * @param fout		Pointer to calculated test vector value, double
 * @param iout		Pointer to calculated test vector value, int32_t
 */
static void gen_testvector_linspace_int32(double a, double b, int step_count, int point,
					  int qformat, double *fout, int32_t *iout)
{
	double fstep = (b - a) / (step_count - 1);
	double fvalue = a + fstep * point;
	int64_t itmp;

	itmp = (int64_t)round(fvalue * (double)(1 << qformat));
	*iout = saturate32(itmp);
	*fout = (double)*iout / (1 << qformat);
	return;
}

/**
 * Test for sofm_exp_approx()
 * @param state		Cmocka internal state
 */
static void test_function_sofm_exp_approx(void **state)
{
	(void)state;

	int32_t accum;
	int i;
	double a_i;
	double max_ulp = 0;
	double ulp;
	double a_tmp = -8;
	double b_tmp =  8;
	int32_t b_i;

	for (i = 0; i < NUMTESTSAMPLES; i++) {
		gen_testvector_linspace_int32(a_tmp, b_tmp, NUMTESTSAMPLES, i, 28, &a_i, &b_i);
		accum = sofm_exp_approx(b_i);
		ulp = fabs(exp(a_i) - (double)accum / (1 << 19)) / ULP_SCALE;
		if (ulp > max_ulp)
			max_ulp = ulp;

		if (ulp > ULP_TOLERANCE) {
			printf("%s: ULP for %.16f: value = %.16f, Exp = %.16f\n", __func__,
			       ulp, (double)b_i / (1 << 28), (double)accum / (1 << 19));
			assert_true(false);
		}
	}
	printf("%s: Worst-case ULP: %g ULP_SCALE %g\n", __func__, max_ulp, ULP_SCALE);
}


/**
 * Calculate reference exponent value
 * @param x		Input value
 * @param qformat	Fractional bits y in Qx.y format
 * @return		Saturated exponent value to match fractional format
 */
static double ref_exp(double x, int qformat)
{
	double yf;
	int64_t yi;

	yf = exp(x);
	yi = yf * (1 << qformat);
	if (yi > INT32_MAX)
		yi = INT32_MAX;
	else if (yi < INT32_MIN)
		yi = INT32_MIN;

	yf = (double)yi / (1 << qformat);
	return yf;
}

/**
 * Calculates test exponent function and compares result to reference exponent.
 * @param ivalue		Fractional format input value Q5.27
 * @param iexp_value		Fractional format output value Q12.20
 * @param abs_delta_max		Calculated absolute error
 * @param rel_delta_max		Calculated relative error
 * @param abs_delta_tolerance	Tolerance for absolute error
 * @param rel_delta_tolerance	Tolerance for relative error
 */
static void test_exp_with_input_value(int32_t ivalue, int32_t *iexp_value,
				      double *abs_delta_max, double *rel_delta_max,
				      double abs_delta_tolerance, double rel_delta_tolerance)
{
	double fvalue, fexp_value, ref_exp_value;
	double rel_delta, abs_delta;
	double eps = 1e-9;

	*iexp_value = sofm_exp_fixed(ivalue);
	fvalue = (double)ivalue / (1 << 27); /* Q5.27 */
	fexp_value = (double)*iexp_value / (1 << 20); /* Q12.20 */
	ref_exp_value = ref_exp(fvalue, 20);
	abs_delta = fabs(ref_exp_value - fexp_value);
	rel_delta = abs_delta / (ref_exp_value + eps);
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

/**
 * Test for sofm_exp_fixed()
 * @param state		Cmocka internal state
 */
static void test_function_sofm_exp_fixed(void **state)
{
	(void)state;
	double rel_delta_max, abs_delta_max;
	double tmp;
	int32_t ivalue, iexp_value;
	int i;

	/* Test max int32_t range with coarse grid */
	rel_delta_max = 0;
	abs_delta_max = 0;
	for (i = 0; i < NUMTESTSAMPLES_TEST2; i++) {
		gen_testvector_linspace_int32(-16, 16, NUMTESTSAMPLES_TEST2, i, 27, &tmp, &ivalue);
		test_exp_with_input_value(ivalue, &iexp_value, &abs_delta_max, &rel_delta_max,
					  ABS_DELTA_TOLERANCE_TEST2, REL_DELTA_TOLERANCE_TEST2);
	}

	printf("%s: Absolute max error was %.6e (max range).\n", __func__, abs_delta_max);
	printf("%s: Relative max error was %.6e (max range).\n", __func__, rel_delta_max);

	/* Test max int32_t middle range with fine grid */
	rel_delta_max = 0;
	abs_delta_max = 0;
	for (i = 0; i < NUMTESTSAMPLES_TEST3; i++) {
		gen_testvector_linspace_int32(SOFM_EXP_FIXED_ARG_MIN, SOFM_EXP_FIXED_ARG_MAX,
					      NUMTESTSAMPLES_TEST3, i, 27, &tmp, &ivalue);
		test_exp_with_input_value(ivalue, &iexp_value, &abs_delta_max, &rel_delta_max,
					  ABS_DELTA_TOLERANCE_TEST3, REL_DELTA_TOLERANCE_TEST3);
	}

	printf("%s: Absolute max error was %.6e (middle).\n", __func__, abs_delta_max);
	printf("%s: Relative max error was %.6e (middle).\n", __func__, rel_delta_max);
}

/**
 * Reference function for dB to linear conversion
 * @param x		Input value
 * @param qformat	Fractional bits y in Qx.y format for saturation
 * @return		Saturated linear value
 */
static double ref_db2lin(double x, int qformat)
{
	double fref;
	int64_t iref;

	fref = pow(10, x / 20);
	iref = fref * (1 << qformat);
	return (double)saturate32(iref) / (1 << qformat);
}

/**
 * Test for sofm_db2lin_fixed()
 * @param state		Cmocka internal state
 */
static void test_function_sofm_db2lin_fixed(void **state)
{
	(void)state;
	double abs_delta, rel_delta, abs_delta_max, rel_delta_max;
	double fin, fout, fref;
	double eps = 1e-9;
	int32_t iin, iout;
	int i;

	rel_delta_max = 0;
	abs_delta_max = 0;
	for (i = 0; i < NUMTESTSAMPLES_TEST2; i++) {
		gen_testvector_linspace_int32(-128, 128, NUMTESTSAMPLES_TEST4, i, 24, &fin, &iin);
		iout = sofm_db2lin_fixed(iin);
		fout = (double)iout / (1 << 20);
		fref = ref_db2lin(fin, 20);
		abs_delta = fabs(fref - fout);
		rel_delta = abs_delta / (fref + eps);
		if (abs_delta > abs_delta_max)
			abs_delta_max = abs_delta;

		if (rel_delta > rel_delta_max)
			rel_delta_max = rel_delta;

		if (abs_delta > ABS_DELTA_TOLERANCE_TEST4) {
			printf("%s: Absolute error %g exceeds limit %g, input %g output %g.\n",
			       __func__, abs_delta, ABS_DELTA_TOLERANCE_TEST4, fin, fout);
			assert_true(false);
		}

		if (rel_delta > REL_DELTA_TOLERANCE_TEST4) {
			printf("%s: Relative error %g exceeds limit %g, input %g output %g.\n",
			       __func__, rel_delta, REL_DELTA_TOLERANCE_TEST4, fin, fout);
			assert_true(false);
		}
	}
	printf("%s: Absolute max error was %.6e.\n", __func__, abs_delta_max);
	printf("%s: Relative max error was %.6e.\n", __func__, rel_delta_max);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_function_sofm_exp_approx),
		cmocka_unit_test(test_function_sofm_exp_fixed),
		cmocka_unit_test(test_function_sofm_db2lin_fixed),
	};

	cmocka_set_message_output(CM_OUTPUT_TAP);

	return cmocka_run_group_tests(tests, NULL, NULL);
}
