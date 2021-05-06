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
#include <sof/lib/cache.h>
#if !defined(__ASSEMBLER__) && !defined(LINKER)
#include <sof/lib/cpu.h>
#endif

/* data cache line alignment */
#define PLATFORM_DCACHE_ALIGN	DCACHE_LINE_SIZE

#define SRAM_BANK_SIZE			(64 * 1024)

#define EBB_BANKS_IN_SEGMENT		32

#define EBB_SEGMENT_SIZE		EBB_BANKS_IN_SEGMENT

#if CONFIG_LP_MEMORY_BANKS
#define PLATFORM_LPSRAM_EBB_COUNT	CONFIG_LP_MEMORY_BANKS
#else
#define PLATFORM_LPSRAM_EBB_COUNT	0
#endif

#define PLATFORM_HPSRAM_EBB_COUNT	CONFIG_HP_MEMORY_BANKS

#define MAX_MEMORY_SEGMENTS		PLATFORM_HPSRAM_SEGMENTS

#if CONFIG_LP_MEMORY_BANKS
#define LP_SRAM_SIZE \
	(CONFIG_LP_MEMORY_BANKS * SRAM_BANK_SIZE)
#else
#define LP_SRAM_SIZE 0
#endif

#define HP_SRAM_SIZE \
	(CONFIG_HP_MEMORY_BANKS * SRAM_BANK_SIZE)

#define PLATFORM_HPSRAM_SEGMENTS DIV_ROUND_UP(PLATFORM_HPSRAM_EBB_COUNT, \
					      EBB_BANKS_IN_SEGMENT)

#if defined(__ASSEMBLER__)
#define LPSRAM_MASK(ignored)	((1 << PLATFORM_LPSRAM_EBB_COUNT) - 1)

#define HPSRAM_MASK(seg_idx)	((1 << (PLATFORM_HPSRAM_EBB_COUNT \
	- EBB_BANKS_IN_SEGMENT * seg_idx)) - 1)
#else
#define LPSRAM_MASK(ignored)	((1ULL << PLATFORM_LPSRAM_EBB_COUNT) - 1)

#define HPSRAM_MASK(seg_idx)	((1ULL << (PLATFORM_HPSRAM_EBB_COUNT \
	- EBB_BANKS_IN_SEGMENT * seg_idx)) - 1)
#endif

#define LPSRAM_SIZE (PLATFORM_LPSRAM_EBB_COUNT * SRAM_BANK_SIZE)

#define HEAP_BUF_ALIGNMENT		PLATFORM_DCACHE_ALIGN

/** \brief EDF task's default stack size in bytes. */
#define PLATFORM_TASK_DEFAULT_STACK_SIZE	0x1000

#if !defined(__ASSEMBLER__) && !defined(LINKER)

struct sof;

/**
 * \brief Data shared between different cores.
 * Placed into dedicated section, which should be accessed through
 * uncached memory region. SMP platforms without uncache can simply
 * align to cache line size instead.
 */
#if CONFIG_CORE_COUNT > 1 && !defined(UNIT_TEST) && !defined __ZEPHYR__
#define SHARED_DATA	__section(".shared_data")
#else
#define SHARED_DATA
#endif

#define SRAM_ALIAS_BASE		0x9E000000
#define SRAM_ALIAS_MASK		0xFF000000
#define SRAM_ALIAS_OFFSET	0x20000000

#if !defined UNIT_TEST && !defined __ZEPHYR__
#define uncache_to_cache(address) \
	((__typeof__((address)))((uint32_t)((address)) + SRAM_ALIAS_OFFSET))
#define cache_to_uncache(address) \
	((__typeof__((address)))((uint32_t)((address)) - SRAM_ALIAS_OFFSET))
#define is_uncached(address) \
	(((uint32_t)(address) & SRAM_ALIAS_MASK) == SRAM_ALIAS_BASE)
#else
#define uncache_to_cache(address)	address
#define cache_to_uncache(address)	address
#define is_uncached(address)		0
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
	dcache_invalidate_region(ptr, bytes);
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

#endif /* __CAVS_LIB_MEMORY_H__ */

#else

#error "This file shouldn't be included from outside of platform/lib/memory.h"

#endif /* __PLATFORM_LIB_MEMORY_H__ */
