/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 *
 * Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

#ifndef __SOF_LIB_MEMORY_H__
#define __SOF_LIB_MEMORY_H__

#include <platform/lib/memory.h>

#if !defined(ASSEMBLY) && !defined(LINKER)
#include <sof/common.h>
#include <sof/compiler_attributes.h>
#include <rtos/panic.h>

static inline void __sparse_cache *uncache_to_cache(void *address)
{
#if CONFIG_DEBUG_CACHE_ADDR_CONVERSION && !CONFIG_XT_BOOT_LOADER
	assert(IS_ALIGNED((uintptr_t)address, PLATFORM_DCACHE_ALIGN));
#endif
	return platform_uncache_to_cache(address);
}

static inline void *cache_to_uncache(void __sparse_cache *address)
{
#if CONFIG_DEBUG_CACHE_ADDR_CONVERSION && !CONFIG_XT_BOOT_LOADER
	assert(IS_ALIGNED((uintptr_t)address, PLATFORM_DCACHE_ALIGN));
#endif
	return platform_cache_to_uncache(address);
}
#endif /* !ASSEMBLY && !LINKER */

#endif /* __SOF_LIB_MEMORY_H__ */
