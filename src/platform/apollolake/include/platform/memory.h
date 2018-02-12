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
 *         Keyon Jie <yang.jie@linux.intel.com>
 */

#ifndef __PLATFORM_MEMORY_H__
#define __PLATFORM_MEMORY_H__

#include <config.h>

/* physical DSP addresses */

/* shim */
#define SHIM_BASE		0x00001000
#define SHIM_SIZE		0x00000100

/* cmd IO to audio codecs */
#define CMD_BASE		0x00001100
#define CMD_SIZE		0x00000010

/* resource allocation */
#define RES_BASE		0x00001110
#define RES_SIZE		0x00000010

/* IPC to the host */
#define IPC_HOST_BASE		0x00001180
#define IPC_HOST_SIZE		0x00000020

/* IPC to the CSME */
#define IPC_CSME_BASE		0x000011A0
#define IPC_CSME_SIZE		0x00000020

/* intra DSP  IPC */
#define IPC_DSP_SIZE		0x00000080
#define IPC_DSP_BASE(x)		(0x00001200 + x * IPC_DSP_SIZE)

/* SHA 256 */
#define SHA256_BASE		0x00001400
#define SHA256_SIZE		0x00000100

/* L2 cache */
#define L2C_CTRL_BASE		0x00001500
#define L2C_CTRL_SIZE		0x00000010

#define L2C_PREF_BASE		0x00001510
#define L2C_PREF_SIZE		0x00000010

#define L2C_PERF_BASE		0x00001520
#define L2C_PERF_SIZE		0x00000010

/* SRAM window for HOST */
#define HOST_WIN_SIZE		0x00000008
#define HOST_WIN_BASE(x)	(0x00001580 + x * HOST_WIN_SIZE)

/* SRAM window for CSME */
#define CSME_WIN_SIZE		0x00000008
#define CSME_WIN_BASE(x)	(0x000015A0 + x * HOST_WIN_SIZE)

/* IRQ controller */
#define IRQ_BASE		 0x00001600
#define IRQ_SIZE		0x00000200

/* time stamping */
#define TIME_BASE		0x00001800
#define TIME_SIZE		0x00000200

/* M/N dividers */
#define MN_BASE			0x00008E00
#define MN_SIZE			0x00000200

/* low power DMA position */
#define LP_GP_DMA_LINK_SIZE	0x00000080
#define LP_GP_DMA_LINK_BASE(x) (0x00001C00 + x * LP_GP_DMA_LINK_SIZE)

/* high performance DMA position */
#define HP_GP_DMA_LINK_SIZE	0x00000800
#define HP_GP_DMA_LINK_BASE(x)	(0x00001D00 + x * HP_GP_DMA_LINK_SIZE)

/* link DMAC stream */
#define GTW_LINK_OUT_STREAM_SIZE	0x00000020
#define GTW_LINK_OUT_STREAM_BASE(x) \
				(0x00002400 + x * GTW_LINK_OUT_STREAM_SIZE)

#define GTW_LINK_IN_STREAM_SIZE	0x00000020
#define GTW_LINK_IN_STREAM_BASE(x) \
				(0x00002600 + x * GTW_LINK_IN_STREAM_SIZE)

/* host DMAC stream */
#define GTW_HOST_OUT_STREAM_SIZE	0x00000040
#define GTW_HOST_OUT_STREAM_BASE(x) \
				(0x00002800 + x * GTW_HOST_OUT_STREAM_SIZE)

#define GTW_HOST_IN_STREAM_SIZE		0x00000040
#define GTW_HOST_IN_STREAM_BASE(x) \
				(0x00002C00 + x * GTW_HOST_IN_STREAM_SIZE)

/* code loader */
#define GTW_CODE_LDR_SIZE	0x00000040
#define GTW_CODE_LDR_BASE	0x00002BC0

/* L2 TLBs */
#define L2_HP_SRAM_TLB_SIZE	0x00001000
#define L2_HP_SRAM_TLB_BASE	0x00003000

/* DMICs */
#define DMIC_BASE		0x00004000
#define DMIC_SIZE		0x00004000

/* SSP */
#define SSP_SIZE		0x0000200
#define SSP_BASE(x)		(0x00008000 + x * SSP_SIZE)

