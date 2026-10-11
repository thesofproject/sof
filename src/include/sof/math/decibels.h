
/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 *
 * Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
 */

/**
 * @file sof/math/decibels.h
 * @brief Fixed-point exponential and decibel-to-linear conversion functions.
 * @ingroup dsp_math_api
 */

#ifndef __SOF_MATH_DECIBELS_H__
#define __SOF_MATH_DECIBELS_H__

#include <stdint.h>

#define EXP_FIXED_INPUT_QY 27
#define EXP_FIXED_OUTPUT_QY 20
#define DB2LIN_FIXED_INPUT_QY 24
#define DB2LIN_FIXED_OUTPUT_QY 20

/**
 * @brief Compute fixed-point exponential e^x.
 * @param x Input argument in Q5.27 format.
 * @return Result e^x in Q12.20 format.
 */
int32_t exp_fixed(int32_t x);

/**
 * @brief Convert decibels to linear amplitude scale 10^(x/20).
 * @param x Input decibel value in Q8.24 format.
 * @return Linear gain factor in Q12.20 format.
 */
int32_t db2lin_fixed(int32_t x);

#endif /* __SOF_MATH_DECIBELS_H__ */
