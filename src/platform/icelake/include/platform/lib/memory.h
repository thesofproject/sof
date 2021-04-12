/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2017 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 *         Rander Wang <rander.wang@intel.com>
 */

#ifdef __SOF_LIB_MEMORY_H__

#ifndef __PLATFORM_LIB_MEMORY_H__
#define __PLATFORM_LIB_MEMORY_H__

#include <cavs/lib/memory.h>
#include <sof/lib/cpu.h>

/* physical DSP addresses */

/* shim */
#define SHIM_BASE		0x00071F00
#define SHIM_SIZE		0x00000100

/* Digital Mic Shim Registers */
#define DMIC_SHIM_BASE	0x00071E80
#define DMICLCTL_OFFSET 0x04
#define DMICLCTL	(DMIC_SHIM_BASE + DMICLCTL_OFFSET)

/* cmd IO to audio codecs */
#define CMD_BASE		0x00001100
#define CMD_SIZE		0x00000010

/* resource allocation */
#define RES_BASE		0x00001110
#define RES_SIZE		0x00000010

/* IPC to the host */
#define IPC_HOST_BASE		0x00071E00
#define IPC_HOST_SIZE		0x00000020

/* intra DSP  IPC */
#define IPC_DSP_SIZE		0x00000080
#define IPC_DSP_BASE(x)		(0x00001200 + x * IPC_DSP_SIZE)

/* SRAM window for HOST */
#define HOST_WIN_SIZE		0x00000008
#define HOST_WIN_BASE(x)	(0x00071A00 + x * HOST_WIN_SIZE)

/* IRQ controller */
#define IRQ_BASE		0x00078800
#define IRQ_SIZE		0x00000200

/* time stamping */
#define TIME_BASE		0x00071800
#define TIME_SIZE		0x00000200

/* M/N dividers */
#define MN_BASE			0x00078C00
#define MN_SIZE			0x00000200

/* low power DMA position */
#define LP_GP_DMA_LINK_SIZE	0x00000010
#define LP_GP_DMA_LINK_BASE(x) (0x00001C00 + x * LP_GP_DMA_LINK_SIZE)

/* high performance DMA position */
#define HP_GP_DMA_LINK_SIZE	0x00000010
#define HP_GP_DMA_LINK_BASE(x)	(0x00001D00 + x * HP_GP_DMA_LINK_SIZE)

/* link DMAC stream */
#define GTW_LINK_OUT_STREAM_SIZE	0x00000020
#define GTW_LINK_OUT_STREAM_BASE(x) \
				(0x00072400 + x * GTW_LINK_OUT_STREAM_SIZE)

#define GTW_LINK_IN_STREAM_SIZE	0x00000020
#define GTW_LINK_IN_STREAM_BASE(x) \
				(0x00072600 + x * GTW_LINK_IN_STREAM_SIZE)

/* host DMAC stream */
#define GTW_HOST_OUT_STREAM_SIZE	0x00000040
#define GTW_HOST_OUT_STREAM_BASE(x) \
				(0x00072800 + x * GTW_HOST_OUT_STREAM_SIZE)

#define GTW_HOST_IN_STREAM_SIZE		0x00000040
#define GTW_HOST_IN_STREAM_BASE(x) \
				(0x00072C00 + x * GTW_HOST_IN_STREAM_SIZE)

/* code loader */
#define GTW_CODE_LDR_SIZE	0x00000040
#define GTW_CODE_LDR_BASE	0x00002BC0

/* L2 TLBs */
#define L2_HP_SRAM_TLB_SIZE	0x00001000
#define L2_HP_SRAM_TLB_BASE	0x00003000

/* DMICs */
#define DMIC_BASE		0x00010000
#define DMIC_SIZE		0x00008000

/* SSP */
#define SSP_BASE(x)		(0x00077000 + x * SSP_SIZE)
#define SSP_SIZE		0x0000200

/* ALH */
#define ALH_BASE		0x000071000
#define ALH_TXDA_OFFSET		0x000000400
#define ALH_RXDA_OFFSET		0x000000500
#define ALH_STREAM_OFFSET	0x000000004

/* Timestamping */
#define TIMESTAMP_BASE		0x00071800

/* low power DMACs */
#define LP_GP_DMA_SIZE		0x00001000
#define LP_GP_DMA_BASE(x)	(0x0007C000 + x * LP_GP_DMA_SIZE)

