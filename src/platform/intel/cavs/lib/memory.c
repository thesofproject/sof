// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Janusz Jankowski <janusz.jankowski@linux.intel.com>

#include <sof/common.h>
#include <sof/lib/alloc.h>
#include <sof/lib/cache.h>
#include <sof/lib/cpu.h>
#include <sof/lib/memory.h>
#include <sof/platform.h>
#include <ipc/topology.h>
#include <stdint.h>

extern uintptr_t _system_heap, _system_runtime_heap, _module_heap;
extern uintptr_t _buffer_heap, _sof_core_s_start;

/* Heap blocks for system runtime for master core */
static struct block_hdr sys_rt_0_block64[HEAP_SYS_RT_0_COUNT64]
	__aligned(PLATFORM_DCACHE_ALIGN);
static struct block_hdr sys_rt_0_block512[HEAP_SYS_RT_0_COUNT512]
	__aligned(PLATFORM_DCACHE_ALIGN);
static struct block_hdr sys_rt_0_block1024[HEAP_SYS_RT_0_COUNT1024]
	__aligned(PLATFORM_DCACHE_ALIGN);

/* Heap blocks for system runtime for slave core */
#if PLATFORM_CORE_COUNT > 1
static struct block_hdr
	sys_rt_x_block64[PLATFORM_CORE_COUNT - 1][HEAP_SYS_RT_X_COUNT64]
	__aligned(PLATFORM_DCACHE_ALIGN);
static struct block_hdr
	sys_rt_x_block512[PLATFORM_CORE_COUNT - 1][HEAP_SYS_RT_X_COUNT512]
	__aligned(PLATFORM_DCACHE_ALIGN);
static struct block_hdr
	sys_rt_x_block1024[PLATFORM_CORE_COUNT - 1][HEAP_SYS_RT_X_COUNT1024]
	__aligned(PLATFORM_DCACHE_ALIGN);
#endif

/* Heap memory for system runtime */
static struct block_map sys_rt_heap_map[PLATFORM_CORE_COUNT][3] = {
	{ BLOCK_DEF(64, HEAP_SYS_RT_0_COUNT64, sys_rt_0_block64),
	  BLOCK_DEF(512, HEAP_SYS_RT_0_COUNT512, sys_rt_0_block512),
	  BLOCK_DEF(1024, HEAP_SYS_RT_0_COUNT1024, sys_rt_0_block1024), },
#if PLATFORM_CORE_COUNT > 1
	{ BLOCK_DEF(64, HEAP_SYS_RT_X_COUNT64, sys_rt_x_block64[0]),
	  BLOCK_DEF(512, HEAP_SYS_RT_X_COUNT512, sys_rt_x_block512[0]),
	  BLOCK_DEF(1024, HEAP_SYS_RT_X_COUNT1024, sys_rt_x_block1024[0]), },
#endif
#if PLATFORM_CORE_COUNT > 2
	{ BLOCK_DEF(64, HEAP_SYS_RT_X_COUNT64, sys_rt_x_block64[1]),
	  BLOCK_DEF(512, HEAP_SYS_RT_X_COUNT512, sys_rt_x_block512[1]),
	  BLOCK_DEF(1024, HEAP_SYS_RT_X_COUNT1024, sys_rt_x_block1024[1]), },
#endif
#if PLATFORM_CORE_COUNT > 3
	{ BLOCK_DEF(64, HEAP_SYS_RT_X_COUNT64, sys_rt_x_block64[2]),
	  BLOCK_DEF(512, HEAP_SYS_RT_X_COUNT512, sys_rt_x_block512[2]),
	  BLOCK_DEF(1024, HEAP_SYS_RT_X_COUNT1024, sys_rt_x_block1024[2]), },
#endif
};

/* Heap blocks for modules */
static struct block_hdr
	mod_block64[HEAP_RT_COUNT64] __aligned(PLATFORM_DCACHE_ALIGN);
static struct block_hdr
	mod_block128[HEAP_RT_COUNT128] __aligned(PLATFORM_DCACHE_ALIGN);
static struct block_hdr
	mod_block256[HEAP_RT_COUNT256] __aligned(PLATFORM_DCACHE_ALIGN);
static struct block_hdr
	mod_block512[HEAP_RT_COUNT512] __aligned(PLATFORM_DCACHE_ALIGN);
static struct block_hdr
	mod_block1024[HEAP_RT_COUNT1024] __aligned(PLATFORM_DCACHE_ALIGN);

/* Heap memory map for modules */
static struct block_map rt_heap_map[] = {
	BLOCK_DEF(64, HEAP_RT_COUNT64, mod_block64),
	BLOCK_DEF(128, HEAP_RT_COUNT128, mod_block128),
	BLOCK_DEF(256, HEAP_RT_COUNT256, mod_block256),
	BLOCK_DEF(512, HEAP_RT_COUNT512, mod_block512),
	BLOCK_DEF(1024, HEAP_RT_COUNT1024, mod_block1024),
};

/* Heap blocks for buffers */
static struct block_hdr
	buf_block[HEAP_BUFFER_COUNT] __aligned(PLATFORM_DCACHE_ALIGN);
static struct block_hdr
	lp_buf_block[HEAP_LP_BUFFER_COUNT] __aligned(PLATFORM_DCACHE_ALIGN);

