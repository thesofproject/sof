/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright 2020 NXP
 *
 * Author: Daniel Baluta <daniel.baluta@nxp.com>
 */

#ifdef __SOF_LIB_MEMORY_H__

#ifndef __PLATFORM_LIB_MEMORY_H__
#define __PLATFORM_LIB_MEMORY_H__

#include <rtos/cache.h>

/* physical DSP addresses */

#define DRAM0_BASE	0x3b6e8000
#define DRAM0_SIZE	0x8000

#define DRAM1_BASE	0x3b6f0000
#define DRAM1_SIZE	0x8000

#define SDRAM0_BASE	0x92400000
#define SDRAM0_SIZE	0x800000

#define SDRAM1_SIZE	0x800000

#define XSHAL_MU2_SIDEB_BYPASS_PADDR 0x30E70000
#define MU_BASE		XSHAL_MU2_SIDEB_BYPASS_PADDR

/*
 * The Heap and Stack on i.MX8M are organised like this :-
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

#include <platform/lib/mailbox_layout.h>

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
#define HEAP_RT_COUNT512	32
#define HEAP_RT_COUNT1024	4
#define HEAP_RT_COUNT2048	4
#define HEAP_RT_COUNT4096	4

/* Heap section sizes for system runtime heap */
#define HEAP_SYS_RT_COUNT64	128
#define HEAP_SYS_RT_COUNT512	16
#define HEAP_SYS_RT_COUNT1024	8

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
	HEAP_RT_COUNT2048 * 2048 + HEAP_RT_COUNT4096 * 4096)

#define HEAP_BUFFER_BASE	(HEAP_RUNTIME_BASE + HEAP_RUNTIME_SIZE)
#define HEAP_BUFFER_SIZE \
	(SDRAM1_SIZE - SOF_MAILBOX_SIZE - HEAP_RUNTIME_SIZE - SOF_STACK_TOTAL_SIZE -\
	HEAP_SYS_RUNTIME_SIZE - HEAP_SYSTEM_SIZE)

#define HEAP_BUFFER_BLOCK_SIZE		0x100

/* Stack configuration */
#define SOF_STACK_SIZE		(CONFIG_SOF_STACK_SIZE)
#define SOF_STACK_TOTAL_SIZE	SOF_STACK_SIZE
#define SOF_STACK_BASE		(SDRAM1_BASE + SDRAM1_SIZE)
#define SOF_STACK_END		(SOF_STACK_BASE - SOF_STACK_TOTAL_SIZE)

#if !defined(__ASSEMBLER__) && !defined(LINKER)

struct sof;

void platform_init_memmap(struct sof *sof);
#endif

#define host_to_local(addr) (addr)
#define local_to_host(addr) (addr)

#endif /* __PLATFORM_LIB_MEMORY_H__ */

#else

#error "This file shouldn't be included from outside of sof/lib/memory.h"

#endif /* __SOF_LIB_MEMORY_H__ */
