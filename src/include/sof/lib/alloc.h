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
#include <sof/spinlock.h>
#include <sof/string.h>
#include <user/trace.h>
#include <config.h>
#include <stddef.h>
#include <stdint.h>

struct dma_copy;
struct dma_sg_config;
struct sof;

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
#define RZONE_SYS		BIT(0)
#define RZONE_RUNTIME		BIT(1)
#define RZONE_BUFFER		BIT(2)
#define RZONE_SYS_RUNTIME	BIT(3)

/* heap zone flags */
#define RZONE_FLAG_UNCACHED	BIT(4)

#define RZONE_TYPE_MASK	0xf
#define RZONE_FLAG_MASK	0xf0

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
} __aligned(PLATFORM_DCACHE_ALIGN);

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
} __aligned(PLATFORM_DCACHE_ALIGN);

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
	spinlock_t *lock;	/* all allocs and frees are atomic */
} __aligned(PLATFORM_DCACHE_ALIGN);

/* heap allocation and free */
void *_malloc(int zone, uint32_t caps, size_t bytes);
void *_zalloc(int zone, uint32_t caps, size_t bytes);
void *_balloc(int zone, uint32_t caps, size_t bytes, uint32_t alignment);
void *_realloc(void *ptr, int zone, uint32_t caps, size_t bytes);
void *_brealloc(void *ptr, int zone, uint32_t caps, size_t bytes,
		uint32_t alignment);
void rfree(void *ptr);

#if CONFIG_DEBUG_HEAP

#include <sof/trace/trace.h>

#define rmalloc(zone, caps, bytes)			\
	({void *_ptr;					\
	do {						\
		_ptr = _malloc(zone, caps, bytes);	\
		if (!_ptr) {				\
			trace_error(TRACE_CLASS_MEM,	\
				   "failed to alloc 0x%x bytes caps 0x%x", \
				   bytes, caps);	\
			alloc_trace_runtime_heap(zone, caps, bytes);	\
		}					\
	} while (0);					\
	_ptr; })

#define rzalloc(zone, caps, bytes)			\
	({void *_ptr;					\
	do {						\
		_ptr = _zalloc(zone, caps, bytes);	\
		if (!_ptr) {				\
			trace_error(TRACE_CLASS_MEM,	\
				   "failed to alloc 0x%x bytes caps 0x%x", \
				   bytes, caps);	\
			alloc_trace_runtime_heap(zone, caps, bytes);	\
		}					\
	} while (0);					\
	_ptr; })

#define rballoc(zone, caps, bytes)			\
	({void *_ptr;					\
	do {						\
		_ptr = _balloc(zone, caps, bytes, PLATFORM_DCACHE_ALIGN);\
		if (!_ptr) {				\
			trace_error(TRACE_CLASS_MEM,	\
				   "failed to alloc 0x%x bytes caps 0x%x", \
				   bytes, caps);	\
			alloc_trace_buffer_heap(zone, caps, bytes);	\
		}					\
	} while (0);					\
	_ptr; })

#define rrealloc(ptr, zone, caps, bytes)		\
	({void *_ptr;					\
	do {						\
		_ptr = _realloc(ptr, zone, caps, bytes);	\
		if (!_ptr) {				\
			trace_error(TRACE_CLASS_MEM,	\
				   "failed to alloc 0x%x bytes caps 0x%x", \
				   bytes, caps);	\
			alloc_trace_buffer_heap(zone, caps, bytes);	\
		}					\
	} while (0);					\
	_ptr; })

#define rbrealloc(ptr, zone, caps, bytes)		\
	({void *_ptr;					\
	do {						\
		_ptr = _brealloc(ptr, zone, caps, bytes,\
				 PLATFORM_DCACHE_ALIGN);\
		if (!_ptr) {				\
			trace_error(TRACE_CLASS_MEM,	\
				   "failed to alloc 0x%x bytes caps 0x%x", \
				   bytes, caps);	\
			alloc_trace_buffer_heap(zone, caps, bytes);	\
		}					\
	} while (0);					\
	_ptr; })

#define rbrealloc_align(ptr, zone, caps, bytes, alignment)		\
	({void *_ptr;					\
	do {						\
		_ptr = _brealloc(ptr, zone, caps, bytes, alignment);	\
		if (!_ptr) {				\
			trace_error(TRACE_CLASS_MEM,	\
				   "failed to alloc 0x%x bytes caps 0x%x", \
				   bytes, caps);	\
			alloc_trace_buffer_heap(zone, caps, bytes);	\
		}					\
	} while (0);					\
	_ptr; })

#define rballoc(zone, caps, bytes, alignment)			\
	({void *_ptr;					\
	do {						\
		_ptr = _balloc(zone, caps, bytes, alignment);	\
		if (!_ptr) {				\
			trace_error(TRACE_CLASS_MEM,	\
				   "failed to alloc 0x%x bytes caps 0x%x", \
				   bytes, caps);	\
			alloc_trace_buffer_heap(zone, caps, bytes);	\
		}					\
	} while (0);					\
	_ptr; })

void alloc_trace_runtime_heap(int zone, uint32_t caps, size_t bytes);
void alloc_trace_buffer_heap(int zone, uint32_t caps, size_t bytes);

#else

#define rmalloc(zone, caps, bytes)	_malloc(zone, caps, bytes)
#define rzalloc(zone, caps, bytes)	_zalloc(zone, caps, bytes)
#define rballoc(zone, caps, bytes)	\
	_balloc(zone, caps, bytes, PLATFORM_DCACHE_ALIGN)
#define rballoc_align(zone, caps, bytes, alignment)	\
	_balloc(zone, caps, bytes, alignment)
#define rrealloc(ptr, zone, caps, bytes)	\
	_realloc(ptr, zone, caps, bytes)
#define rbrealloc(ptr, zone, caps, bytes)	\
	_brealloc(ptr, zone, caps, bytes, PLATFORM_DCACHE_ALIGN)
#define rbrealloc_align(ptr, zone, caps, bytes, alignment)	\
	_brealloc(ptr, zone, caps, bytes, alignment)

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
void free_heap(int zone);

/* status */
void heap_trace_all(int force);
void heap_trace(struct mm_heap *heap, int size);

#endif /* __SOF_LIB_ALLOC_H__ */