/* Heap memory map for buffers */
static struct block_map buf_heap_map[] = {
	BLOCK_DEF(HEAP_BUFFER_BLOCK_SIZE, HEAP_BUFFER_COUNT, buf_block),
};

static struct block_map lp_buf_heap_map[] = {
	BLOCK_DEF(HEAP_LP_BUFFER_BLOCK_SIZE, HEAP_LP_BUFFER_COUNT,
			  lp_buf_block),
};

struct mm memmap;

void platform_init_memmap(void)
{
	int i;

	/* .system master core initialization */
	memmap.system[0].heap = (uintptr_t)&_system_heap;
	memmap.system[0].size = HEAP_SYSTEM_M_SIZE;
	memmap.system[0].info.free = HEAP_SYSTEM_M_SIZE;
	memmap.system[0].caps = SOF_MEM_CAPS_RAM | SOF_MEM_CAPS_EXT |
		SOF_MEM_CAPS_CACHE;

	/* .system_runtime master core initialization */
	memmap.system_runtime[0].blocks = ARRAY_SIZE(sys_rt_heap_map[0]);
	memmap.system_runtime[0].map = sys_rt_heap_map[0];
	memmap.system_runtime[0].heap = (uintptr_t)&_system_runtime_heap;
	memmap.system_runtime[0].size = HEAP_SYS_RUNTIME_M_SIZE;
	memmap.system_runtime[0].info.free = HEAP_SYS_RUNTIME_M_SIZE;
	memmap.system_runtime[0].caps = SOF_MEM_CAPS_RAM |
		SOF_MEM_CAPS_EXT | SOF_MEM_CAPS_CACHE |
		SOF_MEM_CAPS_DMA;

	/* .system and .system_runtime  slave core initialization */
	for (i = 1; i < PLATFORM_CORE_COUNT; i++) {
		/* .system init */
		memmap.system[i].heap = (uintptr_t)&_sof_core_s_start +
					((i - 1) * SOF_CORE_S_SIZE);
		memmap.system[i].size = HEAP_SYSTEM_S_SIZE;
		memmap.system[i].info.free = HEAP_SYSTEM_S_SIZE;
		memmap.system[i].caps = SOF_MEM_CAPS_RAM | SOF_MEM_CAPS_EXT |
			SOF_MEM_CAPS_CACHE;

		/* .system_runtime init */
		memmap.system_runtime[i].blocks =
			ARRAY_SIZE(sys_rt_heap_map[i]);
		memmap.system_runtime[i].map = sys_rt_heap_map[i];
		memmap.system_runtime[i].heap = (uintptr_t)&_sof_core_s_start +
			((i - 1) * SOF_CORE_S_SIZE) + HEAP_SYSTEM_S_SIZE;
		memmap.system_runtime[i].size = HEAP_SYS_RUNTIME_S_SIZE;
		memmap.system_runtime[i].info.free = HEAP_SYS_RUNTIME_S_SIZE;
		memmap.system_runtime[i].caps = SOF_MEM_CAPS_RAM |
			SOF_MEM_CAPS_EXT | SOF_MEM_CAPS_CACHE |
			SOF_MEM_CAPS_DMA;
	}

	/* .runtime init*/
	memmap.runtime[0].blocks = ARRAY_SIZE(rt_heap_map);
	memmap.runtime[0].map = rt_heap_map;
	memmap.runtime[0].heap = (uintptr_t)&_module_heap;
	memmap.runtime[0].size = HEAP_RUNTIME_SIZE;
	memmap.runtime[0].info.free = HEAP_RUNTIME_SIZE;
	memmap.runtime[0].caps = SOF_MEM_CAPS_RAM | SOF_MEM_CAPS_EXT |
		SOF_MEM_CAPS_CACHE;

	/* heap buffer init */
	memmap.buffer[0].blocks = ARRAY_SIZE(buf_heap_map);
	memmap.buffer[0].map = buf_heap_map;
	memmap.buffer[0].heap = (uintptr_t)&_buffer_heap;
	memmap.buffer[0].size = HEAP_BUFFER_SIZE;
	memmap.buffer[0].info.free = HEAP_BUFFER_SIZE;
	memmap.buffer[0].caps = SOF_MEM_CAPS_RAM | SOF_MEM_CAPS_HP |
		SOF_MEM_CAPS_CACHE | SOF_MEM_CAPS_DMA;

	/* heap lp buffer init */
	memmap.buffer[1].blocks = ARRAY_SIZE(lp_buf_heap_map);
	memmap.buffer[1].map = lp_buf_heap_map;
	memmap.buffer[1].heap = HEAP_LP_BUFFER_BASE;
	memmap.buffer[1].size = HEAP_LP_BUFFER_SIZE;
	memmap.buffer[1].info.free = HEAP_LP_BUFFER_SIZE;
	memmap.buffer[1].caps = SOF_MEM_CAPS_RAM | SOF_MEM_CAPS_LP |
		SOF_MEM_CAPS_CACHE | SOF_MEM_CAPS_DMA;

	/* .total init */
	memmap.total.free = HEAP_SYSTEM_T_SIZE + HEAP_SYS_RUNTIME_T_SIZE +
		HEAP_RUNTIME_SIZE + HEAP_BUFFER_SIZE +
		HEAP_LP_BUFFER_SIZE;
}
