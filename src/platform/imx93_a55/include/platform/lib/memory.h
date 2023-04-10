/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright 2023 NXP
 */

#ifdef __SOF_LIB_MEMORY_H__

#ifndef __PLATFORM_LIB_MEMORY_H__
#define __PLATFORM_LIB_MEMORY_H__

#include <rtos/cache.h>

#define PLATFORM_DCACHE_ALIGN DCACHE_LINE_SIZE

/* seems like in xtensa architecture cached data
 * may be placed at some special addresses in the
 * SRAM?
 *
 * since we're running on A55 cores, there's no such
 * thing so all the below cache/shared data "management"
 * functions aren't necessary.
 */
#define SHARED_DATA

#define uncache_to_cache(address) address
#define cache_to_uncache(address) address
#define cache_to_uncache_init(address) address
#define is_uncached(address) 0

/* number of heaps to be used.
 *
 * i.MX93 is non-SMP so we're only going to use 1 heap.
 */
#define PLATFORM_HEAP_SYSTEM 1
#define PLATFORM_HEAP_SYSTEM_RUNTIME 1
#define PLATFORM_HEAP_RUNTIME 1
#define PLATFORM_HEAP_BUFFER 1

/* host and firmware use shared memory */
#define host_to_local(addr) (addr)
#define local_to_host(addr) (addr)

/* if this gets too big then it will be
 * necessary to increase the SRAM size from
 * the Zephyr DTS. This should be fine as
 * we're going to reserve 800MB for Jailhouse
 * usage.
 */
#define HEAPMEM_SIZE 0x00010000

/* SOF uses A side of the WAKEUPMIX MU */
#define MU_BASE 0x42430000

/* SOF uses EDMA2 (a.k.a EDMA4 in the TRM) */
#define EDMA2_BASE 0x42010000
#define EDMA2_CHAN_SIZE 0x8000

/* WM8962 is connected to SAI3 */
#define SAI3_BASE 0x42660000

static inline void *platform_shared_get(void *ptr, int bytes)
{
	return ptr;
}

#endif /* __PLATFORM_LIB_MEMORY_H__ */

#else

#error "This file shouldn't be included from outside of sof/lib/memory.h"

#endif /* __SOF_LIB_MEMORY_H__*/
