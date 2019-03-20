/*
 * Copyright (c) 2018, Intel Corporation
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
 *         Janusz Jankowski <janusz.jankowski@linux.intel.com>
 */

#include <sof/alloc.h>
#include <sof/trace.h>
#include <cavs/memory.h>

/* Heap blocks for system runtime for master core */
static struct block_hdr sys_rt_0_block64[HEAP_SYS_RT_0_COUNT64];
static struct block_hdr sys_rt_0_block128[HEAP_SYS_RT_0_COUNT128];
static struct block_hdr sys_rt_0_block192[HEAP_SYS_RT_0_COUNT192];
static struct block_hdr sys_rt_0_block256[HEAP_SYS_RT_0_COUNT256];
static struct block_hdr sys_rt_0_block320[HEAP_SYS_RT_0_COUNT320];

/* Heap memory for system runtime for master core */
static struct block_map sys_rt_0_heap_map[] = {
	BLOCK_DEF(64, HEAP_SYS_RT_0_COUNT64, sys_rt_0_block64),
	BLOCK_DEF(128, HEAP_SYS_RT_0_COUNT128, sys_rt_0_block128),
	BLOCK_DEF(192, HEAP_SYS_RT_0_COUNT192, sys_rt_0_block192),
	BLOCK_DEF(256, HEAP_SYS_RT_0_COUNT256, sys_rt_0_block256),
	BLOCK_DEF(320, HEAP_SYS_RT_0_COUNT320, sys_rt_0_block320),
};

/* Heap blocks for system runtime for slave core */
static struct block_hdr sys_rt_x_block64[HEAP_SYS_RT_X_COUNT64];
static struct block_hdr sys_rt_x_block128[HEAP_SYS_RT_X_COUNT128];
static struct block_hdr sys_rt_x_block192[HEAP_SYS_RT_X_COUNT192];
static struct block_hdr sys_rt_x_block256[HEAP_SYS_RT_X_COUNT256];
static struct block_hdr sys_rt_x_block320[HEAP_SYS_RT_X_COUNT320];

/* Heap memory for system runtime for slave core */
static struct block_map sys_rt_x_heap_map[] = {
	BLOCK_DEF(64, HEAP_SYS_RT_X_COUNT64, sys_rt_x_block64),
	BLOCK_DEF(128, HEAP_SYS_RT_X_COUNT128, sys_rt_x_block128),
	BLOCK_DEF(192, HEAP_SYS_RT_X_COUNT192, sys_rt_x_block192),
	BLOCK_DEF(256, HEAP_SYS_RT_X_COUNT256, sys_rt_x_block256),
	BLOCK_DEF(320, HEAP_SYS_RT_X_COUNT320, sys_rt_x_block320),
};

/* Heap blocks for modules */
static struct block_hdr mod_block64[HEAP_RT_COUNT64];
static struct block_hdr mod_block128[HEAP_RT_COUNT128];
static struct block_hdr mod_block192[HEAP_RT_COUNT192];
static struct block_hdr mod_block256[HEAP_RT_COUNT256];
static struct block_hdr mod_block320[HEAP_RT_COUNT320];

/* Heap memory map for modules */
static struct block_map rt_heap_map[] = {
	BLOCK_DEF(64, HEAP_RT_COUNT64, mod_block64),
	BLOCK_DEF(128, HEAP_RT_COUNT128, mod_block128),
	BLOCK_DEF(192, HEAP_RT_COUNT192, mod_block192),
	BLOCK_DEF(256, HEAP_RT_COUNT256, mod_block256),
	BLOCK_DEF(320, HEAP_RT_COUNT320, mod_block320),
};

/* Heap blocks for buffers */
static struct block_hdr buf_block[HEAP_BUFFER_COUNT];
static struct block_hdr hp_buf_block[HEAP_HP_BUFFER_COUNT];
static struct block_hdr lp_buf_block[HEAP_LP_BUFFER_COUNT];

/* Heap memory map for buffers */
static struct block_map buf_heap_map[] = {
	BLOCK_DEF(HEAP_BUFFER_BLOCK_SIZE, HEAP_BUFFER_COUNT, buf_block),
};

static struct block_map hp_buf_heap_map[] = {
	BLOCK_DEF(HEAP_HP_BUFFER_BLOCK_SIZE, HEAP_HP_BUFFER_COUNT,
		hp_buf_block),
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
	memmap.system[0].heap = HEAP_SYSTEM_0_BASE;
	memmap.system[0].size = HEAP_SYSTEM_M_SIZE;
	memmap.system[0].info.free = HEAP_SYSTEM_M_SIZE;
	memmap.system[0].caps = SOF_MEM_CAPS_RAM | SOF_MEM_CAPS_EXT |
		SOF_MEM_CAPS_CACHE;

