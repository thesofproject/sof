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

/* Use a local definition to avoid adding a dependency on <math.h> */
#define _M_PI		3.14159265358979323846	/* pi */

/* 652032874 , deg = 69.586061*/
const int32_t cordic_sine_cos_lut_q29fl	 =  Q_CONVERT_FLOAT(1.214505869895220, 29);
/* 1686629713, deg = 90.000000	*/
const int32_t cordic_sine_cos_piovertwo_q30fl  = Q_CONVERT_FLOAT(_M_PI / 2, 30);
/* 421657428 , deg = 90.000000 */
const int32_t cord_sincos_piovertwo_q28fl  = Q_CONVERT_FLOAT(_M_PI / 2, 28);
/* 843314857,  deg = 90.000000	*/
const int32_t cord_sincos_piovertwo_q29fl  = Q_CONVERT_FLOAT(_M_PI / 2, 29);
/* arc trignometry constant*/
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
/* 379625062,  deg = 81.0284683480568475 or round(1.4142135605216026*2^28) */
const int32_t cord_arcsincos_q28fl  = Q_CONVERT_FLOAT(1.4142135605216026 / 2, 28);
/* 1073741824, deg = 57.2957795130823229 or round(1*2^30)*/
const int32_t cord_arcsincos_q30fl  = Q_CONVERT_FLOAT(1.0000000000000000, 30);
/**
 * CORDIC-based approximation of sine, cosine and complex exponential
 */
void cordic_approx(int32_t th_rad_fxp, int32_t a_idx, int32_t *sign, int32_t *b_yn, int32_t *xn,
		   int32_t *th_cdc_fxp)
{
	int32_t b_idx;
	int32_t xtmp;
	int32_t ytmp;
	*sign = 1;
	/* Addition or subtraction by a multiple of pi/2 is done in the data type
	 * of the input. When the fraction length is 29, then the quantization error
	 * introduced by the addition or subtraction of pi/2 is done with 29 bits of
	 * precision.Input range of cordicsin must be in the range [-2*pi, 2*pi),
	 * a signed type with fractionLength = wordLength-4 will fit this range
	 * without overflow.Increase of fractionLength makes the addition or
	 * subtraction of a multiple of pi/2 more precise
	 */
	if (th_rad_fxp > cord_sincos_piovertwo_q28fl) {
		if ((th_rad_fxp - cord_sincos_piovertwo_q29fl) <= cord_sincos_piovertwo_q28fl) {
			th_rad_fxp -= cord_sincos_piovertwo_q29fl;
			*sign  = -1;
		} else {
			th_rad_fxp -= cordic_sine_cos_piovertwo_q30fl;
		}
	} else if (th_rad_fxp < -cord_sincos_piovertwo_q28fl) {
		if ((th_rad_fxp + cord_sincos_piovertwo_q29fl) >= -cord_sincos_piovertwo_q28fl) {
			th_rad_fxp += cord_sincos_piovertwo_q29fl;
			*sign  = -1;
		} else {
			th_rad_fxp += cordic_sine_cos_piovertwo_q30fl;
		}
	}

	th_rad_fxp <<= 2;
	*b_yn = 0;
	*xn = cordic_sine_cos_lut_q29fl;
	xtmp = cordic_sine_cos_lut_q29fl;
	ytmp = 0;

	/* Calculate the correct coefficient values from rotation angle.
	 * Find difference between the coefficients from the lookup table
	 * and those from the calculation
	 */
	for (b_idx = 0; b_idx < a_idx; b_idx++) {
		if (th_rad_fxp < 0) {
			th_rad_fxp += cordic_lookup[b_idx];
			*xn += ytmp;
			*b_yn -= xtmp;
		} else {
			th_rad_fxp -= cordic_lookup[b_idx];
			*xn -= ytmp;
			*b_yn += xtmp;
		}
		xtmp = *xn >> (b_idx + 1);
		ytmp = *b_yn >> (b_idx + 1);
	}
	/* Q2.30 format -sine, cosine*/
	*th_cdc_fxp = th_rad_fxp;
}
EXPORT_SYMBOL(cordic_approx);

