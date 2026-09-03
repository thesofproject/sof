/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2021-2026 Intel Corporation.
 *
 * Author: Shriram Shastry <malladi.sastry@linux.intel.com>
 *
 */

#ifndef __SOF_MATH_SQRT_H__
#define __SOF_MATH_SQRT_H__

#include <stdint.h>

/**
 * sofm_sqrt_int16() - Calculate 16-bit fractional square root function.
 * @param u Input value in Q4.12 format, from 0 to 16.0.
 * @return Calculated square root of n in Q4.12 format, from 0 to 4.0.
 */
uint16_t sofm_sqrt_int16(uint16_t u);

/**
 * sofm_sqrt_int32() - Calculate 32-bit fractional square root function.
 * @param n Input value in Q2.30 format, from 0 to 2.0.
 * @return Calculated square root of n in Q2.30 format.
 *
 * The input range of square root function is matched with Q1.31
 * complex numbers range where the magnitude squared can be to 2.0.
 * The function returns zero for non-positive input values.
 */
int32_t sofm_sqrt_int32(int32_t n);

#endif /* __SOF_MATH_SQRT_H__ */
