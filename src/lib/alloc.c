// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>

#include <rtos/panic.h>
#include <rtos/alloc.h>
#include <rtos/cache.h>
#include <sof/lib/cpu.h>
#include <sof/lib/dma.h>
#include <sof/lib/memory.h>
#include <sof/lib/mm_heap.h>
#include <sof/lib/uuid.h>
#include <sof/math/numbers.h>
#include <rtos/spinlock.h>
#include <rtos/string.h>
#include <ipc/topology.h>
#include <ipc/trace.h>

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

LOG_MODULE_REGISTER(memory, CONFIG_SOF_LOG_LEVEL);

SOF_DEFINE_REG_UUID(mem);

DECLARE_TR_CTX(mem_tr, SOF_UUID(mem_uuid), LOG_LEVEL_INFO);

/* debug to set memory value on every allocation */
#if CONFIG_DEBUG_BLOCK_FREE
#define DEBUG_BLOCK_FREE_VALUE_8BIT ((uint8_t)0xa5)
#define DEBUG_BLOCK_FREE_VALUE_32BIT ((uint32_t)0xa5a5a5a5)
#endif

/* We have 3 memory pools
 *
 * 1) System memory pool does not have a map and it's size is fixed at build
 *    time. Memory cannot be freed from this pool. Used by device drivers
 *    and any system core. Saved as part of PM context.
 * 2) Runtime memory pool has variable size allocation map and memory is freed
 *    on calls to rfree(). Saved as part of PM context. Global size
 *    set at build time.
 * 3) Buffer memory pool has fixed size allocation map and can be freed on
 *    module removal or calls to rfree(). Saved as part of PM context.
 */

#if CONFIG_DEBUG_BLOCK_FREE
/* Check whole memory region for debug pattern to find if memory was freed
 * second time
 */
static void validate_memory(void *ptr, size_t size)
{
	uint32_t *ptr_32 = ptr;
	int i, not_matching = 0;

	for (i = 0; i < size / 4; i++) {
		if (ptr_32[i] != DEBUG_BLOCK_FREE_VALUE_32BIT)
			not_matching = 1;
	}

	if (not_matching) {
		tr_info(&mem_tr, "validate_memory() pointer: %p freed pattern not detected",
			ptr);
	} else {
		tr_err(&mem_tr, "validate_memory() freeing pointer: %p double free detected",
		       ptr);
	}
}
#endif

#if CONFIG_DEBUG_BLOCK_FREE
static void write_pattern(struct mm_heap *heap_map, int heap_depth,
			  uint8_t pattern)
{
	struct mm_heap *heap;
	struct block_map *current_map;
	int i, j;

	for (i = 0; i < heap_depth; i++) {
		heap = &heap_map[i];

		for (j = 0; j < heap->blocks; j++) {
			current_map = &heap->map[j];
			memset(
				(void *)current_map->base, pattern,
				current_map->count * current_map->block_size);
		}

	}
}
#endif

static void init_heap_map(struct mm_heap *heap, int count)
{
	struct block_map *next_map;
	struct block_map *current_map;
	int i;
	int j;

	for (i = 0; i < count; i++) {
		/* init the map[0] */
		current_map = &heap[i].map[0];
		current_map->base = heap[i].heap;

		/* map[j]'s base is calculated based on map[j-1] */
		for (j = 1; j < heap[i].blocks; j++) {
			next_map = &heap[i].map[j];
			next_map->base = current_map->base +
				current_map->block_size *
				current_map->count;

			current_map = &heap[i].map[j];
		}

	}
}

/* allocate from system memory pool */
static void *rmalloc_sys(struct mm_heap *heap, uint32_t flags, int caps, size_t bytes)
{
	void *ptr;
	size_t alignment = 0;

	if ((heap->caps & caps) != caps)
		sof_panic(SOF_IPC_PANIC_MEM);

	/* align address to dcache line size */
	if (heap->info.used % PLATFORM_DCACHE_ALIGN)
		alignment = PLATFORM_DCACHE_ALIGN -
			(heap->info.used % PLATFORM_DCACHE_ALIGN);

	/* always succeeds or panics */
	if (alignment + bytes > heap->info.free) {
		tr_err(&mem_tr, "rmalloc_sys(): core = %d, bytes = %d",
		       cpu_get_id(), bytes);
		sof_panic(SOF_IPC_PANIC_MEM);
	}
	heap->info.used += alignment;

	ptr = (void *)(heap->heap + heap->info.used);

	heap->info.used += bytes;
	heap->info.free -= alignment + bytes;

	return ptr;
}