/**
 * CORDIC-based approximation for inverse cosine
 * Arguments	: int32_t cosvalue
 *		  int16_t numiters
 * Return Type	: int32_t
 */
int32_t is_scalar_cordic_acos(int32_t cosvalue, int16_t numiters)
{
	int32_t xdshift;
	int32_t ydshift;
	int32_t yshift;
	int32_t xshift;
	int32_t x = 0;
	int32_t y = 0;
	int32_t z = 0;
	int32_t sign;
	int32_t b_i;
	int i;
	int j;
	int k;

	/* Initialize the variables for the cordic iteration
	 * angles less than pi/4, we initialize (x,y) along the x-axis.
	 * angles greater than or equal to pi/4, we initialize (x,y)
	 * along the y-axis. This improves the accuracy of the algorithm
	 * near the edge of the domain of convergence
	 */
	if ((cosvalue >> 1) < cord_arcsincos_q28fl) {
		x = 0;
		y = cord_arcsincos_q30fl;
		z = PI_DIV2_Q3_29;
	} else {
		x = cord_arcsincos_q30fl;
		y = 0;
		z = 0;
	}

	/* DCORDIC(Double CORDIC) algorithm */
	/* Double iterations method consists in the fact that unlike the classical */
	/* CORDIC method,where the iteration step value changes EVERY time, i.e. on */
	/* each iteration, in the double iteration method, the iteration step value */
	/* is repeated twice and changes only through one iteration */
	i = numiters - 1;
	for (b_i = 0; b_i < i; b_i++) {
		j = (b_i + 1) << 1;
		if (j >= 31)
			j = 31;

		if (b_i < 31)
			k = b_i;
		else
			k = 31;

		xshift = x >> k;
		xdshift = x >> j;
		yshift = y >> k;
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
 * Arguments	: int32_t sinvalue
 *		  int16_t numiters
 * Return Type	: int32_t
 */
int32_t is_scalar_cordic_asin(int32_t sinvalue, int16_t numiters)
{
	int32_t xdshift;
	int32_t ydshift;
	int32_t xshift;
	int32_t yshift;
	int32_t x = 0;
	int32_t y = 0;
	int32_t z = 0;
	int32_t sign;
	int32_t b_i;
	int i;
	int j;
	int k;

	/* Initialize the variables for the cordic iteration
	 * angles less than pi/4, we initialize (x,y) along the x-axis.
	 * angles greater than or equal to pi/4, we initialize (x,y)
	 * along the y-axis. This improves the accuracy of the algorithm
	 * near the edge of the domain of convergence
	 */
	if ((sinvalue >> 1) > cord_arcsincos_q28fl) {
		x = 0;
		y = cord_arcsincos_q30fl;
		z = PI_DIV2_Q3_29;
	} else {
		x = cord_arcsincos_q30fl;
		y = 0;
		z = 0;
	}

	/* DCORDIC(Double CORDIC) algorithm */
	/* Double iterations method consists in the fact that unlike the classical */
	/* CORDIC method,where the iteration step value changes EVERY time, i.e. on */
	/* each iteration, in the double iteration method, the iteration step value */
	/* is repeated twice and changes only through one iteration */
	i = numiters - 1;
	for (b_i = 0; b_i < i; b_i++) {
		j = (b_i + 1) << 1;
		if (j >= 31)
			j = 31;

		if (b_i < 31)
			k = b_i;
		else
			k = 31;

		xshift = x >> k;
		xdshift = x >> j;
		yshift = y >> k;
		ydshift = y >> j;
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
 * approximated complex result
 * Arguments	: int32_t sign
 *		  int32_t b_yn
 *		  int32_t xn
 *		  enum    type
 *		  struct  cordic_cmpx
 * Return Type	: none
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
