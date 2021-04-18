/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifdef __SOF_LIB_MEMORY_H__

#ifndef __PLATFORM_LIB_MEMORY_H__
#define __PLATFORM_LIB_MEMORY_H__

#include <inttypes.h>
#include <stddef.h>

struct sof;

#define PLATFORM_DCACHE_ALIGN	sizeof(void *)

#define HEAP_BUFFER_SIZE	(1024 * 128)
#define SOF_STACK_SIZE		0x1000

uint8_t *get_library_mailbox(void);

#define MAILBOX_BASE	get_library_mailbox()

#define PLATFORM_HEAP_SYSTEM		2
#define PLATFORM_HEAP_SYSTEM_RUNTIME	1
#define PLATFORM_HEAP_RUNTIME		1
#define PLATFORM_HEAP_BUFFER		3
#define PLATFORM_HEAP_SYSTEM_SHARED	1
#define PLATFORM_HEAP_RUNTIME_SHARED	1

#define SHARED_DATA

static inline void *platform_shared_get(void *ptr, int bytes)
{
	return ptr;
}

void platform_init_memmap(struct sof *sof);

static inline void *platform_rfree_prepare(void *ptr)
{
	return ptr;
}

#define uncache_to_cache(address)	address
#define cache_to_uncache(address)	address
#define is_uncached(address)		1

#define ARCH_OOPS_SIZE	0

static inline void *arch_get_stack_entry(void)
{
	return NULL;
}

static inline uint32_t arch_get_stack_size(void)
{
	return 0;
}

/* NOTE - FAKE Memory configurations are used by UT's to test allocator */

#define SRAM_BANK_SIZE	0x10000
#define LP_SRAM_SIZE SRAM_BANK_SIZE
#define HP_SRAM_SIZE SRAM_BANK_SIZE

#define HP_SRAM_BASE	0
#define LP_SRAM_BASE	0

/* Heap section sizes for system runtime heap for primary core */
#define HEAP_SYS_RT_0_COUNT64		128
#define HEAP_SYS_RT_0_COUNT512		16
#define HEAP_SYS_RT_0_COUNT1024		4

/* Heap section sizes for system runtime heap for secondary core */
#define HEAP_SYS_RT_X_COUNT64		64
#define HEAP_SYS_RT_X_COUNT512		8
#define HEAP_SYS_RT_X_COUNT1024		4

/* Heap section sizes for module pool */
#define HEAP_RT_COUNT64		128
#define HEAP_RT_COUNT128	64
#define HEAP_RT_COUNT256	128
#define HEAP_RT_COUNT512	8
#define HEAP_RT_COUNT1024	4
#define HEAP_RT_COUNT2048	1
#define HEAP_RT_COUNT4096	1

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

#define HEAP_BUFFER_BLOCK_SIZE		0x100
#define HEAP_BUFFER_COUNT	(HEAP_BUFFER_SIZE / HEAP_BUFFER_BLOCK_SIZE)

#define HEAP_SYSTEM_M_SIZE		0x8000 /* heap primary core size */
#define HEAP_SYSTEM_S_SIZE		0x6000 /* heap secondary core size */

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

/* Heap section sizes for module pool */
#define HEAP_RT_LP_COUNT8			0
#define HEAP_RT_LP_COUNT16			256
#define HEAP_RT_LP_COUNT32			128
#define HEAP_RT_LP_COUNT64			64
#define HEAP_RT_LP_COUNT128			64
#define HEAP_RT_LP_COUNT256			96
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

#define HEAP_LP_BUFFER_BLOCK_SIZE		0x180
#define HEAP_LP_BUFFER_COUNT \
	(HEAP_LP_BUFFER_SIZE / HEAP_LP_BUFFER_BLOCK_SIZE)

#define HEAP_LP_BUFFER_BASE LP_SRAM_BASE
#define HEAP_LP_BUFFER_SIZE LP_SRAM_SIZE

/* SOF Core S configuration */
#define SOF_CORE_S_SIZE \
	ALIGN((HEAP_SYSTEM_S_SIZE + HEAP_SYS_RUNTIME_S_SIZE + SOF_STACK_SIZE),\
	SRAM_BANK_SIZE)
#define SOF_CORE_S_T_SIZE ((CONFIG_CORE_COUNT - 1) * SOF_CORE_S_SIZE)

#endif /* __PLATFORM_LIB_MEMORY_H__ */

#else

#error "This file shouldn't be included from outside of sof/lib/memory.h"

#endif /* __SOF_LIB_MEMORY_H__ */
