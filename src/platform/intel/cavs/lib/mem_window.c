// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Marcin Maka <marcin.maka@linux.intel.com>

/**
 * \file
 * \brief Memory windows programming and initialization
 * \author Marcin Maka <marcin.maka@linux.intel.com>
 */

#include <cavs/mem_window.h>
#include <sof/lib/memory.h>
#include <rtos/alloc.h>
#include <sof/lib/io.h>
#include <sof/lib/shim.h>

static inline void memory_window_init(uint32_t index,
				      uint32_t base, uint32_t size,
				      uint32_t zero_base, uint32_t zero_size,
				      uint32_t wnd_flags, uint32_t init_flags)
{
	io_reg_write(DMWLO(index), size | 0x7);
	io_reg_write(DMWBA(index), base | wnd_flags);
	if (init_flags & MEM_WND_INIT_CLEAR) {
		bzero((void *)zero_base, zero_size);
		dcache_writeback_region((__sparse_force void __sparse_cache *)zero_base, zero_size);
	}
}

void platform_memory_windows_init(uint32_t flags)
{
	/* window0, for fw status & outbox/uplink mbox */
	memory_window_init(0, HP_SRAM_WIN0_BASE, HP_SRAM_WIN0_SIZE,
			   HP_SRAM_WIN0_BASE + SRAM_REG_FW_END,
			   HP_SRAM_WIN0_SIZE - SRAM_REG_FW_END,
			   DMWBA_READONLY | DMWBA_ENABLE, flags);

	/* window1, for inbox/downlink mbox */
	memory_window_init(1, HP_SRAM_WIN1_BASE, HP_SRAM_WIN1_SIZE,
			   HP_SRAM_WIN1_BASE, HP_SRAM_WIN1_SIZE,
			   DMWBA_ENABLE, flags);

	/* window2, for debug */
	memory_window_init(2, HP_SRAM_WIN2_BASE, HP_SRAM_WIN2_SIZE,
			   HP_SRAM_WIN2_BASE, HP_SRAM_WIN2_SIZE,
			   DMWBA_ENABLE, flags);

	/* window3, for trace
	 * zeroed by trace initialization
	 */
	memory_window_init(3, HP_SRAM_WIN3_BASE, HP_SRAM_WIN3_SIZE,
			   HP_SRAM_WIN3_BASE, HP_SRAM_WIN3_SIZE,
			   DMWBA_READONLY | DMWBA_ENABLE, 0);
}
