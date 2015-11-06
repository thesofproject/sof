/* 
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 */

#include <reef/alloc.h>
#include <platform/memory.h>
#include <stdint.h>

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
	uint8_t flags;		/* usage flags for page */
	uint16_t offset;	/* block offset */ 
} __attribute__ ((packed));

struct block_map {
	uint16_t block_size;	/* size of block in bytes */
	uint16_t count;		/* number of blocks in map */
	uint16_t free_count;	/* number of free blocks */
	uint16_t first_free;	/* index of first free block */
	struct block_hdr *block;	/* base block header */
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
uint32_t *system_heap;

void *rmalloc(int zone, int module, size_t bytes)
{

}

/* allocates buffer on 4k boundary */
void *rballoc(int zone, int module, size_t bytes)
{

}

void rfree(int zone, int module, void *ptr)
{

}
