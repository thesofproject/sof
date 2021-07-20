// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
//         Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>
//         Shriram Shastry <malladi.sastry@linux.intel.com>

#include <sof/audio/format.h>
#include <sof/math/trig.h>
#include <stdint.h>

#ifdef CONFIG_CORDIC_TRIGONOMETRY_FIXED
/* Use a local definition to avoid adding a dependency on <math.h> */
#define _M_PI		3.14159265358979323846	/* pi */
/**
 * cordic_atan2_lookup_table = atan(2.^-(0:N-1)) N = 31/16
 * CORDIC Gain is cordic_gain = prod(sqrt(1 + 2.^(-2*(0:31/16-1))))
 * Inverse CORDIC Gain,inverse_cordic_gain = 1 / cordic_gain
 */
static const int32_t cordic_lookup[CORDIC_31B_TABLE_SIZE] = { 843314857, 497837829,
	263043837, 133525159, 67021687, 33543516, 16775851, 8388437, 4194283, 2097149,
	1048576, 524288, 262144, 131072, 65536, 32768, 16384, 8192, 4096, 2048, 1024,
	512, 256, 128, 64, 32, 16, 8, 4, 2, 1 };

/* 652032874 , deg = 69.586061*/
const int32_t cordic_sine_cos_lut_q29fl	 =  Q_CONVERT_FLOAT(1.214505869895220, 29);
/* 1686629713, deg = 90.000000	*/
const int32_t cordic_sine_cos_piovertwo_q30fl  = Q_CONVERT_FLOAT(_M_PI / 2, 30);
/* 421657428 , deg = 90.000000 */
const int32_t cord_sincos_piovertwo_q28fl  = Q_CONVERT_FLOAT(_M_PI / 2, 28);
/* 843314857,  deg = 90.000000	*/
const int32_t cord_sincos_piovertwo_q29fl  = Q_CONVERT_FLOAT(_M_PI / 2, 29);

/**
 * CORDIC-based approximation of sine and cosine
 */
void cordic_sin_cos(int32_t th_rad_fxp, cordic_cfg type, int32_t *sign, int32_t *b_yn, int32_t *xn,
		    int32_t *th_cdc_fxp)
{
	int32_t b_idx;
	int32_t xtmp;
	int32_t ytmp;
	int32_t a_idx = CORDIC_31B_TABLE_SIZE;
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

	if ((type == EN_16B_CORDIC_SINE) || (type == EN_16B_CORDIC_COSINE))
		a_idx = CORDIC_SIN_COS_15B_TABLE_SIZE;

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
	/* Q2.30 format */
	*th_cdc_fxp = th_rad_fxp;

}

#endif
