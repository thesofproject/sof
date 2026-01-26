// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.

#include <math.h>
#include <sof/common.h>
#include <sof/math/icomplex32.h>
#include <zephyr/logging/log.h>
#include <zephyr/ztest.h>

/* Test data tables from Octave generated reference */
#include "test_complex_polar_tables.h"

LOG_MODULE_REGISTER(test_complex_polar, LOG_LEVEL_INF);

#define COMPLEX_ABS_TOL 1.2e-8
#define MAGNITUDE_ABS_TOL 7.1e-8
#define ANGLE_ABS_TOL 4.4e-5

/**
 * @brief Test complex to polar conversion function
 *
 * This test validates the icomplex32_to_polar() function against
 * Octave-generated reference values. The test includes 1000 data points.
 *
 * Complex number values are Q1.31 -1.0 to +1.0
 * Polar magnitude values are Q2.30 0 to +2.0
 * Polar angle values are Q3.29 from -pi to +pi
 */
ZTEST(math_complex, test_icomplex32_to_polar)
{
	struct icomplex32 complex, complex_mag_max, complex_ang_max;
	struct ipolar32 polar;
	double ref_magnitude, ref_angle;
	double magnitude, angle;
	double delta_mag, delta_ang;
	double magnitude_scale_q30 = 1.0 / 1073741824.0;
	double angle_scale_q29 = 1.0 / 536870912.0;
	double delta_mag_max = 0;
	double delta_ang_max = 0;
	int i;

	for (i = 0; i < TEST_COMPLEX_POLAR_NUM_POINTS; i++) {
		complex.real = test_real_values[i];
		complex.imag = test_imag_values[i];
		ref_magnitude = magnitude_scale_q30 * test_magnitude_values[i];
		ref_angle = angle_scale_q29 * test_angle_values[i];
		sofm_icomplex32_to_polar(&complex, &polar);

		magnitude = magnitude_scale_q30 * polar.magnitude;
		delta_mag = fabs(ref_magnitude - magnitude);
		if (delta_mag > delta_mag_max) {
			delta_mag_max = delta_mag;
			complex_mag_max = complex;
		}

		angle = angle_scale_q29 * polar.angle;
		delta_ang = fabs(ref_angle - angle);
		if (delta_ang > delta_ang_max) {
			delta_ang_max = delta_ang;
			complex_ang_max = complex;
		}

		zassert_true(delta_mag <= MAGNITUDE_ABS_TOL, "Magnitude calc error at (%d, %d)",
			     complex.real, complex.imag);
		zassert_true(delta_ang <= ANGLE_ABS_TOL, "Angle calc error at (%d, %d)",
			     complex.real, complex.imag);
	}

	/* Re-run worst cases to print info */
	sofm_icomplex32_to_polar(&complex_mag_max, &polar);
	printf("delta_mag_max = %g at (%d, %d) -> (%d, %d)\n", delta_mag_max, complex_mag_max.real,
	       complex_mag_max.imag, polar.magnitude, polar.angle);

	sofm_icomplex32_to_polar(&complex_ang_max, &polar);
	printf("delta_ang_max = %g at (%d, %d) -> (%d, %d)\n", delta_ang_max, complex_ang_max.real,
	       complex_ang_max.imag, polar.magnitude, polar.angle);
}

ZTEST(math_complex, test_ipolar32_to_complex)
{
	struct icomplex32 complex;
	struct ipolar32 polar, polar_real_max, polar_imag_max;
	double ref_real, ref_imag;
	double real, imag;
	double delta_real, delta_imag;
	double scale_q31 = 1.0 / 2147483648.0;
	double delta_real_max = 0;
	double delta_imag_max = 0;
	int i;

	for (i = 0; i < TEST_COMPLEX_POLAR_NUM_POINTS; i++) {
		polar.magnitude = test_magnitude_values[i];
		polar.angle = test_angle_values[i];
		ref_real = scale_q31 * test_real_values[i];
		ref_imag = scale_q31 * test_imag_values[i];
		sofm_ipolar32_to_complex(&polar, &complex);

		real = scale_q31 * complex.real;
		delta_real = fabs(ref_real - real);
		if (delta_real > delta_real_max) {
			delta_real_max = delta_real;
			polar_real_max = polar;
		}

		imag = scale_q31 * complex.imag;
		delta_imag = fabs(ref_imag - imag);
		if (delta_imag > delta_imag_max) {
			delta_imag_max = delta_imag;
			polar_imag_max = polar;
		}

		zassert_true(delta_real <= COMPLEX_ABS_TOL, "Real calc error at (%d, %d)",
			     polar.magnitude, polar.angle);
		zassert_true(delta_imag <= COMPLEX_ABS_TOL, "Imag calc error at (%d, %d)",
			     polar.magnitude, polar.angle);
	}

	/* Re-run worst cases to print info */
	sofm_ipolar32_to_complex(&polar_real_max, &complex);
	printf("delta_real_max = %g at (%d, %d) -> (%d, %d)\n", delta_real_max,
	       polar_real_max.magnitude, polar_real_max.angle, complex.real, complex.imag);

	sofm_ipolar32_to_complex(&polar_imag_max, &complex);
	printf("delta_imag_max = %g at (%d, %d) -> (%d, %d)\n", delta_imag_max,
	       polar_imag_max.magnitude, polar_imag_max.angle, complex.real, complex.imag);
}

ZTEST_SUITE(math_complex, NULL, NULL, NULL, NULL, NULL);
