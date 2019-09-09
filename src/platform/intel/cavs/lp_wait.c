// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Marcin Maka <marcin.maka@linux.intel.com>

#include <xtensa/tie/xt_interrupt.h>
#include <arch/lib/wait.h>
#include <cavs/lp_ctx.h>
#include <cavs/lp_wait.h>
#include <cavs/mem_wnd.h>
#include <sof/common.h>
#include <sof/platform.h>
#include <sof/drivers/interrupt.h>
#include <sof/lib/pm_runtime.h>
#include <sof/lib/shim.h>
#include <sof/schedule/task.h>
#include <config.h>
#include <stdint.h>

#define LPSRAM_MAGIC_VALUE 0x13579BDF
struct lpsram_header {
	uint32_t alt_reset_vector;
	uint32_t adsp_lpsram_magic;
	void *lp_restore_vector;
	uint32_t reserved;

	uint8_t rom_bypass_vectors_reserved[0xc00 - 16];
};

#define LPSRAM_HEADER_BYPASS_ADDR (LP_SRAM_BASE - SRAM_ALIAS_OFFSET)

#define LPS_POWER_FLOW_D0_D0I3		1
#define LPS_POWER_FLOW_D0I3_D0		0

__aligned(PLATFORM_DCACHE_ALIGN) uint8_t lps_boot_stack[0x1000];
__aligned(PLATFORM_DCACHE_ALIGN) lp_ctx lp_restore;
static void *pg_task_ctx;
static uint8_t pg_task_stack[0x1000];

static void platform_pg_int_handler(void *arg);

static void platform_pg_task(void)
{
	struct lpsram_header *lpsram_hdr = (struct lpsram_header *)
			LPSRAM_HEADER_BYPASS_ADDR;
	size_t vector_size;
	size_t offset_to_entry;
	int schedule_irq;

	_xtos_set_intlevel(5);
	xthal_window_spill();

	offset_to_entry = (uint32_t)&lps_pic_restore_vector
			- (uint32_t)&lps_pic_restore_vector_literals;
	vector_size = ALIGN_UP((size_t)&lps_pic_restore_vector_end
			       - (size_t)&lps_pic_restore_vector_literals, 4);

	/* Half of area is available,
	 * another half is reserved for custom vectors in future (like WHM)
	 */
	memcpy_s((void *)LPS_RESTORE_VECTOR_ADDR, LPS_RESTORE_VECTOR_SIZE,
		 &lps_pic_restore_vector_literals, vector_size);
	xthal_dcache_region_writeback_inv((void *)LPS_RESTORE_VECTOR_ADDR,
					  vector_size);

	/* set magic and vector in LPSRAM */
	lpsram_hdr->adsp_lpsram_magic = LPSRAM_MAGIC_VALUE;
	lpsram_hdr->lp_restore_vector = (void *)(LPS_RESTORE_VECTOR_ADDR
			+ offset_to_entry);

	/* re-register to change the direction (arg) */
	schedule_irq = interrupt_get_irq(IRQ_NUM_SOFTWARE3, NULL);
	interrupt_register(schedule_irq,
			   platform_pg_int_handler,
			   (void *)LPS_POWER_FLOW_D0I3_D0);


	/* enable all INTs that should turn the dsp on */
	arch_interrupt_enable_mask(BIT(PLATFORM_SCHEDULE_IRQ) |
				   BIT(IRQ_NUM_EXT_LEVEL2) |
				   BIT(IRQ_NUM_EXT_LEVEL5));

	while (1) {
		/* flush caches and handle int or pwr off */
		xthal_dcache_all_writeback_inv();
		arch_wait_for_interrupt(0);
	}
}

static void platform_pg_int_handler(void *arg)
{
	uint32_t dir = (uint32_t)arg;

	if (dir == LPS_POWER_FLOW_D0_D0I3) {
		pm_runtime_put(PM_RUNTIME_DSP, PLATFORM_MASTER_CORE_ID);

		/* init power flow task */
		if (!pg_task_ctx)
			task_context_alloc(&pg_task_ctx);
		task_context_init(pg_task_ctx, platform_pg_task, NULL, NULL,
				  PLATFORM_MASTER_CORE_ID,
				  pg_task_stack, sizeof(pg_task_stack));

		/* set TCB to power flow task */
		task_context_set(pg_task_ctx);

		arch_interrupt_disable_mask(0xffffffff);
	} else {
		pm_runtime_get(PM_RUNTIME_DSP, PLATFORM_MASTER_CORE_ID);

		/* set TCB to the one stored in platform_power_gate() */
		task_context_set(lp_restore.task_ctx);
		arch_interrupt_disable_mask(0xffffffff);
#if CONFIG_MEM_WND
		platform_memory_windows_init(0);
#endif
		arch_interrupt_enable_mask(lp_restore.intenable);
	}
}

void lp_wait_for_interrupt(int level)
{
	int schedule_irq;

	/* store the current state */

	lp_restore.intenable = XT_RSR_INTENABLE();

	lp_restore.threadptr = cpu_read_threadptr();
	lp_restore.task_ctx = (void *)task_context_get();
	lp_restore.memmap_vecbase_reset = (long)XT_RSR_VECBASE();
	lp_restore.vector_level_2 = (void *)XT_RSR_EXCSAVE2();
	lp_restore.vector_level_3 = (void *)XT_RSR_EXCSAVE3();
	lp_restore.vector_level_4 = (void *)XT_RSR_EXCSAVE4();
	lp_restore.vector_level_5 = (void *)XT_RSR_EXCSAVE5();

	/* use SW INT handler to do the context switch directly there */
	schedule_irq = interrupt_get_irq(IRQ_NUM_SOFTWARE3, NULL);
	interrupt_register(schedule_irq,
			   platform_pg_int_handler,
			   (void *)LPS_POWER_FLOW_D0_D0I3);
	arch_interrupt_disable_mask(0xffffffff);
	_xtos_set_intlevel(0);
	interrupt_enable(schedule_irq, NULL);
	interrupt_set(schedule_irq);
}

