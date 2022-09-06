/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 *
 * Author: Marcin Rajwa <marcin.rajwa@linux.intel.com>
 */

#ifdef __SOF_LIB_MEMORY_H__

#ifndef __PLATFORM_LIB_MEMORY_H__
#define __PLATFORM_LIB_MEMORY_H__

#include <ace/lib/memory.h>
#include <adsp_memory.h>
#include <sof/lib/cpu.h>

/* HP SRAM windows */
/* window 0 */
#define SRAM_SW_REG_BASE		((uint32_t)HP_SRAM_WIN0_BASE)
#define SRAM_SW_REG_SIZE		((uint32_t)HP_SRAM_WIN0_SIZE)

#define SRAM_OUTBOX_BASE		(SRAM_SW_REG_BASE + SRAM_SW_REG_SIZE)
#define SRAM_OUTBOX_SIZE		0x1000

/* window 1 */
#define SRAM_INBOX_BASE			(SRAM_OUTBOX_BASE + SRAM_OUTBOX_SIZE)
#define SRAM_INBOX_SIZE			0x2000

/* window 2 */
#define SRAM_DEBUG_BASE			((uint32_t)HP_SRAM_WIN2_BASE)
#define SRAM_DEBUG_SIZE			((uint32_t)HP_SRAM_WIN2_SIZE)

#define SRAM_EXCEPT_BASE		(SRAM_DEBUG_BASE + SRAM_DEBUG_SIZE)
#define SRAM_EXCEPT_SIZE		0x800

#define SRAM_STREAM_BASE		(SRAM_EXCEPT_BASE + SRAM_EXCEPT_SIZE)
#define SRAM_STREAM_SIZE		0x1000

/* Stack configuration */
#define SOF_STACK_SIZE			0x1000

#define PLATFORM_HEAP_SYSTEM		CONFIG_CORE_COUNT /* one per core */
#define PLATFORM_HEAP_SYSTEM_RUNTIME	CONFIG_CORE_COUNT /* one per core */
#define PLATFORM_HEAP_RUNTIME		1
#define PLATFORM_HEAP_RUNTIME_SHARED	1
#define PLATFORM_HEAP_SYSTEM_SHARED	1
#define PLATFORM_HEAP_BUFFER		2

/* boot loader in IMR */
#define IMR_BOOT_LDR_MANIFEST_BASE	0xA1042000
#define IMR_BOOT_LDR_MANIFEST_SIZE	0x6000

#endif /* __PLATFORM_LIB_MEMORY_H__ */

#else

#error "This file shouldn't be included from outside of sof/lib/memory.h"

#endif /* __SOF_LIB_MEMORY_H__ */