/* low power DMACs */
#define LP_GP_DMA_SIZE		0x00001000
#define LP_GP_DMA_BASE(x)	(0x0000C000 + x * LP_GP_DMA_SIZE)

/* high performance DMACs */
#define HP_GP_DMA_SIZE		0x00001000
#define HP_GP_DMA_BASE(x)	(0x0000E000 + x * HP_GP_DMA_SIZE)

/* ROM */
#define ROM_BASE		0xBEFE0000
#define ROM_SIZE		0x00002000

/* 
 * The L2 SRAM Heap and Stack on Apololake are organised like this :-
 *
 * +--------------------------------------------------------------------------+
 * | Offset              | Region         |  Size                             |
 * +---------------------+----------------+-----------------------------------+
 * | L2_SRAM_BASE        | RO Data        |  REEF_DATA_SIZE                   |
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

/* L2 SRAM */
#define L2_SRAM_BASE		0xA000A000
#define L2_SRAM_SIZE		0x00056000

/* Heap section sizes for module pool */
#define HEAP_RT_COUNT8			0
#define HEAP_RT_COUNT16		    	256
#define HEAP_RT_COUNT32		    	128
#define HEAP_RT_COUNT64		    	64
#define HEAP_RT_COUNT128		32
#define HEAP_RT_COUNT256		16
#define HEAP_RT_COUNT512		8
#define HEAP_RT_COUNT1024		4

/* text and data share the same L2 SRAM on Broxton */
#define REEF_TEXT_START		L2_SRAM_BASE
#define REEF_TEXT_START_SIZE		0x400
#define L2_VECTOR_SIZE		0x1000

#define REEF_TEXT_BASE		(L2_SRAM_BASE + L2_VECTOR_SIZE)
#define REEF_TEXT_SIZE		0x18000

/* initialized data */
#define REEF_DATA_SIZE		0x18000

/* bss data */
#define REEF_BSS_DATA_SIZE		0x2000

/* Heap configuration */
#define HEAP_SYSTEM_BASE		(REEF_TEXT_START + REEF_TEXT_SIZE + \
	REEF_DATA_SIZE + REEF_BSS_DATA_SIZE)
#define HEAP_SYSTEM_SIZE		0x2000

#define HEAP_RUNTIME_BASE		(HEAP_SYSTEM_BASE + HEAP_SYSTEM_SIZE)
#define HEAP_RUNTIME_SIZE \
	(HEAP_RT_COUNT8 * 8 + HEAP_RT_COUNT16 * 16 + \
	HEAP_RT_COUNT32 * 32 + HEAP_RT_COUNT64 * 64 + \
	HEAP_RT_COUNT128 * 128 + HEAP_RT_COUNT256 * 256 + \
	HEAP_RT_COUNT512 * 512 + HEAP_RT_COUNT1024 * 1024)

#define HEAP_BUFFER_BASE		(HEAP_RUNTIME_BASE + HEAP_RUNTIME_SIZE)
#define HEAP_BUFFER_SIZE	\
    (L2_SRAM_SIZE - L2_VECTOR_SIZE - REEF_TEXT_SIZE - REEF_DATA_SIZE - \
     REEF_BSS_DATA_SIZE - HEAP_RUNTIME_SIZE - REEF_STACK_SIZE - HEAP_SYSTEM_SIZE)

#define HEAP_BUFFER_BLOCK_SIZE		0x180
#define HEAP_BUFFER_COUNT	(HEAP_BUFFER_SIZE / HEAP_BUFFER_BLOCK_SIZE)


/* Stack configuration */
#define REEF_STACK_SIZE				0x1000
#define REEF_STACK_BASE				(L2_SRAM_BASE + L2_SRAM_SIZE)
#define REEF_STACK_END				(REEF_STACK_BASE - REEF_STACK_SIZE)

