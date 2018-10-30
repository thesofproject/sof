/*
 * Copyright (c) 2016, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the Intel Corporation nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 */

#include <sof/alloc.h>
#include <sof/sof.h>
#include <sof/debug.h>
#include <sof/panic.h>
#include <sof/trace.h>
#include <sof/lock.h>
#include <sof/cpu.h>
#include <platform/memory.h>
#include <stdint.h>

/* debug to set memory value on every allocation */
#define DEBUG_BLOCK_ALLOC		0
#define DEBUG_BLOCK_ALLOC_VALUE		0x6b6b6b6b

/* debug to set memory value on every free TODO: not working atm */
#define DEBUG_BLOCK_FREE		0
#define DEBUG_BLOCK_FREE_VALUE		0x5a5a5a5a

/* memory tracing support */
#if DEBUG_BLOCK_ALLOC || DEBUG_BLOCK_FREE
#define trace_mem(__e)	trace_event(TRACE_CLASS_MEM, __e)
#else
#define trace_mem(__e)
#endif

#define trace_mem_error(__e)	trace_error(TRACE_CLASS_MEM, __e)

extern struct mm memmap;

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


/* flush block map from cache to sram */
static inline void flush_block_map(struct block_map *map)
{
	dcache_writeback_invalidate_region(map->block,
					   sizeof(*map->block) * map->count);
	dcache_writeback_invalidate_region(map, sizeof(*map));
}

/* total size of block */
static inline uint32_t block_get_size(struct block_map *map)
{
	return sizeof(*map) + map->count *
		(map->block_size + sizeof(struct block_hdr));
}

/* total size of heap */
static inline uint32_t heap_get_size(struct mm_heap *heap)
{
	uint32_t size = sizeof(struct mm_heap);
	int i;

	for (i = 0; i < heap->blocks; i++) {
		size += block_get_size(cache_to_uncache(&heap->map[i]));
	}

	return size;
}

#if DEBUG_BLOCK_ALLOC || DEBUG_BLOCK_FREE
static void alloc_memset_region(void *ptr, uint32_t bytes, uint32_t val)
{
	uint32_t count = bytes >> 2;
	uint32_t *dest = ptr, i;

	for (i = 0; i < count; i++)
		dest[i] = val;
}
#endif

/* allocate from system memory pool */
static void *rmalloc_sys(int zone, int core, size_t bytes)
{
	void *ptr;
	struct mm_heap *cpu_heap;
	size_t alignment = 0;

	/* use the heap dedicated for the selected core */
	cpu_heap = cache_to_uncache(memmap.system + core);

	/* align address to dcache line size */
	if (cpu_heap->info.used % PLATFORM_DCACHE_ALIGN)
		alignment = PLATFORM_DCACHE_ALIGN -
			(cpu_heap->info.used % PLATFORM_DCACHE_ALIGN);

	/* always succeeds or panics */
	if (alignment + bytes > cpu_heap->info.free) {
		trace_error(TRACE_CLASS_MEM,
			    "rmalloc-sys eM1 zone %x core %d bytes %d",
			    zone, core, bytes);
		panic(SOF_IPC_PANIC_MEM);
	}
	cpu_heap->info.used += alignment;

	ptr = (void *)(cpu_heap->heap + cpu_heap->info.used);

	cpu_heap->info.used += bytes;
	cpu_heap->info.free -= alignment + bytes;

#if DEBUG_BLOCK_ALLOC
	alloc_memset_region(ptr, bytes, DEBUG_BLOCK_ALLOC_VALUE);
#endif

	if ((zone & RZONE_FLAG_MASK) == RZONE_FLAG_UNCACHED)
		ptr = cache_to_uncache(ptr);

	return ptr;
}

/* allocate single block */
static void *alloc_block(struct mm_heap *heap, int level,
	uint32_t caps)
{
	struct block_map *map = cache_to_uncache(&heap->map[level]);
	struct block_hdr *hdr = cache_to_uncache(&map->block[map->first_free]);
	void *ptr;
	int i;

	map->free_count--;
	ptr = (void *)(map->base + map->first_free * map->block_size);
	hdr->size = 1;
	hdr->used = 1;
	heap->info.used += map->block_size;
	heap->info.free -= map->block_size;

	/* find next free */
	for (i = map->first_free; i < map->count; ++i) {

		hdr = cache_to_uncache(&map->block[i]);

		if (hdr->used == 0) {
			map->first_free = i;
			break;
		}
	}

#if DEBUG_BLOCK_ALLOC
	alloc_memset_region(ptr, map->block_size, DEBUG_BLOCK_ALLOC_VALUE);
#endif

	return ptr;
}

