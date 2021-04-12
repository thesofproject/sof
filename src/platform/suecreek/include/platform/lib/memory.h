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

/* resource allocation */
#define RES_BASE		0x00001110
#define RES_SIZE		0x00000010

/* IPC to the host */
#define IPC_HOST_BASE		0x00071E00
#define IPC_HOST_SIZE		0x00000020

/* intra DSP  IPC */
#define IPC_DSP_SIZE		0x00000080
#define IPC_DSP_BASE(x)		(0x00001200 + x * IPC_DSP_SIZE)

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

/* Timestamping */
#define TIMESTAMP_BASE		0x00071800

/* low power DMACs */
#define LP_GP_DMA_SIZE		0x00001000
#define LP_GP_DMA_BASE(x)	(0x0007C000 + x * LP_GP_DMA_SIZE)

/* high performance DMACs */
#define HP_GP_DMA_SIZE		0x00001000
#define HP_GP_DMA_BASE(x)	(0x00004000 + x * HP_GP_DMA_SIZE)

/* DW SPI slave/master */
#define DW_SPI_SLAVE_BASE	0x80000
#define DW_SPI_SLAVE_SIZE	0x400
#define DW_SPI_MASTER_BASE	0xE000
#define DW_SPI_MASTER_SIZE	0x400

/* DW I2C controller */
#define DW_I2C_BASE		0x80400
#define DW_I2C_SIZE		0x400

/* DW UART controller */
#define DW_UART_BASE		0x88000
#define DW_UART_SIZE		0x400

/* DW GPIO controller */
#define DW_GPIO_BASE		0x80c00
#define DW_GPIO_SIZE		0x400

/* DW timer */
#define DW_TIMER_BASE		0x81000
#define DW_TIMER_SIZE		0x400

/* DW watchdog */
#define DW_WDT_BASE		0x81400
#define DW_WDT_SIZE		0x400

/* DW 3rd level IRQ controller */
#define DW_IRQ_BASE		0x81800
#define DW_IRQ_SIZE		0x400

/* Misc external controller */
#define EXT_CTRL_BASE		0x81c00
#define EXT_CTRL_SIZE		0x400

/* DW USB 2.0 controller */
#define DW_USB_BASE		0xA0000
#define DW_USB_SIZE		0x6000

/* Parallel memory controller */
#define DW_PMEMCTRL_BASE	0xE400
#define DW_PMEMCTRL_SIZE	0x400

/* GNA controller */
#define DW_GNA_BASE		0xE800
#define DW_GNA_SIZE		0x400

/* XIP SPI memory */
#define DW_SPIMEM_UNCACHE_BASE	0xC0000000
#define DW_SPIMEM_CACHE_BASE	0xE0000000
#define DW_SPIMEML_SIZE		0x8000000

/* Parallel memory */
#define DW_PARMEM_UNCACHE_BASE	0xD0000000
#define DW_PARMEM_CACHE_BASE	0xF0000000
#define DW_PARMEML_SIZE		0x4000000

/* ROM */
#define ROM_BASE		0xBEFE0000
#define ROM_SIZE		0x00002000

#define L2_VECTOR_SIZE		0x2000

#define UUID_ENTRY_ELF_BASE	0x1FFFA000
#define UUID_ENTRY_ELF_SIZE	0x6000

#define LOG_ENTRY_ELF_BASE	0x20000000
#define LOG_ENTRY_ELF_SIZE	0x2000000

#define EXT_MANIFEST_ELF_BASE	(LOG_ENTRY_ELF_BASE + LOG_ENTRY_ELF_SIZE)
#define EXT_MANIFEST_ELF_SIZE	0x2000000

