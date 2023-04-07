/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 */

#ifndef __ZEPHYR_RTOS_CACHE_H__
#define __ZEPHYR_RTOS_CACHE_H__

#define __SOF_LIB_CACHE_H__

#include <zephyr/cache.h>

#if defined(CONFIG_XTENSA) && defined(CONFIG_INTEL)
#define SRAM_UNCACHED_ALIAS 0x20000000
#define is_cached(ptr) ((ptr) == arch_xtensa_cached_ptr(ptr))
#endif

/* writeback and invalidate data */
#define CACHE_WRITEBACK_INV	0

/* invalidate data */
#define CACHE_INVALIDATE	1

#if !defined(CONFIG_DCACHE_LINE_SIZE_DETECT) && \
	(CONFIG_DCACHE_LINE_SIZE > 0)

#define DCACHE_LINE_SIZE CONFIG_DCACHE_LINE_SIZE

#else

/* If CONFIG_DCACHE_LINE_SIZE is not set appropriately then
 * fallback to a default value. This ought to be alright
 * since all platforms that support Zephyr use this value.
 * This is better than forcing all vendors to set
 * CONFIG_DCACHE_LINE_SIZE in order to avoid compilation
 * errors.
 */
#define DCACHE_LINE_SIZE 128

#endif

static inline void dcache_writeback_region(void *addr, size_t size)
{
	/* TODO: return value should probably be checked here */
	sys_cache_data_flush_range(addr, size);
}

static inline void dcache_invalidate_region(void *addr, size_t size)
{
	/* TODO: return value should probably be checked here */
	sys_cache_data_invd_range(addr, size);
}

static inline void icache_invalidate_region(void *addr, size_t size)
{
	/* TODO: return value should probably be checked here */
	sys_cache_instr_invd_range(addr, size);
}

static inline void dcache_writeback_invalidate_region(void *addr, size_t size)
{
	/* TODO: return value should probably be checked here */
	sys_cache_data_flush_and_invd_range(addr, size);
}

#endif /* __ZEPHYR_RTOS_CACHE_H__ */