/* At this point the pointer we have should be unaligned
 * (it was checked level higher) and be power of 2
 */
static void *align_ptr(struct mm_heap *heap, uint32_t alignment,
		       void *ptr, struct block_hdr *hdr)
{
	/* Save unaligned ptr to block hdr */
	hdr->unaligned_ptr = ptr;

	/* If ptr is not already aligned we calculate alignment shift */
	if (alignment <= 1)
		return ptr;

	return (void *)ALIGN_UP((uintptr_t)ptr, alignment);
}

/* allocate single block */
static void *alloc_block_index(struct mm_heap *heap, int level,
			       uint32_t alignment, int index)
{
	struct block_map *map = &heap->map[level];
	struct block_hdr *hdr;
	void *ptr;
	int i;

	if (index < 0)
		index = map->first_free;

	map->free_count--;

	hdr = &map->block[index];
	ptr = (void *)(map->base + index * map->block_size);
	ptr = align_ptr(heap, alignment, ptr, hdr);

	hdr->size = 1;
	hdr->used = 1;

	heap->info.used += map->block_size;
	heap->info.free -= map->block_size;

	if (index == map->first_free)
		/* find next free */
		for (i = map->first_free; i < map->count; ++i) {
			hdr = &map->block[i];

			if (hdr->used == 0) {
				map->first_free = i;
				break;
			}
		}

	return ptr;
}

static void *alloc_block(struct mm_heap *heap, int level,
			 uint32_t caps, uint32_t alignment)
{
	return alloc_block_index(heap, level, alignment, -1);
}

/* allocates continuous blocks */
static void *alloc_cont_blocks(struct mm_heap *heap, int level,
			       uint32_t caps, size_t bytes, uint32_t alignment)
{
	struct block_map *map = &heap->map[level];
	struct block_hdr *hdr;
	void *ptr = NULL, *unaligned_ptr;
	unsigned int current;
	unsigned int count = 0;			/* keep compiler quiet */
	unsigned int start = 0;			/* keep compiler quiet */
	uintptr_t blk_start = 0, aligned = 0;	/* keep compiler quiet */
	size_t found = 0, total_bytes = bytes;

	/* check if we have enough consecutive blocks for requested
	 * allocation size.
	 */
	if ((map->count - map->first_free) * map->block_size < bytes)
		return NULL;

	/*
	 * Walk all blocks in the map, beginning with the first free one, until
	 * a sufficiently large sequence is found, in which the first block
	 * contains an address with the requested alignment.
	 */
	for (current = map->first_free, hdr = map->block + current;
	     current < map->count && found < total_bytes;
	     current++, hdr++) {
		if (hdr->used) {
			/* Restart the search */
			found = 0;
			count = 0;
			total_bytes = bytes;
			continue;
		}

		if (!found) {
			/* A possible beginning of a sequence */
			blk_start = map->base + current * map->block_size;
			start = current;

			/* Check if we can start a sequence here */
			if (alignment) {
				aligned = ALIGN_UP(blk_start, alignment);

				if (blk_start & (alignment - 1) &&
				    aligned >= blk_start + map->block_size)
					/*
					 * This block doesn't contain an address
					 * with required alignment, it is useless
					 * as the beginning of the sequence
					 */
					continue;

				/*
				 * Found a potentially suitable beginning of a
				 * sequence, from here we'll check if we get
				 * enough blocks
				 */
				total_bytes += aligned - blk_start;
			} else {
				aligned = blk_start;
			}
		}

		count++;
		found += map->block_size;
	}

	if (found < total_bytes) {
		tr_err(&mem_tr, "failed to allocate %u", total_bytes);
		goto out;
	}

	ptr = (void *)aligned;

	/* we found enough space, let's allocate it */
	map->free_count -= count;
	unaligned_ptr = (void *)blk_start;

	hdr = &map->block[start];
	hdr->size = count;

	heap->info.used += count * map->block_size;
	heap->info.free -= count * map->block_size;

	/*
	 * if .first_free has to be updated, set it to first free block or past
	 * the end of the map
	 */
	if (map->first_free == start) {
		for (current = map->first_free + count, hdr = &map->block[current];
		     current < map->count && hdr->used;
		     current++, hdr++)
			;

		map->first_free = current;
	}

	/* update each block */
	for (current = start; current < start + count; current++) {
		hdr = &map->block[current];
		hdr->used = 1;
		hdr->unaligned_ptr = unaligned_ptr;
	}

out:

	return ptr;
}

