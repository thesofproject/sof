/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright 2024 NXP
 */

#ifdef __SOF_LIB_MEMORY_H__

#ifndef __PLATFORM_LIB_MEMORY_H__
#define __PLATFORM_LIB_MEMORY_H__

#include <rtos/cache.h>

#define PLATFORM_DCACHE_ALIGN DCACHE_LINE_SIZE

#define SHARED_DATA

#define uncache_to_cache(address) address
#define cache_to_uncache(address) address
#define cache_to_uncache_init(address) address
#define is_uncached(address) 0

/* no address translation required */
#define host_to_local(addr) (addr)
#define local_to_host(addr) (addr)

#define HEAPMEM_SIZE 0x00010000

/* WAKEUP domain MU7 side B */
#define MU_BASE 0x42440000UL

static inline void *platform_shared_get(void *ptr, int bytes)
{
	return ptr;
}

#endif /* __PLATFORM_LIB_MEMORY_H__ */

#else

#error "This file shouldn't be included from outside of sof/lib/memory.h"

#endif /* __SOF_LIB_MEMORY_H__*/
