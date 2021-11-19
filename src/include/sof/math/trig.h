/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
 *         Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 *         Shriram Shastry <malladi.sastry@linux.intel.com>
 */

#ifndef __SOF_MATH_TRIG_H__
#define __SOF_MATH_TRIG_H__

#include <stdint.h>

#define PI_DIV2_Q4_28 421657428
#define PI_DIV2_Q3_29 843314856
#define PI_Q4_28      843314857
#define PI_MUL2_Q4_28     1686629713
#define CORDIC_31B_TABLE_SIZE		31
#define CORDIC_15B_TABLE_SIZE		15
#define CORDIC_30B_ITABLE_SIZE		30
#define CORDIC_16B_ITABLE_SIZE		16

typedef enum {
	EN_32B_CORDIC_SINE,
	EN_32B_CORDIC_COSINE,
	EN_32B_CORDIC_CEXP,
	EN_16B_CORDIC_SINE,
	EN_16B_CORDIC_COSINE,
	EN_16B_CORDIC_CEXP,
} cordic_cfg;

struct cordic_cmpx {
	int32_t re;
	int32_t im;
};

void cordic_approx(int32_t th_rad_fxp, int32_t a_idx, int32_t *sign, int32_t *b_yn, int32_t *xn,
		   int32_t *th_cdc_fxp);
int32_t is_scalar_cordic_acos(int32_t realvalue, int16_t numiters);
int32_t is_scalar_cordic_asin(int32_t realvalue, int16_t numiters);
void cmpx_cexp(int32_t sign, int32_t b_yn, int32_t xn, cordic_cfg type, struct cordic_cmpx *cexp);
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
	int32_t th_cdc_fxp;
	int32_t sign;
	int32_t b_yn;
	int32_t xn;
	cordic_approx(th_rad_fxp, CORDIC_31B_TABLE_SIZE, &sign, &b_yn, &xn, &th_cdc_fxp);

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
 * | 32 | 28  |  1    | 32 | 31 |   1	| 4.28	 | 1.31   |
 * +------------------+-----------------+--------+--------+
 */
static inline int32_t cos_fixed_32b(int32_t th_rad_fxp)
{
	int32_t th_cdc_fxp;
	int32_t sign;
	int32_t b_yn;
	int32_t xn;
	cordic_approx(th_rad_fxp, CORDIC_31B_TABLE_SIZE, &sign, &b_yn, &xn, &th_cdc_fxp);

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
	int32_t th_cdc_fxp;
	int32_t sign;
	int32_t b_yn;
	int32_t xn;

	cordic_approx(th_rad_fxp, CORDIC_15B_TABLE_SIZE, &sign, &b_yn, &xn, &th_cdc_fxp);

	th_cdc_fxp = sign * b_yn;
	/*convert Q2.30 to Q1.15 format*/
	return sat_int16(Q_SHIFT_RND(th_cdc_fxp, 30, 15));
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
	int32_t th_cdc_fxp;
	int32_t sign;
	int32_t b_yn;
	int32_t xn;

	cordic_approx(th_rad_fxp, CORDIC_15B_TABLE_SIZE, &sign, &b_yn, &xn, &th_cdc_fxp);

	th_cdc_fxp = sign * xn;
	/*convert Q2.30 to Q1.15 format*/
	return sat_int16(Q_SHIFT_RND(th_cdc_fxp, 30, 15));
}