static inline struct mm_heap *find_in_heap_arr(struct mm_heap *heap_arr, int arr_len, void *ptr)
{
	struct mm_heap *heap;
	int i;

	for (i = 0; i < arr_len; i++) {
		heap = &heap_arr[i];
		if ((uint32_t)ptr >= heap->heap &&
		    (uint32_t)ptr < heap->heap + heap->size)
			return heap;
	}
	return NULL;
}

static struct mm_heap *get_heap_from_ptr(void *ptr)
{
	struct mm *memmap = memmap_get();
	struct mm_heap *heap;

	/* find mm_heap that ptr belongs to */
	heap = find_in_heap_arr(memmap->system_runtime + cpu_get_id(), 1, ptr);
	if (heap)
		goto out;

	heap = find_in_heap_arr(memmap->runtime, PLATFORM_HEAP_RUNTIME, ptr);
	if (heap)
		goto out;

#if CONFIG_CORE_COUNT > 1
	heap = find_in_heap_arr(memmap->runtime_shared, PLATFORM_HEAP_RUNTIME_SHARED, ptr);
	if (heap)
		goto out;
#endif

	heap = find_in_heap_arr(memmap->buffer, PLATFORM_HEAP_BUFFER, ptr);
	if (heap)
		goto out;

	return NULL;

out:

	return heap;
}

static struct mm_heap *get_heap_from_caps(struct mm_heap *heap, int count,
					  uint32_t caps)
{
	uint32_t mask;
	int i;

	/* find first heap that support type */
	for (i = 0; i < count; i++) {
		mask = heap[i].caps & caps;
		if (mask == caps)
			return &heap[i];
	}

	return NULL;
}

static void *get_ptr_from_heap(struct mm_heap *heap, uint32_t flags,
			       uint32_t caps, size_t bytes, uint32_t alignment)
{
	struct block_map *map;
	int i, temp_bytes = bytes;
	void *ptr = NULL;

	/* Only allow alignment as a power of 2 */
	if ((alignment & (alignment - 1)) != 0)
		sof_panic(SOF_IPC_PANIC_MEM);

	for (i = 0; i < heap->blocks; i++) {
		map = &heap->map[i];

		/* size of requested buffer is adjusted for alignment purposes
		 * we check if first free block is already aligned if not
		 * we need to allocate bigger size for alignment
		 */
		if (alignment &&
		    ((map->base + (map->block_size * map->first_free)) %
		     alignment))
			temp_bytes += alignment;

		/* is block big enough */
		if (map->block_size < temp_bytes) {
			temp_bytes = bytes;
			continue;
		}

		/* does block have free space */
		if (map->free_count == 0) {
			temp_bytes = bytes;
			continue;
		}

		/* free block space exists */
		ptr = alloc_block(heap, i, caps, alignment);

		break;
	}

	return ptr;
}

