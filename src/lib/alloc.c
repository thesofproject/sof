/* 
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 */

#include <reef/alloc.h>
#include <reef/reef.h>
#include <reef/debug.h>
#include <reef/trace.h>
#include <reef/lock.h>
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

/* block status */
#define BLOCK_FREE	0
#define BLOCK_USED	1

/* We have 3 memory pools
 *
 * 1) System memory pool does not have a map and it's size is fixed at build
 *    time. Memory cannot be freed from this pool. Used by device drivers
 *    and any system core. Saved as part of PM context.
 * 2) Module memory pool has variable size allocation map and memory is freed
 *    on module or calls to rfree(). Saved as part of PM context. Global size
 *    set at build time.
 * 3) Buffer memory pool has fixed size allocation map and can be freed on
 *    module removal or calls to rfree(). Saved as part of PM context.
 */

struct block_hdr {
	uint8_t module;		/* module that owns this page */
	uint8_t size;		/* size in blocks of this continious allocation */
	uint8_t flags;		/* usage flags for page */
	uint8_t instance;	/* module instance ID */
} __attribute__ ((packed));

struct block_map {
	uint16_t block_size;	/* size of block in bytes */
	uint16_t count;		/* number of blocks in map */
	uint16_t free_count;	/* number of free blocks */
	uint16_t first_free;	/* index of first free block */
	struct block_hdr *block;	/* base block header */
	uint32_t base;		/* base address of space */
} __attribute__ ((packed));

#define BLOCK_DEF(sz, cnt, hdr) \
	{.block_size = sz, .count = cnt, .free_count = cnt, .block = hdr}

/* Heap blocks for modules */
//static struct block_hdr mod_block8[HEAP_MOD_COUNT8];
static struct block_hdr mod_block16[HEAP_MOD_COUNT16];
static struct block_hdr mod_block32[HEAP_MOD_COUNT32];
static struct block_hdr mod_block64[HEAP_MOD_COUNT64];
static struct block_hdr mod_block128[HEAP_MOD_COUNT128];
static struct block_hdr mod_block256[HEAP_MOD_COUNT256];
static struct block_hdr mod_block512[HEAP_MOD_COUNT512];
static struct block_hdr mod_block1024[HEAP_MOD_COUNT1024];

/* Heap memory map for modules */
static struct block_map mod_heap_map[] = {
/*	BLOCK_DEF(8, HEAP_MOD_COUNT8, mod_block8), */
	BLOCK_DEF(16, HEAP_MOD_COUNT16, mod_block16),
	BLOCK_DEF(32, HEAP_MOD_COUNT32, mod_block32),
	BLOCK_DEF(64, HEAP_MOD_COUNT64, mod_block64),
	BLOCK_DEF(128, HEAP_MOD_COUNT128, mod_block128),
	BLOCK_DEF(256, HEAP_MOD_COUNT256, mod_block256),
	BLOCK_DEF(512, HEAP_MOD_COUNT512, mod_block512),
	BLOCK_DEF(1024, HEAP_MOD_COUNT1024, mod_block1024),
};

/* Heap blocks for buffers */
static struct block_hdr buf_block1024[HEAP_BUF_COUNT];

/* Heap memory map for buffers */
static struct block_map buf_heap_map[] = {
	BLOCK_DEF(1024, HEAP_BUF_COUNT, buf_block1024),
};

/* memory heap start locations from linker */
extern uint32_t _system_heap;
extern uint32_t _module_heap;
extern uint32_t _buffer_heap;
extern uint32_t _stack_sentry;

struct mm_heap {
	uint32_t blocks;
	struct block_map *map;
	uint32_t heap;
	uint32_t heap_end;
	struct mm_info info;
};

/* heap block memory map */
struct mm {

	struct mm_heap module;	/* general heap for components */
	struct mm_heap system;	/* general component buffer heap */
	struct mm_heap buffer;	/* system heap - used during init cannot be freed */
	struct mm_info total;
	spinlock_t lock;	/* all allocs and frees are atomic */
};

