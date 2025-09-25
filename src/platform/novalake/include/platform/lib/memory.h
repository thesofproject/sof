/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022-2024 Intel Corporation.
 *
 * Author: Marcin Rajwa <marcin.rajwa@linux.intel.com>
 */

#ifdef __SOF_LIB_MEMORY_H__

#ifndef __PLATFORM_LIB_MEMORY_H__
#define __PLATFORM_LIB_MEMORY_H__

/* prioritize definitions in Zephyr SoC layer */
#include <adsp_memory.h>

#include <ace/lib/memory.h>
#include <mem_window.h>
#include <sof/lib/cpu.h>

/* HP SRAM windows */
#define WIN_BASE(n)			DT_REG_ADDR(DT_PHANDLE(MEM_WINDOW_NODE(n), memory))

/* window 0 */
#define SRAM_SW_REG_BASE		((uint32_t)(WIN_BASE(0) + WIN0_OFFSET))
#define SRAM_SW_REG_SIZE		0x1000

#define SRAM_OUTBOX_BASE		(SRAM_SW_REG_BASE + SRAM_SW_REG_SIZE)
#define SRAM_OUTBOX_SIZE		0x1000

/* window 1 */
#define SRAM_INBOX_BASE			((uint32_t)(WIN_BASE(1) + WIN1_OFFSET))
#define SRAM_INBOX_SIZE			((uint32_t)WIN_SIZE(1))

/* window 2 */
#define SRAM_DEBUG_BASE			((uint32_t)(WIN_BASE(2) + WIN2_OFFSET))
#define SRAM_DEBUG_SIZE			0x800

#define SRAM_EXCEPT_BASE		(SRAM_DEBUG_BASE + SRAM_DEBUG_SIZE)
#define SRAM_EXCEPT_SIZE		0x800

#define SRAM_STREAM_BASE		(SRAM_EXCEPT_BASE + SRAM_EXCEPT_SIZE)
#define SRAM_STREAM_SIZE		0x1000

/* Stack configuration */
#define SOF_STACK_SIZE			(CONFIG_SOF_STACK_SIZE)

#define PLATFORM_HEAP_SYSTEM		CONFIG_CORE_COUNT /* one per core */
#define PLATFORM_HEAP_SYSTEM_RUNTIME	CONFIG_CORE_COUNT /* one per core */
#define PLATFORM_HEAP_RUNTIME		1
#define PLATFORM_HEAP_RUNTIME_SHARED	1
#define PLATFORM_HEAP_SYSTEM_SHARED	1
#define PLATFORM_HEAP_BUFFER		2

/**
 * size of HPSRAM system heap
 */
#define HEAPMEM_SIZE CONFIG_SOF_ZEPHYR_HEAP_SIZE

#if CONFIG_COLD_STORE_EXECUTE_DRAM && \
	(CONFIG_LLEXT_TYPE_ELF_RELOCATABLE || !defined(LL_EXTENSION_BUILD))
#define __cold __section(".cold")
#define __cold_rodata __section(".coldrodata")
#endif

#endif /* __PLATFORM_LIB_MEMORY_H__ */

#else

#error "This file shouldn't be included from outside of sof/lib/memory.h"

#endif /* __SOF_LIB_MEMORY_H__ */