/* allocates continuous blocks */
static void *alloc_cont_blocks(struct mm_heap *heap, int level,
	uint32_t caps, size_t bytes)
{
	struct block_map *map = cache_to_uncache(&heap->map[level]);
	struct block_hdr *hdr;
	void *ptr;
	unsigned int start;
	unsigned int current;
	unsigned int count = bytes / map->block_size;
	unsigned int i;
	unsigned int remaining = map->count - count;
	unsigned int end;

	if (bytes % map->block_size)
		count++;

	/* check for continuous blocks from "start" */
	for (start = map->first_free; start < remaining; start++) {

		/* check that we have enough free blocks from start pos */
		end = start + count;
		for (current = start; current < end; current++) {
			hdr = cache_to_uncache(&map->block[current]);

			/* is block used */
			if (hdr->used)
				break;
		}

		/* enough free blocks ? */
		if (current == end)
			goto found;
	}

	/* not found */
	trace_mem_error("eCb");
	return NULL;

found:
	/* found some free blocks */
	map->free_count -= count;
	ptr = (void *)(map->base + start * map->block_size);
	hdr = cache_to_uncache(&map->block[start]);
	hdr->size = count;
	heap->info.used += count * map->block_size;
	heap->info.free -= count * map->block_size;

	/* allocate each block */
	for (current = start; current < end; current++) {
		hdr = cache_to_uncache(&map->block[current]);
		hdr->used = 1;
	}

	/* do we need to find a new first free block ? */
	if (start == map->first_free) {

		/* find next free */
		for (i = map->first_free + count; i < map->count; ++i) {

			hdr = cache_to_uncache(&map->block[i]);

			if (hdr->used == 0) {
				map->first_free = i;
				break;
			}
		}
	}

#if DEBUG_BLOCK_ALLOC
	alloc_memset_region(ptr, bytes, DEBUG_BLOCK_ALLOC_VALUE);
#endif

	return ptr;
}

static struct mm_heap *get_heap_from_ptr(void *ptr)
{
	struct mm_heap *heap;
	int i;

	/* find mm_heap that ptr belongs to */
	for (i = 0; i < PLATFORM_HEAP_RUNTIME; i++) {
		heap = cache_to_uncache(&memmap.runtime[i]);

		if ((uint32_t)ptr >= heap->heap &&
			(uint32_t)ptr < heap->heap + heap->size)
			return heap;
	}

	for (i = 0; i < PLATFORM_HEAP_BUFFER; i++) {
		heap = cache_to_uncache(&memmap.buffer[i]);

		if ((uint32_t)ptr >= heap->heap &&
			(uint32_t)ptr < heap->heap + heap->size)
			return heap;
	}

	return NULL;
}

static struct mm_heap *get_runtime_heap_from_caps(uint32_t caps)
{
	struct mm_heap *heap;
	uint32_t mask;
	int i;

	/* find first heap that support type */
	for (i = 0; i < PLATFORM_HEAP_RUNTIME; i++) {
		heap = cache_to_uncache(&memmap.runtime[i]);
		mask = heap->caps & caps;
		if (mask == caps)
			return heap;
	}

	return NULL;
}

static struct mm_heap *get_buffer_heap_from_caps(uint32_t caps)
{
	struct mm_heap *heap;
	uint32_t mask;
	int i;

	/* find first heap that support type */
	for (i = 0; i < PLATFORM_HEAP_BUFFER; i++) {
		heap = cache_to_uncache(&memmap.buffer[i]);
		mask = heap->caps & caps;
		if (mask == caps)
			return heap;
	}

	return NULL;
}