/* free block(s) */
static void free_block(void *ptr)
{
	struct mm_heap *heap;
	struct block_map *block_map = NULL;
	struct block_hdr *hdr;
	void *cached_ptr = uncache_to_cache(ptr);
	void *uncached_ptr = cache_to_uncache(ptr);
	void *free_ptr;
	int i;
	int block;
	int used_blocks;
	bool heap_is_full;

	/* try cached_ptr first */
	heap = get_heap_from_ptr(cached_ptr);

	/* try uncached_ptr if needed */
	if (!heap) {
		heap = get_heap_from_ptr(uncached_ptr);
		if (!heap) {
			tr_err(&mem_tr, "free_block(): invalid heap, ptr = %p, cpu = %d",
			       ptr, cpu_get_id());
			return;
		}
		free_ptr = uncached_ptr;
	} else {
		free_ptr = cached_ptr;
	}

	/* find block that ptr belongs to */
	for (i = 0; i < heap->blocks; i++) {
		block_map = &heap->map[i];

		/* is ptr in this block */
		if ((uint32_t)free_ptr < (block_map->base +
		    (block_map->block_size * block_map->count)))
			break;

	}

	if (i == heap->blocks) {

		/* not found */
		tr_err(&mem_tr, "free_block(): invalid free_ptr = %p cpu = %d",
		       free_ptr, cpu_get_id());
		return;
	}

	/* calculate block header */
	block = ((uint32_t)free_ptr - block_map->base) / block_map->block_size;

	hdr = &block_map->block[block];

	/* bring back original unaligned pointer position
	 * and calculate correct hdr for free operation (it could
	 * be from different block since we got user pointer here
	 * or null if header was not set)
	 */
	if (hdr->unaligned_ptr != free_ptr && hdr->unaligned_ptr) {
		free_ptr = hdr->unaligned_ptr;
		block = ((uint32_t)free_ptr - block_map->base)
			 / block_map->block_size;
		hdr = &block_map->block[block];
	}

	/* report an error if ptr is not aligned to block */
	if (block_map->base + block_map->block_size * block != (uint32_t)free_ptr)
		sof_panic(SOF_IPC_PANIC_MEM);

	/* There may still be live dirty cache lines in the region
	 * on the current core. Those must be invalidated, otherwise
	 * they will be evicted from the cache at some point in the
	 * future, on top of the memory region now being used for
	 * different purposes on another core.
	 */
	dcache_writeback_invalidate_region(ptr, block_map->block_size * hdr->size);

	heap_is_full = !block_map->free_count;

	/* free block header and continuous blocks */
	used_blocks = block + hdr->size;

	for (i = block; i < used_blocks; i++) {
		hdr = &block_map->block[i];
		hdr->size = 0;
		hdr->used = 0;
		hdr->unaligned_ptr = NULL;
		block_map->free_count++;
		heap->info.used -= block_map->block_size;
		heap->info.free += block_map->block_size;
	}

	/* set first free block */
	if (block < block_map->first_free || heap_is_full)
		block_map->first_free = block;

#if CONFIG_DEBUG_BLOCK_FREE
	/* memset the whole block in case of unaligned ptr */
	validate_memory(
		(void *)(block_map->base + block_map->block_size * block),
		block_map->block_size * (i - block));
	memset(
		(void *)(block_map->base + block_map->block_size * block),
		DEBUG_BLOCK_FREE_VALUE_8BIT, block_map->block_size *
		(i - block));
#endif
}

#if CONFIG_TRACE
void heap_trace(struct mm_heap *heap, int size)
{
	struct block_map *current_map;
	int i;
	int j;

	for (i = 0; i < size; i++) {
		tr_info(&mem_tr, " heap: 0x%x size %d blocks %d caps 0x%x",
			heap->heap, heap->size, heap->blocks,
			heap->caps);
		tr_info(&mem_tr, "  (In Bytes) used %d free %d", heap->info.used,
			heap->info.free);

		/* map[j]'s base is calculated based on map[j-1] */
		for (j = 0; j < heap->blocks; j++) {
			current_map = &heap->map[j];

			tr_info(&mem_tr, " %d Bytes blocks ID:%d base 0x%x",
				current_map->block_size, j, current_map->base);
			tr_info(&mem_tr, "   Number of Blocks: total %d used %d free %d",
				current_map->count,
				(current_map->count - current_map->free_count),
				current_map->free_count);
		}

		heap++;
	}
}

void heap_trace_all(int force)
{
	struct mm *memmap = memmap_get();

	/* has heap changed since last shown */
	if (memmap->heap_trace_updated || force) {
		tr_info(&mem_tr, "heap: system status");
		heap_trace(memmap->system, PLATFORM_HEAP_SYSTEM);
		tr_info(&mem_tr, "heap: system runtime status");
		heap_trace(memmap->system_runtime, PLATFORM_HEAP_SYSTEM_RUNTIME);
		tr_info(&mem_tr, "heap: buffer status");
		heap_trace(memmap->buffer, PLATFORM_HEAP_BUFFER);
		tr_info(&mem_tr, "heap: runtime status");
		heap_trace(memmap->runtime, PLATFORM_HEAP_RUNTIME);
#if CONFIG_CORE_COUNT > 1
		tr_info(&mem_tr, "heap: runtime shared status");
		heap_trace(memmap->runtime_shared, PLATFORM_HEAP_RUNTIME_SHARED);
		tr_info(&mem_tr, "heap: system shared status");
		heap_trace(memmap->system_shared, PLATFORM_HEAP_SYSTEM_SHARED);
#endif
	}

	memmap->heap_trace_updated = 0;

}
#else
void heap_trace_all(int force) { }
void heap_trace(struct mm_heap *heap, int size) { }
#endif

