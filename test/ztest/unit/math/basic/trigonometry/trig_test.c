// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2025 Intel Corporation. All rights reserved.
//
// These contents may have been developed with support from one or more Intel-operated
// generative artificial intelligence solutions.
//
// Converted from CMock to Ztest
//
// Original tests from sof/test/cmocka/src/math/trig/:
// - sin_32b_fixed.c (Author: Slawomir Blauciak <slawomir.blauciak@linux.intel.com>)
// - sin_16b_fixed.c (Author: Shriram Shastry <malladi.sastry@linux.intel.com>)
// - cos_32b_fixed.c (Author: Shriram Shastry <malladi.sastry@linux.intel.com>)
// - cos_16b_fixed.c (Author: Shriram Shastry <malladi.sastry@linux.intel.com>)
// - asin_32b_fixed.c (Author: Shriram Shastry <malladi.sastry@linux.intel.com>)
// - asin_16b_fixed.c (Author: Shriram Shastry <malladi.sastry@linux.intel.com>)
// - acos_32b_fixed.c (Author: Shriram Shastry <malladi.sastry@linux.intel.com>)
// - acos_16b_fixed.c (Author: Shriram Shastry <malladi.sastry@linux.intel.com>)
// - lut_sin_16b_fixed.c (Authors: Slawomir Blauciak <slawomir.blauciak@linux.intel.com>,
//                                 Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>)

#define _USE_MATH_DEFINES
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <zephyr/ztest.h>
#include <sof/audio/format.h>
#include <sof/audio/format_generic.h>
#include <sof/math/trig.h>
#include <sof/math/cordic.h>
#include <sof/math/lut_trig.h>
#include "trig_tables.h"

/* Define M_PI if not available */
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Conversion factor from degrees to radians (PI/180) */
#define DEGREES_TO_RADIANS 0.017453292519943295

/*
 * Helper function for rounding double values to nearest integer
 * Implements custom rounding: round to 0 if absolute value < 0.5, otherwise round normally
 */
static inline int32_t round_to_nearest_int(double value)
{
	double abs_value = fabs(value);

	return (int32_t) (abs_value >= 0.5) ? floor(value + 0.5) : 0.0;
}

/* Test sin_32b_fixed function */
ZTEST(trigonometry, test_sin_32b_fixed)
{
	int theta;
	float delta;
	double rad;
	int32_t sin_val;

	for (theta = 0; theta < ARRAY_SIZE(sin_ref_table); ++theta) {
		rad = M_PI * ((double)theta / 180.0);
		sin_val = sin_fixed_32b(Q_CONVERT_FLOAT(rad, 28));
		delta = fabsf(sin_ref_table[theta] - Q_CONVERT_QTOF(sin_val, 31));
		zassert_true(delta <= CMP_TOLERANCE_32B,
			     "sin_32b_fixed failed for angle %d", theta);
	}
}

/* Test sin_16b_fixed function */
ZTEST(trigonometry, test_sin_16b_fixed)
{
	int theta;
	float delta;
	double rad;
	int16_t sin_val;

	for (theta = 0; theta < ARRAY_SIZE(sin_ref_table); ++theta) {
		rad = M_PI * ((double)theta / 180.0);
		sin_val = sin_fixed_16b(Q_CONVERT_FLOAT(rad, 28));
		delta = fabsf(sin_ref_table[theta] - Q_CONVERT_QTOF(sin_val, 15));
		zassert_true(delta <= CMP_TOLERANCE_16B,
			     "sin_16b_fixed failed for angle %d", theta);
	}
}

/* Test cos_32b_fixed function */
ZTEST(trigonometry, test_cos_32b_fixed)
{
	int theta;
	float delta;
	double rad;
	int32_t cos_val;

	for (theta = 0; theta < ARRAY_SIZE(cos_ref_table); ++theta) {
		rad = M_PI * ((double)theta / 180.0);
		cos_val = cos_fixed_32b(Q_CONVERT_FLOAT(rad, 28));
		delta = fabsf(cos_ref_table[theta] - Q_CONVERT_QTOF(cos_val, 31));
		zassert_true(delta <= CMP_TOLERANCE_32B,
			     "cos_32b_fixed failed for angle %d", theta);
	}
}

/* Test cos_16b_fixed function */
ZTEST(trigonometry, test_cos_16b_fixed)
{
	int theta;
	float delta;
	double rad;
	int16_t cos_val;

	for (theta = 0; theta < ARRAY_SIZE(cos_ref_table); ++theta) {
		rad = M_PI * ((double)theta / 180.0);
		cos_val = cos_fixed_16b(Q_CONVERT_FLOAT(rad, 28));
		delta = fabsf(cos_ref_table[theta] - Q_CONVERT_QTOF(cos_val, 15));
		zassert_true(delta <= CMP_TOLERANCE_16B,
			     "cos_16b_fixed failed for angle %d", theta);
	}
}

