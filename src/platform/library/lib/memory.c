// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Janusz Jankowski <janusz.jankowski@linux.intel.com>

#include <sof/common.h>
#include <sof/lib/mm_heap.h>
#include <rtos/cache.h>
#include <sof/lib/cpu.h>
#include <sof/lib/memory.h>
#include <sof/platform.h>
#include <sof/sof.h>
#include <ipc/topology.h>
#include <stdint.h>
#include <stdlib.h>

#define uncached_block_hdr(hdr)	cache_to_uncache_init((struct block_hdr *)(hdr))
#define uncached_block_map(map)	cache_to_uncache((struct block_map *)(map))

/* Memory mock for memmap */
#define HEAP_SYSTEM_0_BASE     0xBE100000
#define HEAP_SYS_RUNTIME_0_BASE 0xBE120000
#define SOF_CORE_S_START       0xBE140000
#define HEAP_RUNTIME_BASE      0xBE180000
#define HEAP_BUFFER_BASE       0xBE1C0000

/* Heap blocks for system runtime for primary core */
static SHARED_DATA struct block_hdr sys_rt_0_block64[HEAP_SYS_RT_0_COUNT64];
static SHARED_DATA struct block_hdr sys_rt_0_block512[HEAP_SYS_RT_0_COUNT512];
static SHARED_DATA struct block_hdr sys_rt_0_block1024[HEAP_SYS_RT_0_COUNT1024];

/* Heap blocks for system runtime for secondary core */
#if CONFIG_CORE_COUNT > 1
static SHARED_DATA struct block_hdr
	sys_rt_x_block64[CONFIG_CORE_COUNT - 1][HEAP_SYS_RT_X_COUNT64];
static SHARED_DATA struct block_hdr
	sys_rt_x_block512[CONFIG_CORE_COUNT - 1][HEAP_SYS_RT_X_COUNT512];
static SHARED_DATA struct block_hdr
	sys_rt_x_block1024[CONFIG_CORE_COUNT - 1][HEAP_SYS_RT_X_COUNT1024];
#endif

/* Heap memory for system runtime */
static SHARED_DATA struct block_map sys_rt_heap_map[CONFIG_CORE_COUNT][3] = {
	{ BLOCK_DEF(64, HEAP_SYS_RT_0_COUNT64,
		    uncached_block_hdr(sys_rt_0_block64)),
	  BLOCK_DEF(512, HEAP_SYS_RT_0_COUNT512,
		    uncached_block_hdr(sys_rt_0_block512)),
	  BLOCK_DEF(1024, HEAP_SYS_RT_0_COUNT1024,
		    uncached_block_hdr(sys_rt_0_block1024)), },
#if CONFIG_CORE_COUNT > 1
	{ BLOCK_DEF(64, HEAP_SYS_RT_X_COUNT64,
		    uncached_block_hdr(sys_rt_x_block64[0])),
	  BLOCK_DEF(512, HEAP_SYS_RT_X_COUNT512,
		    uncached_block_hdr(sys_rt_x_block512[0])),
	  BLOCK_DEF(1024, HEAP_SYS_RT_X_COUNT1024,
		    uncached_block_hdr(sys_rt_x_block1024[0])), },
#endif
#if CONFIG_CORE_COUNT > 2
	{ BLOCK_DEF(64, HEAP_SYS_RT_X_COUNT64,
		    uncached_block_hdr(sys_rt_x_block64[1])),
	  BLOCK_DEF(512, HEAP_SYS_RT_X_COUNT512,
		    uncached_block_hdr(sys_rt_x_block512[1])),
	  BLOCK_DEF(1024, HEAP_SYS_RT_X_COUNT1024,
		    uncached_block_hdr(sys_rt_x_block1024[1])), },
#endif
#if CONFIG_CORE_COUNT > 3
	{ BLOCK_DEF(64, HEAP_SYS_RT_X_COUNT64,
		    uncached_block_hdr(sys_rt_x_block64[2])),
	  BLOCK_DEF(512, HEAP_SYS_RT_X_COUNT512,
		    uncached_block_hdr(sys_rt_x_block512[2])),
	  BLOCK_DEF(1024, HEAP_SYS_RT_X_COUNT1024,
		    uncached_block_hdr(sys_rt_x_block1024[2])), },
#endif
};