#define _ALLOC_FAILURE(bytes, zone, caps, flags) \
	tr_err(&mem_tr, \
	       "failed to alloc 0x%x bytes zone 0x%x caps 0x%x flags 0x%x", \
	       bytes, zone, caps, flags)

#if CONFIG_DEBUG_HEAP
#define DEBUG_TRACE_PTR(ptr, bytes, zone, caps, flags) do { \
		if (trace_get()) { \
			if (!(ptr)) \
				_ALLOC_FAILURE(bytes, zone, caps, flags); \
			heap_trace_all(0); \
		} \
	} while (0)
#else
#define DEBUG_TRACE_PTR(ptr, bytes, zone, caps, flags) do { \
		if (trace_get()) { \
			if (!(ptr)) { \
				_ALLOC_FAILURE(bytes, zone, caps, flags); \
				heap_trace_all(0); \
			} \
		} \
	} while (0)
#endif

/* allocate single block for system runtime */
static void *rmalloc_sys_runtime(uint32_t flags, int caps, int core,
				 size_t bytes)
{
	struct mm *memmap = memmap_get();
	struct mm_heap *cpu_heap;
	void *ptr;

	/* use the heap dedicated for the selected core */
	cpu_heap = memmap->system_runtime + core;
	if ((cpu_heap->caps & caps) != caps)
		sof_panic(SOF_IPC_PANIC_MEM);

	ptr = get_ptr_from_heap(cpu_heap, flags, caps, bytes,
				PLATFORM_DCACHE_ALIGN);

	return ptr;
}

/* allocate single block for runtime */
static void *rmalloc_runtime(uint32_t flags, uint32_t caps, size_t bytes)
{
	struct mm *memmap = memmap_get();
	struct mm_heap *heap;

	/* check runtime heap for capabilities */
	heap = get_heap_from_caps(memmap->runtime, PLATFORM_HEAP_RUNTIME, caps);
	if (!heap) {
		/* next check buffer heap for capabilities */
		heap = get_heap_from_caps(memmap->buffer, PLATFORM_HEAP_BUFFER,
					  caps);
		if (!heap) {

			tr_err(&mem_tr, "rmalloc_runtime(): caps = %x, bytes = %d",
			       caps, bytes);

			return NULL;
		}
	}

	return get_ptr_from_heap(heap, flags, caps, bytes,
				 PLATFORM_DCACHE_ALIGN);
}

#if CONFIG_CORE_COUNT > 1
/* allocate single block for shared */
static void *rmalloc_runtime_shared(uint32_t flags, uint32_t caps, size_t bytes)
{
	struct mm *memmap = memmap_get();
	struct mm_heap *heap;

	/* check shared heap for capabilities */
	heap = get_heap_from_caps(memmap->runtime_shared, PLATFORM_HEAP_RUNTIME_SHARED, caps);
	if (!heap) {
		tr_err(&mem_tr, "rmalloc_runtime_shared(): caps = %x, bytes = %d", caps, bytes);
		return NULL;
	}

	return get_ptr_from_heap(heap, flags, caps, bytes, PLATFORM_DCACHE_ALIGN);
}
#endif

static void *_malloc_unlocked(enum mem_zone zone, uint32_t flags, uint32_t caps,
			      size_t bytes)
{
	struct mm *memmap = memmap_get();
	void *ptr = NULL;

	switch (zone) {
	case SOF_MEM_ZONE_SYS:
		ptr = rmalloc_sys(memmap->system + cpu_get_id(), flags, caps, bytes);
		break;
	case SOF_MEM_ZONE_SYS_RUNTIME:
		ptr = rmalloc_sys_runtime(flags, caps, cpu_get_id(), bytes);
		break;
	case SOF_MEM_ZONE_RUNTIME:
		ptr = rmalloc_runtime(flags, caps, bytes);
		break;
#if CONFIG_CORE_COUNT > 1
	case SOF_MEM_ZONE_RUNTIME_SHARED:
		ptr = rmalloc_runtime_shared(flags, caps, bytes);
		break;
	case SOF_MEM_ZONE_SYS_SHARED:
		ptr = rmalloc_sys(memmap->system_shared, flags, caps, bytes);
		break;
#else
	case SOF_MEM_ZONE_RUNTIME_SHARED:
		ptr = rmalloc_runtime(flags, caps, bytes);
		break;
	case SOF_MEM_ZONE_SYS_SHARED:
		ptr = rmalloc_sys(memmap->system, flags, caps, bytes);
		break;
#endif

	default:
		tr_err(&mem_tr, "rmalloc(): invalid zone");
		sof_panic(SOF_IPC_PANIC_MEM); /* logic non recoverable problem */
		break;
	}

#if CONFIG_DEBUG_BLOCK_FREE
	if (ptr)
		bzero(ptr, bytes);
#endif

	memmap->heap_trace_updated = 1;

	return ptr;
}

