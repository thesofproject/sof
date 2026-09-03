// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
//         Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>
//         Shriram Shastry <malladi.sastry@linux.intel.com>

#include <rtos/symbol.h>
#include <sof/audio/format.h>
#include <sof/math/trig.h>
#include <sof/math/cordic.h>
#include <stdint.h>

#define CORDIC_SINE_COS_LUT_Q29 652032874 /* deg = 69.586061, int32(1.214505869895220 * 2^29) */

#define CORDIC_SINCOS_PIOVERTWO_Q28 421657428	  /* int32(pi / 2 * 2^28) */
#define CORDIC_SINCOS_PI_Q28 843314857		  /* int32(pi * 2^28) */
#define CORDIC_SINCOS_TWOPI_Q28 1686629713	  /* int32(2 * pi * 2^28) */
#define CORDIC_SINCOS_ONEANDHALFPI_Q28 1264972285 /* int32(1.5 * pi * 2^28) */

/**
 * CORDIC-based approximation of sine and cosine
 * \+----------+----------------------------------------+--------------------+-------------------+
 * \|values    |		Q_CONVERT_FLOAT		|(180/pi)*angleInRad |(pi/180)*angleInDeg|
 * \+----------+----------------------------------------+--------------------+-------------------+
 * \|379625062 | Q_CONVERT_FLOAT(1.4142135605216026, 28)|    81.0284683480568| 1.41421356052160  |
 * \|1073741824| Q_CONVERT_FLOAT(1.0000000000000000, 30)|    57.2957795130823| 1.00000000000000  |
 * \|843314856 | Q_CONVERT_FLOAT(1.5707963258028030, 29)|    89.9999999431572| 1.57079632580280  |
 * \|1686629713| Q_CONVERT_FLOAT(1.5707963267341256, 30)|    89.9999999965181| 1.57079632673413  |
 * \+----------+----------------------------------------+--------------------+-------------------+
 */

#define CORDIC_ARCSINCOS_SQRT2_DIV4_Q30 379625062 /* int32(sqrt(2) / 4 * 2^30) */
#define CORDIC_ARCSINCOS_ONE_Q30 1073741824	  /* int32(1 * 2^30) */

/**
 * CORDIC-based approximation of sine, cosine and complex exponential
 */
void cordic_approx(int32_t th_rad_fxp, int32_t a_idx, int32_t *sign, int32_t *b_yn, int32_t *xn,
		   int32_t *th_cdc_fxp)
{
	int32_t direction;
	int32_t abs_th;
	int32_t b_idx;
	int32_t xn_local = CORDIC_SINE_COS_LUT_Q29;
	int32_t yn_local = 0;
	int32_t xtmp = CORDIC_SINE_COS_LUT_Q29;
	int32_t ytmp = 0;
	int shift;

	/* Addition or subtraction by a multiple of pi/2 is done in the data type
	 * of the input. When the fraction length is 29, then the quantization error
	 * introduced by the addition or subtraction of pi/2 is done with 29 bits of
	 * precision.Input range of cordicsin must be in the range [-2*pi, 2*pi),
	 * a signed type with fractionLength = wordLength-4 will fit this range
	 * without overflow.Increase of fractionLength makes the addition or
	 * subtraction of a multiple of pi/2 more precise
	 */
	abs_th = (th_rad_fxp >= 0) ? th_rad_fxp : -th_rad_fxp;
	direction = (th_rad_fxp >= 0) ? 1 : -1;
	*sign = 1;
	if (abs_th > CORDIC_SINCOS_PIOVERTWO_Q28) {
		if (abs_th <= CORDIC_SINCOS_ONEANDHALFPI_Q28) {
			th_rad_fxp -= direction * CORDIC_SINCOS_PI_Q28;
			*sign = -1;
		} else {
			th_rad_fxp -= direction * CORDIC_SINCOS_TWOPI_Q28;
		}
	}

	th_rad_fxp <<= 2;

	/* Calculate the correct coefficient values from rotation angle.
	 * Find difference between the coefficients from the lookup table
	 * and those from the calculation
	 */
	for (b_idx = 0; b_idx < a_idx; b_idx++) {
		direction = (th_rad_fxp >= 0) ? 1 : -1;
		shift = b_idx + 1;
		th_rad_fxp -= direction * cordic_lookup[b_idx];
		xn_local -= direction * ytmp;
		yn_local += direction * xtmp;
		xtmp = xn_local >> shift;
		ytmp = yn_local >> shift;
	}

	/* Write back results once */
	*xn = xn_local;
	*b_yn = yn_local;
	*th_cdc_fxp = th_rad_fxp;
}
EXPORT_SYMBOL(cordic_approx);

/**
 * CORDIC-based approximation for inverse cosine
 * cosvalue is Q2.30, return value is angle in Q3.29 format
 */
