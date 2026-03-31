// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.

#include <zephyr/cache.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <rtos/userspace_helper.h>

#include <sof/schedule/ll_schedule_domain.h>
#include <sof/schedule/ll_schedule.h>

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
};

static struct zephyr_ll_mem_resources ll_mem_resources;

/* Heap allocator for LL scheduler memory (user accessible pointer) */
APP_TASK_DATA static struct k_heap *zephyr_ll_heap;

static struct k_heap *zephyr_ll_heap_init(void)
{
	struct k_heap *heap = sys_user_heap_init();
	struct k_mem_partition mem_partition;
	int ret;

	/*
	 * TODO: the size of LL heap should be independently configurable and
	 *       not tied to CONFIG_SOF_ZEPHYR_USERSPACE_MODULE_HEAP_SIZE
	 */

	if (!heap) {
		tr_err(&ll_tr, "heap alloc fail");
		k_panic();
	}

	uintptr_t cached_ptr = (uintptr_t)sys_cache_cached_ptr_get(heap->heap.init_mem);
	uintptr_t uncached_ptr = (uintptr_t)sys_cache_uncached_ptr_get(heap->heap.init_mem);

	/* Create memory partition for sch_data array */
	mem_partition.start = cached_ptr;
	mem_partition.size = heap->heap.init_bytes;
	mem_partition.attr = K_MEM_PARTITION_P_RW_U_RW | XTENSA_MMU_CACHED_WB;

	ret = k_mem_domain_add_partition(&ll_mem_resources.mem_domain, &mem_partition);
	tr_dbg(&ll_tr, "init ll heap %p, size %u (cached), ret %d",
	       (void *)mem_partition.start, heap->heap.init_bytes, ret);
	if (ret)
		k_panic();

	if (cached_ptr != uncached_ptr) {
		mem_partition.start = uncached_ptr;
		mem_partition.attr = K_MEM_PARTITION_P_RW_U_RW;
		ret = k_mem_domain_add_partition(&ll_mem_resources.mem_domain, &mem_partition);
		tr_dbg(&ll_tr, "init ll heap %p, size %u (uncached), ret %d",
		       (void *)mem_partition.start, heap->heap.init_bytes, ret);
		if (ret)
			k_panic();
	}

	return heap;
}

void zephyr_ll_user_resources_init(void)
{
	k_mem_domain_init(&ll_mem_resources.mem_domain, 0, NULL);

	zephyr_ll_heap = zephyr_ll_heap_init();

	/* attach common partition to LL domain */
	user_memory_attach_common_partition(zephyr_ll_mem_domain());
	user_memory_attach_system_user_partition(zephyr_ll_mem_domain());
}

struct k_heap *zephyr_ll_user_heap(void)
{
	return zephyr_ll_heap;
}

struct k_mem_domain *zephyr_ll_mem_domain(void)
{
	return &ll_mem_resources.mem_domain;
}
