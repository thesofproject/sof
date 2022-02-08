/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2017 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifdef __SOF_LIB_CACHE_H__

#ifndef __ARCH_LIB_CACHE_H__
#define __ARCH_LIB_CACHE_H__

#include <xtensa/config/core-isa.h>

#define DCACHE_LINE_SIZE	XCHAL_DCACHE_LINESIZE

#if !defined(__ASSEMBLER__) && !defined(LINKER)

#include <xtensa/hal.h>
#include <sof/compiler_attributes.h>
#include <stddef.h>
#include <stdint.h>

#ifdef CONFIG_COMPILER_WORKAROUND_CACHE_ATTR
#include <sof/drivers/cache_attr.h>
#endif

#define SRAM_UNCACHED_ALIAS	0x20000000

#ifdef CONFIG_IMX

#ifdef CONFIG_COMPILER_WORKAROUND_CACHE_ATTR
/*
 * We want to avoid buggy compiler optimization (function inlining).
 * So we replace the call to glb_addr_attr() from glb_is_cached()
 * with a function pointer that is initialized in
 * src/arch/xtensa/driver/cache_attr.c
 */
#define is_cached(address) glb_is_cached(address)

#else /* CONFIG_COMPILER_WORKAROUND_CACHE_ATTR */
/*
 * The _memmap_cacheattr_reset linker script variable has
 * dedicate cache attribute for every 512M in 4GB space
 * 1: write through
 * 2: cache bypass
 * 4: write back
 * F: invalid access
 */
extern uint32_t _memmap_cacheattr_reset;

/*
 * Since each hex digit keeps the attributes for a 512MB region,
 * we have the following address ranges:
 * Address range       - hex digit
 * 0        - 1FFFFFFF - 0
 * 20000000 - 3FFFFFFF - 1
 * 40000000 - 5FFFFFFF - 2
 * 60000000 - 7FFFFFFF - 3
 * 80000000 - 9FFFFFFF - 4
 * A0000000 - BFFFFFFF - 5
 * C0000000 - DFFFFFFF - 6
 * E0000000 - FFFFFFFF - 7
 */

/*
 * Based on the above information, get the address region id (0-7)
 */
#define _addr_range(address) (((uintptr_t)(address) >> 29) & 0x7)
/*
 * Get the position of the cache attribute for a certain memory region.
 * There are 4 bits per hex digit.
 */
#define _addr_shift(address) ((_addr_range(address)) << 2)
/*
 * For the given address, get the corresponding hex digit
 * from the linker script variable that contains the cache attributes
 */
#define _addr_attr(address) ((((uint32_t)(&_memmap_cacheattr_reset)) >> \
			     (_addr_shift(address))) & 0xF)
/*
 * Check if the address is cacheable or not, by verifying the _addr_attr,
 * which for cacheable addresses might be 1 or 4
 */
#define is_cached(address) ((_addr_attr(address) == 1) || \
			    (_addr_attr(address) == 4))
#endif /* CONFIG_COMPILER_WORKAROUND_CACHE_ATTR */

#else /* CONFIG_IMX */
#define is_cached(address) (!!((uintptr_t)(address) & SRAM_UNCACHED_ALIAS))
#endif

static inline void dcache_writeback_region(void __sparse_cache *addr, size_t size)
{
#if XCHAL_DCACHE_SIZE > 0
	if (is_cached(addr))
		xthal_dcache_region_writeback((__sparse_force void *)addr, size);
#endif
}

static inline void dcache_writeback_all(void)
{
#if XCHAL_DCACHE_SIZE > 0
	xthal_dcache_all_writeback();
#endif
}

static inline void dcache_invalidate_region(void __sparse_cache *addr, size_t size)
{
#if XCHAL_DCACHE_SIZE > 0
	if (is_cached(addr))
		xthal_dcache_region_invalidate((__sparse_force void *)addr, size);
#endif
}

static inline void dcache_invalidate_all(void)
{
#if XCHAL_DCACHE_SIZE > 0
	xthal_dcache_all_invalidate();
#endif
}

static inline void icache_invalidate_region(void *addr, size_t size)
{
#if XCHAL_ICACHE_SIZE > 0
	xthal_icache_region_invalidate(addr, size);
#endif
}

static inline void icache_invalidate_all(void)
{
#if XCHAL_ICACHE_SIZE > 0
	xthal_icache_all_invalidate();
#endif
}

static inline void dcache_writeback_invalidate_region(void __sparse_cache *addr, size_t size)
{
#if XCHAL_DCACHE_SIZE > 0
	if (is_cached(addr))
		xthal_dcache_region_writeback_inv((__sparse_force void *)addr, size);
#endif
}

static inline void dcache_writeback_invalidate_all(void)
{
#if XCHAL_DCACHE_SIZE > 0
	xthal_dcache_all_writeback_inv();
#endif
}

#endif /* !defined(__ASSEMBLER__) && !defined(LINKER) */

#endif /* __ARCH_LIB_CACHE_H__ */

#else

#error "This file shouldn't be included from outside of sof/lib/cache.h"

#endif /* __SOF_LIB_CACHE_H__ */