/*
 * The HP SRAM Region on Sue Creek is organised like this :-
 * +--------------------------------------------------------------------------+
 * | Offset           | Region                  |  Size                       |
 * +------------------+-------------------------+-----------------------------+
 * | SOF_FW_START     | text                    |                             |
 * |                  | data                    |                             |
 * |                  | ----------------------- |                             |
 * |                  ||BSS:                   ||                             |
 * |                  ||-----------------------++-----------------------------+
 * |                  ||Runtime Heap           ||  HEAP_RUNTIME_SIZE          |
 * |                  ||-----------------------++-----------------------------+
 * |                  ||Runtime shared Heap    ||  HEAP_RUNTIME_SHARED_SIZE   |
 * |                  ||-----------------------++-----------------------------+
 * |                  ||System shared Heap     ||  HEAP_SYSTEM_SHARED_SIZE    |
 * |                  ||-----------------------++-----------------------------+
 * |                  ||Module Buffers         ||  HEAP_BUFFER_SIZE           |
 * |                  ||-----------------------++-----------------------------+
 * |                  ||Primary core Sys Heap  ||  HEAP_SYSTEM_M_SIZE         |
 * |                  ||-----------------------++-----------------------------+
 * |                  ||Pri. Sys Runtime Heap  ||  HEAP_SYS_RUNTIME_M_SIZE    |
 * |                  ||-----------------------++-----------------------------+
 * |                  ||Primary core Stack     ||  SOF_STACK_SIZE             |
 * |                  ||-----------------------++-----------------------------+
 * |                  ||Sec. core Sys Heap     ||  SOF_CORE_S_T_SIZE          |
 * |                  ||Sec. Sys Runtime Heap  ||                             |
 * |                  ||Secondary core Stack   ||                             |
 * |                  | ----------------------- |                             |
 * +------------------+-------------------------+-----------------------------+
 */

/* HP SRAM */
#define HP_SRAM_BASE		0xBE000000

/* Mailbox configuration */
#define SRAM_OUTBOX_BASE	HP_SRAM_BASE
#define SRAM_OUTBOX_SIZE	0x1000

#define SRAM_INBOX_BASE		(SRAM_OUTBOX_BASE + SRAM_OUTBOX_SIZE)
#define SRAM_INBOX_SIZE		0x1000

#define SRAM_DEBUG_BASE		(SRAM_INBOX_BASE + SRAM_INBOX_SIZE)
#define SRAM_DEBUG_SIZE		0x800

#define SRAM_EXCEPT_BASE	(SRAM_DEBUG_BASE + SRAM_DEBUG_SIZE)
#define SRAM_EXCEPT_SIZE	0x800

#define SRAM_STREAM_BASE	(SRAM_EXCEPT_BASE + SRAM_EXCEPT_SIZE)
#define SRAM_STREAM_SIZE	0x1000

#define SRAM_TRACE_BASE		(SRAM_STREAM_BASE + SRAM_STREAM_SIZE)
#if CONFIG_TRACE
#define SRAM_TRACE_SIZE		0x1000
#else
#define SRAM_TRACE_SIZE		0x0
#endif

/* Suecreek HPSRAM does not have SW_REG window*/
#define SRAM_SW_REG_BASE	0
#define SRAM_SW_REG_SIZE	0

#define SOF_MAILBOX_SIZE	(SRAM_INBOX_SIZE + SRAM_OUTBOX_SIZE \
				+ SRAM_DEBUG_SIZE + SRAM_EXCEPT_SIZE \
				+ SRAM_STREAM_SIZE + SRAM_TRACE_SIZE)

/* HP SRAM Base */
#define HP_SRAM_VECBASE_RESET	SRAM_VECBASE_RESET

#define SOF_TEXT_START_SIZE	0x400

/* text and data share the same HP L2 SRAM on Suecreek */
#define SOF_FW_START		(HP_SRAM_VECBASE_RESET + SOF_TEXT_START_SIZE)
#define SOF_FW_BASE		(SOF_FW_START)

#define SOF_TEXT_START		(SOF_FW_START)
#define SOF_TEXT_BASE		(SOF_FW_START)

/* max size for all var-size sections (text/rodata/bss) */
#define SOF_FW_MAX_SIZE		(HP_SRAM_BASE + HP_SRAM_SIZE - SOF_FW_BASE)

/* Heap section sizes for system runtime heap for  core */
#define HEAP_SYS_RT_0_COUNT64		64
#define HEAP_SYS_RT_0_COUNT512		16
#define HEAP_SYS_RT_0_COUNT1024		4

/* Heap section sizes for system runtime heap for secondary core */
#define HEAP_SYS_RT_X_COUNT64		32
#define HEAP_SYS_RT_X_COUNT512		8
#define HEAP_SYS_RT_X_COUNT1024		4

/* Heap section sizes for module pool */
#define HEAP_RT_COUNT64			192
#define HEAP_RT_COUNT128		32
#define HEAP_RT_COUNT256		80
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

#define HEAP_BUFFER_SIZE	0x10000
#define HEAP_BUFFER_BLOCK_SIZE	0x100
#define HEAP_BUFFER_COUNT	(HEAP_BUFFER_SIZE / HEAP_BUFFER_BLOCK_SIZE)

#define HEAP_SYSTEM_M_SIZE		0x8000	/* heap  core size */
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
 * The LP SRAM Heap and Stack on Suecreek are organised like this :-
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

