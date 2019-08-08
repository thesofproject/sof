/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 *
 * Author: Bartosz Kokoszko <bartoszx.kokoszko@linux.intel.com>
 */

#ifdef __PLATFORM_LIB_MEMORY_H__

#ifndef __CAVS_LIB_MEMORY_H__
#define __CAVS_LIB_MEMORY_H__

#include <sof/lib/cache.h>
#include <config.h>

/* data cache line alignment */
#define PLATFORM_DCACHE_ALIGN	DCACHE_LINE_SIZE

#define SRAM_BANK_SIZE			(64 * 1024)

#define EBB_BANKS_IN_SEGMENT		32

#define EBB_SEGMENT_SIZE		EBB_BANKS_IN_SEGMENT

#if CONFIG_LP_MEMORY_BANKS
#define PLATFORM_LPSRAM_EBB_COUNT	CONFIG_LP_MEMORY_BANKS
#else
#define PLATFORM_LPSRAM_EBB_COUNT	0
#endif

#define PLATFORM_HPSRAM_EBB_COUNT	CONFIG_HP_MEMORY_BANKS

#define MAX_MEMORY_SEGMENTS		PLATFORM_HPSRAM_SEGMENTS

#if CONFIG_LP_MEMORY_BANKS
#define LP_SRAM_SIZE \
	(CONFIG_LP_MEMORY_BANKS * SRAM_BANK_SIZE)
#else
#define LP_SRAM_SIZE 0
#endif

#define HP_SRAM_SIZE \
	(CONFIG_HP_MEMORY_BANKS * SRAM_BANK_SIZE)

#define PLATFORM_HPSRAM_SEGMENTS	((PLATFORM_HPSRAM_EBB_COUNT \
	+ EBB_BANKS_IN_SEGMENT - 1) / EBB_BANKS_IN_SEGMENT)

#if defined(__ASSEMBLER__)
#define LPSRAM_MASK(ignored)	((1 << PLATFORM_LPSRAM_EBB_COUNT) - 1)

#define HPSRAM_MASK(seg_idx)	((1 << (PLATFORM_HPSRAM_EBB_COUNT \
	- EBB_BANKS_IN_SEGMENT * seg_idx)) - 1)
#else
#define LPSRAM_MASK(ignored)	((1ULL << PLATFORM_LPSRAM_EBB_COUNT) - 1)

#define HPSRAM_MASK(seg_idx)	((1ULL << (PLATFORM_HPSRAM_EBB_COUNT \
	- EBB_BANKS_IN_SEGMENT * seg_idx)) - 1)
#endif

#define LPSRAM_SIZE (PLATFORM_LPSRAM_EBB_COUNT * SRAM_BANK_SIZE)

#define HEAP_BUF_ALIGNMENT		PLATFORM_DCACHE_ALIGN

#if !defined(__ASSEMBLER__) && !defined(LINKER)
void platform_init_memmap(void);
#endif

#endif /* __CAVS_LIB_MEMORY_H__ */

#else

#error "This file shouldn't be included from outside of platform/lib/memory.h"

#endif /* __PLATFORM_LIB_MEMORY_H__ */
