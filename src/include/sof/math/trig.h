/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
 *         Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 */

#ifndef __SOF_MATH_TRIG_H__
#define __SOF_MATH_TRIG_H__

#include <stdint.h>

#ifndef UNIT_CORDIC_TEST
#define CONFIG_CORDIC_TRIGONOMETRY_FIXED
#endif


#define PI_DIV2_Q4_28 421657428
#define PI_Q4_28      843314857
#define PI_MUL2_Q4_28     1686629713
#define CORDIC_31B_TABLE_SIZE		31
#define CORDIC_SIN_COS_15B_TABLE_SIZE	15

typedef enum {
	EN_32B_CORDIC_SINE,
	EN_32B_CORDIC_COSINE,
	EN_16B_CORDIC_SINE,
	EN_16B_CORDIC_COSINE,
} cordic_cfg;

void cordic_sin_cos(int32_t th_rad_fxp, cordic_cfg type, int32_t *sign, int32_t *b_yn, int32_t *xn,
		    int32_t *th_cdc_fxp);
/* Input is Q4.28, output is Q1.31 */
/**
 * Compute fixed point cordicsine with table lookup and interpolation
 * The cordic sine algorithm converges, when the angle is in the range
 * [-pi/2, pi/2).If an angle is outside of this range, then a multiple of
 * pi/2 is added or subtracted from the angle until it is within the range
 * [-pi/2,pi/2).Start with the angle in the range [-2*pi, 2*pi) and output
 * has range in [-1.0 to 1.0]
 * +------------------+-----------------+--------+--------+
 * | thRadFxp	      | cdcsinth	|thRadFxp|cdcsinth|
 * +----+-----+-------+----+----+-------+--------+--------+
 * |WLen| FLen|Signbit|WLen|FLen|Signbit| Qformat| Qformat|
 * +----+-----+-------+----+----+-------+--------+--------+
 * | 32 | 28  |  1    | 32 | 31 |   1	| 4.28	  | 1.31  |
 * +------------------+-----------------+--------+--------+
 */
static inline int32_t sin_fixed_32b(int32_t th_rad_fxp)
{
	int32_t sign;
	int32_t b_yn;
	int32_t xn;
	int32_t th_cdc_fxp;
	cordic_cfg type = EN_32B_CORDIC_SINE;
	cordic_sin_cos(th_rad_fxp, type, &sign, &b_yn, &xn, &th_cdc_fxp);

	th_cdc_fxp = sign * b_yn;
	/*convert Q2.30 to Q1.31 format*/
	return sat_int32(Q_SHIFT_LEFT((int64_t)th_cdc_fxp, 30, 31));
}
/**
 * Compute fixed point cordicsine with table lookup and interpolation
 * The cordic cosine algorithm converges, when the angle is in the range
 * [-pi/2, pi/2).If an angle is outside of this range, then a multiple of
 * pi/2 is added or subtracted from the angle until it is within the range
 * [-pi/2,pi/2).Start with the angle in the range [-2*pi, 2*pi) and output
 * has range in [-1.0 to 1.0]
 * +------------------+-----------------+--------+--------+
 * | thRadFxp	      | cdccosth	|thRadFxp|cdccosth|
 * +----+-----+-------+----+----+-------+--------+--------+
 * |WLen| FLen|Signbit|WLen|FLen|Signbit| Qformat| Qformat|
 * +----+-----+-------+----+----+-------+--------+--------+
 * | 32 | 28  |  1    | 32 | 31 |   1	| 4.28	  | 1.31  |
 * +------------------+-----------------+--------+--------+
 */
static inline int32_t cos_fixed_32b(int32_t th_rad_fxp)
{
	int32_t sign;
	int32_t b_yn;
	int32_t xn;
	int32_t th_cdc_fxp;
	cordic_cfg type = EN_32B_CORDIC_COSINE;
	cordic_sin_cos(th_rad_fxp, type, &sign, &b_yn, &xn, &th_cdc_fxp);

	th_cdc_fxp = sign * xn;
	/*convert Q2.30 to Q1.31 format*/
	return sat_int32(Q_SHIFT_LEFT((int64_t)th_cdc_fxp, 30, 31));
}

/* Input is Q4.28, output is Q1.15 */
/**
 * Compute fixed point cordic sine with table lookup and interpolation
 * The cordic sine algorithm converges, when the angle is in the range
 * [-pi/2, pi/2).If an angle is outside of this range, then a multiple of
 * pi/2 is added or subtracted from the angle until it is within the range
 * [-pi/2,pi/2).Start with the angle in the range [-2*pi, 2*pi) and output
 * has range in [-1.0 to 1.0]
 * +------------------+-----------------+--------+------------+
 * | thRadFxp	      | cdcsinth	|thRadFxp|    cdcsinth|
 * +----+-----+-------+----+----+-------+--------+------------+
 * |WLen| FLen|Signbit|WLen|FLen|Signbit| Qformat| Qformat    |
 * +----+-----+-------+----+----+-------+--------+------------+
 * | 32 | 28  |  1    | 32 | 15 |   1	| 4.28	 | 1.15       |
 * +------------------+-----------------+--------+------------+
 */
static inline int16_t sin_fixed_16b(int32_t th_rad_fxp)
{
	int32_t sign;
	int32_t b_yn;
	int32_t xn;
	int32_t th_cdc_fxp;
	cordic_cfg type = EN_16B_CORDIC_SINE;
	/* compute coeff from angles*/
	cordic_sin_cos(th_rad_fxp, type, &sign, &b_yn, &xn, &th_cdc_fxp);
	th_cdc_fxp = sign * b_yn;
	/*convert Q1.31 to Q1.15 format*/
	return sat_int16(Q_SHIFT_RND((sat_int32(Q_SHIFT_LEFT((int64_t)th_cdc_fxp, 30, 31))),
	31, 15));
}
/**
 * Compute fixed point cordic cosine with table lookup and interpolation
 * The cordic cos algorithm converges, when the angle is in the range
 * [-pi/2, pi/2).If an angle is outside of this range, then a multiple of
 * pi/2 is added or subtracted from the angle until it is within the range
 * [-pi/2,pi/2).Start with the angle in the range [-2*pi, 2*pi) and output
 * has range in [-1.0 to 1.0]
 * +------------------+-----------------+--------+------------+
 * | thRadFxp	      | cdccosth	|thRadFxp|    cdccosth|
 * +----+-----+-------+----+----+-------+--------+------------+
 * |WLen| FLen|Signbit|WLen|FLen|Signbit| Qformat| Qformat    |
 * +----+-----+-------+----+----+-------+--------+------------+
 * | 32 | 28  |  1    | 32 | 15 |   1	| 4.28	 | 1.15       |
 * +------------------+-----------------+--------+------------+
 */
static inline int16_t cos_fixed_16b(int32_t th_rad_fxp)
{
	int32_t sign;
	int32_t b_yn;
	int32_t xn;
	int32_t th_cdc_fxp;
	cordic_cfg type = EN_16B_CORDIC_COSINE;
	/* compute coeff from angles*/
	cordic_sin_cos(th_rad_fxp, type, &sign, &b_yn, &xn, &th_cdc_fxp);
	th_cdc_fxp = sign * xn;
	/*convert Q1.31 to Q1.15 format*/
	return sat_int16(Q_SHIFT_RND((sat_int32(Q_SHIFT_LEFT((int64_t)th_cdc_fxp, 30, 31))),
	31, 15));
}

#endif /* __SOF_MATH_TRIG_H__ */
