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
#include <sof/audio/drc/drc_plat_conf.h>

#if DRC_HIFI3

#include <xtensa/tie/xt_hifi3.h>

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

#endif /* DRC_HIFI3 */

int32_t drc_lin2db_fixed(int32_t linear); /* Input:Q6.26 Output:Q11.21 */
int32_t drc_log_fixed(int32_t x); /* Input:Q6.26 Output:Q6.26 */
int32_t drc_sin_fixed(int32_t x); /* Input:Q2.30 Output:Q1.31 */
int32_t drc_asin_fixed(int32_t x); /* Input:Q2.30 Output:Q2.30 */
int32_t drc_pow_fixed(int32_t x, int32_t y); /* Input:Q6.26, Q2.30 Output:Q12.20 */
int32_t drc_inv_fixed(int32_t x, int32_t precision_x, int32_t precision_y);

#endif //  __SOF_AUDIO_DRC_DRC_MATH_H__
