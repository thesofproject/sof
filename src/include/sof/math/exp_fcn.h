/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 *
 * Author: Shriram Shastry <malladi.sastry@linux.intel.com>
 *
 */
#ifndef __SOFM_EXP_H__
#define __SOFM_EXP_H__

#include <stdint.h>

#if defined(__XCC__)
/* HiFi */
#include <xtensa/config/core-isa.h>
#if XCHAL_HAVE_HIFI4 == 1
#define SOFM_EXPONENTIAL_HIFI4 1
#endif
#else
/* !XCC */
#define EXPONENTIAL_GENERIC 1

#endif

int32_t sofm_exp_int32(int32_t x);

#endif
