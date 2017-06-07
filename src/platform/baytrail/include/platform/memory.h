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

#define SHIM_BASE	0xFF340000
#define SHIM_SIZE	0x00004000

#define IRAM_BASE	0xFF2C0000
#define IRAM_SIZE	0x00014000

#define DRAM0_BASE	0xFF300000
#define DRAM0_SIZE	0x00028000
#define DRAM0_VBASE	0xC0000000

#define MAILBOX_BASE	0xFF344000
#define MAILBOX_SIZE	0x00001000

#define DMA0_BASE	0xFF298000
#define DMA0_SIZE	0x00004000

#define DMA1_BASE	0xFF29C000
#define DMA1_SIZE	0x00004000

#define DMA2_BASE	0xFF294000
#define DMA2_SIZE	0x00004000

#define SSP0_BASE	0xFF2A0000
#define SSP0_SIZE	0x00001000

#define SSP1_BASE	0xFF2A1000
#define SSP1_SIZE	0x00001000

#define SSP2_BASE	0xFF2A2000
#define SSP2_SIZE	0x00001000

#define SSP3_BASE	0xFF2A4000
#define SSP3_SIZE	0x00001000

#define SSP4_BASE	0xFF2A5000
#define SSP4_SIZE	0x00001000

#define SSP5_BASE	0xFF2A6000
#define SSP5_SIZE	0x00001000

/*
 * The Heap and Stack on Baytrail are organised like this :-
 *
 * +--------------------------------------------------------------------------+
 * | Offset              | Region         |  Size                             |
 * +---------------------+----------------+-----------------------------------+
 * | DRAM0_BASE          | RO Data        |  REEF_DATA_SIZE                   |
 * |                     | Data           |                                   |
 * |                     | BSS            |                                   |
 * +---------------------+----------------+-----------------------------------+
 * | HEAP_SYSTEM_BASE    | System Heap    |  HEAP_SYSTEM_SIZE                 |
 * +---------------------+----------------+-----------------------------------+
 * | HEAP_RUNTIME_BASE   | Runtime Heap   |  HEAP_RUNTIME_SIZE                |
 * +---------------------+----------------+-----------------------------------+
 * | HEAP_BUFFER_BASE    | Module Buffers |  HEAP_BUFFER_SIZE                 |
 * +---------------------+----------------+-----------------------------------+
 * | REEF_STACK_END      | Stack          |  REEF_STACK_SIZE                  |
 * +---------------------+----------------+-----------------------------------+
 * | REEF_STACK_BASE     |                |                                   |
 * +---------------------+----------------+-----------------------------------+
 */


/* Heap section sizes for module pool */
#define HEAP_RT_COUNT8			0
#define HEAP_RT_COUNT16		    256
#define HEAP_RT_COUNT32		    128
#define HEAP_RT_COUNT64		    64
#define HEAP_RT_COUNT128		32
#define HEAP_RT_COUNT256		16
#define HEAP_RT_COUNT512		8
#define HEAP_RT_COUNT1024		4

/* Heap configuration */
#define REEF_DATA_SIZE			0x6800

#define HEAP_SYSTEM_BASE		(DRAM0_BASE + REEF_DATA_SIZE)
#define HEAP_SYSTEM_SIZE		0x2000

#define HEAP_RUNTIME_BASE		(HEAP_SYSTEM_BASE + HEAP_SYSTEM_SIZE)
#define HEAP_RUNTIME_SIZE \
	(HEAP_RT_COUNT8 * 8 + HEAP_RT_COUNT16 * 16 + \
	HEAP_RT_COUNT32 * 32 + HEAP_RT_COUNT64 * 64 + \
	HEAP_RT_COUNT128 * 128 + HEAP_RT_COUNT256 * 256 + \
	HEAP_RT_COUNT512 * 512 + HEAP_RT_COUNT1024 * 1024)

#define HEAP_BUFFER_BASE		(HEAP_RUNTIME_BASE + HEAP_RUNTIME_SIZE)
#define HEAP_BUFFER_SIZE	\
    (DRAM0_SIZE - HEAP_RUNTIME_SIZE - REEF_STACK_SIZE - HEAP_SYSTEM_SIZE)

#define HEAP_BUFFER_BLOCK_SIZE		0x180
#define HEAP_BUFFER_COUNT	(HEAP_BUFFER_SIZE / HEAP_BUFFER_BLOCK_SIZE)

/* DMA buffer heap is the same physical memory as buffer heap on baytrail */
#define HEAP_DMA_BUFFER_BASE		0
#define HEAP_DMA_BUFFER_SIZE		0
#define HEAP_DMA_BUFFER_BLOCK_SIZE	0
#define HEAP_DMA_BUFFER_COUNT		0

/* Stack configuration */
#define REEF_STACK_SIZE				0x1000
#define REEF_STACK_BASE				(DRAM0_BASE + DRAM0_SIZE)
#define REEF_STACK_END				(REEF_STACK_BASE - REEF_STACK_SIZE)

/* Vector and literal sizes - not in core-isa.h */
#define REEF_MEM_VECT_LIT_SIZE		0x4
#define REEF_MEM_VECT_TEXT_SIZE		0x1c
#define REEF_MEM_VECT_SIZE		(REEF_MEM_VECT_TEXT_SIZE + REEF_MEM_VECT_LIT_SIZE)

#define REEF_MEM_RESET_TEXT_SIZE	0x2e0
#define REEF_MEM_RESET_LIT_SIZE		0x120
#define REEF_MEM_VECBASE_LIT_SIZE	0x178

#define REEF_MEM_RO_SIZE			0x8

#endif
