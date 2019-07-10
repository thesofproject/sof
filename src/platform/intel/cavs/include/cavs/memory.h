/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 *
 * Author: Bartosz Kokoszko <bartoszx.kokoszko@linux.intel.com>
 */

#ifndef __CAVS_MEMORY_H__
#define __CAVS_MEMORY_H__

#include <config.h>
#include <cavs/cpu.h>

#define SRAM_BANK_SIZE			(64 * 1024)

#define EBB_BANKS_IN_SEGMENT		32

#define EBB_SEGMENT_SIZE		EBB_BANKS_IN_SEGMENT

#define PLATFORM_LPSRAM_EBB_COUNT	CONFIG_LP_MEMORY_BANKS

#define PLATFORM_HPSRAM_EBB_COUNT	CONFIG_HP_MEMORY_BANKS

#define MAX_MEMORY_SEGMENTS		PLATFORM_HPSRAM_SEGMENTS

#define LP_SRAM_SIZE \
	(CONFIG_LP_MEMORY_BANKS * SRAM_BANK_SIZE)

#define HP_SRAM_SIZE \
	(CONFIG_HP_MEMORY_BANKS * SRAM_BANK_SIZE)

#define PLATFORM_HPSRAM_SEGMENTS	((PLATFORM_HPSRAM_EBB_COUNT \
	+ EBB_BANKS_IN_SEGMENT - 1) / EBB_BANKS_IN_SEGMENT)

#define LPSRAM_MASK(ignored)	((1 << PLATFORM_LPSRAM_EBB_COUNT) - 1)

#define HPSRAM_MASK(seg_idx)	((1 << (PLATFORM_HPSRAM_EBB_COUNT \
	- EBB_BANKS_IN_SEGMENT * seg_idx)) - 1)

#define LPSRAM_SIZE (PLATFORM_LPSRAM_EBB_COUNT * SRAM_BANK_SIZE)

#define HEAP_BUF_ALIGNMENT		XCHAL_DCACHE_LINESIZE

#if !defined(__ASSEMBLER__) && !defined(LINKER)
void platform_init_memmap(void);
#endif

#endif /* __CAVS_MEMORY_H__ */
