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
#include <platform/memory.h>
#include <stdint.h>

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
static struct block_hdr mod_block8[HEAP_MOD_COUNT8];
static struct block_hdr mod_block16[HEAP_MOD_COUNT16];
static struct block_hdr mod_block32[HEAP_MOD_COUNT32];
static struct block_hdr mod_block64[HEAP_MOD_COUNT64];
static struct block_hdr mod_block128[HEAP_MOD_COUNT128];
static struct block_hdr mod_block256[HEAP_MOD_COUNT256];
static struct block_hdr mod_block512[HEAP_MOD_COUNT512];
static struct block_hdr mod_block1024[HEAP_MOD_COUNT1024];

/* Heap memory map for modules */
static struct block_map mod_heap_map[] = {
	BLOCK_DEF(8, HEAP_MOD_COUNT8, mod_block8),
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

/* system memory heap */
uint32_t system_heap;
uint32_t system_heap_end;

/* module heap */
uint32_t module_heap;
uint32_t module_heap_end;

/* buffer heap */
uint32_t buffer_heap;
uint32_t buffer_heap_end;

/* allocate from system memory pool */
static void *rmalloc_dev(size_t bytes)
{
	void *ptr = (void *)system_heap;

	/* always suceeds or panics */
	system_heap += bytes;
	if (system_heap >= system_heap_end)
		panic(PANIC_MEM);

	return ptr;
}

/* allocate single block */
static void *alloc_block(struct block_map *map, int module)
{
	struct block_hdr *hdr = &map->block[map->first_free];
	void *ptr;
	int i;

	map->free_count--;
	ptr = (void *)(map->base + map->first_free * map->block_size);
	hdr->module = module;
	hdr->flags = BLOCK_USED;

	/* find next free */
	for (i = map->first_free; i < map->count; ++i) {

		hdr = &map->block[i];

		if (hdr->flags == BLOCK_FREE) {
			map->first_free = i;
			break;
		}
	}

	return ptr;
}

/* allocates continious blocks */
static void *alloc_cont_blocks(struct block_map *map, int module, size_t bytes)
{
	struct block_hdr *hdr = &map->block[map->first_free];
	void *ptr;
	int start, current;
	size_t count = bytes / map->block_size;
	int i;

	if (bytes % map->block_size)
		count++;

	/* check for continious blocks from "start" */
	for (start = map->first_free; start < map->count - count; start++) {

		/* check that we have enough free blocks from start pos */
		for (current = start; current < start + count; current++) {

			hdr = &map->block[current];

			/* is block free */
			if (hdr->flags == BLOCK_USED)
				break;
		}

		/* enough free blocks ? */
		if (current == start + count)
			goto found;
	}

	/* not found */
	return NULL;

found:
	/* found some free blocks */
	map->free_count -= count;
	ptr = (void *)(map->base + start * map->block_size);
	hdr = &map->block[current];
	hdr->size = count;

	/* allocate each block */
	for (current = start; current < start + count; current++) {
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

	return ptr;
}

/* free block(s) */
static void free_block(int module, void *ptr)
{
	struct block_map *map;
	struct block_hdr *hdr;
	int i, block;

	/* find block that ptr belongs to */
	for (i = 0; i < ARRAY_SIZE(mod_heap_map) - 1; i ++) {

		/* is ptr in this block */
		if ((uint32_t)ptr >= mod_heap_map[i].base &&
			(uint32_t)ptr >= mod_heap_map[i + 1].base)
			break;
	}

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
	}

	/* set first free */
	if (block < map->first_free)
		map->first_free = block;
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
		return alloc_block(&mod_heap_map[i], module);
	}

	return NULL;
}

void *rmalloc(int zone, int module, size_t bytes)
{
	switch (zone) {
	case RZONE_DEV:
		return rmalloc_dev(bytes);
	case RZONE_MODULE:
		return rmalloc_mod(module, bytes);
	default:
		return NULL;
	}
}

/* allocates continuous buffer on 1k boundary */
void *rballoc(int zone, int module, size_t bytes)
{
	int i;

	/* will request fit in single block */
	for (i = 0; i < ARRAY_SIZE(buf_heap_map); i ++) {

		/* is block big enough */
		if (buf_heap_map[i].block_size < bytes)
			continue;

		/* does block have free space */
		if (buf_heap_map[i].free_count == 0)
			continue;

		/* allocate block */
		return alloc_block(&buf_heap_map[i], module);
	}

	/* request spans > 1 block */

	/* only 1 choice for block size */
	if (ARRAY_SIZE(buf_heap_map) == 1)
		return alloc_cont_blocks(&buf_heap_map[0], module, bytes);
	else {
		/* find best block size for request */
		for (i = 0; i < ARRAY_SIZE(buf_heap_map); i ++) {

			/* allocate is block size smaller than request */
			if (buf_heap_map[i].block_size < bytes)
				alloc_cont_blocks(&buf_heap_map[i], module,
					bytes);
		}
	}

	return alloc_cont_blocks(&buf_heap_map[ARRAY_SIZE(buf_heap_map) - 1],
		module, bytes);
}

void rfree(int zone, int module, void *ptr)
{
	switch (zone) {
	case RZONE_DEV:
		panic(PANIC_MEM);
		break;
	case RZONE_MODULE:
		return free_block(module, ptr);
	default:
		break;
	}
}

/* initialise map */
void init_heap(void)
{
	struct block_map *nmap, *omap;
	int i;

	/* initialise buffer map */
	omap = &buf_heap_map[0];
	omap->base = buffer_heap;

	for (i = 1; i < ARRAY_SIZE(buf_heap_map); i ++) {
		nmap = &buf_heap_map[i];
		nmap->base = omap->base + omap->block_size * omap->count;
		omap = &buf_heap_map[i];
	}

	/* initialise buffer map */
	omap = &mod_heap_map[0];
	omap->base = module_heap;

	for (i = 1; i < ARRAY_SIZE(mod_heap_map); i ++) {
		nmap = &mod_heap_map[i];
		nmap->base = omap->base + omap->block_size * omap->count;
		omap = &mod_heap_map[i];
	}
}
