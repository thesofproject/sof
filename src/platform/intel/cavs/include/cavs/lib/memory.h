/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 *
 * Author: Bartosz Kokoszko <bartoszx.kokoszko@linux.intel.com>
 */

#ifdef __PLATFORM_LIB_MEMORY_H__

#ifndef __CAVS_LIB_MEMORY_H__
#define __CAVS_LIB_MEMORY_H__

#include <sof/common.h>
#include <rtos/cache.h>
#if !defined(__ASSEMBLER__) && !defined(LINKER)
#include <sof/lib/cpu.h>
#endif

/* data cache line alignment */
#define PLATFORM_DCACHE_ALIGN	DCACHE_LINE_SIZE

#define HEAP_BUF_ALIGNMENT		PLATFORM_DCACHE_ALIGN

/** \brief EDF task's default stack size in bytes. */
/* increase stack size for RTNR and GOOGLE_RTC_AUDIO_PROCESSING */
#if defined(CONFIG_COMP_RTNR) || defined(CONFIG_COMP_GOOGLE_RTC_AUDIO_PROCESSING)
#define PLATFORM_TASK_DEFAULT_STACK_SIZE        0x2000
#else
#define PLATFORM_TASK_DEFAULT_STACK_SIZE	0x1000
#endif

#if !defined(__ASSEMBLER__) && !defined(LINKER)

struct sof;

/**
 * \brief Data shared between different cores.
 * Placed into dedicated section, which should be accessed through
 * uncached memory region. SMP platforms without uncache can simply
 * align to cache line size instead.
 */
#if !defined(UNIT_TEST) && !defined __ZEPHYR__
#define SHARED_DATA	__section(".shared_data") __attribute__((aligned(PLATFORM_DCACHE_ALIGN)))
#else
#define SHARED_DATA
#endif

#define SRAM_ALIAS_BASE		0x9E000000
#define SRAM_ALIAS_MASK		0xFF000000
#define SRAM_ALIAS_OFFSET	SRAM_UNCACHED_ALIAS

#if !defined UNIT_TEST
static inline void __sparse_cache *uncache_to_cache(void *address)
{
	return (void __sparse_cache *)((uintptr_t)(address) | SRAM_ALIAS_OFFSET);
}

static inline void *cache_to_uncache(void __sparse_cache *address)
{
	return (void *)((uintptr_t)(address) & ~SRAM_ALIAS_OFFSET);
}

#define is_uncached(address) \
	(((uint32_t)(address) & SRAM_ALIAS_MASK) == SRAM_ALIAS_BASE)
#else
#define uncache_to_cache(address)	address
#define cache_to_uncache(address)	address
#define is_uncached(address)		0
#endif

#if !defined UNIT_TEST && !defined __ZEPHYR__
#define cache_to_uncache_init(address) \
	((__typeof__((address)))((uint32_t)((address)) - SRAM_ALIAS_OFFSET))
#else
#define cache_to_uncache_init(address)	address
#endif

/**
 * \brief Returns pointer to the memory shared by multiple cores.
 * \param[in,out] ptr Initial pointer to the allocated memory.
 * \param[in] bytes Size of the allocated memory
 * \return Appropriate pointer to the shared memory.
 *
 * This function is called only once right after allocation of shared memory.
 * Platforms with uncached memory region should return aliased address.
 * On platforms without such region simple invalidate is enough.
 */
static inline void *platform_shared_get(void *ptr, int bytes)
{
#if CONFIG_CORE_COUNT > 1 && !defined __ZEPHYR__
	dcache_invalidate_region((__sparse_force void __sparse_cache *)ptr, bytes);
	return cache_to_uncache(ptr);
#else
	return ptr;
#endif
}

/**
 * \brief Transforms pointer if necessary before freeing the memory.
 * \param[in,out] ptr Pointer to the allocated memory.
 * \return Appropriate pointer to the memory ready to be freed.
 */
static inline void *platform_rfree_prepare(void *ptr)
{
	return ptr;
}

void platform_init_memmap(struct sof *sof);

#endif

#define host_to_local(addr) (addr)
#define local_to_host(addr) (addr)

#endif /* __CAVS_LIB_MEMORY_H__ */

#else

#error "This file shouldn't be included from outside of platform/lib/memory.h"

#endif /* __PLATFORM_LIB_MEMORY_H__ */