/* 
 * The HP SRAM Region Apololake is organised like this :-
 * +--------------------------------------------------------------------------+
 * | Offset              | Region         |  Size                             |
 * +---------------------+----------------+-----------------------------------+
 * | HP_SRAM_BASE        | DMA            |  HEAP_HP_BUFFER_SIZE              |
 * +---------------------+----------------+-----------------------------------+
 * | SRAM_TRACE_BASE     | Trace Buffer W3|  SRAM_TRACE_SIZE                  |
 * +---------------------+----------------+-----------------------------------+
 * | SRAM_DEBUG_BASE     | Debug data  W2 |  SRAM_DEBUG_SIZE                  |
 * +---------------------+----------------+-----------------------------------+
 * | SRAM_INBOX_BASE     | Inbox  W1      |  SRAM_INBOX_SIZE                  |
 * +---------------------+----------------+-----------------------------------+
 * | SRAM_SW_REG_BASE    | SW Registers W0|  SRAM_SW_REG_SIZE                 |
 * +---------------------+----------------+-----------------------------------+
 * | SRAM_OUTBOX_BASE    | Outbox W0      |  SRAM_MAILBOX_SIZE                |
 * +---------------------+----------------+-----------------------------------+ 
 */

/* HP SRAM */
#define HP_SRAM_BASE		0xBE000000
#define HP_SRAM_SIZE		0x00020000

/* HP SRAM windows */	

/* window 3 */
#define SRAM_TRACE_BASE		(HEAP_HP_BUFFER_BASE + HEAP_HP_BUFFER_SIZE)
#define SRAM_TRACE_SIZE		0x2000

/* window 2 */
#define SRAM_DEBUG_BASE		(SRAM_TRACE_BASE + SRAM_TRACE_SIZE)
#define SRAM_DEBUG_SIZE		0x1000

/* window 1 */
#define SRAM_INBOX_BASE		(SRAM_DEBUG_BASE + SRAM_DEBUG_SIZE)
#define SRAM_INBOX_SIZE		0x2000

/* window 0 */
#define SRAM_SW_REG_BASE	(SRAM_INBOX_BASE + SRAM_INBOX_SIZE)
#define SRAM_SW_REG_SIZE	0x1000

#define SRAM_OUTBOX_BASE	(SRAM_SW_REG_BASE + SRAM_SW_REG_SIZE)
#define SRAM_OUTBOX_SIZE	0x1000

/* read only for host, can be used as trace/debug/stream... */
//#define SRAM_MAILBOX_UP_BASE	SRAM_MAILBOX_BASE
//#define SRAM_MAILBOX_UP_SIZE	0x1000

#define HP_SRAM_WIN0_BASE	SRAM_SW_REG_BASE
#define HP_SRAM_WIN0_SIZE	(SRAM_SW_REG_SIZE + SRAM_OUTBOX_SIZE)
#define HP_SRAM_WIN1_BASE	SRAM_INBOX_BASE
#define HP_SRAM_WIN1_SIZE	SRAM_INBOX_SIZE
#define HP_SRAM_WIN2_BASE	SRAM_DEBUG_BASE
#define HP_SRAM_WIN2_SIZE	SRAM_DEBUG_SIZE
#define HP_SRAM_WIN3_BASE	SRAM_TRACE_BASE
#define HP_SRAM_WIN3_SIZE	SRAM_TRACE_SIZE

/* DMA buffer heap is the HP physical memory on Broxton */
#define HEAP_DMA_BUFFER_BASE		HP_SRAM_BASE
#define HEAP_DMA_BUFFER_SIZE		HEAP_HP_BUFFER_SIZE
#define HEAP_DMA_BUFFER_BLOCK_SIZE	HEAP_HP_BUFFER_BLOCK_SIZE
#define HEAP_DMA_BUFFER_COUNT		HEAP_HP_BUFFER_COUNT

/* HP SRAM Heap */
#define HEAP_HP_BUFFER_BASE	HP_SRAM_BASE
#define HEAP_HP_BUFFER_SIZE \
		(HP_SRAM_SIZE - \
		SRAM_TRACE_SIZE - \
		SRAM_DEBUG_SIZE - \
		SRAM_INBOX_SIZE - \
		SRAM_OUTBOX_SIZE - \
		SRAM_SW_REG_SIZE)

#define HEAP_HP_BUFFER_BLOCK_SIZE	0x180
#define HEAP_HP_BUFFER_COUNT \
				(HEAP_HP_BUFFER_SIZE / HEAP_HP_BUFFER_BLOCK_SIZE)

