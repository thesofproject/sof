/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 *
 * Author: Shriram Shastry <malladi.sastry@linux.intel.com>
 *
 */
#ifndef __SOF_MATH_BASE_EXPONENTIAL_H__
#define __SOF_MATH_BASE_EXPONENTIAL_H__

#define BIT_MASK_Q63P1 4611686018427387904LL
#define BIT_MASK_LOW_Q27P5 134217728ULL
#define CONVERG_ERROR 28823037607936LL
#define QUOTIENT_SCALE 17179869184LL
#define TERMS_Q23P9 8388608LL
#define NUM_Q14P18 16384LL

/* Include Files */
#include <stdint.h>

/* Function Declarations */
int32_t exp_int32(int32_t x);

#endif
