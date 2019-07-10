/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2017 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifndef __ARCH_CACHE_H__
#define __ARCH_CACHE_H__

#include <stdint.h>
#include <stddef.h>
#include <xtensa/config/core.h>
#include <xtensa/hal.h>

#define DCACHE_LINE_SIZE	XCHAL_DCACHE_LINESIZE

static inline void dcache_writeback_region(void *addr, size_t size)
{
#if XCHAL_DCACHE_SIZE > 0
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
	xthal_dcache_region_writeback_inv(addr, size);
#endif
}

static inline void dcache_writeback_invalidate_all(void)
{
#if XCHAL_DCACHE_SIZE > 0
	xthal_dcache_all_writeback_inv();
#endif
}

#endif /* __ARCH_CACHE_H__ */