/* Heap blocks for modules */
static SHARED_DATA struct block_hdr mod_block64[HEAP_COUNT64];
static SHARED_DATA struct block_hdr mod_block128[HEAP_COUNT128];
static SHARED_DATA struct block_hdr mod_block256[HEAP_COUNT256];
static SHARED_DATA struct block_hdr mod_block512[HEAP_COUNT512];
static SHARED_DATA struct block_hdr mod_block1024[HEAP_COUNT1024];
static SHARED_DATA struct block_hdr mod_block2048[HEAP_COUNT2048];
static SHARED_DATA struct block_hdr mod_block4096[HEAP_COUNT4096];

/* Heap memory map for modules */
static SHARED_DATA struct block_map rt_heap_map[] = {
	BLOCK_DEF(64, HEAP_COUNT64, uncached_block_hdr(mod_block64)),
	BLOCK_DEF(128, HEAP_COUNT128, uncached_block_hdr(mod_block128)),
	BLOCK_DEF(256, HEAP_COUNT256, uncached_block_hdr(mod_block256)),
	BLOCK_DEF(512, HEAP_COUNT512, uncached_block_hdr(mod_block512)),
	BLOCK_DEF(1024, HEAP_COUNT1024, uncached_block_hdr(mod_block1024)),
	BLOCK_DEF(2048, HEAP_COUNT2048, uncached_block_hdr(mod_block2048)),
	BLOCK_DEF(4096, HEAP_COUNT4096, uncached_block_hdr(mod_block4096)),
};

#if CONFIG_CORE_COUNT > 1
/* Heap blocks for runtime shared */
static SHARED_DATA struct block_hdr rt_shared_block64[HEAP_RUNTIME_SHARED_COUNT64];
static SHARED_DATA struct block_hdr rt_shared_block128[HEAP_RUNTIME_SHARED_COUNT128];
static SHARED_DATA struct block_hdr rt_shared_block256[HEAP_RUNTIME_SHARED_COUNT256];
static SHARED_DATA struct block_hdr rt_shared_block512[HEAP_RUNTIME_SHARED_COUNT512];
static SHARED_DATA struct block_hdr rt_shared_block1024[HEAP_RUNTIME_SHARED_COUNT1024];

/* Heap memory map for runtime shared */
static SHARED_DATA struct block_map rt_shared_heap_map[] = {
	BLOCK_DEF(64, HEAP_RUNTIME_SHARED_COUNT64, uncached_block_hdr(rt_shared_block64)),
	BLOCK_DEF(128, HEAP_RUNTIME_SHARED_COUNT128, uncached_block_hdr(rt_shared_block128)),
	BLOCK_DEF(256, HEAP_RUNTIME_SHARED_COUNT256, uncached_block_hdr(rt_shared_block256)),
	BLOCK_DEF(512, HEAP_RUNTIME_SHARED_COUNT512, uncached_block_hdr(rt_shared_block512)),
	BLOCK_DEF(1024, HEAP_RUNTIME_SHARED_COUNT1024, uncached_block_hdr(rt_shared_block1024)),
};
#endif

/* Heap blocks for buffers */
static SHARED_DATA struct block_hdr buf_block[HEAP_BUFFER_COUNT_MAX];
static SHARED_DATA struct block_hdr lp_buf_block[HEAP_LP_BUFFER_COUNT];

/* Heap memory map for buffers */
static SHARED_DATA struct block_map buf_heap_map[] = {
	BLOCK_DEF(HEAP_BUFFER_BLOCK_SIZE, HEAP_BUFFER_COUNT_MAX,
		  uncached_block_hdr(buf_block)),
};