void *rmalloc(enum mem_zone zone, uint32_t flags, uint32_t caps, size_t bytes)
{
	struct mm *memmap = memmap_get();
	k_spinlock_key_t key;
	void *ptr = NULL;

	key = k_spin_lock(&memmap->lock);

	ptr = _malloc_unlocked(zone, flags, caps, bytes);

	k_spin_unlock(&memmap->lock, key);

	DEBUG_TRACE_PTR(ptr, bytes, zone, caps, flags);
	return ptr;
}

/* allocates and clears memory - not for direct use, clients use rzalloc() */
void *rzalloc(enum mem_zone zone, uint32_t flags, uint32_t caps, size_t bytes)
{
	void *ptr;

	ptr = rmalloc(zone, flags, caps, bytes);
	if (ptr)
		bzero(ptr, bytes);

	return ptr;
}

void *rzalloc_core_sys(int core, size_t bytes)
{
	struct mm *memmap = memmap_get();
	k_spinlock_key_t key;
	void *ptr = NULL;

	key = k_spin_lock(&memmap->lock);

	ptr = rmalloc_sys(memmap->system + core, 0, 0, bytes);
	if (ptr)
		bzero(ptr, bytes);

	k_spin_unlock(&memmap->lock, key);
	return ptr;
}

/* allocates continuous buffers - not for direct use, clients use rballoc() */
static void *alloc_heap_buffer(struct mm_heap *heap, uint32_t flags,
			       uint32_t caps, size_t bytes, uint32_t alignment)
{
	struct block_map *map;
#if CONFIG_DEBUG_BLOCK_FREE
	unsigned int temp_bytes = bytes;
#endif
	unsigned int j;
	int i;
	void *ptr = NULL;

	/* Only allow alignment as a power of 2 */
	if ((alignment & (alignment - 1)) != 0)
		sof_panic(SOF_IPC_PANIC_MEM);

	/*
	 * There are several cases when a memory allocation request can be
	 * satisfied with one buffer:
	 * 1. allocate 30 bytes 32-byte aligned from 32 byte buffers. Any free
	 * buffer is acceptable, the beginning of the buffer is used.
	 * 2. allocate 30 bytes 256-byte aligned from 0x180 byte buffers. 1
	 * buffer is also always enough, but in some buffers a part of the
	 * buffer has to be skipped.
	 * 3. allocate 200 bytes 256-byte aligned from 0x180 byte buffers. 1
	 * buffer is enough, but not every buffer is suitable.
	 */

	/* will request fit in single block */
	for (i = 0, map = heap->map; i < heap->blocks; i++, map++) {
		struct block_hdr *hdr;
		uintptr_t free_start;

		if (map->block_size < bytes || !map->free_count)
			continue;

		if (alignment <= 1) {
			/* found: grab a block */
			ptr = alloc_block(heap, i, caps, alignment);
			break;
		}

		/*
		 * Usually block sizes are a power of 2 and all blocks are
		 * respectively aligned. But it's also possible to have
		 * non-power of 2 sized blocks, e.g. to optimize for typical
		 * ALSA allocations a map with 0x180 byte buffers can be used.
		 * For performance reasons we could first check the power-of-2
		 * case. This can be added as an optimization later.
		 */
		for (j = map->first_free, hdr = map->block + j,
		     free_start = map->base + map->block_size * j;
		     j < map->count;
		     j++, hdr++, free_start += map->block_size) {
			uintptr_t aligned;

			if (hdr->used)
				continue;

			aligned = ALIGN_UP(free_start, alignment);

			if (aligned + bytes > free_start + map->block_size)
				continue;

			/* Found, alloc_block_index() cannot fail */
			ptr = alloc_block_index(heap, i, alignment, j);
#if CONFIG_DEBUG_BLOCK_FREE
			temp_bytes += aligned - free_start;
#endif
			break;
		}

		if (ptr)
			break;
	}

	/* request spans > 1 block */
	if (!ptr) {
		/* size of requested buffer is adjusted for alignment purposes
		 * since we span more blocks we have to assume worst case scenario
		 */
		bytes += alignment;

		if (heap->size < bytes)
			return NULL;

		/*
		 * Find the best block size for request. We know, that we failed
		 * to find a single large enough block, so, skip those.
		 */
		for (i = heap->blocks - 1; i >= 0; i--) {
			map = &heap->map[i];

			/* allocate if block size is smaller than request */
			if (map->block_size < bytes) {
				ptr = alloc_cont_blocks(heap, i, caps,
							bytes, alignment);
				if (ptr)
					break;
			}
		}
	}

#if CONFIG_DEBUG_BLOCK_FREE
	if (ptr)
		bzero(ptr, temp_bytes);
#endif

	return ptr;
}

