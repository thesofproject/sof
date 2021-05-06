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
#include <stddef.h>
#include <stdint.h>

#define SRAM_UNCACHED_ALIAS	0x20000000

#define is_cached(address) (!!((uintptr_t)(address) & SRAM_UNCACHED_ALIAS))

static inline void dcache_writeback_region(void *addr, size_t size)
{
#if XCHAL_DCACHE_SIZE > 0
	if (is_cached(addr))
		xthal_dcache_region_writeback(addr, size);
#endif
}

static inline void dcache_writeback_all(void)
{
#if XCHAL_DCACHE_SIZE > 0
	xthal_dcache_all_writeback();
#endif
}

static inline void dcache_invalidate_region(void *addr, size_t size)
{
#if XCHAL_DCACHE_SIZE > 0
	if (is_cached(addr))
		xthal_dcache_region_invalidate(addr, size);
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

static inline void dcache_writeback_invalidate_region(void *addr, size_t size)
{
#if XCHAL_DCACHE_SIZE > 0
	if (is_cached(addr))
		xthal_dcache_region_writeback_inv(addr, size);
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