static SHARED_DATA struct block_map lp_buf_heap_map[] = {
	BLOCK_DEF(HEAP_LP_BUFFER_BLOCK_SIZE, HEAP_LP_BUFFER_COUNT,
		  uncached_block_hdr(lp_buf_block)),
};

static SHARED_DATA struct mm memmap;

void platform_init_memmap(struct sof *sof)
{
	uint32_t heap_buffer_size = SOF_FW_END - HEAP_BUFFER_BASE;
	uint32_t buffer_count;
	int i;

	/* calculate the buffer heap size */
	buffer_count = heap_buffer_size / HEAP_BUFFER_BLOCK_SIZE;
	heap_buffer_size = buffer_count * HEAP_BUFFER_BLOCK_SIZE;

	for (i = 0; i < ARRAY_SIZE(buf_heap_map); i++) {
		buf_heap_map[i].count = buffer_count;
		buf_heap_map[i].free_count = buffer_count;
	}
	dcache_writeback_region(buf_heap_map,
				sizeof(struct block_map) * ARRAY_SIZE(buf_heap_map));

	/* access memory map through uncached region */
	sof->memory_map = cache_to_uncache(&memmap);

	/* .system primary core initialization */
	sof->memory_map->system[0].heap = (unsigned long)malloc(HEAP_SYSTEM_M_SIZE);
	sof->memory_map->system[0].size = HEAP_SYSTEM_M_SIZE;
	sof->memory_map->system[0].info.free = HEAP_SYSTEM_M_SIZE;
	sof->memory_map->system[0].caps = SOF_MEM_CAPS_RAM | SOF_MEM_CAPS_EXT |
		SOF_MEM_CAPS_CACHE;

	/* .system_runtime primary core initialization */
	sof->memory_map->system_runtime[0].blocks =
		ARRAY_SIZE(sys_rt_heap_map[0]);
	sof->memory_map->system_runtime[0].map =
		uncached_block_map(sys_rt_heap_map[0]);
	sof->memory_map->system_runtime[0].heap =
		(unsigned long)malloc(HEAP_SYS_RUNTIME_M_SIZE);
	sof->memory_map->system_runtime[0].size = HEAP_SYS_RUNTIME_M_SIZE;
	sof->memory_map->system_runtime[0].info.free = HEAP_SYS_RUNTIME_M_SIZE;
	sof->memory_map->system_runtime[0].caps = SOF_MEM_CAPS_RAM |
		SOF_MEM_CAPS_EXT | SOF_MEM_CAPS_CACHE |
		SOF_MEM_CAPS_DMA;

	/* .system and .system_runtime secondary core initialization */
	for (i = 1; i < CONFIG_CORE_COUNT; i++) {
		/* .system init */
		sof->memory_map->system[i].heap =
				(unsigned long)malloc(HEAP_SYSTEM_S_SIZE) +
			((i - 1) * SOF_CORE_S_SIZE);
		sof->memory_map->system[i].size = HEAP_SYSTEM_S_SIZE;
		sof->memory_map->system[i].info.free = HEAP_SYSTEM_S_SIZE;
		sof->memory_map->system[i].caps = SOF_MEM_CAPS_RAM |
			SOF_MEM_CAPS_EXT | SOF_MEM_CAPS_CACHE;

		/* .system_runtime init */
		sof->memory_map->system_runtime[i].blocks =
			ARRAY_SIZE(sys_rt_heap_map[i]);
		sof->memory_map->system_runtime[i].map =
			uncached_block_map(sys_rt_heap_map[i]);
		sof->memory_map->system_runtime[i].heap =
			(unsigned long)malloc(HEAP_SYS_RUNTIME_S_SIZE) +
			((i - 1) * SOF_CORE_S_SIZE) + HEAP_SYSTEM_S_SIZE;
		sof->memory_map->system_runtime[i].size =
			HEAP_SYS_RUNTIME_S_SIZE;
		sof->memory_map->system_runtime[i].info.free =
			HEAP_SYS_RUNTIME_S_SIZE;
		sof->memory_map->system_runtime[i].caps = SOF_MEM_CAPS_RAM |
			SOF_MEM_CAPS_EXT | SOF_MEM_CAPS_CACHE |
			SOF_MEM_CAPS_DMA;
	}

#if CONFIG_CORE_COUNT > 1
	/* .runtime_shared init */
	sof->memory_map->runtime_shared[0].blocks = ARRAY_SIZE(rt_shared_heap_map);
	sof->memory_map->runtime_shared[0].map = uncached_block_map(rt_shared_heap_map);
	sof->memory_map->runtime_shared[0].heap =
			(unsigned long)cache_to_uncache(malloc(HEAP_RUNTIME_SHARED_SIZE));
	sof->memory_map->runtime_shared[0].size = HEAP_RUNTIME_SHARED_SIZE;
	sof->memory_map->runtime_shared[0].info.free = HEAP_RUNTIME_SHARED_SIZE;
	sof->memory_map->runtime_shared[0].caps = SOF_MEM_CAPS_RAM | SOF_MEM_CAPS_EXT |
		SOF_MEM_CAPS_CACHE;

	/* .system_shared init */
	sof->memory_map->system_shared[0].heap =
		(unsigned long)cache_to_uncache(malloc(HEAP_SYSTEM_SHARED_SIZE));
	sof->memory_map->system_shared[0].size = HEAP_SYSTEM_SHARED_SIZE;
	sof->memory_map->system_shared[0].info.free = HEAP_SYSTEM_SHARED_SIZE;
	sof->memory_map->system_shared[0].caps = SOF_MEM_CAPS_RAM | SOF_MEM_CAPS_EXT |
		SOF_MEM_CAPS_CACHE;

#endif

	/* .runtime init*/
	sof->memory_map->runtime[0].blocks = ARRAY_SIZE(rt_heap_map);
	sof->memory_map->runtime[0].map = uncached_block_map(rt_heap_map);
	sof->memory_map->runtime[0].heap = (unsigned long)malloc(HEAP_RUNTIME_SIZE);
	sof->memory_map->runtime[0].size = HEAP_RUNTIME_SIZE;
	sof->memory_map->runtime[0].info.free = HEAP_RUNTIME_SIZE;
	sof->memory_map->runtime[0].caps = SOF_MEM_CAPS_RAM | SOF_MEM_CAPS_EXT |
		SOF_MEM_CAPS_CACHE;

	/* heap buffer init */
	sof->memory_map->buffer[0].blocks = ARRAY_SIZE(buf_heap_map);
	sof->memory_map->buffer[0].map = uncached_block_map(buf_heap_map);
	sof->memory_map->buffer[0].heap = (unsigned long)malloc(heap_buffer_size);
	sof->memory_map->buffer[0].size = heap_buffer_size;
	sof->memory_map->buffer[0].info.free = heap_buffer_size;
	sof->memory_map->buffer[0].caps = SOF_MEM_CAPS_RAM | SOF_MEM_CAPS_HP |
		SOF_MEM_CAPS_CACHE | SOF_MEM_CAPS_DMA;

	/* heap lp buffer init */
	sof->memory_map->buffer[1].blocks = ARRAY_SIZE(lp_buf_heap_map);
	sof->memory_map->buffer[1].map = uncached_block_map(lp_buf_heap_map);
	sof->memory_map->buffer[1].heap = (unsigned long)malloc(HEAP_LP_BUFFER_SIZE);
	sof->memory_map->buffer[1].size = HEAP_LP_BUFFER_SIZE;
	sof->memory_map->buffer[1].info.free = HEAP_LP_BUFFER_SIZE;
	sof->memory_map->buffer[1].caps = SOF_MEM_CAPS_RAM | SOF_MEM_CAPS_LP |
		SOF_MEM_CAPS_CACHE | SOF_MEM_CAPS_DMA;

	/* .total init */
	sof->memory_map->total.free = HEAP_SYSTEM_T_SIZE +
		HEAP_SYS_RUNTIME_T_SIZE + HEAP_RUNTIME_SIZE + heap_buffer_size +
		HEAP_LP_BUFFER_SIZE;

}
