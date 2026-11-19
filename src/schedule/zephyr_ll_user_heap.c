// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.

/*
 * Allocator of the LL user heap, the user part of the LL memory region.
 *
 * The heap metadata is in memory that LL and IPC user threads can write, so
 * the allocator runs in user mode: kernel code only initialises the heap at
 * boot and never parses it after that. The sys_heap object, its lock and the
 * usage counters are in the system user partition, mapped only into the LL
 * memory domain. The lock is a sys_mutex, which user threads can take.
 */

#include <zephyr/cache.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/math_extras.h>
#include <zephyr/sys/mutex.h>
#include <zephyr/sys/sys_heap.h>

#include <rtos/alloc.h>
#include <rtos/userspace_data.h>

#include <sof/common.h>
#include <sof/schedule/ll_schedule.h>

LOG_MODULE_DECLARE(mem_allocator, CONFIG_SOF_LOG_LEVEL);

static APP_SYSUSER_BSS struct sys_heap ll_user_sys_heap;
static APP_SYSUSER_BSS SYS_MUTEX_DEFINE(ll_user_heap_lock);
static APP_SYSUSER_BSS struct zephyr_ll_user_heap_stats ll_user_heap_stats;

void zephyr_ll_user_heap_init(void *mem, size_t bytes)
{
	__ASSERT(!k_is_user_context(), "kernel context only");

	sys_heap_init(&ll_user_sys_heap, mem, bytes);

	/* sys_heap_init() does not set these */
	ll_user_sys_heap.init_mem = mem;
	ll_user_sys_heap.init_bytes = bytes;
}

void *zephyr_ll_user_heap_alloc(uint32_t flags, size_t bytes, size_t alignment)
{
	void *ptr;
	int ret;

	/*
	 * The heap metadata is next to the allocations and both aliases are
	 * used, so never share a cache line between a cached allocation and
	 * metadata or another allocation.
	 */
	alignment = MAX(PLATFORM_DCACHE_ALIGN, alignment);
	if (bytes > SIZE_MAX - PLATFORM_DCACHE_ALIGN)
		return NULL;
	bytes = ALIGN_UP(bytes, PLATFORM_DCACHE_ALIGN);

	ret = sys_mutex_lock(&ll_user_heap_lock, K_FOREVER);
	if (ret)
		return NULL;

	ptr = sys_heap_aligned_alloc(&ll_user_sys_heap, alignment, bytes);
	if (ptr) {
		ll_user_heap_stats.allocated_bytes += sys_heap_usable_size(&ll_user_sys_heap, ptr);
		ll_user_heap_stats.max_allocated_bytes = MAX(ll_user_heap_stats.max_allocated_bytes,
							     ll_user_heap_stats.allocated_bytes);
	} else {
		ll_user_heap_stats.fail_count++;
	}

#if CONFIG_SYS_HEAP_RUNTIME_STATS && CONFIG_IPC_MAJOR_4
	/* like the kernel heaps, see heap_alloc_aligned() in zephyr/lib/alloc.c */
	struct sys_memory_stats stats;

	sys_heap_runtime_stats_get(&ll_user_sys_heap, &stats);
	LOG_INF("LL user heap allocated: %zu free: %zu max allocated: %zu",
		stats.allocated_bytes, stats.free_bytes, stats.max_allocated_bytes);
#endif

	sys_mutex_unlock(&ll_user_heap_lock);

	if (ptr && !(flags & SOF_MEM_FLAG_COHERENT))
		ptr = (__sparse_force void *)sys_cache_cached_ptr_get(ptr);

	return ptr;
}

/* bounds from user-writable memory, so for user-mode checks only */
static bool ll_user_heap_owns(const void *ptr)
{
	void __sparse_cache *cptr = (__sparse_force void __sparse_cache *)ptr;
	uintptr_t start = (uintptr_t)ll_user_sys_heap.init_mem;
	uintptr_t addr = (uintptr_t)sys_cache_uncached_ptr_get(cptr);

	return addr >= start && addr - start < ll_user_sys_heap.init_bytes;
}

void zephyr_ll_user_heap_free(void *ptr)
{
	size_t usable;
	int ret;

	if (!ptr)
		return;

	if (!ll_user_heap_owns(ptr)) {
		k_oops();
		return;
	}

	ret = sys_mutex_lock(&ll_user_heap_lock, K_FOREVER);
	if (ret)
		return;

	if (sys_cache_is_ptr_cached(ptr)) {
		void __sparse_cache *cptr = (__sparse_force void __sparse_cache *)ptr;
		void *uncached = sys_cache_uncached_ptr_get(cptr);

		sys_cache_data_flush_and_invd_range(ptr,
						    sys_heap_usable_size(&ll_user_sys_heap,
									 uncached));
		ptr = uncached;
	}

	usable = sys_heap_usable_size(&ll_user_sys_heap, ptr);
	ll_user_heap_stats.allocated_bytes -= MIN(usable, ll_user_heap_stats.allocated_bytes);

	sys_heap_free(&ll_user_sys_heap, ptr);

	sys_mutex_unlock(&ll_user_heap_lock);
}

void zephyr_ll_user_heap_stats_get(struct zephyr_ll_user_heap_stats *stats)
{
	/* user-writable values, only copied as numbers */
	stats->allocated_bytes = ll_user_heap_stats.allocated_bytes;
	stats->max_allocated_bytes = ll_user_heap_stats.max_allocated_bytes;
	stats->fail_count = ll_user_heap_stats.fail_count;
}