/* 
 * The LP SRAM Heap and Stack on Apololake are organised like this :-
 *
 * +--------------------------------------------------------------------------+
 * | Offset              | Region         |  Size                             |
 * +---------------------+----------------+-----------------------------------+
 * | LP_SRAM_BASE        | RO Data        |  REEF_LP_DATA_SIZE                |
 * |                     | Data           |                                   |
 * |                     | BSS            |                                   |
 * +---------------------+----------------+-----------------------------------+
 * | HEAP_LP_SYSTEM_BASE | System Heap    |  HEAP_LP_SYSTEM_SIZE                 |
 * +---------------------+----------------+-----------------------------------+
 * | HEAP_LP_RUNTIME_BASE| Runtime Heap   |  HEAP_LP_RUNTIME_SIZE                |
 * +---------------------+----------------+-----------------------------------+
 * | HEAP_LP_BUFFER_BASE | Module Buffers |  HEAP_LP_BUFFER_SIZE                 |
 * +---------------------+----------------+-----------------------------------+
 * | REEF_LP_STACK_END   | Stack          |  REEF_LP_STACK_SIZE                  |
 * +---------------------+----------------+-----------------------------------+ 
 * | REEF_STACK_BASE     |                |                                   |
 * +---------------------+----------------+-----------------------------------+
 */

/* LP SRAM */
#define LP_SRAM_BASE		0xBE800000
#define LP_SRAM_SIZE		0x00020000

/* Heap section sizes for module pool */
#define HEAP_RT_LP_COUNT8			0
#define HEAP_RT_LP_COUNT16		    	256
#define HEAP_RT_LP_COUNT32		    	128
#define HEAP_RT_LP_COUNT64		    	64
#define HEAP_RT_LP_COUNT128			32
#define HEAP_RT_LP_COUNT256			16
#define HEAP_RT_LP_COUNT512			8
#define HEAP_RT_LP_COUNT1024			4

/* Heap configuration */
#define REEF_LP_DATA_SIZE			0x4000

#define HEAP_LP_SYSTEM_BASE		(LP_SRAM_BASE + REEF_LP_DATA_SIZE)
#define HEAP_LP_SYSTEM_SIZE		0x1000

#define HEAP_LP_RUNTIME_BASE		(HEAP_LP_SYSTEM_BASE + HEAP_LP_SYSTEM_SIZE)
#define HEAP_LP_RUNTIME_SIZE \
	(HEAP_RT_LP_COUNT8 * 8 + HEAP_RT_LP_COUNT16 * 16 + \
	HEAP_RT_LP_COUNT32 * 32 + HEAP_RT_LP_COUNT64 * 64 + \
	HEAP_RT_LP_COUNT128 * 128 + HEAP_RT_LP_COUNT256 * 256 + \
	HEAP_RT_LP_COUNT512 * 512 + HEAP_RT_LP_COUNT1024 * 1024)

#define HEAP_LP_BUFFER_BASE		(HEAP_LP_RUNTIME_BASE + HEAP_LP_RUNTIME_SIZE)
#define HEAP_LP_BUFFER_SIZE	\
    (LP_SRAM_SIZE - HEAP_LP_RUNTIME_SIZE - REEF_LP_STACK_SIZE - HEAP_LP_SYSTEM_SIZE)

#define HEAP_LP_BUFFER_BLOCK_SIZE		0x180
#define HEAP_LP_BUFFER_COUNT	(HEAP_LP_BUFFER_SIZE / HEAP_LP_BUFFER_BLOCK_SIZE)


/* Stack configuration */
#define REEF_LP_STACK_SIZE			0x1000
#define REEF_LP_STACK_BASE			(LP_SRAM_BASE + LP_SRAM_SIZE)
#define REEF_LP_STACK_END			(REEF_LP_STACK_BASE - REEF_LP_STACK_SIZE)


/* Vector and literal sizes - not in core-isa.h */
#define REEF_MEM_VECT_LIT_SIZE		0x8
#define REEF_MEM_VECT_TEXT_SIZE		0x38
#define REEF_MEM_VECT_SIZE		(REEF_MEM_VECT_TEXT_SIZE + REEF_MEM_VECT_LIT_SIZE)

#define REEF_MEM_ERROR_TEXT_SIZE	0x180
#define REEF_MEM_ERROR_LIT_SIZE		0x8

#define REEF_MEM_RESET_TEXT_SIZE	0x268
#define REEF_MEM_RESET_LIT_SIZE		0x8
#define REEF_MEM_VECBASE_LIT_SIZE	0x178

#define REEF_MEM_RO_SIZE			0x8
#endif
