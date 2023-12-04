/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 *
 * Author: Shriram Shastry <malladi.sastry@linux.intel.com>
 *
 */
#ifndef __SOFM_EXP_FCN_H__
#define __SOFM_EXP_FCN_H__

#include <stdint.h>

#if defined(__XCC__)
/* HiFi */
#include <xtensa/config/core-isa.h>
#if XCHAL_HAVE_HIFI5 == 1
#define SOFM_EXPONENTIAL_HIFI5 1
#elif XCHAL_HAVE_HIFI4 == 1
#define SOFM_EXPONENTIAL_HIFI4 1
#elif XCHAL_HAVE_HIFI3 == 1
#define SOFM_EXPONENTIAL_HIFI3 1
#endif
#else
/* !XCC */
#define EXPONENTIAL_GENERIC 1

#endif

/* TODO: Is there a MCPS difference */
#define USING_QCONVERT	1

#if USING_QCONVERT

#include <sof/audio/format.h>
#define SOFM_EXP_FIXED_INPUT_MIN	Q_CONVERT_FLOAT(-11.5, 27)		/* Q5.27 */
#define SOFM_EXP_FIXED_INPUT_MAX	Q_CONVERT_FLOAT(7.6245, 27)		/* Q5.27 */
#define SOFM_EXP_TWO_Q27		Q_CONVERT_FLOAT(2.0, 27)		/* Q5.27 */
#define SOFM_EXP_MINUS_TWO_Q27		Q_CONVERT_FLOAT(-2.0, 27)		/* Q5.27 */
#define SOFM_EXP_ONE_Q20		Q_CONVERT_FLOAT(1.0, 20)		/* Q12.20 */
#define SOFM_EXP_MINUS_100_Q24		Q_CONVERT_FLOAT(-100.0, 24)		/* Q8.24 */
#define SOFM_EXP_LOG10_DIV20_Q27	Q_CONVERT_FLOAT(0.1151292546, 27)	/* Q5.27 */

#else

#define SOFM_EXP_FIXED_INPUT_MIN	-1543503872	/* Q_CONVERT_FLOAT(-11.5, 27) */
#define SOFM_EXP_FIXED_INPUT_MAX	 1023343067	/* Q_CONVERT_FLOAT(7.6245, 27) */
#define SOFM_EXP_TWO_Q27		 268435456	/* Q_CONVERT_FLOAT(2.0, 27) */
#define SOFM_EXP_MINUS_TWO_Q27		-268435456	/* Q_CONVERT_FLOAT(-2.0, 27) */
#define SOFM_EXP_ONE_Q20		 1048576	/* Q_CONVERT_FLOAT(1.0, 20) */
#define SOFM_EXP_MINUS_100_Q24		-1677721600	/* Q_CONVERT_FLOAT(-100.0, 24) */
#define SOFM_EXP_LOG10_DIV20_Q27	 15452387	/* Q_CONVERT_FLOAT(0.1151292546, 27) */

#endif

#define SOFM_EXP_BIT_MASK_LOW_Q27P5 0x0000000008000000
#define SOFM_EXP_BIT_MASK_Q62P2 0x4000000000000000LL
#define SOFM_EXP_QUOTIENT_SCALE 0x40000000
#define SOFM_EXP_TERMS_Q23P9 0x800000
#define SOFM_EXP_LSHIFT_BITS 0x2000

int32_t sofm_exp_int32(int32_t x);

#endif /* __SOFM_EXP_FCN_H__ */
