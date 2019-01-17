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

/* Heap blocks for system runtime for master core */
static struct block_hdr sys_rt_0_block64[HEAP_SYS_RT_0_COUNT64];
static struct block_hdr sys_rt_0_block512[HEAP_SYS_RT_0_COUNT512];
static struct block_hdr sys_rt_0_block1024[HEAP_SYS_RT_0_COUNT1024];

/* Heap memory for system runtime for master core */
static struct block_map sys_rt_0_heap_map[] = {
	BLOCK_DEF(64, HEAP_SYS_RT_0_COUNT64, sys_rt_0_block64),
	BLOCK_DEF(512, HEAP_SYS_RT_0_COUNT512, sys_rt_0_block512),
	BLOCK_DEF(1024, HEAP_SYS_RT_0_COUNT1024, sys_rt_0_block1024),
};

/* Heap blocks for system runtime for slave core */
static struct block_hdr sys_rt_x_block64[HEAP_SYS_RT_X_COUNT64];
static struct block_hdr sys_rt_x_block512[HEAP_SYS_RT_X_COUNT512];
static struct block_hdr sys_rt_x_block1024[HEAP_SYS_RT_X_COUNT1024];

/* Heap memory for system runtime for slave core */
static struct block_map sys_rt_x_heap_map[] = {
	BLOCK_DEF(64, HEAP_SYS_RT_X_COUNT64, sys_rt_x_block64),
	BLOCK_DEF(512, HEAP_SYS_RT_X_COUNT512, sys_rt_x_block512),
	BLOCK_DEF(1024, HEAP_SYS_RT_X_COUNT1024, sys_rt_x_block1024),
};

/* Heap blocks for modules */
static struct block_hdr mod_block64[HEAP_RT_COUNT64];
static struct block_hdr mod_block128[HEAP_RT_COUNT128];
static struct block_hdr mod_block256[HEAP_RT_COUNT256];
static struct block_hdr mod_block512[HEAP_RT_COUNT512];
static struct block_hdr mod_block1024[HEAP_RT_COUNT1024];

/* Heap memory map for modules */
static struct block_map rt_heap_map[] = {
	BLOCK_DEF(64, HEAP_RT_COUNT64, mod_block64),
	BLOCK_DEF(128, HEAP_RT_COUNT128, mod_block128),
	BLOCK_DEF(256, HEAP_RT_COUNT256, mod_block256),
	BLOCK_DEF(512, HEAP_RT_COUNT512, mod_block512),
	BLOCK_DEF(1024, HEAP_RT_COUNT1024, mod_block1024),
};

/* Heap blocks for buffers */
static struct block_hdr buf_block[HEAP_BUFFER_COUNT];
static struct block_hdr hp_buf_block[HEAP_HP_BUFFER_COUNT];
/* To be removed if Apollolake gets LP memory*/
#ifndef CONFIG_APOLLOLAKE
static struct block_hdr lp_buf_block[HEAP_LP_BUFFER_COUNT];
#endif

/* Heap memory map for buffers */
static struct block_map buf_heap_map[] = {
	BLOCK_DEF(HEAP_BUFFER_BLOCK_SIZE, HEAP_BUFFER_COUNT, buf_block),
};

static struct block_map hp_buf_heap_map[] = {
	BLOCK_DEF(HEAP_HP_BUFFER_BLOCK_SIZE, HEAP_HP_BUFFER_COUNT,
		hp_buf_block),
};

/* To be removed if Apollolake gets LP memory*/
#ifndef CONFIG_APOLLOLAKE
static struct block_map lp_buf_heap_map[] = {
	BLOCK_DEF(HEAP_LP_BUFFER_BLOCK_SIZE, HEAP_LP_BUFFER_COUNT,
			  lp_buf_block),
};
#endif

