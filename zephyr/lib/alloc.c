// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 *
 */

#include <sof/init.h>
#include <rtos/alloc.h>
#include <rtos/idc.h>
#include <rtos/interrupt.h>
#include <sof/drivers/interrupt-map.h>
#include <sof/lib/dma.h>
#include <sof/schedule/schedule.h>
#include <sof/lib/notifier.h>
#include <sof/lib/pm_runtime.h>
#include <sof/audio/pipeline.h>
#include <sof/audio/component_ext.h>
#include <sof/trace/trace.h>
#include <rtos/symbol.h>
#include <rtos/wait.h>

/* Zephyr includes */
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/pm/policy.h>
#include <version.h>
#include <zephyr/sys/__assert.h>
#include <zephyr/cache.h>

#if CONFIG_SYS_HEAP_RUNTIME_STATS && CONFIG_IPC_MAJOR_4
#include <zephyr/sys/sys_heap.h>
#endif

LOG_MODULE_REGISTER(mem_allocator, CONFIG_SOF_LOG_LEVEL);

extern struct tr_ctx zephyr_tr;

/*
 * Memory - Create Zephyr HEAP for SOF.
 *
 * Currently functional but some items still WIP.
 */

#ifndef HEAP_RUNTIME_SIZE
#define HEAP_RUNTIME_SIZE	0
#endif

/* system size not declared on some platforms */
#ifndef HEAP_SYSTEM_SIZE
#define HEAP_SYSTEM_SIZE	0
#endif

/* The Zephyr heap */

#ifdef CONFIG_IMX

#ifdef CONFIG_XTENSA
#define HEAPMEM_SIZE		(HEAP_SYSTEM_SIZE + HEAP_RUNTIME_SIZE + HEAP_BUFFER_SIZE)

/*
 * Include heapmem variable in .heap_mem section, otherwise the HEAPMEM_SIZE is
 * duplicated in two sections and the sdram0 region overflows.
 */
__section(".heap_mem") static uint8_t __aligned(64) heapmem[HEAPMEM_SIZE];

#else

/* for ARM64 the heap is placed inside the .bss section.
 *
 * This is because we want to avoid introducing new sections in
 * the arm64 linker script. Also, is there really a need to place
 * it inside a special section?
 *
 * i.MX93 is the only ARM64-based platform so defining the heap this way
 * for all ARM64-based platforms should be safe.
 */
static uint8_t __aligned(PLATFORM_DCACHE_ALIGN) heapmem[HEAPMEM_SIZE];
#endif /* CONFIG_XTENSA */

#elif CONFIG_ACE

/*
 * System heap definition for ACE is defined below.
 * It needs to be explicitly packed into dedicated section
 * to allow memory management driver to control unused
 * memory pages.
 */
__section(".heap_mem") static uint8_t __aligned(PLATFORM_DCACHE_ALIGN) heapmem[HEAPMEM_SIZE];

#elif defined(CONFIG_ARCH_POSIX)

/* Zephyr native_posix links as a host binary and lacks the automated heap markers */
#define HEAPMEM_SIZE (256 * 1024)
char __aligned(8) heapmem[HEAPMEM_SIZE];

#else

extern char _end[], _heap_sentry[];
#define heapmem ((uint8_t *)ALIGN_UP((uintptr_t)_end, PLATFORM_DCACHE_ALIGN))
#define HEAPMEM_SIZE ((uint8_t *)_heap_sentry - heapmem)

#endif

static struct k_heap sof_heap;

#if CONFIG_L3_HEAP
static struct k_heap l3_heap;

/**
 * Returns the start of L3 memory heap.
 * @return Pointer to the L3 memory location which can be used for L3 heap.
 */
static inline uintptr_t get_l3_heap_start(void)
{
	/*
	 * TODO: parse the actual offset using:
	 * - HfIMRIA1 register
	 * - rom_ext_load_offset
	 * - main_fw_load_offset
	 * - main fw size in manifest
	 */
	return (uintptr_t)(ROUND_UP(IMR_L3_HEAP_BASE, L3_MEM_PAGE_SIZE));
}

/**
 * Returns the size of L3 memory heap.
 * @return Size of the L3 memory region which can be used for L3 heap.
 */
static inline size_t get_l3_heap_size(void)
{
	 /*
	  * Calculate the IMR heap size using:
	  * - total IMR size
	  * - IMR base address
	  * - actual IMR heap start
	  */
	return ROUND_DOWN(IMR_L3_HEAP_SIZE, L3_MEM_PAGE_SIZE);
}

/**
 * Checks whether pointer is from L3 heap memory range.
 * @param ptr Pointer to memory being checked.
 * @return True if pointer falls into L3 heap region, false otherwise.
 */
static bool is_l3_heap_pointer(void *ptr)
{
	uintptr_t l3_heap_start = get_l3_heap_start();
	uintptr_t l3_heap_end = l3_heap_start + get_l3_heap_size();

	if ((POINTER_TO_UINT(ptr) >= l3_heap_start) && (POINTER_TO_UINT(ptr) < l3_heap_end))
		return true;

	return false;
}