struct mm memmap = {
	.system = {
		.heap = (uint32_t)&_system_heap,
		.heap_end = (uint32_t)&_module_heap,
		.info = {.free = SYSTEM_MEM,},
	},

	.module = {
		.blocks = ARRAY_SIZE(mod_heap_map),
		.map = mod_heap_map,
		.heap = (uint32_t)&_module_heap,
		.heap_end = (uint32_t)&_buffer_heap,
		.info = {.free = HEAP_MOD_SIZE,},
	},

	.buffer = {
		.blocks = ARRAY_SIZE(buf_heap_map),
		.map = buf_heap_map,
		.heap = (uint32_t)&_buffer_heap,
		.heap_end = (uint32_t)&_stack_sentry,
		.info = {.free = HEAP_BUF_SIZE,},
	},
	.total = {.free = SYSTEM_MEM + HEAP_MOD_SIZE + HEAP_BUF_SIZE,},
};

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
		size += block_get_size(&heap->map[i]);
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
static void *rmalloc_dev(size_t bytes)
{
	void *ptr = (void *)memmap.system.heap;

	/* always suceeds or panics */
	memmap.system.heap += bytes;
	if (memmap.system.heap >= memmap.system.heap_end) {
		trace_mem_error("eMd");
		panic(PANIC_MEM);
	}

#if DEBUG_BLOCK_ALLOC
	alloc_memset_region(ptr, bytes, DEBUG_BLOCK_ALLOC_VALUE);
#endif

	return ptr;
}

/* allocate single block */
static void *alloc_block(struct mm_heap *heap, int level, int module)
{
	struct block_map *map = &heap->map[level];
	struct block_hdr *hdr = &map->block[map->first_free];
	void *ptr;
	int i;

	map->free_count--;
	ptr = (void *)(map->base + map->first_free * map->block_size);
	hdr->module = module;
	hdr->size = 1;
	hdr->flags = BLOCK_USED;
	heap->info.used += map->block_size;
	heap->info.free -= map->block_size;

	/* find next free */
	for (i = map->first_free; i < map->count; ++i) {

		hdr = &map->block[i];

		if (hdr->flags == BLOCK_FREE) {
			map->first_free = i;
			break;
		}
	}

#if DEBUG_BLOCK_ALLOC
	alloc_memset_region(ptr, map->block_size, DEBUG_BLOCK_ALLOC_VALUE);
#endif

	return ptr;
}