	/* .system_runtime slave core initialization */
	memmap.system_runtime[0].blocks = ARRAY_SIZE(sys_rt_0_heap_map);
	memmap.system_runtime[0].map = sys_rt_0_heap_map;
	memmap.system_runtime[0].heap = HEAP_SYS_RUNTIME_0_BASE;
	memmap.system_runtime[0].size = HEAP_SYS_RUNTIME_M_SIZE;
	memmap.system_runtime[0].info.free = HEAP_SYS_RUNTIME_M_SIZE;
	memmap.system_runtime[0].caps = SOF_MEM_CAPS_RAM |
		SOF_MEM_CAPS_EXT | SOF_MEM_CAPS_CACHE |
		SOF_MEM_CAPS_DMA;

	/* .system and .system_runtime  slave core initialization */
	for (i = 1; i < PLATFORM_CORE_COUNT; i++) {
		/* .system init */
		memmap.system[i].heap = HEAP_SYSTEM_0_BASE +
			HEAP_SYSTEM_M_SIZE + ((i - 1) * HEAP_SYSTEM_S_SIZE);
		memmap.system[i].size = HEAP_SYSTEM_S_SIZE;
		memmap.system[i].info.free = HEAP_SYSTEM_S_SIZE;
		memmap.system[i].caps = SOF_MEM_CAPS_RAM | SOF_MEM_CAPS_EXT |
			SOF_MEM_CAPS_CACHE;

		/* .system_runtime init */
		memmap.system_runtime[i].blocks = ARRAY_SIZE(sys_rt_x_heap_map);
		memmap.system_runtime[i].map = sys_rt_x_heap_map;
		memmap.system_runtime[i].heap = HEAP_SYS_RUNTIME_0_BASE +
			HEAP_SYS_RUNTIME_M_SIZE + ((i - 1) *
			HEAP_SYS_RUNTIME_S_SIZE);
		memmap.system_runtime[i].size = HEAP_SYS_RUNTIME_S_SIZE;
		memmap.system_runtime[i].info.free = HEAP_SYS_RUNTIME_S_SIZE;
		memmap.system_runtime[i].caps = SOF_MEM_CAPS_RAM |
			SOF_MEM_CAPS_EXT | SOF_MEM_CAPS_CACHE |
			SOF_MEM_CAPS_DMA;
	}

	/* .runtime init*/
	memmap.runtime[0].blocks = ARRAY_SIZE(rt_heap_map);
	memmap.runtime[0].map = rt_heap_map;
	memmap.runtime[0].heap = HEAP_RUNTIME_BASE;
	memmap.runtime[0].size = HEAP_RUNTIME_SIZE;
	memmap.runtime[0].info.free = HEAP_RUNTIME_SIZE;
	memmap.runtime[0].caps = SOF_MEM_CAPS_RAM | SOF_MEM_CAPS_EXT |
		SOF_MEM_CAPS_CACHE;

	/* heap buffer init */
	memmap.buffer[0].blocks = ARRAY_SIZE(buf_heap_map);
	memmap.buffer[0].map = buf_heap_map;
	memmap.buffer[0].heap = HEAP_BUFFER_BASE;
	memmap.buffer[0].size = HEAP_BUFFER_SIZE;
	memmap.buffer[0].info.free = HEAP_BUFFER_SIZE;
	memmap.buffer[0].caps = SOF_MEM_CAPS_RAM | SOF_MEM_CAPS_EXT |
		SOF_MEM_CAPS_CACHE;

	/* heap hp buffer init */
	memmap.buffer[1].blocks = ARRAY_SIZE(hp_buf_heap_map);
	memmap.buffer[1].map = hp_buf_heap_map;
	memmap.buffer[1].heap = HEAP_HP_BUFFER_BASE;
	memmap.buffer[1].size = HEAP_HP_BUFFER_SIZE;
	memmap.buffer[1].info.free = HEAP_HP_BUFFER_SIZE;
	memmap.buffer[1].caps = SOF_MEM_CAPS_RAM | SOF_MEM_CAPS_HP |
		SOF_MEM_CAPS_CACHE | SOF_MEM_CAPS_DMA;

	/* heap lp buffer init */
	memmap.buffer[2].blocks = ARRAY_SIZE(lp_buf_heap_map);
	memmap.buffer[2].map = lp_buf_heap_map;
	memmap.buffer[2].heap = HEAP_LP_BUFFER_BASE;
	memmap.buffer[2].size = HEAP_LP_BUFFER_SIZE;
	memmap.buffer[2].info.free = HEAP_LP_BUFFER_SIZE;
	memmap.buffer[2].caps = SOF_MEM_CAPS_RAM | SOF_MEM_CAPS_LP |
		SOF_MEM_CAPS_CACHE | SOF_MEM_CAPS_DMA;

	/* .total init */
	memmap.total.free = HEAP_SYSTEM_T_SIZE + HEAP_SYS_RUNTIME_T_SIZE +
		HEAP_RUNTIME_SIZE + HEAP_BUFFER_SIZE + HEAP_HP_BUFFER_SIZE +
		HEAP_LP_BUFFER_SIZE;
}
