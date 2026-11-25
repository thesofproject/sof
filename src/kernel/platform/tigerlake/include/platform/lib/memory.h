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

/* prioritize definitions in Zephyr SoC layer */
#ifdef __ZEPHYR__
#include <adsp_memory.h>
#endif

/* physical DSP addresses */

/* IPC to the host */
#define IPC_HOST_BASE		0x00071E00
#define IPC_HOST_SIZE		0x00000020

/*
 * The HP SRAM Region on Tigerlake is organised like this :-
 * +----------------------------------------------------------------------------+
 * | Offset             | Region                  |  Size                       |
 * +--------------------+-------------------------+-----------------------------+
 * | SRAM_SW_REG_BASE   | SW Registers W0         |  SRAM_SW_REG_SIZE           |
 * +--------------------+-------------------------+-----------------------------+
 * | SRAM_OUTBOX_BASE   | Outbox W0               |  SRAM_OUTBOX_SIZE           |
 * +--------------------+-------------------------+-----------------------------+
 * | SRAM_INBOX_BASE    | Inbox  W1               |  SRAM_INBOX_SIZE            |
 * +--------------------+-------------------------+-----------------------------+
 * | SRAM_DEBUG_BASE    | Debug data  W2          |  SRAM_DEBUG_SIZE            |
 * +--------------------+-------------------------+-----------------------------+
 * | SRAM_EXCEPT_BASE   | Debug data  W2          |  SRAM_EXCEPT_SIZE           |
 * +--------------------+-------------------------+-----------------------------+
 * | SRAM_STREAM_BASE   | Stream data W2          |  SRAM_STREAM_SIZE           |
 * +--------------------+-------------------------+-----------------------------+
 * | SRAM_TRACE_BASE    | Trace Buffer W3         |  SRAM_TRACE_SIZE            |
 * +--------------------+-------------------------+-----------------------------+
 * | HEAP_HP_BUFFER_BASE| DMA                     |  HEAP_HP_BUFFER_SIZE        |
 * +--------------------+-------------------------+-----------------------------+
 * | SOF_FW_START       | text                    |                             |
 * |                    | data                    |                             |
 * |                    | BSS                     |                             |
 * +--------------------+-------------------------+-----------------------------+
 * |                    | Runtime Heap            |  HEAP_RUNTIME_SIZE          |
 * +--------------------+-------------------------+-----------------------------+
 * |                    | Runtime shared Heap     |  HEAP_RUNTIME_SHARED_SIZE   |
 * |                    |-------------------------+-----------------------------+
 * |                    | System shared Heap      |  HEAP_SYSTEM_SHARED_SIZE    |
 * |                    |-------------------------+-----------------------------+
 * |                    | Module Buffers          |  HEAP_BUFFER_SIZE           |
 * +--------------------+-------------------------+-----------------------------+
 * |                    | Primary core Sys Heap   |  HEAP_SYSTEM_M_SIZE         |
 * +--------------------+-------------------------+-----------------------------+
 * |                    | Pri. Sys Runtime Heap   |  HEAP_SYS_RUNTIME_M_SIZE    |
 * +--------------------+-------------------------+-----------------------------+
 * |                    | Primary core Stack      |  SOF_STACK_SIZE             |
 * +--------------------+-------------------------+-----------------------------+
 * |                    | Sec. core Sys Heap      |  SOF_CORE_S_T_SIZE          |
 * |                    | Sec. Sys Runtime Heap   |                             |
 * |                    | Secondary core Stack    |                             |
 * +--------------------+-------------------------+-----------------------------+
 */

#include <platform/lib/mailbox_layout.h>

#define HEAP_HP_BUFFER_BASE		(SRAM_TRACE_BASE + SRAM_TRACE_SIZE)
#define HEAP_HP_BUFFER_SIZE		0x20000

/* HP SRAM Base */
#define HP_SRAM_VECBASE_RESET	(HEAP_HP_BUFFER_BASE + HEAP_HP_BUFFER_SIZE)

/* text and data share the same HP L2 SRAM on Tigerlake */
#define SOF_FW_START		(HP_SRAM_VECBASE_RESET + 0x400)

#define SOF_FW_END             (HP_SRAM_BASE + HP_SRAM_SIZE)

#define SOF_TEXT_START		(SOF_FW_START)

/* Heap section counts base */
#define HEAP_COUNT64		128
#define HEAP_COUNT128		128
#define HEAP_COUNT256		96
#define HEAP_COUNT512		8
#define HEAP_COUNT1024		4
#define HEAP_COUNT2048		2
#define HEAP_COUNT4096		1

#if HP_SRAM_SIZE < 0x200000
#define RT_TIMES	3
#else
/* Reserve more memory for RTNR */
#if defined(CONFIG_COMP_RTNR) || defined(CONFIG_COMP_GOOGLE_RTC_AUDIO_PROCESSING)
#define RT_TIMES	2
#else
#define RT_TIMES	8
#endif /* CONFIG_COMP_RTNR */
#endif

/* Heap section sizes for module pool */
#define HEAP_RT_COUNT64			(HEAP_COUNT64 * RT_TIMES)
#define HEAP_RT_COUNT128		(HEAP_COUNT128 * RT_TIMES)
#define HEAP_RT_COUNT256		(HEAP_COUNT256 * RT_TIMES)
#define HEAP_RT_COUNT512		(HEAP_COUNT512 * RT_TIMES)
#define HEAP_RT_COUNT1024		(HEAP_COUNT1024 * RT_TIMES)
#define HEAP_RT_COUNT2048		(HEAP_COUNT2048 * RT_TIMES)
#define HEAP_RT_COUNT4096		(HEAP_COUNT4096 * RT_TIMES)

/* Heap configuration */
#define HEAP_RUNTIME_SIZE \
	(HEAP_RT_COUNT64 * 64 + HEAP_RT_COUNT128 * 128 + \
	HEAP_RT_COUNT256 * 256 + HEAP_RT_COUNT512 * 512 + \
	HEAP_RT_COUNT1024 * 1024 + HEAP_RT_COUNT2048 * 2048 + \
	HEAP_RT_COUNT4096 * 4096)

/* Stack configuration */
#define SOF_STACK_SIZE		(CONFIG_SOF_STACK_SIZE)

/*
 * The LP SRAM Heap and Stack on Tigerlake are organised like this :-
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

/* boot loader in IMR */

#ifndef __ZEPHYR__
#define IMR_BOOT_LDR_MANIFEST_BASE	0xB0032000
#define IMR_BOOT_LDR_MANIFEST_SIZE	0x6000

#ifndef IMR_BOOT_LDR_TEXT_ENTRY_BASE
#define IMR_BOOT_LDR_TEXT_ENTRY_BASE	0xB0038000
#endif
#endif

#define host_to_local(addr) (addr)
#define local_to_host(addr) (addr)

#endif /* __PLATFORM_LIB_MEMORY_H__ */

#else

#error "This file shouldn't be included from outside of sof/lib/memory.h"

#endif /* __SOF_LIB_MEMORY_H__ */