/* allocates continious blocks */
static void *alloc_cont_blocks(struct mm_heap *heap, int level, int module, size_t bytes)
{
	struct block_map *map = &heap->map[level];
	struct block_hdr *hdr = &map->block[map->first_free];
	void *ptr;
	unsigned int start, current, count = bytes / map->block_size;
	unsigned int i, remaining = map->count - count, end;

	if (bytes % map->block_size)
		count++;

	/* check for continious blocks from "start" */
	for (start = map->first_free; start < remaining; start++) {

		/* check that we have enough free blocks from start pos */
		end = start + count;
		for (current = start; current < end; current++) {
			hdr = &map->block[current];

			/* is block free */
			if (hdr->flags == BLOCK_USED)
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
	hdr = &map->block[start];
	hdr->size = count;
	heap->info.used += count * map->block_size;
	heap->info.free -= count * map->block_size;

	/* allocate each block */
	for (current = start; current < end; current++) {
		hdr = &map->block[current];
		hdr->module = module;
		hdr->flags = BLOCK_USED;
	}

	/* do we need to find a new first free block ? */
	if (start == map->first_free) {

		/* find next free */
		for (i = map->first_free + count; i < map->count; ++i) {

			hdr = &map->block[i];

			if (hdr->flags == BLOCK_FREE) {
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

/* free block(s) */
static void free_block(struct mm_heap *heap, int module, void *ptr)
{
	struct block_map *map;
	struct block_hdr *hdr;
	int i, block;

	/* sanity check */
	if (ptr == NULL)
		return;

	/* find block that ptr belongs to */
	for (i = 0; i < ARRAY_SIZE(mod_heap_map) - 1; i ++) {

		/* is ptr in this block */
		if ((uint32_t)ptr >= mod_heap_map[i].base &&
			(uint32_t)ptr < mod_heap_map[i + 1].base)
			goto found;
	}

	/* not found */
	trace_mem_error("eMF");
	return;

found:
	/* calculate block header */
	map = &mod_heap_map[i];
	block = ((uint32_t)ptr - map->base) / map->block_size;
	hdr = &map->block[block];

	/* free block header and continious blocks */
	for (i = block; i < block + hdr->size; i++) {
		hdr = &map->block[i];
		hdr->module = 0;
		hdr->size = 0;
		hdr->flags = BLOCK_FREE;
		map->free_count++;
		heap->info.used -= map->block_size;
		heap->info.free += map->block_size;
	}

	/* set first free */
	if (block < map->first_free)
		map->first_free = block;

#if DEBUG_BLOCK_FREE
	alloc_memset_region(ptr, map->block_size * (i - 1), DEBUG_BLOCK_FREE_VALUE);
#endif
}

/* allocate single block for module */
static void *rmalloc_mod(int module, size_t bytes)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(mod_heap_map); i ++) {

		/* is block big enough */
		if (mod_heap_map[i].block_size < bytes)
			continue;

		/* does block have free space */
		if (mod_heap_map[i].free_count == 0)
			continue;

		/* free block space exists */
		return alloc_block(&memmap.module, i, module);
	}

	trace_mem_error("eMm");
	return NULL;
}

void *rmalloc(int zone, int module, size_t bytes)
{
	uint32_t flags;
	void *ptr = NULL;

	spin_lock_irq(&memmap.lock, flags);

	switch (zone) {
	case RZONE_DEV:
		ptr = rmalloc_dev(bytes);
		break;
	case RZONE_MODULE:
		ptr = rmalloc_mod(module, bytes);
		break;
	default:
		trace_mem_error("eMz");
		break;
	}

	spin_unlock_irq(&memmap.lock, flags);
	return ptr;
}

void *rzalloc(int zone, int module, size_t bytes)
{
	void *ptr = NULL;

	ptr = rmalloc(zone, module, bytes);
	if (ptr != NULL) {
		bzero(ptr, bytes);
	}

	return ptr;
}

/* allocates continuous buffer on 1k boundary */
void *rballoc(int zone, int module, size_t bytes)
{
	uint32_t flags;
	void *ptr = NULL;
	int i;

	spin_lock_irq(&memmap.lock, flags);

	/* will request fit in single block */
	for (i = 0; i < ARRAY_SIZE(buf_heap_map); i++) {

		/* is block big enough */
		if (buf_heap_map[i].block_size < bytes)
			continue;

		/* does block have free space */
		if (buf_heap_map[i].free_count == 0)
			continue;

		/* allocate block */
		ptr = alloc_block(&memmap.buffer, i, module);
		goto out;
	}

	/* request spans > 1 block */

	/* only 1 choice for block size */
	if (ARRAY_SIZE(buf_heap_map) == 1) {
		ptr = alloc_cont_blocks(&memmap.buffer, 0, module, bytes);
		goto out;
	} else {

		/* find best block size for request */
		for (i = 0; i < ARRAY_SIZE(buf_heap_map); i++) {

			/* allocate is block size smaller than request */
			if (buf_heap_map[i].block_size < bytes)
				alloc_cont_blocks(&memmap.buffer, i, module,
					bytes);
		}
	}

	ptr = alloc_cont_blocks(&memmap.buffer, ARRAY_SIZE(buf_heap_map) - 1,
		module, bytes);

out:
	spin_unlock_irq(&memmap.lock, flags);
	return ptr;
}

void rfree(int zone, int module, void *ptr)
{
	uint32_t flags;

	spin_lock_irq(&memmap.lock, flags);

	switch (zone) {
	case RZONE_DEV:
		trace_mem_error("eMF");
		panic(PANIC_MEM);
		break;
	case RZONE_MODULE:
		free_block(&memmap.module, module, ptr);
		break;
	default:
		trace_mem_error("eMf");
		break;
	}

	spin_unlock_irq(&memmap.lock, flags);
}

void rbfree(int zone, int module, void *ptr)
{
	uint32_t flags;

	spin_lock_irq(&memmap.lock, flags);

	switch (zone) {
	case RZONE_DEV:
		trace_mem_error("eMF");
		panic(PANIC_MEM);
		break;
	case RZONE_MODULE:
		free_block(&memmap.buffer, module, ptr);
		break;
	default:
		trace_mem_error("eMf");
		break;
	}

	spin_unlock_irq(&memmap.lock, flags);
}

uint32_t mm_pm_context_size(void)
{
	uint32_t size;

	/* calc context size for each area  */	
	size = memmap.buffer.info.used;
	size += memmap.module.info.used;
	size += memmap.system.info.used;

	/* add memory maps */
	size += heap_get_size(&memmap.buffer);
	size += heap_get_size(&memmap.module);
	size += heap_get_size(&memmap.system);

	/* recalc totals */
	memmap.total.free = memmap.buffer.info.free +
		memmap.module.info.free + memmap.system.info.free;
	memmap.total.used = memmap.buffer.info.used +
		memmap.module.info.used + memmap.system.info.used;

	return size;
}

/* 
 * Save the DSP memories that are in use the system and modules. All pipeline and modules
 * must be disabled before calling this functions. No allocations are permitted after
 * calling this and before calling restore.
 */ 
int mm_pm_context_save(struct dma_sg_config *sg)
{
	uint32_t used;
	int32_t offset = 0, ret;

	/* first make sure SG buffer has enough space on host for DSP context */
	used = mm_pm_context_size();
	if (used > dma_sg_get_size(sg))
		return -EINVAL;

	/* copy memory maps to SG */
	ret = dma_copy_to_host(sg, offset,
		(void *)&memmap, sizeof(memmap));
	if (ret < 0)
		return ret;
	
	/* copy system memory contents to SG */
	ret = dma_copy_to_host(sg, offset + ret,
		(void *)memmap.system.heap,
		(int32_t)(memmap.system.heap_end - memmap.system.heap));
	if (ret < 0)
		return ret;

	/* copy module memory contents to SG */
	// TODO: iterate over module block map and copy contents of each block
	// to the host.

	/* copy buffer memory contents to SG */
	// TODO: iterate over buffer block map and copy contents of each block
	// to the host.

	return ret;
}

/*
 * Restore the DSP memories to modules abd the system. This must be called immediately
 * after booting before any pipeline work.
 */
int mm_pm_context_restore(struct dma_sg_config *sg)
{
	int32_t offset = 0, ret;

	/* copy memory maps from SG */
	ret = dma_copy_from_host(sg, offset,
		(void *)&memmap, sizeof(memmap));
	if (ret < 0)
		return ret;

	/* copy system memory contents from SG */
	ret = dma_copy_to_host(sg, offset + ret,
		(void *)memmap.system.heap,
		(int32_t)(memmap.system.heap_end - memmap.system.heap));
	if (ret < 0)
		return ret;

	/* copy module memory contents from SG */
	// TODO: iterate over module block map and copy contents of each block
	// to the host. This is the same block order used by the context store

	/* copy buffer memory contents from SG */
	// TODO: iterate over buffer block map and copy contents of each block
	// to the host. This is the same block order used by the context store

	return 0;
}

/* initialise map */
void init_heap(void)
{
	struct block_map *next_map, *current_map;
	int i;

	spinlock_init(&memmap.lock);

	/* initialise buffer map */
	current_map = &buf_heap_map[0];
	current_map->base = memmap.buffer.heap;

	for (i = 1; i < ARRAY_SIZE(buf_heap_map); i++) {
		next_map = &buf_heap_map[i];
		next_map->base = current_map->base +
			current_map->block_size * current_map->count;
		current_map = &buf_heap_map[i];
	}

	/* initialise module map */
	current_map = &mod_heap_map[0];
	current_map->base = memmap.module.heap;

	for (i = 1; i < ARRAY_SIZE(mod_heap_map); i++) {
		next_map = &mod_heap_map[i];
		next_map->base = current_map->base +
			current_map->block_size * current_map->count;
		current_map = &mod_heap_map[i];
	}
}
