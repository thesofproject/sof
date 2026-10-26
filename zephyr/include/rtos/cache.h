/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 */

/**
 * @file rtos/cache.h
 * @brief RTOS data and instruction cache invalidation and writeback helpers.
 * @ingroup rtos_api
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

/** Alignment that keeps a buffer on data cache lines of its own */
#define PLATFORM_DCACHE_ALIGN	DCACHE_LINE_SIZE

/*
 * Conversion between the cached and uncached aliases of an address, for
 * platforms whose memory is mapped twice. Platforms without such aliases
 * use the same address for both.
 */
#if defined(CONFIG_ACE)

#define uncache_to_cache(address)	sys_cache_cached_ptr_get(address)
#define cache_to_uncache(address)	sys_cache_uncached_ptr_get(address)
#define is_uncached(address)		(!sys_cache_is_ptr_cached(address))

#elif defined(CONFIG_CAVS)

#define SRAM_ALIAS_BASE		0x9E000000
#define SRAM_ALIAS_MASK		0xFF000000
#define SRAM_ALIAS_OFFSET	SRAM_UNCACHED_ALIAS

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

/**
 * @brief Write back (flush) modified data cache lines to memory.
 * @param addr Starting address of the memory region to flush.
 * @param size Size in bytes of the region to flush.
 *
 * Flushes dirty lines in the data cache covering the specified address range
 * back to physical memory or SRAM. Mandatory before handing buffers over to
 * hardware DMA or notifying peer cores / host.
 */
static inline void dcache_writeback_region(void __sparse_cache *addr, size_t size)
{
	/* TODO: return value should probably be checked here */
	sys_cache_data_flush_range((__sparse_force void *)addr, size);
}

/**
 * @brief Invalidate data cache lines in the specified memory region.
 * @param addr Starting address of the memory region to invalidate.
 * @param size Size in bytes of the region to invalidate.
 *
 * Discards cached data lines covering the address range so subsequent reads
 * fetch fresh data directly from physical SRAM or DMA-updated memory.
 */
static inline void dcache_invalidate_region(void __sparse_cache *addr, size_t size)
{
	/* TODO: return value should probably be checked here */
	sys_cache_data_invd_range((__sparse_force void *)addr, size);
}

/**
 * @brief Invalidate instruction cache lines in the specified memory region.
 * @param addr Starting address of the code memory region to invalidate.
 * @param size Size in bytes of the code region.
 *
 * Used after dynamically loading or relocating executable code (e.g. LLEXT
 * modules) to ensure the instruction pipeline does not execute stale instructions.
 */
static inline void icache_invalidate_region(void __sparse_cache *addr, size_t size)
{
	/* TODO: return value should probably be checked here */
	sys_cache_instr_invd_range((__sparse_force void *)addr, size);
}

/**
 * @brief Write back and invalidate data cache lines in the specified memory region.
 * @param addr Starting address of the memory region.
 * @param size Size in bytes of the region.
 *
 * Flushes modified cache lines back to memory and then invalidates them.
 * Typically used for bidirectional DMA buffers or when reallocating SRAM pools.
 */
static inline void dcache_writeback_invalidate_region(void __sparse_cache *addr, size_t size)
{
	/* TODO: return value should probably be checked here */
	sys_cache_data_flush_and_invd_range((__sparse_force void *)addr, size);
}

#endif /* !defined(__ASSEMBLER__) && !defined(LINKER) */

#endif /* __ZEPHYR_RTOS_CACHE_H__ */