static void *_balloc_unlocked(uint32_t flags, uint32_t caps, size_t bytes,
			      uint32_t alignment)
{
	struct mm *memmap = memmap_get();
	struct mm_heap *heap;
	unsigned int i, n;
	void *ptr = NULL;

	for (i = 0, n = PLATFORM_HEAP_BUFFER, heap = memmap->buffer;
	     i < PLATFORM_HEAP_BUFFER;
	     i = heap - memmap->buffer + 1, n = PLATFORM_HEAP_BUFFER - i,
	     heap++) {
		heap = get_heap_from_caps(heap, n, caps);
		if (!heap)
			break;

		ptr = alloc_heap_buffer(heap, flags, caps, bytes, alignment);
		if (ptr)
			break;

		/* Continue from the next heap */
	}

	/* return directly if allocation failed */
	if (!ptr)
		return ptr;

#ifdef	CONFIG_DEBUG_FORCE_COHERENT_BUFFER
	return cache_to_uncache(ptr);
#else
	return (flags & SOF_MEM_FLAG_COHERENT) && (CONFIG_CORE_COUNT > 1) ?
		cache_to_uncache(ptr) : uncache_to_cache(ptr);
#endif
}

/* allocates continuous buffers - not for direct use, clients use rballoc() */
void *rballoc_align(uint32_t flags, uint32_t caps, size_t bytes,
		    uint32_t alignment)
{
	struct mm *memmap = memmap_get();
	void *ptr = NULL;
	k_spinlock_key_t key;

	key = k_spin_lock(&memmap->lock);

	ptr = _balloc_unlocked(flags, caps, bytes, alignment);

	k_spin_unlock(&memmap->lock, key);

	DEBUG_TRACE_PTR(ptr, bytes, SOF_MEM_ZONE_BUFFER, caps, flags);
	return ptr;
}

static void _rfree_unlocked(void *ptr)
{
	struct mm *memmap = memmap_get();
	struct mm_heap *heap;

	/* sanity check - NULL ptrs are fine */
	if (!ptr)
		return;

	/* prepare pointer if it's platform requirement */
	ptr = platform_rfree_prepare(ptr);

	/* use the heap dedicated for the core or shared memory */
#if CONFIG_CORE_COUNT > 1
	if (is_uncached(ptr))
		heap = memmap->system_shared;
	else
		heap = memmap->system + cpu_get_id();
#else
	heap = memmap->system;
#endif

	/* panic if pointer is from system heap */
	if (ptr >= (void *)heap->heap &&
	    (char *)ptr < (char *)heap->heap + heap->size) {
		tr_err(&mem_tr, "rfree(): attempt to free system heap = %p, cpu = %d",
		       ptr, cpu_get_id());
		sof_panic(SOF_IPC_PANIC_MEM);
	}

	/* free the block */
	free_block(ptr);
	memmap->heap_trace_updated = 1;

}

void rfree(void *ptr)
{
	struct mm *memmap = memmap_get();
	k_spinlock_key_t key;

	key = k_spin_lock(&memmap->lock);
	_rfree_unlocked(ptr);
	k_spin_unlock(&memmap->lock, key);
}

void *rbrealloc_align(void *ptr, uint32_t flags, uint32_t caps, size_t bytes,
		      size_t old_bytes, uint32_t alignment)
{
	struct mm *memmap = memmap_get();
	void *new_ptr = NULL;
	k_spinlock_key_t key;
	size_t copy_bytes = MIN(bytes, old_bytes);

	if (!bytes)
		return new_ptr;

	key = k_spin_lock(&memmap->lock);

	new_ptr = _balloc_unlocked(flags, caps, bytes, alignment);

	if (new_ptr && ptr && !(flags & SOF_MEM_FLAG_NO_COPY))
		memcpy_s(new_ptr, copy_bytes, ptr, copy_bytes);

	if (new_ptr)
		_rfree_unlocked(ptr);

	k_spin_unlock(&memmap->lock, key);

	DEBUG_TRACE_PTR(ptr, bytes, SOF_MEM_ZONE_BUFFER, caps, flags);
	return new_ptr;
}

