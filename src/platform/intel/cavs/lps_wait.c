// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Marcin Maka <marcin.maka@linux.intel.com>

#include <arch/lib/wait.h>
#include <cavs/lps_ctx.h>
#include <cavs/lps_wait.h>
#include <cavs/mem_window.h>
#include <sof/common.h>
#include <sof/platform.h>
#include <sof/drivers/interrupt.h>
#include <sof/lib/pm_runtime.h>
#include <sof/lib/shim.h>
#include <sof/schedule/task.h>

#include <stdint.h>

#ifdef __ZEPHYR__
#include <cavs/lib/memory.h>
#include <platform/lib/memory.h>
/* TODO: declare local copy to avoid naming collisions with Zephyr and SOF */
/* headers until common functions can be separated out */
int memcpy_s(void *dest, size_t dest_size, const void *src, size_t src_size);
#endif

#define LPSRAM_MAGIC_VALUE 0x13579BDF

#define LPSRAM_HEADER_SIZE 0xc00

struct lpsram_header {
	uint32_t alt_reset_vector;
	uint32_t adsp_lpsram_magic;
	void *lp_restore_vector;
	uint32_t reserved;

	/* align total size of the structure to the full header size
	 * with bypass vector area
	 */
	uint8_t rom_bypass_vectors_reserved[LPSRAM_HEADER_SIZE - 16];
};

#define LPSRAM_HEADER_BYPASS_ADDR (LP_SRAM_BASE - SRAM_ALIAS_OFFSET)

#define LPS_POWER_FLOW_D0_D0I3		1
#define LPS_POWER_FLOW_D0I3_D0		0

#define LPS_BOOT_STACK_SIZE		4096
#define PG_TASK_STACK_SIZE		4096

__aligned(PLATFORM_DCACHE_ALIGN) uint8_t lps_boot_stack[LPS_BOOT_STACK_SIZE];
__aligned(PLATFORM_DCACHE_ALIGN) lps_ctx lps_restore;
static void *pg_task_ctx;
static uint8_t pg_task_stack[PG_TASK_STACK_SIZE];

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
	vector_size = ALIGN_UP_COMPILE((size_t)&lps_pic_restore_vector_end
			       - (size_t)&lps_pic_restore_vector_literals, 4);

	/* Half of area is available,
	 * another half is reserved for custom vectors in future (like WHM)
	 */
	memcpy_s((void *)LPS_RESTORE_VECTOR_ADDR, LPS_RESTORE_VECTOR_SIZE,
		 &lps_pic_restore_vector_literals, vector_size);
	dcache_writeback_invalidate_region((void *)LPS_RESTORE_VECTOR_ADDR,
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
		pm_runtime_put(PM_RUNTIME_DSP, PLATFORM_PRIMARY_CORE_ID);

		/* init power flow task */
		if (!pg_task_ctx)
			task_context_alloc(&pg_task_ctx);
		task_context_init(pg_task_ctx, platform_pg_task, NULL, NULL,
				  PLATFORM_PRIMARY_CORE_ID,
				  pg_task_stack, sizeof(pg_task_stack));

		/* set TCB to power flow task */
		task_context_set(pg_task_ctx);

		arch_interrupt_disable_mask(0xffffffff);
	} else {
		pm_runtime_get(PM_RUNTIME_DSP, PLATFORM_PRIMARY_CORE_ID);

		/* set TCB to the one stored in platform_power_gate() */
		task_context_set(lps_restore.task_ctx);
		arch_interrupt_disable_mask(0xffffffff);
#if CONFIG_MEM_WND
		platform_memory_windows_init(0);
#endif
		arch_interrupt_enable_mask(lps_restore.intenable);
	}
}

void lps_wait_for_interrupt(int level)
{
	int schedule_irq;

	/* store the current state */

	lps_restore.intenable = arch_interrupt_get_enabled();

	lps_restore.threadptr = cpu_read_threadptr();
	lps_restore.task_ctx = (void *)task_context_get();
	lps_restore.memmap_vecbase_reset = cpu_read_vecbase();
	lps_restore.vector_level_2 = (void *)cpu_read_excsave2();
	lps_restore.vector_level_3 = (void *)cpu_read_excsave3();
	lps_restore.vector_level_4 = (void *)cpu_read_excsave4();
	lps_restore.vector_level_5 = (void *)cpu_read_excsave5();

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

