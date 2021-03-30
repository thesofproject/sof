/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
 *         Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 */

#ifndef __SOF_MATH_TRIG_H__
#define __SOF_MATH_TRIG_H__

#include <stdint.h>

#define PI_DIV2_Q4_28 421657428
#define PI_Q4_28      843314857
#define PI_MUL2_Q4_28     1686629713

int32_t sin_fixed(int32_t th_rad_fxp); /* Input is Q4.28, output is Q1.31 */

#endif /* __SOF_MATH_TRIG_H__ */