/* TODO: all mm_pm_...() routines to be implemented for IMR storage */
uint32_t mm_pm_context_size(void)
{
	return 0;
}

void free_heap(enum mem_zone zone)
{
	struct mm *memmap = memmap_get();
	struct mm_heap *cpu_heap;

	/* to be called by secondary cores only for sys heap,
	 * otherwise this is critical flow issue.
	 */
	if (cpu_get_id() == PLATFORM_PRIMARY_CORE_ID ||
	    zone != SOF_MEM_ZONE_SYS) {
		tr_err(&mem_tr, "free_heap(): critical flow issue");
		sof_panic(SOF_IPC_PANIC_MEM);
	}

	cpu_heap = memmap->system + cpu_get_id();
	cpu_heap->info.used = 0;
	cpu_heap->info.free = cpu_heap->size;

}

/* initialise map */
void init_heap(struct sof *sof)
{
	struct mm *memmap = sof->memory_map;

#if !CONFIG_LIBRARY
	extern uintptr_t _system_heap_start;

	/* sanity check for malformed images or loader issues */
	if (memmap->system[0].heap != (uintptr_t)&_system_heap_start)
		sof_panic(SOF_IPC_PANIC_MEM);
#endif

	init_heap_map(memmap->system_runtime, PLATFORM_HEAP_SYSTEM_RUNTIME);

	init_heap_map(memmap->runtime, PLATFORM_HEAP_RUNTIME);

#if CONFIG_CORE_COUNT > 1
	init_heap_map(memmap->runtime_shared, PLATFORM_HEAP_RUNTIME_SHARED);
#endif

	init_heap_map(memmap->buffer, PLATFORM_HEAP_BUFFER);

#if CONFIG_DEBUG_BLOCK_FREE
	write_pattern((struct mm_heap *)&memmap->buffer, PLATFORM_HEAP_BUFFER,
		      DEBUG_BLOCK_FREE_VALUE_8BIT);
	write_pattern((struct mm_heap *)&memmap->runtime, PLATFORM_HEAP_RUNTIME,
		      DEBUG_BLOCK_FREE_VALUE_8BIT);
#endif

	k_spinlock_init(&memmap->lock);
}

#if CONFIG_DEBUG_MEMORY_USAGE_SCAN
int heap_info(enum mem_zone zone, int index, struct mm_info *out)
{
	struct mm *memmap = memmap_get();
	struct mm_heap *heap;
	k_spinlock_key_t key;

	if (!out)
		goto error;

	switch (zone) {
	case SOF_MEM_ZONE_SYS:
		if (index >= PLATFORM_HEAP_SYSTEM)
			goto error;
		heap = memmap->system + index;
		break;
	case SOF_MEM_ZONE_SYS_RUNTIME:
		if (index >= PLATFORM_HEAP_SYSTEM_RUNTIME)
			goto error;
		heap = memmap->system_runtime + index;
		break;
	case SOF_MEM_ZONE_RUNTIME:
		if (index >= PLATFORM_HEAP_RUNTIME)
			goto error;
		heap = memmap->runtime + index;
		break;
	case SOF_MEM_ZONE_BUFFER:
		if (index >= PLATFORM_HEAP_BUFFER)
			goto error;
		heap = memmap->buffer + index;
		break;
#if CONFIG_CORE_COUNT > 1
	case SOF_MEM_ZONE_SYS_SHARED:
		if (index >= PLATFORM_HEAP_SYSTEM_SHARED)
			goto error;
		heap = memmap->system_shared + index;
		break;
	case SOF_MEM_ZONE_RUNTIME_SHARED:
		if (index >= PLATFORM_HEAP_RUNTIME_SHARED)
			goto error;
		heap = memmap->runtime_shared + index;
		break;
#endif
	default:
		goto error;
	}

	key = k_spin_lock(&memmap->lock);
	*out = heap->info;
	k_spin_unlock(&memmap->lock, key);
	return 0;
error:
	tr_err(&mem_tr, "heap_info(): failed for zone 0x%x index %d out ptr 0x%x", zone, index,
	       (uint32_t)out);
	return -EINVAL;
}
#endif
