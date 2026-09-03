/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2020 Google LLC. All rights reserved.
 *
 * Author: Pin-chih Lin <johnylin@google.com>
 */
#ifndef __SOF_AUDIO_DRC_DRC_MATH_H__
#define __SOF_AUDIO_DRC_DRC_MATH_H__

#include <stddef.h>
#include <stdint.h>
#include <sof/audio/format.h>
#include <sof/math/numbers.h>
#include <sof/math/lut_trig.h>
#include <sof/math/trig.h>
#include <sof/common.h>

/* Unmark this define to use cordic arc sine implementation. */
/* #define DRC_USE_CORDIC_ASIN */

#if SOF_USE_MIN_HIFI(3, DRC)

#include <xtensa/tie/xt_hifi3.h>

#define PI_OVER_TWO_Q30 1686629713 /* Q_CONVERT_FLOAT(1.57079632679489661923, 30); pi/2 */
#define TWO_OVER_PI_Q30 683565248 /* Q_CONVERT_FLOAT(0.63661977236758134, 30); 2/pi */

/*
 * A substitutive function of Q_MULTSR_32X32(a, b, qa, qb, qy) in HIFI manners.
 *
 * In AE_MULF32R, it takes the multiplication of 1.31 and 1.31 to 17.47, which means it does
 * right-shift 15 bits. Let's consider a is .qa, b is .qb, then tmp will be .(qa+qb-15)
 * In AE_ROUND32F48SSYM, it rounds 17.47 to 1.31, equally right-shifts 16 bits. Let's consider the
 * output is .qy, then tmp here needs to be .(qy+16)
 * If we set the left-shift bit from the former tmp to the latter tmp as "lshift", then:
 *     (qa+qb-15) + lshift = (qy+16)
 *     lshift = qy-qa-qb+31
 */
static inline int32_t drc_mult_lshift(int32_t a, int32_t b, int32_t lshift)
{
	ae_f64 tmp;
	ae_f32 y;

	tmp = AE_MULF32R_LL(a, b);
	tmp = AE_SLAA64S(tmp, lshift);
	y = AE_ROUND32F48SSYM(tmp);
	return y;
}

static inline int32_t drc_get_lshift(int32_t qa, int32_t qb, int32_t qy)
{
	return qy - qa - qb + 31;
}

/*
 * Input is Q2.30: (-2.0, 2.0)
 * Output range: (-1.0, 1.0); regulated to Q1.31: (-1.0, 1.0)
 */
static inline int32_t drc_sin_fixed(int32_t x)
{
	const int32_t lshift = drc_get_lshift(30, 30, 28);
	int32_t denorm_x = drc_mult_lshift(x, PI_OVER_TWO_Q30, lshift);

	return sofm_lut_sin_fixed_16b(denorm_x) << 16;
}

#ifdef DRC_USE_CORDIC_ASIN
/*
 * Input is Q2.30; valid range: [-1.0, 1.0]
 * Output range: [-1.0, 1.0]; regulated to Q2.30: (-2.0, 2.0)
 */
static inline int32_t drc_asin_fixed(int32_t x)
{
	const int32_t lshift = drc_get_lshift(30, 30, 30);
	int32_t asin_val = asin_fixed_16b(x); /* Q2.14, [-pi/2, pi/2] */

	return drc_mult_lshift(asin_val << 16, TWO_OVER_PI_Q30, lshift);
}
#endif /* DRC_USE_CORDIC_ASIN */

#else

/*
 * Input is Q2.30: (-2.0, 2.0)
 * Output range: (-1.0, 1.0); regulated to Q1.31: (-1.0, 1.0)
 */
static inline int32_t drc_sin_fixed(int32_t x)
{
	const int32_t PI_OVER_TWO = Q_CONVERT_FLOAT(1.57079632679489661923, 30);

	return sofm_lut_sin_fixed_16b(Q_MULTSR_32X32((int64_t)x, PI_OVER_TWO, 30, 30, 28)) << 16;
}

#ifdef DRC_USE_CORDIC_ASIN
/*
 * Input is Q2.30; valid range: [-1.0, 1.0]
 * Output range: [-1.0, 1.0]; regulated to Q2.30: (-2.0, 2.0)
 */
static inline int32_t drc_asin_fixed(int32_t x)
{
	const int32_t TWO_OVER_PI = Q_CONVERT_FLOAT(0.63661977236758134, 30); /* 2/pi */
	int32_t asin_val = asin_fixed_16b(x); /* Q2.14, [-pi/2, pi/2] */

	return Q_MULTSR_32X32((int64_t)asin_val, TWO_OVER_PI, 14, 30, 30);
}
#endif /* DRC_USE_CORDIC_ASIN */

#endif /* DRC_HIFI_3/4 */

int32_t drc_lin2db_fixed(int32_t linear); /* Input:Q6.26 Output:Q11.21 */
int32_t drc_log_fixed(int32_t x); /* Input:Q6.26 Output:Q6.26 */
int32_t drc_pow_fixed(int32_t x, int32_t y); /* Input:Q6.26, Q2.30 Output:Q12.20 */
int32_t drc_inv_fixed(int32_t x, int32_t precision_x, int32_t precision_y);

#ifndef DRC_USE_CORDIC_ASIN
int32_t drc_asin_fixed(int32_t x); /* Input:Q2.30 Output:Q2.30 */
#endif /* !DRC_USE_CORDIC_ASIN */

#endif //  __SOF_AUDIO_DRC_DRC_MATH_H__
