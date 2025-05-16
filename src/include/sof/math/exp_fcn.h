/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022-2025 Intel Corporation.
 *
 * Author: Shriram Shastry <malladi.sastry@linux.intel.com>
 *         Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
 *	   Pasca, Bogdan <bogdan.pasca@intel.com>
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

/* Q5.27 int32(round(log((2^31 - 1)/2^20) * 2^27)) */
#define SOFM_EXP_FIXED_INPUT_MAX	1023359037

/* Q8.24 int32(round((log((2^31 - 1)/2^20) * 20 / log(10)) * 2^24)) */
#define SOFM_DB2LIN_INPUT_MAX		1111097957

/**
 * Calculates exponent function exp(x) = e^x with accurate and efficient technique that
 * includes range reduction operations, approximation with Taylor series, and reconstruction
 * operations to compensate the range reductions.
 * @param	x	The input argument as Q4.28 from -8 to +8
 * @return		The calculated e^x value as Q13.19 from 3.3546e-04 to 2981.0
 */
int32_t sofm_exp_approx(int32_t x);

/**
 * Calculated exponent function exp(x) = e^x by using sofm_exp_approx(). The input range for
 * large arguments is internally reduced to -8 to +8 with rule exp(x) = exp(x/2) * exp(x/2)
 * and reconstructed back. This function is essentially a wrapper for compatibility with
 * existing usage of exp() function and Q-format choice. Note that the return value saturates
 * to INT32_MAX with input arguments larger than 7.6246.
 * @param	x	The input argument as Q5.27 from -16 to +16
 * @return		The calculated e^x value as Q12.20
 */
int32_t sofm_exp_fixed(int32_t x);


/**
 * Converts a decibels value to liner amplitude lin = 10^(db/20) value with optimized
 * equation exp(db * log(10)/20). Note that due to range limitation of sofm_exp_fixed()
 * the output saturates to maximum with about +66 dB input.
 * @param	db	Decibels value in Q8.24 format, from -128 to +66.226
 * @return		Linear value in Q12.20 format, from 3.9811e-07 to 2048
 */
int32_t sofm_db2lin_fixed(int32_t db);

#endif /* __SOFM_EXP_FCN_H__ */