/* free block(s) */
static void free_block(void *ptr)
{
	struct mm_heap *heap;
	struct block_map *block_map;
	struct block_hdr *hdr;
	int i;
	int block;

	heap = get_heap_from_ptr(ptr);
	if (!heap) {
		trace_error(TRACE_CLASS_MEM, "invalid heap %p cpu %d",
			    (uintptr_t)ptr, cpu_get_id());
		return;
	}

	/* find block that ptr belongs to */
	for (i = 0; i < heap->blocks; i++) {
		block_map = cache_to_uncache(&heap->map[i]);

		/* is ptr in this block */
		if ((uint32_t)ptr < (block_map->base +
		    (block_map->block_size * block_map->count)))
			goto found;
	}

	/* not found */
	trace_error(TRACE_CLASS_MEM, "invalid ptr %p cpu %d",
		    (uintptr_t)ptr, cpu_get_id());
	return;

found:
	/* calculate block header */
	block = ((uint32_t)ptr - block_map->base) / block_map->block_size;
	hdr = cache_to_uncache(&block_map->block[block]);

	/* report a error if prt is not aligned to block */
	if (block_map->base + block_map->block_size * block != (uint32_t)ptr)
		panic(SOF_IPC_PANIC_MEM);

	/* free block header and continuous blocks */
	for (i = block; i < block + hdr->size; i++) {
		hdr = cache_to_uncache(&block_map->block[i]);
		hdr->size = 0;
		hdr->used = 0;
		block_map->free_count++;
		heap->info.used -= block_map->block_size;
		heap->info.free += block_map->block_size;
	}

	/* set first free block */
	if (block < block_map->first_free)
		block_map->first_free = block;

#if DEBUG_BLOCK_FREE
	/* memset the whole block incase some not aligned ptr*/
	alloc_memset_region((void *)(block_map->base + block_map->block_size * block),
			    block_map->block_size * (i - block), DEBUG_BLOCK_FREE_VALUE);
#endif
}

/* allocate single block for runtime */
static void *rmalloc_runtime(int zone, uint32_t caps, size_t bytes)
{
	struct mm_heap *heap;
	struct block_map *map;
	int i;
	void *ptr = NULL;

	/* check runtime heap for capabilities */
	heap = get_runtime_heap_from_caps(caps);
	if (heap)
		goto find;

	/* next check buffer heap for capabilities */
	heap = get_buffer_heap_from_caps(caps);
	if (heap == NULL)
		goto error;

find:
	for (i = 0; i < heap->blocks; i++) {
		map = cache_to_uncache(&heap->map[i]);

		/* is block big enough */
		if (map->block_size < bytes)
			continue;

		/* does block have free space */
		if (map->free_count == 0)
			continue;

		/* free block space exists */
		ptr = alloc_block(heap, i, caps);

		break;
	}

	if ((zone & RZONE_FLAG_MASK) == RZONE_FLAG_UNCACHED)
		ptr = cache_to_uncache(ptr);

	return ptr;

error:
	trace_error(TRACE_CLASS_MEM,
		    "rmalloc-runtime eMm zone %d caps %x bytes %d",
		    zone, caps, bytes);
	return NULL;
}

void *rmalloc(int zone, uint32_t caps, size_t bytes)
{
	uint32_t flags;
	void *ptr = NULL;

	spin_lock_irq(&memmap.lock, flags);

	switch (zone & RZONE_TYPE_MASK) {
	case RZONE_SYS:
		ptr = rmalloc_sys(zone, cpu_get_id(), bytes);
		break;
	case RZONE_RUNTIME:
		ptr = rmalloc_runtime(zone, caps, bytes);
		break;
	default:
		trace_mem_error("eMz");
		break;
	}

	spin_unlock_irq(&memmap.lock, flags);
	return ptr;
}

void *rzalloc(int zone, uint32_t caps, size_t bytes)
{
	void *ptr = NULL;

	ptr = rmalloc(zone, caps, bytes);
	if (ptr != NULL) {
		bzero(ptr, bytes);
	}

	return ptr;
}

void *rzalloc_core_sys(int core, size_t bytes)
{
	uint32_t flags;
	void *ptr = NULL;

	spin_lock_irq(&memmap.lock, flags);

	ptr = rmalloc_sys(RZONE_SYS, core, bytes);
	if (ptr)
		bzero(ptr, bytes);

	spin_unlock_irq(&memmap.lock, flags);

	return ptr;
}

/* allocates continuous buffers */
void *rballoc(int zone, uint32_t caps, size_t bytes)
{
	struct mm_heap *heap;
	struct block_map *map;
	int i;
	uint32_t flags;
	void *ptr = NULL;

	spin_lock_irq(&memmap.lock, flags);

	heap = get_buffer_heap_from_caps(caps);
	if (heap == NULL)
		goto out;

	/* will request fit in single block */
	for (i = 0; i < heap->blocks; i++) {
		map = cache_to_uncache(&heap->map[i]);

		/* is block big enough */
		if (map->block_size < bytes)
			continue;

		/* does block have free space */
		if (map->free_count == 0)
			continue;

		/* allocate block */
		ptr = alloc_block(heap, i, caps);
		goto out;
	}

	/* request spans > 1 block */

	/* only 1 choice for block size */
	if (heap->blocks == 1) {
		ptr = alloc_cont_blocks(heap, 0, caps, bytes);
		goto out;
	} else {

		/* find best block size for request */
		for (i = 0; i < heap->blocks; i++) {
			map = cache_to_uncache(&heap->map[i]);

			/* allocate is block size smaller than request */
			if (map->block_size < bytes)
				alloc_cont_blocks(heap, i, caps, bytes);
		}
	}

	ptr = alloc_cont_blocks(heap, heap->blocks - 1, caps, bytes);

out:
	if (ptr && ((zone & RZONE_FLAG_MASK) == RZONE_FLAG_UNCACHED))
		ptr = cache_to_uncache(ptr);

	spin_unlock_irq(&memmap.lock, flags);
	return ptr;
}