/* Test asin_32b_fixed function */
ZTEST(trigonometry, test_asin_32b_fixed)
{
	int index;
	float delta;
	double u;
	int32_t asin_val, input_val;

	for (index = 0; index < ARRAY_SIZE(degree_table); ++index) {
		/* Convert degree to input value in Q2.30 format like original test */
		/* angleInRadians = PI/180 * angleInDegrees & const Q2.30 format */
		u = (DEGREES_TO_RADIANS * degree_table[index] * 0x40000000);
		input_val = round_to_nearest_int(u);

		asin_val = asin_fixed_32b(input_val);
		delta = fabsf(asin_ref_table[index] - Q_CONVERT_QTOF(asin_val, 29));
		zassert_true(delta <= CMP_TOLERANCE_ASIN_32B,
			     "asin_32b_fixed failed for index %d", index);
	}
}

/* Test asin_16b_fixed function */
ZTEST(trigonometry, test_asin_16b_fixed)
{
	int index;
	float delta;
	double u;
	int16_t asin_val;
	int32_t input_val;

	for (index = 0; index < ARRAY_SIZE(degree_table); ++index) {
		/* Convert degree to input value in Q2.30 format like original test */
		/* angleInRadians = PI/180 * angleInDegrees & const Q2.30 format */
		u = (DEGREES_TO_RADIANS * degree_table[index] * 0x40000000);
		input_val = round_to_nearest_int(u);

		asin_val = asin_fixed_16b(input_val);
		delta = fabsf(asin_ref_table[index] - Q_CONVERT_QTOF(asin_val, 13));
		zassert_true(delta <= CMP_TOLERANCE_ASIN_16B,
			     "asin_16b_fixed failed for index %d", index);
	}
}

/* Test acos_32b_fixed function */
ZTEST(trigonometry, test_acos_32b_fixed)
{
	int index;
	float delta;
	double u;
	int32_t acos_val, input_val;

	for (index = 0; index < ARRAY_SIZE(degree_table); ++index) {
		/* Convert degree to input value in Q2.30 format like original test */
		/* angleInRadians = PI/180 * angleInDegrees & const Q2.30 format */
		u = (DEGREES_TO_RADIANS * degree_table[index] * 0x40000000);
		input_val = round_to_nearest_int(u);

		acos_val = acos_fixed_32b(input_val);
		delta = fabsf(acos_ref_table[index] - Q_CONVERT_QTOF(acos_val, 29));
		zassert_true(delta <= CMP_TOLERANCE_ACOS_32B,
			     "acos_32b_fixed failed for index %d", index);
	}
}

/* Test acos_16b_fixed function */
ZTEST(trigonometry, test_acos_16b_fixed)
{
	int index;
	float delta;
	double u;
	int16_t acos_val;
	int32_t input_val;

	for (index = 0; index < ARRAY_SIZE(degree_table); ++index) {
		/* Convert degree to input value in Q2.30 format like original test */
		/* angleInRadians = PI/180 * angleInDegrees & const Q2.30 format */
		u = (DEGREES_TO_RADIANS * degree_table[index] * 0x40000000);
		input_val = round_to_nearest_int(u);

		acos_val = acos_fixed_16b(input_val);
		delta = fabsf(acos_ref_table[index] - Q_CONVERT_QTOF(acos_val, 13));
		zassert_true(delta <= CMP_TOLERANCE_ACOS_16B,
			     "acos_16b_fixed failed for index %d", index);
	}
}

/* Test sin lookup table function */
ZTEST(trigonometry, test_sin_lut_16b_fixed)
{
	int theta;
	float delta;
	double rad;
	int16_t sin_val;

	for (theta = 0; theta < ARRAY_SIZE(sin_ref_table); ++theta) {
		rad = M_PI * (double)theta / 180.0;
		sin_val = sofm_lut_sin_fixed_16b(Q_CONVERT_FLOAT(rad, 28));
		delta = fabsf(sin_ref_table[theta] - Q_CONVERT_QTOF(sin_val, 15));
		zassert_true(delta <= CMP_TOLERANCE_SIN,
			     "sin_lut_16b_fixed failed for angle %d", theta);
	}
}

ZTEST_SUITE(trigonometry, NULL, NULL, NULL, NULL, NULL);