/**
 * CORDIC-based approximation of complex exponential e^(j*THETA).
 * computes COS(THETA) + j*SIN(THETA) using CORDIC algorithm
 * approximation and returns the complex result.
 * THETA values must be in the range [-2*pi, 2*pi). The cordic
 * exponential algorithm converges, when the angle is in the
 * range [-pi/2, pi/2).If an angle is outside of this range,
 * then a multiple of pi/2 is added or subtracted from the
 * angle until it is within the range [-pi/2,pi/2).Start
 * with the angle in the range [-2*pi, 2*pi) and output has
 * range in [-1.0 to 1.0]
 * Error (max = 0.000000015832484), THD+N  = -167.082852232808847
 * +------------------+-----------------+--------+------------+
 * | thRadFxp	      |cdccexpth        |thRadFxp|   cdccexpth|
 * +----+-----+-------+----+----+-------+--------+------------+
 * |WLen| FLen|Signbit|WLen|FLen|Signbit| Qformat| Qformat    |
 * +----+-----+-------+----+----+-------+--------+------------+
 * | 32 | 28  |  1    | 32 | 15 |   1	| 4.28	 | 2.30       |
 * +------------------+-----------------+--------+------------+
 */
static inline void cmpx_exp_32b(int32_t th_rad_fxp, struct cordic_cmpx *cexp)
{
	int32_t th_cdc_fxp;
	int32_t sign;
	int32_t b_yn;
	int32_t xn;

	cordic_approx(th_rad_fxp, CORDIC_31B_TABLE_SIZE, &sign, &b_yn, &xn, &th_cdc_fxp);
	cmpx_cexp(sign, b_yn, xn, EN_32B_CORDIC_CEXP, cexp);
	/* return the complex(re & im) result in Q2.30*/
}

/**
 * CORDIC-based approximation of complex exponential e^(j*THETA).
 * computes COS(THETA) + j*SIN(THETA) using CORDIC algorithm
 * approximation and returns the complex result.
 * THETA values must be in the range [-2*pi, 2*pi). The cordic
 * exponential algorithm converges, when the angle is in the
 * range [-pi/2, pi/2).If an angle is outside of this range,
 * then a multiple of pi/2 is added or subtracted from the
 * angle until it is within the range [-pi/2,pi/2).Start
 * with the angle in the range [-2*pi, 2*pi) and output has
 * range in [-1.0 to 1.0]
 * Error (max = 0.000060862861574), THD+N  = -89.049303454077403
 * +------------------+-----------------+--------+------------+
 * | thRadFxp	      |cdccexpth        |thRadFxp|   cdccexpth|
 * +----+-----+-------+----+----+-------+--------+------------+
 * |WLen| FLen|Signbit|WLen|FLen|Signbit| Qformat| Qformat    |
 * +----+-----+-------+----+----+-------+--------+------------+
 * | 32 | 28  |  1    | 32 | 15 |   1	| 4.28	 | 1.15       |
 * +------------------+-----------------+--------+------------+
 */
static inline void cmpx_exp_16b(int32_t th_rad_fxp, struct cordic_cmpx *cexp)
{
	int32_t th_cdc_fxp;
	int32_t sign;
	int32_t b_yn;
	int32_t xn;

	/* compute coeff from angles */
	cordic_approx(th_rad_fxp, CORDIC_15B_TABLE_SIZE, &sign, &b_yn, &xn, &th_cdc_fxp);
	cmpx_cexp(sign, b_yn, xn, EN_16B_CORDIC_CEXP, cexp);
	/* return the complex(re & im) result in Q1.15*/
}

/**
 * CORDIC-based approximation of inverse sine
 * inverse sine of cdc_asin_theta based on a CORDIC approximation.
 * asin(cdc_asin_th) inverse sine angle values in radian produces using the DCORDIC
 * (Double CORDIC) algorithm.
 * Inverse sine angle values in rad
 * Q2.30 cdc_asin_th, value in between range of [-1 to 1]
 * Q2.30 th_asin_fxp, output value range [-1.5707963258028 to 1.5707963258028]
 * LUT size set type 15
 * Error (max = 0.000000027939677), THD+N  = -157.454534077921551 (dBc)
 */
