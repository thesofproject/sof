// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright 2023 NXP
 */

#include <rtos/clk.h>
#include <sof/lib/notifier.h>

static const struct freq_table platform_cpu_freq[] = {
	{ CONFIG_SYS_CLOCK_HW_CYCLES_PER_SEC,
		CONFIG_SYS_CLOCK_TICKS_PER_SEC * 1000 },
};

static struct clock_info platform_clocks_info[NUM_CLOCKS];

void platform_clock_init(struct sof *sof)
{
	int i;

	sof->clocks = platform_clocks_info;

	/* The CCM doesn't seem to allow setting a core's
	 * frequency. It probably sets the whole cluster's
	 * frequency to some value (not relevant). Since
	 * we're running on top of Jailhouse we don't want
	 * to allow SOF to change the cluster's frequency
	 * since that would also affect Linux.
	 *
	 * Also, as a consequence to this, on SMP systems,
	 * NUM_CLOCKS and CONFIG_CORE_COUNT will probably
	 * differ so watch out for this when using the below
	 * code. In the case of i.MX93 this is fince since
	 * SOF runs on a single core.
	 */
	for (i = 0; i < CONFIG_CORE_COUNT; i++) {
		sof->clocks[i] = (struct clock_info) {
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
