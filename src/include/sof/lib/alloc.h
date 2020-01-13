/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 */

#ifndef __SOF_LIB_ALLOC_H__
#define __SOF_LIB_ALLOC_H__

#include <sof/bit.h>
#include <sof/common.h>
#include <sof/lib/cache.h>
#include <sof/lib/memory.h>
#include <sof/sof.h>
#include <sof/spinlock.h>
#include <sof/string.h>
#include <sof/trace/trace.h>
#include <user/trace.h>
#include <config.h>
#include <stddef.h>
#include <stdint.h>

struct dma_copy;
struct dma_sg_config;

#define trace_mem_error(__e, ...) \
	trace_error(TRACE_CLASS_MEM, __e, ##__VA_ARGS__)
#define trace_mem_init(__e, ...) \
	trace_event(TRACE_CLASS_MEM, __e, ##__VA_ARGS__)

/* Heap Memory Zones
 *
 * The heap has three different zones from where memory can be allocated :-
 *
 * 1) System Zone. Fixed size heap where alloc always succeeds and is never
 * freed. Used by any init code that will never give up the memory.
 *
 * 2) Runtime Zone. Main and larger heap zone where allocs are not guaranteed to
 * succeed. Memory can be freed here.
 *
 * 3) Buffer Zone. Largest heap zone intended for audio buffers.
 *
 * 4) System Runtime Zone. Heap zone intended for runtime objects allocated
 * by the kernel part of the code.
 *
 * See platform/memory.h for heap size configuration and mappings.
 */

/* heap zone types */
enum mem_zone {
	SOF_MEM_ZONE_SYS = 0,
	SOF_MEM_ZONE_SYS_RUNTIME,
	SOF_MEM_ZONE_RUNTIME,
	SOF_MEM_ZONE_BUFFER,
};

/* heap zone flags */
#define SOF_MEM_FLAG_SHARED	BIT(0)

struct mm_info {
	uint32_t used;
	uint32_t free;
};

struct block_hdr {
	uint16_t size;		/* size in blocks for continuous allocation */
	uint16_t used;		/* usage flags for page */
	void *unaligned_ptr;	/* align ptr */
} __packed;

struct block_map {
	uint16_t block_size;	/* size of block in bytes */
	uint16_t count;		/* number of blocks in map */
	uint16_t free_count;	/* number of free blocks */
	uint16_t first_free;	/* index of first free block */
	struct block_hdr *block;	/* base block header */
	uint32_t base;		/* base address of space */
};

#define BLOCK_DEF(sz, cnt, hdr) \
	{.block_size = sz, .count = cnt, .free_count = cnt, .block = hdr, \
	 .first_free = 0}

struct mm_heap {
	uint32_t blocks;
	struct block_map *map;
	uint32_t heap;
	uint32_t size;
	uint32_t caps;
	struct mm_info info;
};

/* heap block memory map */
struct mm {
	/* system heap - used during init cannot be freed */
	struct mm_heap system[PLATFORM_HEAP_SYSTEM];
	/* system runtime heap - used for runtime system components */
	struct mm_heap system_runtime[PLATFORM_HEAP_SYSTEM_RUNTIME];
	/* general heap for components */
	struct mm_heap runtime[PLATFORM_HEAP_RUNTIME];
	/* general component buffer heap */
	struct mm_heap buffer[PLATFORM_HEAP_BUFFER];

	struct mm_info total;
	uint32_t heap_trace_updated;	/* updates that can be presented */
	spinlock_t lock;	/* all allocs and frees are atomic */
};

/* heap allocation and free */
void *_malloc(enum mem_zone zone, uint32_t flags, uint32_t caps, size_t bytes);
void *_zalloc(enum mem_zone zone, uint32_t flags, uint32_t caps, size_t bytes);
void *_balloc(uint32_t flags, uint32_t caps, size_t bytes, uint32_t alignment);
void *_realloc(void *ptr, enum mem_zone zone, uint32_t flags, uint32_t caps,
	       size_t bytes);
void *_brealloc(void *ptr, uint32_t flags, uint32_t caps, size_t bytes,
		uint32_t alignment);
void rfree(void *ptr);

#if CONFIG_DEBUG_HEAP

#include <sof/trace/trace.h>

#define rmalloc(zone, flags, caps, bytes)				\
	({void *_ptr;							\
	do {								\
		_ptr = _malloc(zone, flags, caps, bytes);		\
		if (!_ptr) {						\
			trace_mem_error("failed to alloc 0x%x bytes caps 0x%x flags 0x%x",\
					bytes, caps, flags);		\
			alloc_trace_runtime_heap(caps, bytes);		\
		}							\
	} while (0);							\
	_ptr; })

#define rzalloc(zone, flags, caps, bytes)				\
	({void *_ptr;							\
	do {								\
		_ptr = _zalloc(zone, flags, caps, bytes);		\
		if (!_ptr) {						\
			trace_mem_error("failed to alloc 0x%x bytes caps 0x%x flags 0x%x",\
					bytes, caps, flags);		\
			alloc_trace_runtime_heap(caps, bytes);		\
		}							\
	} while (0);							\
	_ptr; })

#define rballoc(flags, caps, bytes)				\
	({void *_ptr;						\
	do {							\
		_ptr = _balloc(flags, caps, bytes,		\
			       PLATFORM_DCACHE_ALIGN);		\
		if (!_ptr) {					\
			trace_mem_error("failed to alloc 0x%x bytes caps 0x%x flags 0x%x",\
					bytes, caps, flags);	\
			alloc_trace_buffer_heap(caps, bytes);	\
		}						\
	} while (0);						\
	_ptr; })

#define rrealloc(ptr, zone, flags, caps, bytes)				\
	({void *_ptr;							\
	do {								\
		_ptr = _realloc(ptr, zone, flags, caps, bytes);		\
		if (!_ptr) {						\
			trace_mem_error("failed to alloc 0x%x bytes caps 0x%x flags 0x%x",\
					bytes, caps, flags);		\
			alloc_trace_buffer_heap(caps, bytes);		\
		}							\
	} while (0);							\
	_ptr; })

#define rbrealloc(ptr, flags, caps, bytes)				\
	({void *_ptr;							\
	do {								\
		_ptr = _brealloc(ptr, flags, caps, bytes,		\
				 PLATFORM_DCACHE_ALIGN);		\
		if (!_ptr) {						\
			trace_mem_error("failed to alloc 0x%x bytes caps 0x%x flags 0x%x",\
					bytes, caps, flags);		\
			alloc_trace_buffer_heap(caps, bytes);		\
		}							\
	} while (0);							\
	_ptr; })

#define rbrealloc_align(ptr, flags, caps, bytes, alignment)		\
	({void *_ptr;							\
	do {								\
		_ptr = _brealloc(ptr, flags, caps, bytes, alignment);	\
		if (!_ptr) {						\
			trace_mem_error("failed to alloc 0x%x bytes caps 0x%x flags 0x%x",\
					bytes, caps, flags);		\
			alloc_trace_buffer_heap(caps, bytes);		\
		}							\
	} while (0);							\
	_ptr; })

#define rballoc_align(flags, caps, bytes, alignment)		\
	({void *_ptr;						\
	do {							\
		_ptr = _balloc(flags, caps, bytes, alignment);	\
		if (!_ptr) {					\
			trace_mem_error("failed to alloc 0x%x bytes caps 0x%x flags 0x%x",\
					bytes, caps, flags);	\
			alloc_trace_buffer_heap(caps, bytes);	\
		}						\
	} while (0);						\
	_ptr; })

void alloc_trace_runtime_heap(uint32_t caps, size_t bytes);
void alloc_trace_buffer_heap(uint32_t caps, size_t bytes);

#else

#define rmalloc(zone, flags, caps, bytes) \
	_malloc(zone, flags, caps, bytes)
#define rzalloc(zone, flags, caps, bytes) \
	_zalloc(zone, flags, caps, bytes)
#define rballoc(flags, caps, bytes) \
	_balloc(flags, caps, bytes, PLATFORM_DCACHE_ALIGN)
#define rballoc_align(flags, caps, bytes, alignment) \
	_balloc(flags, caps, bytes, alignment)
#define rrealloc(ptr, zone, flags, caps, bytes) \
	_realloc(ptr, zone, flags, caps, bytes)
#define rbrealloc(ptr, flags, caps, bytes) \
	_brealloc(ptr, flags, caps, bytes, PLATFORM_DCACHE_ALIGN)
#define rbrealloc_align(ptr, flags, caps, bytes, alignment) \
	_brealloc(ptr, flags, caps, bytes, alignment)

#endif

/* system heap allocation for specific core */
void *rzalloc_core_sys(int core, size_t bytes);

/* utility */
#define bzero(ptr, size) \
	arch_bzero(ptr, size)

int rstrlen(const char *s);
int rstrcmp(const char *s1, const char *s2);

/* Heap save/restore contents and context for PM D0/D3 events */
uint32_t mm_pm_context_size(void);
int mm_pm_context_save(struct dma_copy *dc, struct dma_sg_config *sg);
int mm_pm_context_restore(struct dma_copy *dc, struct dma_sg_config *sg);

/* heap initialisation */
void init_heap(struct sof *sof);

/* frees entire heap (supported for slave core system heap atm) */
void free_heap(enum mem_zone zone);

/* status */
void heap_trace_all(int force);
void heap_trace(struct mm_heap *heap, int size);

/* retrieve memory map pointer */
static inline struct mm *memmap_get(void)
{
	return sof_get()->memory_map;
}

#endif /* __SOF_LIB_ALLOC_H__ */
