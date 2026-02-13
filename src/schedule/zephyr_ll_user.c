// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.

#include <zephyr/cache.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <rtos/userspace_helper.h>

#include <sof/schedule/ll_schedule_domain.h>

LOG_MODULE_DECLARE(ll_schedule, CONFIG_SOF_LOG_LEVEL);

/**
 * Memory resources for userspace LL scheduler
 *
 * This structure encapsulates the memory management resources required for the
 * low-latency (LL) scheduler in userspace mode. It provides memory isolation
 * and heap management for LL scheduler threads. Only kernel accessible.
 */
struct zephyr_ll_mem_resources {
	struct k_mem_domain mem_domain; /**< Memory domain for LL thread isolation */
	struct k_heap heap; /**< Heap allocator for LL scheduler memory */
};

static struct zephyr_ll_mem_resources ll_mem_resources;

/**
 * Heap allocator for LL scheduler memory (user accessible pointer)
 *
 * Note: this is also user-writable, so kernel must not rely on this to
 * be correct and must always validate it separately.
 */
APP_SYSUSER_DATA static struct k_heap *zephyr_ll_heap;

static struct k_heap *ll_heap_alloc(void)
{
	const size_t alloc_size = CONFIG_SOF_ZEPHYR_SYS_USER_HEAP_SIZE;

	BUILD_ASSERT(CONFIG_SOF_ZEPHYR_SYS_USER_HEAP_SIZE % CONFIG_MM_DRV_PAGE_SIZE == 0);

	void *mem = rballoc_align(SOF_MEM_FLAG_USER | SOF_MEM_FLAG_COHERENT, alloc_size,
				  CONFIG_MM_DRV_PAGE_SIZE);
	if (!mem)
		return NULL;

	k_heap_init(&ll_mem_resources.heap, mem, alloc_size);

	/*
	 * k_heap_init() does not set these, so set the values
	 * manually here
	 */
	ll_mem_resources.heap.heap.init_mem = mem;
	ll_mem_resources.heap.heap.init_bytes = alloc_size;

	return &ll_mem_resources.heap;
}

static void ll_heap_init(void)
{
	struct k_heap *heap = ll_heap_alloc();
	struct k_mem_partition mem_partition;
	int ret;

	if (!heap) {
		tr_err(&ll_tr, "heap alloc fail");
		k_panic();
	}

	/* Create memory partition for sch_data array */
	mem_partition.start = (uintptr_t)sys_cache_cached_ptr_get(heap->heap.init_mem);
	mem_partition.size = heap->heap.init_bytes;
	mem_partition.attr = K_MEM_PARTITION_P_RW_U_RW | XTENSA_MMU_CACHED_WB;

	ret = k_mem_domain_add_partition(&ll_mem_resources.mem_domain, &mem_partition);
	tr_dbg(&ll_tr, "init ll heap %p, size %u (cached), ret %d",
	       (void *)mem_partition.start, heap->heap.init_bytes, ret);
	if (ret)
		k_panic();

	mem_partition.start = (uintptr_t)sys_cache_uncached_ptr_get(heap->heap.init_mem);
	mem_partition.attr = K_MEM_PARTITION_P_RW_U_RW;
	ret = k_mem_domain_add_partition(&ll_mem_resources.mem_domain, &mem_partition);
	tr_dbg(&ll_tr, "init ll heap %p, size %u (uncached), ret %d",
	       (void *)mem_partition.start, heap->heap.init_bytes, ret);
	if (ret)
		k_panic();
}

void zephyr_ll_user_resources_init(void)
{
	int ret;

	k_mem_domain_init(&ll_mem_resources.mem_domain, 0, NULL);

	ll_heap_init();

	/* store a user-accessible pointer */
	zephyr_ll_heap = &ll_mem_resources.heap;

	/* attach common partition to LL domain */
	user_memory_attach_common_partition(zephyr_ll_mem_domain());

	ret = user_memory_attach_system_user_partition(zephyr_ll_mem_domain());
	if (ret)
		k_panic();
}

/**
 * Check if 'heap' is a valid heap pointer.
 *
 * Available only in kernel mode.
 *
 * @return true if valid
 */
bool zephyr_ll_user_heap_verify(struct k_heap *heap)
{
	return heap == &ll_mem_resources.heap;
}

/**
 * Returns heap object to use in user-space LL code.
 *
 * Can be called from user-space.
 *
 * @return heap pointer that can be passed to sof_heap_alloc()
 */
struct k_heap *zephyr_ll_user_heap(void)
{
	return zephyr_ll_heap;
}

/**
 * Returns pointer to LL user-space memory domain.
 *
 * Available only in kernel mode.
 *
 * @return pointer to memory domain
 */
struct k_mem_domain *zephyr_ll_mem_domain(void)
{
	return &ll_mem_resources.mem_domain;
}
