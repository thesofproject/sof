/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016-2023 Intel Corporation. All rights reserved.
 *
 * Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
 */

#ifndef __SOF_MATH_LUT_TRIG_H__
#define __SOF_MATH_LUT_TRIG_H__

#include <stdint.h>

int16_t sofm_lut_sin_fixed_16b(int32_t w); /* Input is Q4.28, output is Q1.15 */

#endif /* __SOF_MATH_LUT_TRIG_H__ */
