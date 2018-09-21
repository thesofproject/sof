/*
 * Copyright (c) 2016, Intel Corporation
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
 */

#ifndef __PLATFORM_MEMORY_H__
#define __PLATFORM_MEMORY_H__

#include <config.h>

/* physical DSP addresses */


#define SHIM_SIZE	0x00001000

#define IRAM_BASE	0x00000000
#define IRAM_SIZE	0x00050000

#define DRAM0_BASE	0x00400000
#define DRAM0_VBASE	0x00400000

#define MAILBOX_SIZE	0x00001000
#define DMA0_SIZE	0x00001000
#define DMA1_SIZE	0x00001000
#define SSP0_SIZE	0x00001000
#define SSP1_SIZE	0x00001000

#if CONFIG_BROADWELL
#define DRAM0_SIZE	0x000A0000
#define SHIM_BASE	0xFFFFB000
#define DMA0_BASE	0xFFFFE000
#define DMA1_BASE	0xFFFFF000
#define SSP0_BASE	0xFFFFC000
#define SSP1_BASE	0xFFFFD000

#else	/* HASWELL */
#define DRAM0_SIZE	0x00080000
#define SHIM_BASE	0xFFFE7000
#define DMA0_BASE	0xFFFF0000
#define DMA1_BASE	0xFFFF8000
#define SSP0_BASE	0xFFFE8000
#define SSP1_BASE	0xFFFE9000

#endif

#define LOG_ENTRY_ELF_BASE	0x20000000
#define LOG_ENTRY_ELF_SIZE	0x2000000

/*
 * The Heap and Stack on Haswell/Broadwell are organised like this :-
 *
 * +--------------------------------------------------------------------------+
 * | Offset              | Region         |  Size                             |
 * +---------------------+----------------+-----------------------------------+
 * | DRAM0_BASE          | RO Data        |  SOF_DATA_SIZE                   |
 * |                     | Data           |                                   |
 * |                     | BSS            |                                   |
 * +---------------------+----------------+-----------------------------------+
 * | HEAP_SYSTEM_BASE    | System Heap    |  HEAP_SYSTEM_SIZE                 |
 * +---------------------+----------------+-----------------------------------+
 * | HEAP_RUNTIME_BASE   | Runtime Heap   |  HEAP_RUNTIME_SIZE                |
 * +---------------------+----------------+-----------------------------------+
 * | HEAP_BUFFER_BASE    | Module Buffers |  HEAP_BUFFER_SIZE                 |
 * +---------------------+----------------+-----------------------------------+
 * | MAILBOX_BASE        | Mailbox        |  MAILBOX_SIZE                     |
 * +---------------------+----------------+-----------------------------------+
 * | SOF_STACK_END      | Stack          |  SOF_STACK_SIZE                  |
 * +---------------------+----------------+-----------------------------------+ 
 * | SOF_STACK_BASE     |                |                                   |
 * +---------------------+----------------+-----------------------------------+
 */


/* Heap section sizes for module pool */
#define HEAP_RT_COUNT8			0
#define HEAP_RT_COUNT16			256
#define HEAP_RT_COUNT32			128
#define HEAP_RT_COUNT64			64
#define HEAP_RT_COUNT128		32
#define HEAP_RT_COUNT256		16
#define HEAP_RT_COUNT512		8
#define HEAP_RT_COUNT1024		4

/* Heap configuration */
#define SOF_DATA_SIZE			0xa000

#define HEAP_SYSTEM_BASE		(DRAM0_BASE + SOF_DATA_SIZE)
#define HEAP_SYSTEM_SIZE		0x2000

#define HEAP_SYSTEM_0_BASE		HEAP_SYSTEM_BASE

#define HEAP_RUNTIME_BASE		(HEAP_SYSTEM_BASE + HEAP_SYSTEM_SIZE)
#define HEAP_RUNTIME_SIZE \
	(HEAP_RT_COUNT8 * 8 + HEAP_RT_COUNT16 * 16 + \
	HEAP_RT_COUNT32 * 32 + HEAP_RT_COUNT64 * 64 + \
	HEAP_RT_COUNT128 * 128 + HEAP_RT_COUNT256 * 256 + \
	HEAP_RT_COUNT512 * 512 + HEAP_RT_COUNT1024 * 1024)

#define HEAP_BUFFER_BASE		(HEAP_RUNTIME_BASE + HEAP_RUNTIME_SIZE)
#define HEAP_BUFFER_SIZE \
	(DRAM0_SIZE - HEAP_RUNTIME_SIZE - SOF_STACK_SIZE -\
	 HEAP_SYSTEM_SIZE - SOF_DATA_SIZE - MAILBOX_SIZE)

#define HEAP_BUFFER_BLOCK_SIZE		0x180
#define HEAP_BUFFER_COUNT		(HEAP_BUFFER_SIZE / HEAP_BUFFER_BLOCK_SIZE)

#define PLATFORM_HEAP_SYSTEM		1 /* one per core */
#define PLATFORM_HEAP_RUNTIME		1
#define PLATFORM_HEAP_BUFFER		1

/* Stack configuration */
#define SOF_STACK_SIZE			0x1000
#define SOF_STACK_BASE			(DRAM0_BASE + DRAM0_SIZE)
#define SOF_STACK_END			(SOF_STACK_BASE - SOF_STACK_SIZE)

#define MAILBOX_BASE			(SOF_STACK_END - MAILBOX_SIZE)

/* Vector and literal sizes - not in core-isa.h */
#define SOF_MEM_VECT_LIT_SIZE		0x4
#define SOF_MEM_VECT_TEXT_SIZE		0x1c
#define SOF_MEM_VECT_SIZE		(SOF_MEM_VECT_TEXT_SIZE + SOF_MEM_VECT_LIT_SIZE)

#define SOF_MEM_RESET_TEXT_SIZE	0x2e0
#define SOF_MEM_RESET_LIT_SIZE		0x120
#define SOF_MEM_VECBASE_LIT_SIZE	0x178

#define SOF_MEM_RO_SIZE		0x8

#define uncache_to_cache(address)	address
#define cache_to_uncache(address)	address

#endif