/* high performance DMACs */
#define HP_GP_DMA_SIZE		0x00001000
#define HP_GP_DMA_BASE(x)	(0x0000E000 + x * HP_GP_DMA_SIZE)

/* ROM */
#define ROM_BASE		0xBEFE0000
#define ROM_SIZE		0x00002000

#define L2_VECTOR_SIZE		0x1000

#define UUID_ENTRY_ELF_BASE	0x1FFFA000
#define UUID_ENTRY_ELF_SIZE	0x6000

#define LOG_ENTRY_ELF_BASE	0x20000000
#define LOG_ENTRY_ELF_SIZE	0x2000000

#define EXT_MANIFEST_ELF_BASE	(LOG_ENTRY_ELF_BASE + LOG_ENTRY_ELF_SIZE)
#define EXT_MANIFEST_ELF_SIZE	0x2000000

/*
 * The HP SRAM Region on Icelake is organised like this :-
 * +--------------------------------------------------------------------------+
 * | Offset           | Region                  |  Size                       |
 * +------------------+-------------------------+-----------------------------+
 * | SRAM_SW_REG_BASE | SW Registers W0         |  SRAM_SW_REG_SIZE           |
 * +------------------+-------------------------+-----------------------------+
 * | SRAM_OUTBOX_BASE | Outbox W0               |  SRAM_MAILBOX_SIZE          |
 * +------------------+-------------------------+-----------------------------+
 * | SRAM_INBOX_BASE  | Inbox  W1               |  SRAM_INBOX_SIZE            |
 * +------------------+-------------------------+-----------------------------+
 * | SRAM_DEBUG_BASE  | Debug data  W2          |  SRAM_DEBUG_SIZE            |
 * +------------------+-------------------------+-----------------------------+
 * | SRAM_EXCEPT_BASE | Debug data  W2          |  SRAM_EXCEPT_SIZE           |
 * +------------------+-------------------------+-----------------------------+
 * | SRAM_STREAM_BASE | Stream data W2          |  SRAM_STREAM_SIZE           |
 * +------------------+-------------------------+-----------------------------+
 * | SRAM_TRACE_BASE  | Trace Buffer W3         |  SRAM_TRACE_SIZE            |
 * +------------------+-------------------------+-----------------------------+
 * | SOF_FW_START     | text                    |                             |
 * |                  | data                    |                             |
 * |                  | BSS                     |                             |
 * +------------------+-------------------------+-----------------------------+
 * |                  | Runtime Heap            |  HEAP_RUNTIME_SIZE          |
 * +------------------+-------------------------+-----------------------------+
 * |                  | Runtime shared Heap     |  HEAP_RUNTIME_SHARED_SIZE   |
 * |                  |-------------------------+-----------------------------+
 * |                  | System shared Heap      |  HEAP_SYSTEM_SHARED_SIZE    |
 * |                  |-------------------------+-----------------------------+
 * |                  | Module Buffers          |  HEAP_BUFFER_SIZE           |
 * +------------------+-------------------------+-----------------------------+
 * |                  | Primary core Sys Heap   |  HEAP_SYSTEM_M_SIZE         |
 * +------------------+-------------------------+-----------------------------+
 * |                  | Pri. Sys Runtime Heap   |  HEAP_SYS_RUNTIME_M_SIZE    |
 * +------------------+-------------------------+-----------------------------+
 * |                  | Primary core Stack      |  SOF_STACK_SIZE             |
 * +------------------+-------------------------+-----------------------------+
 * |                  | Sec. core Sys Heap      |  SOF_CORE_S_T_SIZE          |
 * |                  | Sec. Sys Runtime Heap   |                             |
 * |                  | Secondary core Stack    |                             |
 * +------------------+-------------------------+-----------------------------+
 */

/* HP SRAM */
#define HP_SRAM_BASE		0xBE000000

/* HP SRAM windows */
/* window 0 */
#define SRAM_SW_REG_BASE	(HP_SRAM_BASE + 0x4000)
#define SRAM_SW_REG_SIZE	0x1000

#define SRAM_OUTBOX_BASE	(SRAM_SW_REG_BASE + SRAM_SW_REG_SIZE)
#define SRAM_OUTBOX_SIZE	0x1000

