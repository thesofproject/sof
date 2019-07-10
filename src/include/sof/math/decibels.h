
/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 *
 * Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
 */

#ifndef __SOF_MATH_DECIBELS_H__
#define __SOF_MATH_DECIBELS_H__

#include <stdint.h>

#define EXP_FIXED_INPUT_QY 27
#define EXP_FIXED_OUTPUT_QY 20
#define DB2LIN_FIXED_INPUT_QY 24
#define DB2LIN_FIXED_OUTPUT_QY 20

int32_t exp_fixed(int32_t x); /* Input is Q5.27, output is Q12.20 */
int32_t db2lin_fixed(int32_t x); /* Input is Q8.24, output is Q12.20 */

#endif /* __SOF_MATH_DECIBELS_H__ */
