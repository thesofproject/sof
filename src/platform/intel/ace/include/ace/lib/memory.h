/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 *
 * Author: Bartosz Kokoszko <bartoszx.kokoszko@linux.intel.com>
 */

#ifdef __PLATFORM_LIB_MEMORY_H__

#ifndef __ACE_LIB_MEMORY_H__
#define __ACE_LIB_MEMORY_H__

#include <sof/common.h>
#include <rtos/cache.h>
#if !defined(__ASSEMBLER__) && !defined(LINKER)
#include <sof/lib/cpu.h>
#endif

#include <zephyr/cache.h>

#endif /* __ACE_LIB_MEMORY_H__ */

#else

#error "This file shouldn't be included from outside of platform/lib/memory.h"

#endif /* __PLATFORM_LIB_MEMORY_H__ */