/* window 1 */
#define SRAM_INBOX_BASE		(SRAM_OUTBOX_BASE + SRAM_OUTBOX_SIZE)
#define SRAM_INBOX_SIZE		0x2000
/* window 2 */
#define SRAM_DEBUG_BASE		(SRAM_INBOX_BASE + SRAM_INBOX_SIZE)
#define SRAM_DEBUG_SIZE		0x800

#define SRAM_EXCEPT_BASE	(SRAM_DEBUG_BASE + SRAM_DEBUG_SIZE)
#define SRAM_EXCEPT_SIZE	0x800

#define SRAM_STREAM_BASE	(SRAM_EXCEPT_BASE + SRAM_EXCEPT_SIZE)
#define SRAM_STREAM_SIZE	0x1000

/* window 3 */
#define SRAM_TRACE_BASE		(SRAM_STREAM_BASE + SRAM_STREAM_SIZE)
#if CONFIG_TRACE
#define SRAM_TRACE_SIZE		0x2000
#else
#define SRAM_TRACE_SIZE		0x0
#endif

#define HP_SRAM_WIN0_BASE	SRAM_SW_REG_BASE
#define HP_SRAM_WIN0_SIZE	(SRAM_SW_REG_SIZE + SRAM_OUTBOX_SIZE)
#define HP_SRAM_WIN1_BASE	SRAM_INBOX_BASE
#define HP_SRAM_WIN1_SIZE	SRAM_INBOX_SIZE
#define HP_SRAM_WIN2_BASE	SRAM_DEBUG_BASE
#define HP_SRAM_WIN2_SIZE	(SRAM_DEBUG_SIZE + SRAM_EXCEPT_SIZE + \
				SRAM_STREAM_SIZE)
#define HP_SRAM_WIN3_BASE	SRAM_TRACE_BASE
#define HP_SRAM_WIN3_SIZE	SRAM_TRACE_SIZE

/* HP SRAM Base */
#define HP_SRAM_VECBASE_RESET	(SRAM_TRACE_BASE + SRAM_TRACE_SIZE)

/* text and data share the same HP L2 SRAM on Icelake */
#define SOF_FW_START		(HP_SRAM_VECBASE_RESET + 0x400)
#define SOF_FW_BASE		(SOF_FW_START)

/* max size for all var-size sections (text/rodata/bss) */
#define SOF_FW_MAX_SIZE		(HP_SRAM_BASE + HP_SRAM_SIZE - SOF_FW_BASE)

#define SOF_TEXT_START		(SOF_FW_START)
#define SOF_TEXT_BASE		(SOF_FW_START)

/* Heap section sizes for system runtime heap for primary core */
#define HEAP_SYS_RT_0_COUNT64		128
#define HEAP_SYS_RT_0_COUNT512		16
#define HEAP_SYS_RT_0_COUNT1024		4

/* Heap section sizes for system runtime heap for secondary core */
#define HEAP_SYS_RT_X_COUNT64		64
#define HEAP_SYS_RT_X_COUNT512		8
#define HEAP_SYS_RT_X_COUNT1024		4

/* Heap section sizes for module pool */
#define HEAP_RT_COUNT64			128
#define HEAP_RT_COUNT128		64
#define HEAP_RT_COUNT256		128
#define HEAP_RT_COUNT512		8
#define HEAP_RT_COUNT1024		4
#define HEAP_RT_COUNT2048		1
#define HEAP_RT_COUNT4096		1

/* Heap configuration */
#define HEAP_RUNTIME_SIZE \
	(HEAP_RT_COUNT64 * 64 + HEAP_RT_COUNT128 * 128 + \
	HEAP_RT_COUNT256 * 256 + HEAP_RT_COUNT512 * 512 + \
	HEAP_RT_COUNT1024 * 1024 + HEAP_RT_COUNT2048 * 2048 + \
	HEAP_RT_COUNT4096 * 4096)

/* Heap section sizes for runtime shared heap */
#define HEAP_RUNTIME_SHARED_COUNT64	(64 + 32 * CONFIG_CORE_COUNT)
#define HEAP_RUNTIME_SHARED_COUNT128	64
#define HEAP_RUNTIME_SHARED_COUNT256	4
#define HEAP_RUNTIME_SHARED_COUNT512	16
#define HEAP_RUNTIME_SHARED_COUNT1024	4

