/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifdef __SOF_LIB_MEMORY_H__

#ifndef __PLATFORM_LIB_MEMORY_H__
#define __PLATFORM_LIB_MEMORY_H__

#if !defined(__ASSEMBLER__) && !defined(LINKER)

struct sof;

/**
 * \brief Data shared between different cores.
 * Does nothing, since BYT doesn't support SMP.
 */
#define SHARED_DATA

void platform_init_memmap(struct sof *sof);

static inline void *platform_shared_get(void *ptr, int bytes)
{
	return ptr;
}

/**
 * \brief Function for keeping shared data synchronized.
 * It's used after usage of data shared by different cores.
 * Such data is either statically marked with SHARED_DATA
 * or dynamically allocated with SOF_MEM_FLAG_SHARED flag.
 * Does nothing, since BYT doesn't support SMP.
 */

static inline void *platform_rfree_prepare(void *ptr)
{
	return ptr;
}

#endif

#define PLATFORM_DCACHE_ALIGN	sizeof(void *)

/** \brief EDF task's default stack size in bytes. */
#define PLATFORM_TASK_DEFAULT_STACK_SIZE	2048

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

#define UUID_ENTRY_ELF_BASE	0x1FFFA000
#define UUID_ENTRY_ELF_SIZE	0x6000

#define LOG_ENTRY_ELF_BASE	0x20000000
#define LOG_ENTRY_ELF_SIZE	0x2000000

#define EXT_MANIFEST_ELF_BASE	(LOG_ENTRY_ELF_BASE + LOG_ENTRY_ELF_SIZE)
#define EXT_MANIFEST_ELF_SIZE	0x2000000

/*
 * The Heap and Stack on Baytrail are organised like this :-
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
 * | SOF_STACK_END      | Stack          |  SOF_STACK_SIZE                  |
 * +---------------------+----------------+-----------------------------------+
 * | SOF_STACK_BASE     |                |                                   |
 * +---------------------+----------------+-----------------------------------+
 */

/* Heap section sizes for module pool */
#define HEAP_RT_COUNT8		0
#define HEAP_RT_COUNT16		32
#define HEAP_RT_COUNT32		32
#define HEAP_RT_COUNT64		32
#define HEAP_RT_COUNT128	32
#define HEAP_RT_COUNT256	32
#define HEAP_RT_COUNT512	2
#define HEAP_RT_COUNT1024	1

/* Heap section sizes for system runtime heap */
#define HEAP_SYS_RT_COUNT64	64
#define HEAP_SYS_RT_COUNT512	8
#define HEAP_SYS_RT_COUNT1024	4

/* Heap configuration */
#define SOF_DATA_SIZE			0x9800

#define HEAP_SYSTEM_BASE		(DRAM0_BASE + SOF_DATA_SIZE)
#define HEAP_SYSTEM_SIZE		0xA800

#define HEAP_SYSTEM_0_BASE		HEAP_SYSTEM_BASE

#define HEAP_SYS_RUNTIME_BASE	(HEAP_SYSTEM_BASE + HEAP_SYSTEM_SIZE)
#define HEAP_SYS_RUNTIME_SIZE \
	(HEAP_SYS_RT_COUNT64 * 64 + HEAP_SYS_RT_COUNT512 * 512 + \
	HEAP_SYS_RT_COUNT1024 * 1024)

#define HEAP_RUNTIME_BASE	(HEAP_SYS_RUNTIME_BASE + HEAP_SYS_RUNTIME_SIZE)
#define HEAP_RUNTIME_SIZE \
	(HEAP_RT_COUNT8 * 8 + HEAP_RT_COUNT16 * 16 + \
	HEAP_RT_COUNT32 * 32 + HEAP_RT_COUNT64 * 64 + \
	HEAP_RT_COUNT128 * 128 + HEAP_RT_COUNT256 * 256 + \
	HEAP_RT_COUNT512 * 512 + HEAP_RT_COUNT1024 * 1024)

#define HEAP_BUFFER_BASE	(HEAP_RUNTIME_BASE + HEAP_RUNTIME_SIZE)
#define HEAP_BUFFER_SIZE \
	(DRAM0_SIZE - HEAP_RUNTIME_SIZE - SOF_STACK_TOTAL_SIZE -\
	HEAP_SYS_RUNTIME_SIZE - HEAP_SYSTEM_SIZE - SOF_DATA_SIZE)

#define HEAP_BUFFER_BLOCK_SIZE		0x100
#define HEAP_BUFFER_COUNT	(HEAP_BUFFER_SIZE / HEAP_BUFFER_BLOCK_SIZE)

#define PLATFORM_HEAP_SYSTEM		1 /* one per core */
#define PLATFORM_HEAP_SYSTEM_RUNTIME	1 /* one per core */
#define PLATFORM_HEAP_RUNTIME		1
#define PLATFORM_HEAP_BUFFER		1

/* Stack configuration */
#define SOF_STACK_SIZE		0x1000
#define SOF_STACK_TOTAL_SIZE	SOF_STACK_SIZE
#define SOF_STACK_BASE		(DRAM0_BASE + DRAM0_SIZE)
#define SOF_STACK_END		(SOF_STACK_BASE - SOF_STACK_TOTAL_SIZE)

/* Vector and literal addresses and sizes - do not use core-isa.h */
#define SOF_MEM_RESET_VECT		0xff2c0000
#define SOF_MEM_VECBASE			0xff2c0400
#define SOF_MEM_INTLEVEL2_VECT		0xff2c057c
#define SOF_MEM_INTLEVEL3_VECT		0xff2c059c
#define SOF_MEM_INTLEVEL4_VECT		0xff2c05bc
#define SOF_MEM_INTLEVEL5_VECT		0xff2c05dc
#define SOF_MEM_INTLEVEL6_VECT		0xff2c05fc
#define SOF_MEM_INTLEVEL7_VECT		0xff2c061c
#define SOF_MEM_KERNEL_VECT		0xff2c063c
#define SOF_MEM_USER_VECT		0xff2c065c
#define SOF_MEM_DOUBLEEXC_VECT		0xff2c067c

#define SOF_MEM_VECT_LIT_SIZE		0x4
#define SOF_MEM_VECT_TEXT_SIZE		0x1c
#define SOF_MEM_VECT_SIZE		(SOF_MEM_VECT_TEXT_SIZE + \
					SOF_MEM_VECT_LIT_SIZE)

#define SOF_MEM_RESET_TEXT_SIZE	0x2e0
#define SOF_MEM_RESET_LIT_SIZE		0x120
#define SOF_MEM_VECBASE_LIT_SIZE	0x178

#define SOF_MEM_RO_SIZE			0x8

#endif /* __PLATFORM_LIB_MEMORY_H__ */

#else

#error "This file shouldn't be included from outside of sof/lib/memory.h"

#endif /* __SOF_LIB_MEMORY_H__ */
