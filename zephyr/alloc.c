// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2020 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#include <sof/lib/alloc.h>

/* Zephyr includes */
#include <device.h>
#include <init.h>

#ifndef CONFIG_KERNEL_COHERENCE
#include <arch/xtensa/cache.h>
#endif

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
#define HEAPMEM_SIZE		(HEAP_SYSTEM_SIZE + HEAP_RUNTIME_SIZE + HEAP_BUFFER_SIZE)
/*
 * Include heapmem variable in .heap_mem section, otherwise the HEAPMEM_SIZE is
 * duplicated in two sections and the sdram0 region overflows.
 */
__section(".heap_mem") static uint8_t __aligned(64) heapmem[HEAPMEM_SIZE];
#else
#define HEAPMEM_SIZE		HEAP_BUFFER_SIZE
#define HEAPMEM_SHARED_SIZE	(HEAP_SYSTEM_SIZE + HEAP_RUNTIME_SIZE + \
				HEAP_RUNTIME_SHARED_SIZE + HEAP_SYSTEM_SHARED_SIZE)

static uint8_t __aligned(PLATFORM_DCACHE_ALIGN)heapmem[HEAPMEM_SIZE];
static uint8_t __aligned(PLATFORM_DCACHE_ALIGN)heapmem_shared[HEAPMEM_SHARED_SIZE];
#endif

/* Use k_heap structure */
static struct k_heap sof_heap;
static struct k_heap sof_heap_shared;

static int statics_init(const struct device *unused)
{
	ARG_UNUSED(unused);

	sys_heap_init(&sof_heap.heap, heapmem, HEAPMEM_SIZE);
#ifndef CONFIG_IMX
	sys_heap_init(&sof_heap_shared.heap, heapmem_shared, HEAPMEM_SHARED_SIZE);
#endif

	return 0;
}

SYS_INIT(statics_init, PRE_KERNEL_1, CONFIG_KERNEL_INIT_PRIORITY_OBJECTS);

static void *heap_alloc_aligned(struct k_heap *h, size_t align, size_t bytes)
{
	void *ret = NULL;

	k_spinlock_key_t key = k_spin_lock(&h->lock);

	ret = sys_heap_aligned_alloc(&h->heap, align, bytes);

	k_spin_unlock(&h->lock, key);

	return ret;
}

static void *heap_alloc_aligned_cached(struct k_heap *h, size_t min_align, size_t bytes)
{
	unsigned int align = MAX(PLATFORM_DCACHE_ALIGN, min_align);
	unsigned int aligned_size = ALIGN_UP(bytes, align);
	void *ptr;

	/*
	 * Zephyr sys_heap stores metadata at start of each
	 * heap allocation. To ensure no allocated cached buffer
	 * overlaps the same cacheline with the metadata chunk,
	 * align both allocation start and size of allocation
	 * to cacheline.
	 */
	ptr = heap_alloc_aligned(h, align, aligned_size);

	if (ptr) {
		ptr = uncache_to_cache(ptr);

		/*
		 * Heap can be used by different cores, so cache
		 * needs to be invalidated before next user
		 */
		z_xtensa_cache_inv(ptr, aligned_size);
	}

	return ptr;
}

static void heap_free(struct k_heap *h, void *mem)
{
	k_spinlock_key_t key = k_spin_lock(&h->lock);

	sys_heap_free(&h->heap, mem);

	k_spin_unlock(&h->lock, key);
}

static inline bool zone_is_cached(enum mem_zone zone)
{
	if (zone == SOF_MEM_ZONE_BUFFER)
		return true;

	return false;
}

void *rmalloc(enum mem_zone zone, uint32_t flags, uint32_t caps, size_t bytes)
{
	if (zone_is_cached(zone))
		return heap_alloc_aligned_cached(&sof_heap, 0, bytes);

#ifdef CONFIG_IMX
	return heap_alloc_aligned(&sof_heap, 8, bytes);
#else
	return heap_alloc_aligned(&sof_heap_shared, 8, bytes);
#endif
}

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
		memcpy(new_ptr, ptr, MIN(bytes, old_bytes));

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

	memset(ptr, 0, bytes);

	return ptr;
}

/**
 * Allocates memory block from SOF_MEM_ZONE_BUFFER.
 * @param flags Flags, see SOF_MEM_FLAG_...
 * @param caps Capabilities, see SOF_MEM_CAPS_...
 * @param bytes Size in bytes.
 * @param alignment Alignment in bytes.
 * @return Pointer to the allocated memory or NULL if failed.
 */
void *rballoc_align(uint32_t flags, uint32_t caps, size_t bytes,
		    uint32_t alignment)
{
	return heap_alloc_aligned_cached(&sof_heap, alignment, bytes);
}

/*
 * Free's memory allocated by above alloc calls.
 */
void rfree(void *ptr)
{
	if (!ptr)
		return;

	/* select heap based on address range */
	if (is_uncached(ptr)) {
		heap_free(&sof_heap_shared, ptr);
		return;
	}

	ptr = cache_to_uncache(ptr);
	heap_free(&sof_heap, ptr);
}

/* debug only - only needed for linking */
void heap_trace_all(int force)
{
}