#define HEAP_RUNTIME_SHARED_SIZE \
	(HEAP_RUNTIME_SHARED_COUNT64 * 64 + HEAP_RUNTIME_SHARED_COUNT128 * 128 + \
	HEAP_RUNTIME_SHARED_COUNT256 * 256 + HEAP_RUNTIME_SHARED_COUNT512 * 512 + \
	HEAP_RUNTIME_SHARED_COUNT1024 * 1024)

/* Heap section sizes for system shared heap */
#define HEAP_SYSTEM_SHARED_SIZE		0x1500

#define HEAP_BUFFER_SIZE	0x50000
#define HEAP_BUFFER_BLOCK_SIZE		0x100
#define HEAP_BUFFER_COUNT	(HEAP_BUFFER_SIZE / HEAP_BUFFER_BLOCK_SIZE)

#define HEAP_SYSTEM_M_SIZE		0x8000	/* heap primary core size */
#define HEAP_SYSTEM_S_SIZE		0x6000	/* heap secondary core size */
#define HEAP_SYSTEM_T_SIZE \
	(HEAP_SYSTEM_M_SIZE + ((CONFIG_CORE_COUNT - 1) * HEAP_SYSTEM_S_SIZE))

#define HEAP_SYS_RUNTIME_M_SIZE \
	(HEAP_SYS_RT_0_COUNT64 * 64 + HEAP_SYS_RT_0_COUNT512 * 512 + \
	HEAP_SYS_RT_0_COUNT1024 * 1024)

#define HEAP_SYS_RUNTIME_S_SIZE \
	(HEAP_SYS_RT_X_COUNT64 * 64 + HEAP_SYS_RT_X_COUNT512 * 512 + \
	HEAP_SYS_RT_X_COUNT1024 * 1024)

#define HEAP_SYS_RUNTIME_T_SIZE \
	(HEAP_SYS_RUNTIME_M_SIZE + ((CONFIG_CORE_COUNT - 1) * \
	HEAP_SYS_RUNTIME_S_SIZE))

/* Stack configuration */
#define SOF_STACK_SIZE		0x1000
#define SOF_STACK_TOTAL_SIZE	(CONFIG_CORE_COUNT * SOF_STACK_SIZE)

/* SOF Core S configuration */
#define SOF_CORE_S_SIZE \
	ALIGN((HEAP_SYSTEM_S_SIZE + HEAP_SYS_RUNTIME_S_SIZE + SOF_STACK_SIZE),\
	SRAM_BANK_SIZE)
#define SOF_CORE_S_T_SIZE ((CONFIG_CORE_COUNT - 1) * SOF_CORE_S_SIZE)

/*
 * The LP SRAM Heap and Stack on Icelake are organised like this :-
 *
 * +--------------------------------------------------------------------------+
 * | Offset              | Region         |  Size                             |
 * +---------------------+----------------+-----------------------------------+
 * | LP_SRAM_BASE        | RO Data        |  SOF_LP_DATA_SIZE                 |
 * |                     | Data           |                                   |
 * |                     | BSS            |                                   |
 * +---------------------+----------------+-----------------------------------+
 * | HEAP_LP_SYSTEM_BASE | System Heap    |  HEAP_LP_SYSTEM_SIZE              |
 * +---------------------+----------------+-----------------------------------+
 * | HEAP_LP_RUNTIME_BASE| Runtime Heap   |  HEAP_LP_RUNTIME_SIZE             |
 * +---------------------+----------------+-----------------------------------+
 * | HEAP_LP_BUFFER_BASE | Module Buffers |  HEAP_LP_BUFFER_SIZE              |
 * +---------------------+----------------+-----------------------------------+
 * | SOF_LP_STACK_END    | Stack          |  SOF_LP_STACK_SIZE                |
 * +---------------------+----------------+-----------------------------------+
 * | SOF_STACK_BASE      |                |                                   |
 * +---------------------+----------------+-----------------------------------+
 */

/* LP SRAM */
#define LP_SRAM_BASE		0xBE800000

#if (CONFIG_CAVS_LPS)
#define LPS_RESTORE_VECTOR_OFFSET 0x1000
#define LPS_RESTORE_VECTOR_SIZE 0x800
#define LPS_RESTORE_VECTOR_ADDR (LP_SRAM_BASE + LPS_RESTORE_VECTOR_OFFSET)
#define HEAP_LP_BUFFER_BASE (LPS_RESTORE_VECTOR_ADDR + LPS_RESTORE_VECTOR_SIZE)
#define HEAP_LP_BUFFER_SIZE (LP_SRAM_SIZE - LPS_RESTORE_VECTOR_SIZE -\
			     LPS_RESTORE_VECTOR_OFFSET)
