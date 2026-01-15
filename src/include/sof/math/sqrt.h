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
#endif /* __SOF_MATH_SQRT_H__ */