static void *l3_heap_alloc_aligned(struct k_heap *h, size_t min_align, size_t bytes)
{
	k_spinlock_key_t key;
	void *ret;
#if CONFIG_SYS_HEAP_RUNTIME_STATS && CONFIG_IPC_MAJOR_4
	struct sys_memory_stats stats;
#endif
	if (!cpu_is_primary(arch_proc_id())) {
		tr_err(&zephyr_tr, "L3_HEAP available only for primary core!");
		return NULL;
	}

	key = k_spin_lock(&h->lock);
	ret = sys_heap_aligned_alloc(&h->heap, min_align, bytes);
	k_spin_unlock(&h->lock, key);

#if CONFIG_SYS_HEAP_RUNTIME_STATS && CONFIG_IPC_MAJOR_4
	sys_heap_runtime_stats_get(&h->heap, &stats);
	tr_info(&zephyr_tr, "heap allocated: %u free: %u max allocated: %u",
		stats.allocated_bytes, stats.free_bytes, stats.max_allocated_bytes);
#endif

	return ret;
}

static void l3_heap_free(struct k_heap *h, void *mem)
{
	if (!cpu_is_primary(arch_proc_id())) {
		tr_err(&zephyr_tr, "L3_HEAP available only for primary core!");
		return;
	}

	k_spinlock_key_t key = k_spin_lock(&h->lock);

	sys_heap_free(&h->heap, mem);
	k_spin_unlock(&h->lock, key);
}

#endif

static void *heap_alloc_aligned(struct k_heap *h, size_t min_align, size_t bytes)
{
	k_spinlock_key_t key;
	void *ret;
#if CONFIG_SYS_HEAP_RUNTIME_STATS && CONFIG_IPC_MAJOR_4
	struct sys_memory_stats stats;
#endif

	key = k_spin_lock(&h->lock);
	ret = sys_heap_aligned_alloc(&h->heap, min_align, bytes);
	k_spin_unlock(&h->lock, key);

#if CONFIG_SYS_HEAP_RUNTIME_STATS && CONFIG_IPC_MAJOR_4
	sys_heap_runtime_stats_get(&h->heap, &stats);
	tr_info(&zephyr_tr, "heap allocated: %u free: %u max allocated: %u",
		stats.allocated_bytes, stats.free_bytes, stats.max_allocated_bytes);
#endif

	return ret;
}

static void __sparse_cache *heap_alloc_aligned_cached(struct k_heap *h,
						      size_t min_align, size_t bytes)
{
	void __sparse_cache *ptr;

	/*
	 * Zephyr sys_heap stores metadata at start of each
	 * heap allocation. To ensure no allocated cached buffer
	 * overlaps the same cacheline with the metadata chunk,
	 * align both allocation start and size of allocation
	 * to cacheline. As cached and non-cached allocations are
	 * mixed, same rules need to be followed for both type of
	 * allocations.
	 */
#ifdef CONFIG_SOF_ZEPHYR_HEAP_CACHED
	min_align = MAX(PLATFORM_DCACHE_ALIGN, min_align);
	bytes = ALIGN_UP(bytes, min_align);
#endif

	ptr = (__sparse_force void __sparse_cache *)heap_alloc_aligned(h, min_align, bytes);

#ifdef CONFIG_SOF_ZEPHYR_HEAP_CACHED
	if (ptr)
		ptr = sys_cache_cached_ptr_get((__sparse_force void *)ptr);
#endif

	return ptr;
}

static void heap_free(struct k_heap *h, void *mem)
{
	k_spinlock_key_t key = k_spin_lock(&h->lock);
#ifdef CONFIG_SOF_ZEPHYR_HEAP_CACHED
	void *mem_uncached;

	if (is_cached(mem)) {
		mem_uncached = sys_cache_uncached_ptr_get((__sparse_force void __sparse_cache *)mem);
		sys_cache_data_flush_and_invd_range(mem,
				sys_heap_usable_size(&h->heap, mem_uncached));

		mem = mem_uncached;
	}
#endif

	sys_heap_free(&h->heap, mem);

	k_spin_unlock(&h->lock, key);
}

static inline bool zone_is_cached(enum mem_zone zone)
{
#ifdef CONFIG_SOF_ZEPHYR_HEAP_CACHED
	switch (zone) {
	case SOF_MEM_ZONE_SYS:
	case SOF_MEM_ZONE_SYS_RUNTIME:
	case SOF_MEM_ZONE_RUNTIME:
	case SOF_MEM_ZONE_BUFFER:
		return true;
	default:
		break;
	}
#endif

	return false;
}