/* Heap section sizes for module pool */
#define HEAP_RT_LP_COUNT8			0
#define HEAP_RT_LP_COUNT16			256
#define HEAP_RT_LP_COUNT32			128
#define HEAP_RT_LP_COUNT64			64
#define HEAP_RT_LP_COUNT128			32
#define HEAP_RT_LP_COUNT256			16
#define HEAP_RT_LP_COUNT512			8
#define HEAP_RT_LP_COUNT1024			4

/* Heap configuration */
#define SOF_LP_DATA_SIZE			0x4000

#define HEAP_LP_SYSTEM_BASE		(LP_SRAM_BASE + SOF_LP_DATA_SIZE)
#define HEAP_LP_SYSTEM_SIZE		0x1000

#define HEAP_LP_RUNTIME_BASE \
	(HEAP_LP_SYSTEM_BASE + HEAP_LP_SYSTEM_SIZE)
#define HEAP_LP_RUNTIME_SIZE \
	(HEAP_RT_LP_COUNT8 * 8 + HEAP_RT_LP_COUNT16 * 16 + \
	HEAP_RT_LP_COUNT32 * 32 + HEAP_RT_LP_COUNT64 * 64 + \
	HEAP_RT_LP_COUNT128 * 128 + HEAP_RT_LP_COUNT256 * 256 + \
	HEAP_RT_LP_COUNT512 * 512 + HEAP_RT_LP_COUNT1024 * 1024)

#define HEAP_LP_BUFFER_BASE	(HEAP_LP_RUNTIME_BASE + HEAP_LP_RUNTIME_SIZE)
#define HEAP_LP_BUFFER_SIZE \
	(LP_SRAM_SIZE - HEAP_LP_RUNTIME_SIZE - SOF_LP_STACK_SIZE - \
	HEAP_LP_SYSTEM_SIZE)

#define HEAP_LP_BUFFER_BLOCK_SIZE		0x180

#if CONFIG_LP_MEMORY_BANKS
#define HEAP_LP_BUFFER_COUNT \
	(HEAP_LP_BUFFER_SIZE / HEAP_LP_BUFFER_BLOCK_SIZE)
#else
#define HEAP_LP_BUFFER_COUNT 0
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

/* boot loader */
#define BOOT_LDR_MANIFEST_BASE		(SRAM_TRACE_BASE + SRAM_TRACE_SIZE)
#define BOOT_LDR_MANIFEST_SIZE		0x6000

/* code loader */
#define BOOT_LDR_TEXT_ENTRY_BASE	(BOOT_LDR_MANIFEST_BASE + \
					BOOT_LDR_MANIFEST_SIZE)
#define BOOT_LDR_TEXT_ENTRY_SIZE	0x200
#define BOOT_LDR_LIT_BASE		(BOOT_LDR_TEXT_ENTRY_BASE + \
					BOOT_LDR_TEXT_ENTRY_SIZE)
#define BOOT_LDR_LIT_SIZE		0x200
#define BOOT_LDR_TEXT_BASE		(BOOT_LDR_LIT_BASE + BOOT_LDR_LIT_SIZE)
#define BOOT_LDR_TEXT_SIZE		0xC00
#define BOOT_LDR_DATA_BASE		(BOOT_LDR_TEXT_BASE + \
					BOOT_LDR_TEXT_SIZE)
#define BOOT_LDR_DATA_SIZE		0x1000
#define BOOT_LDR_BSS_BASE		(BOOT_LDR_DATA_BASE + \
					BOOT_LDR_DATA_SIZE)
#define BOOT_LDR_BSS_SIZE		0x100

/* Temporary stack place for boot_ldr */
#define BOOT_LDR_STACK_BASE		(HP_SRAM_BASE + HP_SRAM_SIZE - \
					SOF_STACK_TOTAL_SIZE)
#define BOOT_LDR_STACK_SIZE		SOF_STACK_TOTAL_SIZE

/* code loader entry point for base fw */
#define _SRAM_VECBASE_RESET (BOOT_LDR_BSS_BASE + BOOT_LDR_BSS_SIZE)
#define SRAM_VECBASE_RESET ALIGN_UP_INTERNAL(_SRAM_VECBASE_RESET, 4096)

#endif /* __PLATFORM_LIB_MEMORY_H__ */

#else

#error "This file shouldn't be included from outside of sof/lib/memory.h"

#endif /* __SOF_LIB_MEMORY_H__ */
