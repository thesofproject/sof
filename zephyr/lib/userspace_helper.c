// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2025 Intel Corporation. All rights reserved.
//
// Author: Jaroslaw Stelter <jaroslaw.stelter@intel.com>
//	   Adrian Warecki <adrian.warecki@intel.com>

/**
 * \file
 * \brief Zephyr userspace helper functions
 * \authors Jaroslaw Stelter <jaroslaw.stelter@intel.com>
 * \authors Adrian Warecki <adrian.warecki@intel.com>
 */

#include <stdint.h>

#include <rtos/alloc.h>

#define DRV_HEAP_CACHED		CONFIG_SOF_ZEPHYR_HEAP_CACHED

#if CONFIG_USERSPACE
struct sys_heap *drv_heap_init(void)
{
	struct sys_heap *drv_heap = rballoc(0, SOF_MEM_CAPS_RAM, sizeof(struct sys_heap));
	if (!drv_heap)
		return NULL;

	void *mem = rballoc_align(SOF_MEM_FLAG_COHERENT, SOF_MEM_CAPS_RAM, DRV_HEAP_SIZE,
				  CONFIG_MM_DRV_PAGE_SIZE);
	if (!mem) {
		rfree(drv_heap);
		return NULL;
	}

	sys_heap_init(drv_heap, mem, DRV_HEAP_SIZE);
	drv_heap->init_mem = mem;
	drv_heap->init_bytes = DRV_HEAP_SIZE;

	return drv_heap;
}

static inline bool zone_is_cached(enum mem_zone zone)
{
#ifdef DRV_HEAP_CACHED
	switch (zone) {
	case SOF_MEM_ZONE_SYS:
	case SOF_MEM_ZONE_SYS_RUNTIME:
	case SOF_MEM_ZONE_RUNTIME:
	case SOF_MEM_ZONE_BUFFER:
		return true;
	default:
		return false;
	}
#endif /* DRV_HEAP_CACHED */
}

void *drv_heap_aligned_alloc(struct sys_heap *drv_heap, uint32_t flags,
			     uint32_t caps, size_t bytes, int32_t align)
{
#ifdef DRV_HEAP_CACHED
	const bool cached = (flags & SOF_MEM_FLAG_COHERENT) == 0;
#endif /* DRV_HEAP_CACHED */

	if (drv_heap) {
#ifdef DRV_HEAP_CACHED
		if (cached) {
			/*
			 * Zephyr sys_heap stores metadata at start of each
			 * heap allocation. To ensure no allocated cached buffer
			 * overlaps the same cacheline with the metadata chunk,
			 * align both allocation start and size of allocation
			 * to cacheline. As cached and non-cached allocations are
			 * mixed, same rules need to be followed for both type of
			 * allocations.
			 */
			align = MAX(PLATFORM_DCACHE_ALIGN, align);
			bytes = ALIGN_UP(bytes, align);
		}
#endif /* DRV_HEAP_CACHED */
		void *mem = sys_heap_aligned_alloc(drv_heap, align, bytes);
#ifdef DRV_HEAP_CACHED
		if (!cached)
			return mem;
		return sys_cache_cached_ptr_get(mem);
#else	/* !DRV_HEAP_CACHED */
		return mem;
#endif	/* DRV_HEAP_CACHED */
	} else
		return rballoc_align(flags, caps, bytes, align);
}

void *drv_heap_rmalloc(struct sys_heap *drv_heap, enum mem_zone zone,
		       uint32_t flags, uint32_t caps, size_t bytes)
{
	int32_t align = 0;

	if (drv_heap) {
#ifdef DRV_HEAP_CACHED
		if (!zone_is_cached(zone))
			flags |= SOF_MEM_FLAG_COHERENT;
#endif /* DRV_HEAP_CACHED */
		return drv_heap_aligned_alloc(drv_heap, flags, caps, bytes, 0);
	} else
		return rmalloc(zone, flags, caps, bytes);
}

void *drv_heap_rzalloc(struct sys_heap *drv_heap, enum mem_zone zone,
		       uint32_t flags, uint32_t caps, size_t bytes)
{
	void *ptr;

	ptr = drv_heap_rmalloc(drv_heap, zone, flags, caps, bytes);
	if (ptr)
		memset(ptr, 0, bytes);

	return ptr;
}

void drv_heap_free(struct sys_heap *drv_heap, void *mem)
{
	if (drv_heap) {
#ifdef DRV_HEAP_CACHED
		void *mem_uncached;

		if (is_cached(mem)) {
			mem_uncached = sys_cache_uncached_ptr_get(
				(__sparse_force void __sparse_cache *)mem);
			sys_cache_data_flush_and_invd_range(mem,
							    sys_heap_usable_size(drv_heap, mem_uncached));

			mem = mem_uncached;
		}
#endif
		sys_heap_free(drv_heap, sys_cache_uncached_ptr_get(mem));
	} else {
		rfree(mem);
	}
}

void drv_heap_remove(struct sys_heap *drv_heap)
{
	if (drv_heap)
		rfree(drv_heap->init_mem);
	rfree(drv_heap);
}

#else /* CONFIG_USERSPACE */

void *drv_heap_rmalloc(struct sys_heap *drv_heap, enum mem_zone zone,
		   uint32_t flags, uint32_t caps, size_t bytes)
{
	return rmalloc(zone, flags, caps, bytes);
}


void *drv_heap_aligned_alloc(struct sys_heap *drv_heap, uint32_t flags, uint32_t caps,
			     size_t bytes, int32_t align)
{
	return rballoc_align(flags, caps, bytes, align);
}


void *drv_heap_rzalloc(struct sys_heap *drv_heap, enum mem_zone zone, uint32_t flags, uint32_t caps,
		       size_t bytes)
{
	return rzalloc(zone, flags, caps, bytes);
}

void drv_heap_free(struct sys_heap *drv_heap, void *mem)
{
	rfree(mem);
}

void drv_heap_remove(struct sys_heap *drv_heap)
{ }

#endif /* CONFIG_USERSPACE */
