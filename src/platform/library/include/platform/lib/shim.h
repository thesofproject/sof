/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifdef __SOF_LIB_SHIM_H__

#ifndef __PLATFORM_LIB_SHIM_H__
#define __PLATFORM_LIB_SHIM_H__

#include <stdint.h>

static inline uint32_t shim_read(uint32_t reg) {return 0; }
static inline void shim_write(uint32_t reg, uint32_t val) {}

#endif /* __PLATFORM_LIB_SHIM_H__ */

#else

#error "This file shouldn't be included from outside of sof/lib/shim.h"

#endif /* __SOF_LIB_SHIM_H__ */
