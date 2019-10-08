/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright 2019 NXP
 *
 * Author: Daniel Baluta <daniel.baluta@nxp.com>
 */

#ifdef __SOF_LIB_MEMORY_H__

#ifndef __PLATFORM_LIB_MEMORY_H__
#define __PLATFORM_LIB_MEMORY_H__

#include <sof/lib/cache.h>
#include <config.h>

/* data cache line alignment */
#define PLATFORM_DCACHE_ALIGN	DCACHE_LINE_SIZE

/* physical DSP addresses */

#define IRAM_BASE	0x596f8000
#define IRAM_SIZE	0x800

#define DRAM0_BASE	0x596e8000
#define DRAM0_SIZE	0x8000

#define DRAM1_BASE	0x596f0000
#define DRAM1_SIZE	0x8000

#define SDRAM0_BASE	0x92400000
#define SDRAM0_SIZE	0x800000

#define SDRAM1_BASE	0x92C00000
#define SDRAM1_SIZE	0x800000

#define XSHAL_MU13_SIDEB_BYPASS_PADDR 0x5D310000
#define MU_BASE		XSHAL_MU13_SIDEB_BYPASS_PADDR

#define EDMA0_BASE	0x59200000
#define EDMA0_SIZE	0x10000

#define ESAI_BASE	0x59010000
#define ESAI_SIZE	0x00010000

#define SAI_1_BASE	0x59050000
#define SAI_1_SIZE	0x00010000

#define LOG_ENTRY_ELF_BASE	0x20000000
#define LOG_ENTRY_ELF_SIZE	0x2000000

/*
 * The Heap and Stack on i.MX8 are organised like this :-
 *
 * +--------------------------------------------------------------------------+
 * | Offset              | Region         |  Size                             |
 * +---------------------+----------------+-----------------------------------+
 * | SDRAM_BASE          | RO Data        |  SOF_DATA_SIZE                    |
 * |                     | Data           |                                   |
 * |                     | BSS            |                                   |
 * +---------------------+----------------+-----------------------------------+
 * | HEAP_SYSTEM_BASE    | System Heap    |  HEAP_SYSTEM_SIZE                 |
 * +---------------------+----------------+-----------------------------------+
 * | HEAP_RUNTIME_BASE   | Runtime Heap   |  HEAP_RUNTIME_SIZE                |
 * +---------------------+----------------+-----------------------------------+
 * | HEAP_BUFFER_BASE    | Module Buffers |  HEAP_BUFFER_SIZE                 |
 * +---------------------+----------------+-----------------------------------+
 * | SOF_STACK_END       | Stack          |  SOF_STACK_SIZE                   |
 * +---------------------+----------------+-----------------------------------+
 * | SOF_STACK_BASE      |                |                                   |
 * +---------------------+----------------+-----------------------------------+
 */

/* Mailbox configuration */
#define SRAM_OUTBOX_BASE	SDRAM1_BASE
#define SRAM_OUTBOX_SIZE	0x1000
#define SRAM_OUTBOX_OFFSET	0

#define SRAM_INBOX_BASE		(SRAM_OUTBOX_BASE + SRAM_OUTBOX_SIZE)
#define SRAM_INBOX_SIZE		0x1000
#define SRAM_INBOX_OFFSET	SRAM_OUTBOX_SIZE

#define SRAM_DEBUG_BASE		(SRAM_INBOX_BASE + SRAM_INBOX_SIZE)
#define SRAM_DEBUG_SIZE		0x800
#define SRAM_DEBUG_OFFSET	(SRAM_INBOX_OFFSET + SRAM_INBOX_SIZE)

#define SRAM_EXCEPT_BASE	(SRAM_DEBUG_BASE + SRAM_DEBUG_SIZE)
#define SRAM_EXCEPT_SIZE	0x800
#define SRAM_EXCEPT_OFFSET	(SRAM_DEBUG_OFFSET + SRAM_DEBUG_SIZE)

#define SRAM_STREAM_BASE	(SRAM_EXCEPT_BASE + SRAM_EXCEPT_SIZE)
#define SRAM_STREAM_SIZE	0x1000
#define SRAM_STREAM_OFFSET	(SRAM_EXCEPT_OFFSET + SRAM_EXCEPT_SIZE)

#define SRAM_TRACE_BASE		(SRAM_STREAM_BASE + SRAM_STREAM_SIZE)
#define SRAM_TRACE_SIZE		0x1000
#define SRAM_TRACE_OFFSET (SRAM_STREAM_OFFSET + SRAM_STREAM_SIZE)