int32_t is_scalar_cordic_acos(int32_t cosvalue, int numiters)
{
	int32_t xdshift;
	int32_t ydshift;
	int32_t yshift;
	int32_t xshift;
	int32_t x = 0;
	int32_t y = 0;
	int32_t z = 0;
	int32_t sign;
	int b_i;
	int j;

	/* Initialize the variables for the cordic iteration
	 * angles less than pi/4, we initialize (x,y) along the x-axis.
	 * angles greater than or equal to pi/4, we initialize (x,y)
	 * along the y-axis. This improves the accuracy of the algorithm
	 * near the edge of the domain of convergence
	 *
	 * Note: not pi/4 but sqrt(2)/4 is used as the threshold
	 */
	if (cosvalue < CORDIC_ARCSINCOS_SQRT2_DIV4_Q30) {
		y = CORDIC_ARCSINCOS_ONE_Q30;
		z = PI_DIV2_Q3_29;
	} else {
		x = CORDIC_ARCSINCOS_ONE_Q30;
	}

	/* DCORDIC(Double CORDIC) algorithm */
	/* Double iterations method consists in the fact that unlike the classical */
	/* CORDIC method,where the iteration step value changes EVERY time, i.e. on */
	/* each iteration, in the double iteration method, the iteration step value */
	/* is repeated twice and changes only through one iteration */
	for (b_i = 0; b_i < numiters; b_i++) {
		j = (b_i + 1) << 1;
		if (j >= 31)
			j = 31;

		xshift = x >> b_i;
		yshift = y >> b_i;
		xdshift = x >> j;
		ydshift = y >> j;
		/* Do nothing if x currently equals the target value. Allowed for
		 * double rotations algorithms, as it is equivalent to rotating by
		 * the same angle in opposite directions sequentially. Accounts for
		 * the scaling effect of CORDIC Pseudo-rotations as well.
		 */
		if (x == cosvalue) {
			x += xdshift;
			y += ydshift;
		} else {
			sign = (((x > cosvalue) && (y >= 0)) ||
				((x < cosvalue) && (y < 0))) ? 1 : -1;
			x = x - xdshift - sign * yshift;
			y = y - ydshift + sign * xshift;
			z += sign * cordic_ilookup[b_i];
		}
		cosvalue += cosvalue >> j;
	}
	if (z < 0)
		z = -z;

	return z;
}

/**
 * CORDIC-based approximation for inverse sine
 * sinvalue is Q2.30, return value is angle in Q2.30 format
 */
int32_t is_scalar_cordic_asin(int32_t sinvalue, int numiters)
{
	int32_t xdshift;
	int32_t ydshift;
	int32_t xshift;
	int32_t yshift;
	int32_t x = 0;
	int32_t y = 0;
	int32_t z = 0;
	int32_t sign;
	int b_i;
	int j;

	/* Initialize the variables for the cordic iteration
	 * angles less than pi/4, we initialize (x,y) along the x-axis.
	 * angles greater than or equal to pi/4, we initialize (x,y)
	 * along the y-axis. This improves the accuracy of the algorithm
	 * near the edge of the domain of convergence
	 *
	 * Note: Instead of pi/4, sqrt(2)/4 is used as the threshold
	 */
	if (sinvalue > CORDIC_ARCSINCOS_SQRT2_DIV4_Q30) {
		y = CORDIC_ARCSINCOS_ONE_Q30;
		z = PI_DIV2_Q3_29;
	} else {
		x = CORDIC_ARCSINCOS_ONE_Q30;
	}

	/* DCORDIC(Double CORDIC) algorithm */
	/* Double iterations method consists in the fact that unlike the classical */
	/* CORDIC method,where the iteration step value changes EVERY time, i.e. on */
	/* each iteration, in the double iteration method, the iteration step value */
	/* is repeated twice and changes only through one iteration */
	for (b_i = 0; b_i < numiters; b_i++) {
		j = (b_i + 1) << 1;
		if (j >= 31)
			j = 31;

		xshift = x >> b_i;
		yshift = y >> b_i;
		ydshift = y >> j;
		xdshift = x >> j;
		/* Do nothing if x currently equals the target value. Allowed for
		 * double rotations algorithms, as it is equivalent to rotating by
		 * the same angle in opposite directions sequentially. Accounts for
		 * the scaling effect of CORDIC Pseudo-rotations as well.
		 */
		if (y == sinvalue) {
			x += xdshift;
			y += ydshift;
		} else {
			sign = (((y >= sinvalue) && (x >= 0)) ||
				((y < sinvalue) && (x < 0))) ? 1 : -1;
			x = x - xdshift + sign * yshift;
			y = y - ydshift - sign * xshift;
			z -= sign * cordic_ilookup[b_i];
		}
		sinvalue += sinvalue >> j;
	}
	if (z < 0)
		z = -z;

	return z;
}

/**
 * cmpx_cexp() - CORDIC-based approximation of complex exponential e^(j*THETA)
 *
 * The sine and cosine values are in Q2.30 format from cordic_approx()function.
 */
void cmpx_cexp(int32_t sign, int32_t b_yn, int32_t xn, cordic_cfg type, struct cordic_cmpx *cexp)
{
	cexp->re = sign * xn;
	cexp->im = sign * b_yn;
	/*convert Q2.30 to Q1.15*/
	if (type == EN_16B_CORDIC_CEXP) {
		cexp->re = sat_int16(Q_SHIFT_RND((cexp->re), 30, 15));
		cexp->im = sat_int16(Q_SHIFT_RND((cexp->im), 30, 15));
	}
}
