// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2024 MediaTek. All rights reserved.
 *
 * Author: Hailong Fan <hailong.fan@mediatek.com>
 */

#include <platform/drivers/mt_reg_base.h>
#include <rtos/clk.h>
#include <rtos/wait.h>
#include <sof/common.h>
#include <sof/lib/cpu.h>
#include <sof/lib/io.h>
#include <sof/lib/memory.h>
#include <sof/lib/notifier.h>
#include <sof/lib/uuid.h>
#include <rtos/sof.h>
#include <sof/trace/trace.h>

SOF_DEFINE_REG_UUID(clkdrv_mt8196);

DECLARE_TR_CTX(clkdrv_tr, SOF_UUID(clkdrv_mt8196_uuid), LOG_LEVEL_INFO);

/* default voltage is 0.75V */
const struct freq_table platform_cpu_freq[] = {
	{  26000000, 26000},
	{ 800000000, 26000},
};

STATIC_ASSERT(ARRAY_SIZE(platform_cpu_freq) == NUM_CPU_FREQ,
	      invalid_number_of_cpu_frequencies);

static SHARED_DATA struct clock_info platform_clocks_info[NUM_CLOCKS];

void platform_clock_init(struct sof *sof)
{
	int i;

	tr_dbg(&clkdrv_tr, "clock init\n");
	sof->clocks = platform_shared_get(platform_clocks_info, sizeof(platform_clocks_info));

	/* When the system is in an active state, the DSP clock operates at 800MHz (0.75V).
	 * In a low power scenario, the DSP enters WFI state, and the clock reduces to 26MHz.
	 * The clock selection is controlled by the host, and we do not allow SOF to change
	 * the ADSP frequency.
	 */
	for (i = 0; i < CONFIG_CORE_COUNT; i++) {
		sof->clocks[i] = (struct clock_info){
			.freqs_num = NUM_CPU_FREQ,
			.freqs = platform_cpu_freq,
			.default_freq_idx = CPU_DEFAULT_IDX,
			.current_freq_idx = CPU_DEFAULT_IDX,
			.notification_id = NOTIFIER_ID_CPU_FREQ,
			.notification_mask = NOTIFIER_TARGET_CORE_MASK(i),
			.set_freq = NULL,
		};
	}
}