static inline int32_t asin_fixed_32b(int32_t cdc_asin_th)
{
	int32_t th_asin_fxp;

	if (cdc_asin_th >= 0)
		th_asin_fxp = is_scalar_cordic_asin(cdc_asin_th,
						    CORDIC_31B_TABLE_SIZE);
	else
		th_asin_fxp = -is_scalar_cordic_asin(-cdc_asin_th,
						     CORDIC_31B_TABLE_SIZE);

	return th_asin_fxp; /* Q2.30 */
}

/**
 * CORDIC-based approximation of inverse cosine
 * inverse cosine of cdc_acos_theta based on a CORDIC approximation
 * acos(cdc_acos_th) inverse cosine angle values in radian produces using the DCORDIC
 * (Double CORDIC) algorithm.
 * Q2.30 cdc_acos_th , input value range [-1 to 1]
 * Q3.29 th_acos_fxp, output value range [3.14159265346825 to 0]
 * LUT size set type 31
 * Error (max = 0.000000026077032), THD+N  = -157.948952635422842 (dBc)
 */
static inline int32_t acos_fixed_32b(int32_t cdc_acos_th)
{
	int32_t th_acos_fxp;

	if (cdc_acos_th >= 0)
		th_acos_fxp = is_scalar_cordic_acos(cdc_acos_th,
						    CORDIC_31B_TABLE_SIZE);
	else
		th_acos_fxp =
		PI_MUL2_Q4_28 - is_scalar_cordic_acos(-cdc_acos_th,
						      CORDIC_31B_TABLE_SIZE);

	return th_acos_fxp; /* Q3.29 */
}

/**
 * CORDIC-based approximation of inverse sine
 * inverse sine of cdc_asin_theta based on a CORDIC approximation.
 * asin(cdc_asin_th) inverse sine angle values in radian produces using the DCORDIC
 * (Double CORDIC) algorithm.
 * Inverse sine angle values in rad
 * Q2.30 cdc_asin_th, value in between range of [-1 to 1]
 * Q2.14 th_asin_fxp, output value range [-1.5707963258028 to 1.5707963258028]
 * LUT size set type 31
 * number of iteration 15
 * Error (max = 0.000059800222516), THD+N  = -89.824282520774048 (dBc)
 */
static inline int16_t asin_fixed_16b(int32_t cdc_asin_th)
{
	int32_t th_asin_fxp;

	if (cdc_asin_th >= 0)
		th_asin_fxp = is_scalar_cordic_asin(cdc_asin_th,
						    CORDIC_16B_ITABLE_SIZE);
	else
		th_asin_fxp = -is_scalar_cordic_asin(-cdc_asin_th,
						     CORDIC_16B_ITABLE_SIZE);
	/*convert Q2.30 to Q2.14 format*/
	return sat_int16(Q_SHIFT_RND(th_asin_fxp, 30, 14));
}

/**
 * CORDIC-based approximation of inverse cosine
 * inverse cosine of cdc_acos_theta based on a CORDIC approximation
 * acos(cdc_acos_th) inverse cosine angle values in radian produces using the DCORDIC
 * (Double CORDIC) algorithm.
 * Q2.30 cdc_acos_th , input value range [-1 to 1]
 * Q3.13 th_acos_fxp, output value range [3.14159265346825 to 0]
 * LUT size set type 31
 * number of iteration 15
 * Error (max = 0.000059799232976), THD+N  = -89.824298401466635 (dBc)
 */
static inline int16_t acos_fixed_16b(int32_t cdc_acos_th)
{
	int32_t th_acos_fxp;

	if (cdc_acos_th >= 0)
		th_acos_fxp = is_scalar_cordic_acos(cdc_acos_th,
						    CORDIC_16B_ITABLE_SIZE);
	else
		th_acos_fxp =
		PI_MUL2_Q4_28 - is_scalar_cordic_acos(-cdc_acos_th,
						      CORDIC_16B_ITABLE_SIZE);

	/*convert Q3.29 to Q3.13 format*/
	return sat_int16(Q_SHIFT_RND(th_acos_fxp, 29, 13));
}

#endif /* __SOF_MATH_TRIG_H__ */