void rfree(void *ptr)
{
	struct mm_heap *cpu_heap;
	uint32_t flags;

	/* sanity check - NULL ptrs are fine */
	if (!ptr)
		return;

	/* use the heap dedicated for the selected core */
	cpu_heap = cache_to_uncache(memmap.system + cpu_get_id());

	/* panic if pointer is from system heap */
	if (ptr >= (void *)cpu_heap->heap &&
	    ptr <= (void *)cpu_heap->heap + cpu_heap->size) {
		trace_error(TRACE_CLASS_MEM, "attempt to free system heap %p cpu %d",
			    (uintptr_t)ptr, cpu_get_id());
		panic(SOF_IPC_PANIC_MEM);
	}

	/* free the block */
	spin_lock_irq(&memmap.lock, flags);
	free_block(ptr);
	spin_unlock_irq(&memmap.lock, flags);
}

/* TODO: all mm_pm_...() routines to be implemented for IMR storage */
uint32_t mm_pm_context_size(void)
{
	return 0;
}

/*
 * Save the DSP memories that are in use the system and modules. All pipeline and modules
 * must be disabled before calling this functions. No allocations are permitted after
 * calling this and before calling restore.
 */
int mm_pm_context_save(struct dma_copy *dc, struct dma_sg_config *sg)
{
	return -ENOTSUP;
}

/*
 * Restore the DSP memories to modules and the system. This must be called immediately
 * after booting before any pipeline work.
 */
int mm_pm_context_restore(struct dma_copy *dc, struct dma_sg_config *sg)
{
	return -ENOTSUP;
}

void free_heap(int zone)
{
	struct mm_heap *cpu_heap;

	/* to be called by slave cores only for sys heap,
	 * otherwise this is critical flow issue.
	 */
	if (cpu_get_id() == PLATFORM_MASTER_CORE_ID ||
	    zone != RZONE_SYS) {
		trace_mem_error("eMf");
		panic(SOF_IPC_PANIC_MEM);
	}

	cpu_heap = cache_to_uncache(memmap.system + cpu_get_id());
	cpu_heap->info.used = 0;
	cpu_heap->info.free = cpu_heap->size;
}

/* initialise map */
void init_heap(struct sof *sof)
{
	struct mm_heap *heap;
	struct block_map *next_map;
	struct block_map *current_map;
	int i;
	int j;
	int k;

	/* sanity check for malformed images or loader issues */
	if (memmap.system[0].heap != HEAP_SYSTEM_0_BASE)
		panic(SOF_IPC_PANIC_MEM);

	spinlock_init(&memmap.lock);

	/* initialise buffer map */
	for (i = 0; i < PLATFORM_HEAP_BUFFER; i++) {
		heap = &memmap.buffer[i];

		for (j = 0; j < heap->blocks; j++) {

			current_map = &heap->map[j];
			current_map->base = heap->heap;
			flush_block_map(current_map);

			for (k = 1; k < heap->blocks; k++) {
				next_map = &heap->map[k];
				next_map->base = current_map->base +
					current_map->block_size *
					current_map->count;
				current_map = &heap->map[k];
				flush_block_map(current_map);
			}
		}

		dcache_writeback_invalidate_region(heap, sizeof(*heap));
	}

	/* initialise runtime map */
	for (i = 0; i < PLATFORM_HEAP_RUNTIME; i++) {
		heap = &memmap.runtime[i];

		for (j = 0; j < heap->blocks; j++) {

			current_map = &heap->map[j];
			current_map->base = heap->heap;
			flush_block_map(current_map);

			for (k = 1; k < heap->blocks; k++) {
				next_map = &heap->map[k];
				next_map->base = current_map->base +
					current_map->block_size *
					current_map->count;
				current_map = &heap->map[k];
				flush_block_map(current_map);
			}
		}

		dcache_writeback_invalidate_region(heap, sizeof(*heap));
	}
}
