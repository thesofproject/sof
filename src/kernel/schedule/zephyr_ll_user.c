// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.

#include <zephyr/cache.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <rtos/userspace_helper.h>

#include <sof/schedule/ll_schedule_domain.h>

LOG_MODULE_DECLARE(ll_schedule, CONFIG_SOF_LOG_LEVEL);

/*
 * Layout of the LL user memory region, each part page aligned:
 *
 *   | kernel RO area | kernel RW area | LL user heap |
 *
 * The heap and the kernel RW area are mapped read-write into the LL memory
 * domain, the kernel RO area read-only. Kernel code allocates objects that
 * LL user threads must access from the kernel areas, see
 * zephyr_ll_karea_alloc(). The allocator state of those areas is kept in
 * ll_mem_resources, which is kernel-only.
 */
#define LL_REGION_SIZE		CONFIG_SOF_ZEPHYR_SYS_USER_HEAP_SIZE
#define LL_KAREA_RO_SIZE	CONFIG_SOF_ZEPHYR_LL_KAREA_RO_SIZE
#define LL_KAREA_RW_SIZE	CONFIG_SOF_ZEPHYR_LL_KAREA_RW_SIZE
#define LL_HEAP_SIZE		(LL_REGION_SIZE - LL_KAREA_RO_SIZE - LL_KAREA_RW_SIZE)

BUILD_ASSERT(LL_REGION_SIZE % CONFIG_MM_DRV_PAGE_SIZE == 0);
BUILD_ASSERT(LL_KAREA_RO_SIZE % CONFIG_MM_DRV_PAGE_SIZE == 0);
BUILD_ASSERT(LL_KAREA_RW_SIZE % CONFIG_MM_DRV_PAGE_SIZE == 0);
BUILD_ASSERT(LL_KAREA_RO_SIZE + LL_KAREA_RW_SIZE < LL_REGION_SIZE,
	     "kernel areas do not leave room for the LL user heap");

/* A kernel-managed area, addresses are the uncached alias */
struct zephyr_ll_karea_state {
	uint8_t *base;
	size_t size;
	size_t used;
};

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
	struct zephyr_ll_karea_state karea[ZEPHYR_LL_KAREA_COUNT]; /**< Kernel-managed areas */
};

static struct zephyr_ll_mem_resources ll_mem_resources;

static void ll_region_map(uint8_t *start, size_t size, k_mem_partition_attr_t attr)
{
	struct k_mem_partition part;
	int ret;

	part.start = (uintptr_t)sys_cache_cached_ptr_get(start);
	part.size = size;
	part.attr = attr | XTENSA_MMU_CACHED_WB;

	ret = k_mem_domain_add_partition(&ll_mem_resources.mem_domain, &part);
	tr_dbg(&ll_tr, "map %p size %#zx (cached), ret %d", (void *)part.start, size, ret);
	if (ret)
		k_panic();

#ifdef CONFIG_CACHE_HAS_MIRRORED_MEMORY_REGIONS
	part.start = (uintptr_t)sys_cache_uncached_ptr_get(start);
	part.attr = attr;

	ret = k_mem_domain_add_partition(&ll_mem_resources.mem_domain, &part);
	tr_dbg(&ll_tr, "map %p size %#zx (uncached), ret %d", (void *)part.start, size, ret);
	if (ret)
		k_panic();
#endif
}

static void ll_region_init(void)
{
	struct zephyr_ll_karea_state *ro = &ll_mem_resources.karea[ZEPHYR_LL_KAREA_RO];
	struct zephyr_ll_karea_state *rw = &ll_mem_resources.karea[ZEPHYR_LL_KAREA_RW];
	uint8_t *mem;

	mem = rballoc_align(SOF_MEM_FLAG_USER | SOF_MEM_FLAG_COHERENT, LL_REGION_SIZE,
			    CONFIG_MM_DRV_PAGE_SIZE);
	if (!mem) {
		tr_err(&ll_tr, "region alloc fail");
		k_panic();
	}

	ro->base = mem;
	ro->size = LL_KAREA_RO_SIZE;
	rw->base = mem + LL_KAREA_RO_SIZE;
	rw->size = LL_KAREA_RW_SIZE;

	mem += LL_KAREA_RO_SIZE + LL_KAREA_RW_SIZE;
	k_heap_init(&ll_mem_resources.heap, mem, LL_HEAP_SIZE);

	/*
	 * k_heap_init() does not set these, so set the values
	 * manually here
	 */
	ll_mem_resources.heap.heap.init_mem = mem;
	ll_mem_resources.heap.heap.init_bytes = LL_HEAP_SIZE;

	if (ro->size)
		ll_region_map(ro->base, ro->size, K_MEM_PARTITION_P_RO_U_RO);

	/* the kernel RW area and the heap are adjacent, map them together */
	ll_region_map(rw->base, rw->size + LL_HEAP_SIZE, K_MEM_PARTITION_P_RW_U_RW);

	tr_info(&ll_tr, "LL region: RO area %p %#zx, RW area %p %#zx, heap %p %#zx",
		(void *)ro->base, ro->size, (void *)rw->base, rw->size, (void *)mem,
		(size_t)LL_HEAP_SIZE);
}

void zephyr_ll_user_resources_init(void)
{
	int ret;

	k_mem_domain_init(&ll_mem_resources.mem_domain, 0, NULL);

	ll_region_init();

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
 * Can be called from both kernel and user-space. The heap object itself is
 * kernel-only, but its address is a link-time constant, so returning it
 * does not read any memory. User threads only pass it to the
 * sof_heap_alloc()/sof_heap_free() system calls, whose validators check it
 * with zephyr_ll_user_heap_verify().
 *
 * Do not cache the pointer in user-writable memory for kernel use: kernel
 * code must call this function, so it never trusts a stored copy.
 *
 * @return heap pointer that can be passed to sof_heap_alloc()
 */
struct k_heap *zephyr_ll_user_heap(void)
{
	return &ll_mem_resources.heap;
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
