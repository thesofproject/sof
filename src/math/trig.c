// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
// Liam Girdwood <liam.r.girdwood@linux.intel.com>
// Keyon Jie <yang.jie@linux.intel.com>
#include <sof/audio/format.h>
#include <sof/math/trig.h>
#include <stdint.h>

/* Compute fixed point cordicsine with table lookup and interpolation*/
/*	 The cordic sine algorithm converges, when the angle is in the range*/
/*	 [-pi/2, pi/2).If an angle is outside of this range, then a multiple of	*/
/*	 pi/2 is added or subtracted from the angle until it is within the range*/
/*	 [-pi/2,pi/2).Start with the angle in the range [-2*pi, 2*pi) and output*/
/*	 has range in [-1.0 to 1.0]	*/
/*	+------------------+-----------------+--------+--------+*/
/*	| thRadFxp		   | cdcsinth		 |thRadFxp|cdcsinth|*/
/*	+----+-----+-------+----+----+-------+--------+--------+*/
/*	|WLen| FLen|Signbit|WLen|FLen|Signbit| Qformat| Qformat|*/
/*	+----+-----+-------+----+----+-------+--------+--------+*/
/*	| 32 | 28  |  1	   | 32	| 30 |	1	 | 4.28	  | 2.30   |*/
/*	+------------------+-----------------+--------+--------+*/

int32_t sin_fixed(int32_t th_rad_fxp)
{
	/*cordic_atan2_lookup_table = atan(2.^-(0:N-1)) N = 31	*/
	/*CORDIC Gain is cordic_gain = prod(sqrt(1 + 2.^(-2*(0:31-1)))) */
	/*Inverse CORDIC Gain,inverse_cordic_gain = 1 / cordic_gain	 */
	static const int32_t inpLUT[31] = { 843314857, 497837829, 263043837, 133525159,
	 67021687, 33543516, 16775851, 8388437, 4194283, 2097149, 1048576, 524288,
	 262144, 131072, 65536, 32768, 16384, 8192, 4096, 2048, 1024, 512, 256, 128,
	 64, 32, 16, 8, 4, 2, 1 };
	int32_t b_idx;
	int32_t b_yn;
	int32_t xn;
	int32_t xtmp;
	int32_t ytmp;
	int32_t cdc_sin_th;
	int32_t negate;
   /* 421657428 , deg = 90.000000 */
	const int32_t CORDICSINE_PIOVERTWO_Q28FL	= Q_CONVERT_FLOAT(1.570796325802803, 28);
   /* 843314857,  deg = 90.000000  */
	const int32_t CORDICSINE_PIOVERTWO_Q29FL	= Q_CONVERT_FLOAT(1.570796327665448, 29);
   /* 1686629713, deg = 90.000000  */
	const int32_t CORDICSINE_PIOVERTWO_Q30FL	= Q_CONVERT_FLOAT(1.570796326734126, 30);
   /* 652032874 , deg = 69.586061*/
	const int32_t CORDICSINE_LUT_Q29FL =	 Q_CONVERT_FLOAT(1.214505869895220, 29);
	/*Addition or subtraction by a multiple of pi/2 is done in the data type*/
	/*of the input. When the fraction length is 28, then the quantization error*/
	/*introduced by the addition or subtraction of pi/2 is done with 28 bits of*/
	/*precision.Input range of cordicsin must be in the range [-2*pi, 2*pi),*/
	/*a signed type with fractionLength = wordLength-4 will fit this range*/
	/*without overflow.Increase of fractionLength makes the addition or */
	/*subtraction of a multiple of pi/2 more precise */

	if (th_rad_fxp > CORDICSINE_PIOVERTWO_Q28FL) {
		if ((th_rad_fxp - CORDICSINE_PIOVERTWO_Q29FL) <= CORDICSINE_PIOVERTWO_Q28FL) {
			th_rad_fxp -= CORDICSINE_PIOVERTWO_Q29FL;
			negate = 1;
		} else {
			th_rad_fxp -= CORDICSINE_PIOVERTWO_Q30FL;
			negate = 0;
		}
	} else if (th_rad_fxp < -CORDICSINE_PIOVERTWO_Q28FL) {
		if ((th_rad_fxp + CORDICSINE_PIOVERTWO_Q29FL) >= -CORDICSINE_PIOVERTWO_Q28FL) {
			th_rad_fxp += CORDICSINE_PIOVERTWO_Q29FL;
			negate = 1;
		} else {
			th_rad_fxp += CORDICSINE_PIOVERTWO_Q30FL;
			negate = 0;
		}
	} else {
		negate = 0;
	}
	th_rad_fxp *= 4;
	b_yn = 0;
	xn = CORDICSINE_LUT_Q29FL;
	xtmp = CORDICSINE_LUT_Q29FL;
	ytmp = 0;
	for (b_idx = 0; b_idx < 31; b_idx++) {
		if (th_rad_fxp < 0) {
			th_rad_fxp += inpLUT[b_idx];
			xn += ytmp;
			b_yn -= xtmp;
		} else {
			th_rad_fxp -= inpLUT[b_idx];
			xn -= ytmp;
			b_yn += xtmp;
		}
	xtmp = ARITHMETIC_SHIFT_RIGHT(xn, (uint32_t)((int32_t)(b_idx + 1)));
	ytmp = ARITHMETIC_SHIFT_RIGHT(b_yn, (uint32_t)((int32_t)(b_idx + 1)));
	}

	if (negate)
		cdc_sin_th = -b_yn;
	else
		cdc_sin_th = b_yn;

	return (int32_t)cdc_sin_th;
}
