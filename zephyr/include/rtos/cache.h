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
