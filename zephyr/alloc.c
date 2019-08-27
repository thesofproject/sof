// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
//         Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>
//         Ranjani Sridharan <ranjani.sridharan@linux.intel.com>

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <sof/alloc.h>
#include <kernel.h>
#include <sys/util.h>

/* Describe one mem_slab */
struct memslab_map {
	uint16_t block_size;    /* size of block in bytes */
	uint16_t count;         /* number of blocks */
	uint32_t size;          /* size of buffer */
	struct k_mem_slab slab;
	void* buffer;           /* pointer to buffer */
} __aligned(PLATFORM_DCACHE_ALIGN);

#define MEMSLAB_DEF(sz, cnt, buf) \
	{ .block_size = sz, .count = cnt, .buffer = buf, \
	  .size = (sz * cnt), \
	}

#define BUF_ALIGN	__aligned(WB_UP(PLATFORM_DCACHE_ALIGN))


/*
 * RZONE_BUFFER for DMA
 */
static struct k_mem_slab hp_dma_buffer;
static struct memslab_map hp_dma_buffer_map =
	MEMSLAB_DEF(HEAP_HP_BUFFER_BLOCK_SIZE, HEAP_HP_BUFFER_COUNT,
		    (void *)HEAP_HP_BUFFER_BASE);

/*
 * RZONE_BUFFER for non-DMA
 *
 * TODO: figure out a good block size.
 */
#define _HEAP_BUFFER_BLOCK_SIZE		1024
#define _HEAP_BUFFER_COUNT		32
#define _HEAP_BUFFER_SIZE \
	(_HEAP_BUFFER_COUNT * _HEAP_BUFFER_BLOCK_SIZE)

static struct k_mem_slab heap_buffer;
static BUF_ALIGN uint8_t _heap_buf[_HEAP_BUFFER_SIZE];
static struct memslab_map heap_buffer_map =
	MEMSLAB_DEF(_HEAP_BUFFER_BLOCK_SIZE,
		    _HEAP_BUFFER_COUNT,
		    _heap_buf);


void malloc_init(void)
{
	/* Initialize RZONE_BUFFER */
	k_mem_slab_init(&hp_dma_buffer, (void *)HEAP_HP_BUFFER_BASE,
			HEAP_HP_BUFFER_BLOCK_SIZE, HEAP_HP_BUFFER_COUNT);

	k_mem_slab_init(&heap_buffer, _heap_buf,
			_HEAP_BUFFER_BLOCK_SIZE, _HEAP_BUFFER_COUNT);
}


void *rmalloc(uint32_t zone, uint32_t caps, size_t bytes)
{
	return k_malloc(bytes);
}

void *rzalloc(uint32_t zone, uint32_t caps, size_t bytes)
{
	return k_calloc(bytes, 1);
}

void *rballoc(uint32_t zone, uint32_t caps, size_t bytes)
{
	void *ptr = NULL;

	if (caps & SOF_MEM_CAPS_DMA) {
		k_mem_slab_alloc(&heap_buffer, &ptr, K_NO_WAIT);
	} else {
		k_mem_slab_alloc(&hp_dma_buffer, &ptr, K_NO_WAIT);
	}

	return ptr;
}

static bool free_in_slab(struct memslab_map *map, void *ptr)
{
	uintptr_t begin = (uintptr_t)map->buffer;
	uintptr_t end = begin + map->size;
	uintptr_t addr = (uintptr_t)ptr;

	if ((addr >= begin) && (addr < end)) {
		k_mem_slab_free(&map->slab, &ptr);
		return true;
	}

	return false;
}

void rfree(void *ptr)
{
	/* RZONE_BUFFER */
	if (free_in_slab(&hp_dma_buffer_map, ptr)) {
		return;
	}

	if (free_in_slab(&heap_buffer_map, ptr)) {
		return;
	}

	/* Not in mem_slab, so assume k_mem_pool */
	k_free(ptr);
}