#define SOF_MAILBOX_SIZE	(SRAM_INBOX_SIZE + SRAM_OUTBOX_SIZE \
				+ SRAM_DEBUG_SIZE + SRAM_EXCEPT_SIZE \
				+ SRAM_STREAM_SIZE + SRAM_TRACE_SIZE)

/* Heap section sizes for module pool */
#define HEAP_RT_COUNT8		0
#define HEAP_RT_COUNT16		48
#define HEAP_RT_COUNT32		48
#define HEAP_RT_COUNT64		32
#define HEAP_RT_COUNT128	32
#define HEAP_RT_COUNT256	32
#define HEAP_RT_COUNT512	4
#define HEAP_RT_COUNT1024	4
#define HEAP_RT_COUNT2048	4

/* Heap section sizes for system runtime heap */
#define HEAP_SYS_RT_COUNT64	64
#define HEAP_SYS_RT_COUNT512	8
#define HEAP_SYS_RT_COUNT1024	4

/* Heap configuration */

#define HEAP_SYSTEM_BASE	SDRAM1_BASE + SOF_MAILBOX_SIZE
#define HEAP_SYSTEM_SIZE	0xe000

#define HEAP_SYSTEM_0_BASE	HEAP_SYSTEM_BASE

#define HEAP_SYS_RUNTIME_BASE	(HEAP_SYSTEM_BASE + HEAP_SYSTEM_SIZE)
#define HEAP_SYS_RUNTIME_SIZE \
	(HEAP_SYS_RT_COUNT64 * 64 + HEAP_SYS_RT_COUNT512 * 512 + \
	HEAP_SYS_RT_COUNT1024 * 1024)

#define HEAP_RUNTIME_BASE	(HEAP_SYS_RUNTIME_BASE + HEAP_SYS_RUNTIME_SIZE)
#define HEAP_RUNTIME_SIZE \
	(HEAP_RT_COUNT8 * 8 + HEAP_RT_COUNT16 * 16 + \
	HEAP_RT_COUNT32 * 32 + HEAP_RT_COUNT64 * 64 + \
	HEAP_RT_COUNT128 * 128 + HEAP_RT_COUNT256 * 256 + \
	HEAP_RT_COUNT512 * 512 + HEAP_RT_COUNT1024 * 1024 + \
	HEAP_RT_COUNT2048 * 2048)

#define HEAP_BUFFER_BASE	(HEAP_RUNTIME_BASE + HEAP_RUNTIME_SIZE)
#define HEAP_BUFFER_SIZE \
	(SDRAM1_SIZE - SOF_MAILBOX_SIZE - HEAP_RUNTIME_SIZE - SOF_STACK_TOTAL_SIZE -\
	HEAP_SYS_RUNTIME_SIZE - HEAP_SYSTEM_SIZE)

#define HEAP_BUFFER_BLOCK_SIZE		0x180
#define HEAP_BUFFER_COUNT	(HEAP_BUFFER_SIZE / HEAP_BUFFER_BLOCK_SIZE)

#define PLATFORM_HEAP_SYSTEM		1 /* one per core */
#define PLATFORM_HEAP_SYSTEM_RUNTIME	1 /* one per core */
#define PLATFORM_HEAP_RUNTIME		1
#define PLATFORM_HEAP_BUFFER		1

/* Stack configuration */
#define SOF_STACK_SIZE		0x1000
#define SOF_STACK_TOTAL_SIZE	SOF_STACK_SIZE
#define SOF_STACK_BASE		(SDRAM1_BASE + SDRAM1_SIZE)
#define SOF_STACK_END		(SOF_STACK_BASE - SOF_STACK_TOTAL_SIZE)

/* Vector and literal sizes - not in core-isa.h */
#define SOF_MEM_VECT_LIT_SIZE		0x4
#define SOF_MEM_VECT_TEXT_SIZE		0x1c
#define SOF_MEM_VECT_SIZE		(SOF_MEM_VECT_TEXT_SIZE + SOF_MEM_VECT_LIT_SIZE)

#define SOF_MEM_RESET_TEXT_SIZE		0x2e0
#define SOF_MEM_RESET_LIT_SIZE		0x120
#define SOF_MEM_VECBASE_LIT_SIZE	0x178

#define SOF_MEM_RO_SIZE			0x8

#define uncache_to_cache(address)	address
#define cache_to_uncache(address)	address
#define is_uncached(address)		0

#define HEAP_BUF_ALIGNMENT		PLATFORM_DCACHE_ALIGN

#if !defined(__ASSEMBLER__) && !defined(LINKER)
void platform_init_memmap(void);
#endif

#endif /* __PLATFORM_LIB_MEMORY_H__ */

#else

#error "This file shouldn't be included from outside of sof/lib/memory.h"

#endif /* __SOF_LIB_MEMORY_H__ */
