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

#define SRAM_BANK_SIZE			(128 * 1024)

#define EBB_BANKS_IN_SEGMENT		32

/* data cache line alignment */
#define PLATFORM_DCACHE_ALIGN		DCACHE_LINE_SIZE

/**
 * \brief Data shared between different cores.
 * Placed into dedicated section, which should be accessed through
 * uncached memory region. SMP platforms without uncache can simply
 * align to cache line size instead.
 */
#define SHARED_DATA

#define PLATFORM_LPSRAM_EBB_COUNT	(DT_REG_SIZE(DT_NODELABEL(sram1)) / SRAM_BANK_SIZE)
#define PLATFORM_HPSRAM_EBB_COUNT	(DT_REG_SIZE(DT_NODELABEL(sram0)) / SRAM_BANK_SIZE)

#define SRAM_CACHED_BASE		0xA0000000
#define SRAM_BASE			0x40000000
#define SRAM_ALIAS_MASK			0xF0000000
#define SRAM_ALIAS_OFFSET		0x60000000

#if !defined UNIT_TEST
#define uncache_to_cache(address) \
				((__typeof__(address))(((uint32_t)(address) &  \
				~SRAM_ALIAS_MASK) | SRAM_CACHED_BASE))
#define is_uncached(address) \
				(((uint32_t)(address) & SRAM_ALIAS_MASK) == SRAM_BASE)
#define cache_to_uncache(address) \
				((__typeof__(address))(((uint32_t)(address) & \
				~SRAM_ALIAS_MASK) | SRAM_BASE))
#else
#define uncache_to_cache(address)       address
#define cache_to_uncache(address)       address
#define is_uncached(address)            0
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
	return ptr;
}

#endif /* __ACE_LIB_MEMORY_H__ */

#else

#error "This file shouldn't be included from outside of platform/lib/memory.h"

#endif /* __PLATFORM_LIB_MEMORY_H__ */