void *rmalloc(enum mem_zone zone, uint32_t flags, uint32_t caps, size_t bytes)
{
	void *ptr;
	struct k_heap *heap;

	/* choose a heap */
	if (caps & SOF_MEM_CAPS_L3) {
#if CONFIG_L3_HEAP
		heap = &l3_heap;
		/* Uncached L3_HEAP should be not used */
		if (!zone_is_cached(zone)) {
			tr_err(&zephyr_tr, "L3_HEAP available for cached zones only!");
			return NULL;
		}
		ptr = (__sparse_force void *)l3_heap_alloc_aligned(heap, 0, bytes);

		if (!ptr && zone == SOF_MEM_ZONE_SYS)
			k_panic();

		return ptr;
#else
		k_panic();
#endif
	} else {
		heap = &sof_heap;
	}

	if (zone_is_cached(zone) && !(flags & SOF_MEM_FLAG_COHERENT)) {
		ptr = (__sparse_force void *)heap_alloc_aligned_cached(heap, 0, bytes);
	} else {
		/*
		 * XTOS alloc implementation has used dcache alignment,
		 * so SOF application code is expecting this behaviour.
		 */
		ptr = heap_alloc_aligned(heap, PLATFORM_DCACHE_ALIGN, bytes);
	}

	if (!ptr && zone == SOF_MEM_ZONE_SYS)
		k_panic();

	return ptr;
}
EXPORT_SYMBOL(rmalloc);

/* Use SOF_MEM_ZONE_BUFFER at the moment */
void *rbrealloc_align(void *ptr, uint32_t flags, uint32_t caps, size_t bytes,
		      size_t old_bytes, uint32_t alignment)
{
	void *new_ptr;

	if (!ptr) {
		/* TODO: Use correct zone */
		return rballoc_align(flags, caps, bytes, alignment);
	}

	/* Original version returns NULL without freeing this memory */
	if (!bytes) {
		/* TODO: Should we call rfree(ptr); */
		tr_err(&zephyr_tr, "realloc failed for 0 bytes");
		return NULL;
	}

	new_ptr = rballoc_align(flags, caps, bytes, alignment);
	if (!new_ptr)
		return NULL;

	if (!(flags & SOF_MEM_FLAG_NO_COPY))
		memcpy_s(new_ptr, bytes, ptr, MIN(bytes, old_bytes));

	rfree(ptr);

	tr_info(&zephyr_tr, "rbealloc: new ptr %p", new_ptr);

	return new_ptr;
}

/**
 * Similar to rmalloc(), guarantees that returned block is zeroed.
 *
 * @note Do not use  for buffers (SOF_MEM_ZONE_BUFFER zone).
 *       rballoc(), rballoc_align() to allocate memory for buffers.
 */
void *rzalloc(enum mem_zone zone, uint32_t flags, uint32_t caps, size_t bytes)
{
	void *ptr = rmalloc(zone, flags, caps, bytes);

	if (ptr)
		memset(ptr, 0, bytes);

	return ptr;
}
EXPORT_SYMBOL(rzalloc);

/**
 * Allocates memory block from SOF_MEM_ZONE_BUFFER.
 * @param flags see SOF_MEM_FLAG_...
 * @param caps Capabilities, see SOF_MEM_CAPS_...
 * @param bytes Size in bytes.
 * @param align Alignment in bytes.
 * @return Pointer to the allocated memory or NULL if failed.
 */
void *rballoc_align(uint32_t flags, uint32_t caps, size_t bytes,
		    uint32_t align)
{
	struct k_heap *heap;

	/* choose a heap */
	if (caps & SOF_MEM_CAPS_L3) {
#if CONFIG_L3_HEAP
		heap = &l3_heap;
		return (__sparse_force void *)l3_heap_alloc_aligned(heap, align, bytes);
#else
		tr_err(&zephyr_tr, "L3_HEAP not available.");
		return NULL;
#endif
	} else {
		heap = &sof_heap;
	}

	if (flags & SOF_MEM_FLAG_COHERENT)
		return heap_alloc_aligned(heap, align, bytes);

	return (__sparse_force void *)heap_alloc_aligned_cached(heap, align, bytes);
}
EXPORT_SYMBOL(rballoc_align);

/*
 * Free's memory allocated by above alloc calls.
 */
void rfree(void *ptr)
{
	if (!ptr)
		return;

#if CONFIG_L3_HEAP
	if (is_l3_heap_pointer(ptr)) {
		l3_heap_free(&l3_heap, ptr);
		return;
	}
#endif

	heap_free(&sof_heap, ptr);
}
EXPORT_SYMBOL(rfree);

static int heap_init(void)
{
	sys_heap_init(&sof_heap.heap, heapmem, HEAPMEM_SIZE);

#if CONFIG_L3_HEAP
	sys_heap_init(&l3_heap.heap, UINT_TO_POINTER(get_l3_heap_start()), get_l3_heap_size());
#endif

	return 0;
}

/* This is a weak stub for the Cadence libc's allocator (which is just
 * a newlib build).  It's traditionally been provided like this in SOF
 * for the benefit of C++ code where the standard library needs to
 * link to a working malloc() even if it will never call it.
 */
struct _reent;
__weak void *_sbrk_r(struct _reent *ptr, ptrdiff_t incr)
{
	k_panic();
	return NULL;
}

SYS_INIT(heap_init, PRE_KERNEL_1, CONFIG_KERNEL_INIT_PRIORITY_OBJECTS);
