/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifndef __PLATFORM_SHIM_H__
#define __PLATFORM_SHIM_H__

#include <platform/memory.h>
#include <stdint.h>

static inline uint32_t shim_read(uint32_t reg) {return 0; }
static inline void shim_write(uint32_t reg, uint32_t val) {}

#endif /* __PLATFORM_SHIM_H__ */