#else
#define HEAP_LP_BUFFER_BASE LP_SRAM_BASE
#define HEAP_LP_BUFFER_SIZE LP_SRAM_SIZE
#endif

#define HEAP_LP_BUFFER_BLOCK_SIZE		0x180

#if CONFIG_LP_MEMORY_BANKS
#define HEAP_LP_BUFFER_COUNT \
	(HEAP_LP_BUFFER_SIZE / HEAP_LP_BUFFER_BLOCK_SIZE)
#else
#define	HEAP_LP_BUFFER_COUNT 0
#endif

#define PLATFORM_HEAP_SYSTEM		CONFIG_CORE_COUNT /* one per core */
#define PLATFORM_HEAP_SYSTEM_RUNTIME	CONFIG_CORE_COUNT /* one per core */
#define PLATFORM_HEAP_RUNTIME		1
#define PLATFORM_HEAP_RUNTIME_SHARED	1
#define PLATFORM_HEAP_SYSTEM_SHARED	1
#define PLATFORM_HEAP_BUFFER		2

/* Stack configuration */
#define SOF_LP_STACK_SIZE		0x1000
#define SOF_LP_STACK_BASE		(LP_SRAM_BASE + LP_SRAM_SIZE)
#define SOF_LP_STACK_END		(SOF_LP_STACK_BASE - SOF_LP_STACK_SIZE)

/* Vector and literal sizes - do not use core-isa.h */
#define SOF_MEM_VECBASE			HP_SRAM_VECBASE_RESET
#define SOF_MEM_VECT_LIT_SIZE		0x8
#define SOF_MEM_VECT_TEXT_SIZE		0x38
#define SOF_MEM_VECT_SIZE		(SOF_MEM_VECT_TEXT_SIZE + \
					SOF_MEM_VECT_LIT_SIZE)

#define SOF_MEM_ERROR_TEXT_SIZE	0x180
#define SOF_MEM_ERROR_LIT_SIZE		0x8

#define SOF_MEM_RESET_TEXT_SIZE	0x268
#define SOF_MEM_RESET_LIT_SIZE		0x8
#define SOF_MEM_VECBASE_LIT_SIZE	0x178

#define SOF_MEM_RO_SIZE			0x8

/* VM ROM sizes */
#define ROM_RESET_TEXT_SIZE	0x400
#define ROM_RESET_LIT_SIZE	0x200

/* boot loader in IMR */
#define IMR_BOOT_LDR_MANIFEST_BASE	0xB0032000
#define IMR_BOOT_LDR_MANIFEST_SIZE	0x6000

#define IMR_BOOT_LDR_TEXT_ENTRY_BASE	0xB0038000
#define IMR_BOOT_LDR_TEXT_ENTRY_SIZE	0x120
#define IMR_BOOT_LDR_LIT_BASE		(IMR_BOOT_LDR_TEXT_ENTRY_BASE + \
					IMR_BOOT_LDR_TEXT_ENTRY_SIZE)
#define IMR_BOOT_LDR_LIT_SIZE		0x22
#define IMR_BOOT_LDR_TEXT_BASE		(IMR_BOOT_LDR_LIT_BASE + \
					IMR_BOOT_LDR_LIT_SIZE)
#define IMR_BOOT_LDR_TEXT_SIZE		0x1C00
#define IMR_BOOT_LDR_DATA_BASE		0xB0039000
#define IMR_BOOT_LDR_DATA_SIZE		0x1000
#define IMR_BOOT_LDR_BSS_BASE		0xB0100000
#define IMR_BOOT_LDR_BSS_SIZE		0x10000

/* Temporary stack place for boot_ldr */
#define BOOT_LDR_STACK_BASE		(HP_SRAM_BASE + HP_SRAM_SIZE - \
					SOF_STACK_TOTAL_SIZE)
#define BOOT_LDR_STACK_SIZE		SOF_STACK_TOTAL_SIZE

#endif /* __PLATFORM_LIB_MEMORY_H__ */

/* L1 init */
#define L1_CACHE_PREFCTL_VALUE 0x0000

#else

#error "This file shouldn't be included from outside of sof/lib/memory.h"

#endif /* __SOF_LIB_MEMORY_H__ */
