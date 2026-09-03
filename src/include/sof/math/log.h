/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2021 Intel Corporation. All rights reserved.
 *
 * Author: Shriram Shastry <malladi.sastry@linux.intel.com>
 *
 */

#ifndef __SOF_MATH_BASE_LOGARITHM_H__
#define __SOF_MATH_BASE_LOGARITHM_H__

#include <stdint.h>

#define ONE_OVER_LOG2_10	2647887844335ULL	/* 1/log2(10),Q1.43 */
#define ONE_OVER_LOG2_E		6096987078286ULL	/* 1/log2(exp(1)), Q1.43 */
/* Function Declarations */
int32_t base2_logarithm(uint32_t u);
uint32_t ln_int32(uint32_t numerator);
uint32_t log10_int32(uint32_t numerator);

#endif