struct mm memmap = {
	.system[0] = {
		.heap = HEAP_SYSTEM_0_BASE,
		.size = HEAP_SYSTEM_0_SIZE,
		.info = {.free = HEAP_SYSTEM_0_SIZE,},
		.caps = SOF_MEM_CAPS_RAM | SOF_MEM_CAPS_EXT |
			SOF_MEM_CAPS_CACHE,
	},
	.system[1] = {
		.heap = HEAP_SYSTEM_1_BASE,
		.size = HEAP_SYSTEM_1_SIZE,
		.info = {.free = HEAP_SYSTEM_1_SIZE,},
		.caps = SOF_MEM_CAPS_RAM | SOF_MEM_CAPS_EXT |
			SOF_MEM_CAPS_CACHE,
	},
#if defined(CONFIG_CANNONLAKE) || defined(CONFIG_ICELAKE)
	.system[2] = {
		.heap = HEAP_SYSTEM_2_BASE,
		.size = HEAP_SYSTEM_2_SIZE,
		.info = {.free = HEAP_SYSTEM_2_SIZE,},
		.caps = SOF_MEM_CAPS_RAM | SOF_MEM_CAPS_EXT |
			SOF_MEM_CAPS_CACHE,
	},
	.system[3] = {
		.heap = HEAP_SYSTEM_3_BASE,
		.size = HEAP_SYSTEM_3_SIZE,
		.info = {.free = HEAP_SYSTEM_3_SIZE,},
		.caps = SOF_MEM_CAPS_RAM | SOF_MEM_CAPS_EXT |
			SOF_MEM_CAPS_CACHE,
	},
#endif
	.system_runtime[0] = {
		.blocks = ARRAY_SIZE(sys_rt_0_heap_map),
		.map = sys_rt_0_heap_map,
		.heap = HEAP_SYS_RUNTIME_0_BASE,
		.size = HEAP_SYS_RUNTIME_0_SIZE,
		.info = {.free = HEAP_SYS_RUNTIME_0_SIZE,},
		.caps = SOF_MEM_CAPS_RAM | SOF_MEM_CAPS_EXT |
			SOF_MEM_CAPS_CACHE | SOF_MEM_CAPS_DMA,
	},
	.system_runtime[1] = {
		.blocks = ARRAY_SIZE(sys_rt_x_heap_map),
		.map = sys_rt_x_heap_map,
		.heap = HEAP_SYS_RUNTIME_1_BASE,
		.size = HEAP_SYS_RUNTIME_1_SIZE,
		.info = {.free = HEAP_SYS_RUNTIME_1_SIZE,},
		.caps = SOF_MEM_CAPS_RAM | SOF_MEM_CAPS_EXT |
			SOF_MEM_CAPS_CACHE | SOF_MEM_CAPS_DMA,
	},
#if defined(CONFIG_CANNONLAKE) || defined(CONFIG_ICELAKE)
	.system_runtime[2] = {
		.blocks = ARRAY_SIZE(sys_rt_x_heap_map),
		.map = sys_rt_x_heap_map,
		.heap = HEAP_SYS_RUNTIME_2_BASE,
		.size = HEAP_SYS_RUNTIME_2_SIZE,
		.info = {.free = HEAP_SYS_RUNTIME_2_SIZE,},
		.caps = SOF_MEM_CAPS_RAM | SOF_MEM_CAPS_EXT |
			SOF_MEM_CAPS_CACHE,
	},
	.system_runtime[3] = {
		.blocks = ARRAY_SIZE(sys_rt_x_heap_map),
		.map = sys_rt_x_heap_map,
		.heap = HEAP_SYS_RUNTIME_3_BASE,
		.size = HEAP_SYS_RUNTIME_3_SIZE,
		.info = {.free = HEAP_SYS_RUNTIME_3_SIZE,},
		.caps = SOF_MEM_CAPS_RAM | SOF_MEM_CAPS_EXT |
			SOF_MEM_CAPS_CACHE,
	},
#endif
	.runtime[0] = {
		.blocks = ARRAY_SIZE(rt_heap_map),
		.map = rt_heap_map,
		.heap = HEAP_RUNTIME_BASE,
		.size = HEAP_RUNTIME_SIZE,
		.info = {.free = HEAP_RUNTIME_SIZE,},
		.caps = SOF_MEM_CAPS_RAM | SOF_MEM_CAPS_EXT |
			SOF_MEM_CAPS_CACHE,
	},
	.buffer[0] = {
		.blocks = ARRAY_SIZE(buf_heap_map),
		.map = buf_heap_map,
		.heap = HEAP_BUFFER_BASE,
		.size = HEAP_BUFFER_SIZE,
		.info = {.free = HEAP_BUFFER_SIZE,},
		.caps = SOF_MEM_CAPS_RAM | SOF_MEM_CAPS_EXT |
			SOF_MEM_CAPS_CACHE,
	},
	.buffer[1] = {
		.blocks = ARRAY_SIZE(hp_buf_heap_map),
		.map = hp_buf_heap_map,
		.heap = HEAP_HP_BUFFER_BASE,
		.size = HEAP_HP_BUFFER_SIZE,
		.info = {.free = HEAP_HP_BUFFER_SIZE,},
		.caps = SOF_MEM_CAPS_RAM | SOF_MEM_CAPS_HP |
			SOF_MEM_CAPS_CACHE | SOF_MEM_CAPS_DMA,
	},
/* To be removed if Apollolake gets LP memory*/
#ifndef CONFIG_APOLLOLAKE
	.buffer[2] = {
		.blocks = ARRAY_SIZE(lp_buf_heap_map),
		.map = lp_buf_heap_map,
		.heap = HEAP_LP_BUFFER_BASE,
		.size = HEAP_LP_BUFFER_SIZE,
		.info = {.free = HEAP_LP_BUFFER_SIZE,},
		.caps = SOF_MEM_CAPS_RAM | SOF_MEM_CAPS_LP |
			SOF_MEM_CAPS_CACHE | SOF_MEM_CAPS_DMA,
	},
#endif
	.total = {.free = HEAP_SYSTEM_T_SIZE + HEAP_RUNTIME_SIZE +
			HEAP_SYS_RUNTIME_T_SIZE + HEAP_BUFFER_SIZE +
			HEAP_HP_BUFFER_SIZE + HEAP_LP_BUFFER_SIZE,},
};
