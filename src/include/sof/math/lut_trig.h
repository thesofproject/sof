/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016-2023 Intel Corporation. All rights reserved.
 *
 * Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
 */

/**
 * @file sof/math/lut_trig.h
 * @brief Fast lookup-table trigonometric sine approximation routines.
 * @ingroup dsp_math_api
 */

#ifndef __SOF_MATH_LUT_TRIG_H__
#define __SOF_MATH_LUT_TRIG_H__

#include <stdint.h>

/**
 * @brief Fast lookup-table 16-bit fixed-point sine approximation.
 * @param w Input phase angle in radians formatted as Q4.28.
 * @return Sine approximation in Q1.15 format.
 */
int16_t sofm_lut_sin_fixed_16b(int32_t w);

#endif /* __SOF_MATH_LUT_TRIG_H__ */
