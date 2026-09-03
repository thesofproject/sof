/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 */

#ifndef __ZEPHYR_RTOS_CACHE_H__
#define __ZEPHYR_RTOS_CACHE_H__

#define __SOF_LIB_CACHE_H__

#if !defined(__ASSEMBLER__) && !defined(LINKER)

#include <zephyr/cache.h>
#include <zephyr/debug/sparse.h>

#if defined(CONFIG_XTENSA) && defined(CONFIG_INTEL)

/* definitions required by xtensa-based Intel platforms.
 *
 * TODO: if possible, move these to Zephyr.
 */
#define SRAM_UNCACHED_ALIAS 0x20000000
#define is_cached(address) (!!((uintptr_t)(address) & SRAM_UNCACHED_ALIAS))

#endif /* defined(CONFIG_XTENSA) && defined(CONFIG_INTEL) */

/* sanity check - make sure CONFIG_DCACHE_LINE_SIZE is valid */
#if !defined(CONFIG_DCACHE_LINE_SIZE_DETECT) && (CONFIG_DCACHE_LINE_SIZE > 0)
#define DCACHE_LINE_SIZE CONFIG_DCACHE_LINE_SIZE
#else
#if defined(CONFIG_LIBRARY) || defined(CONFIG_ZEPHYR_POSIX)
#define DCACHE_LINE_SIZE 64
#else
#error "Invalid cache configuration."
#endif /* defined(CONFIG_LIBRARY) || defined(CONFIG_ZEPHYR_POSIX) */
#endif /* !defined(CONFIG_DCACHE_LINE_SIZE_DETECT) && (CONFIG_DCACHE_LINE_SIZE > 0) */

static inline void dcache_writeback_region(void __sparse_cache *addr, size_t size)
{
	/* TODO: return value should probably be checked here */
	sys_cache_data_flush_range((__sparse_force void *)addr, size);
}

static inline void dcache_invalidate_region(void __sparse_cache *addr, size_t size)
{
	/* TODO: return value should probably be checked here */
	sys_cache_data_invd_range((__sparse_force void *)addr, size);
}

static inline void icache_invalidate_region(void __sparse_cache *addr, size_t size)
{
	/* TODO: return value should probably be checked here */
	sys_cache_instr_invd_range((__sparse_force void *)addr, size);
}

static inline void dcache_writeback_invalidate_region(void __sparse_cache *addr, size_t size)
{
	/* TODO: return value should probably be checked here */
	sys_cache_data_flush_and_invd_range((__sparse_force void *)addr, size);
}

#endif /* !defined(__ASSEMBLER__) && !defined(LINKER) */

#endif /* __ZEPHYR_RTOS_CACHE_H__ */
