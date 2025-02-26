// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>
//         Rander Wang <rander.wang@intel.com>
//         Janusz Jankowski <janusz.jankowski@linux.intel.com>

#include <sof/debug/debug.h>
#include <sof/ipc/common.h>
#include <sof/ipc/driver.h>
#include <sof/ipc/msg.h>
#include <sof/lib/agent.h>
#include <sof/lib/cpu-clk-manager.h>
#include <sof/lib/mm_heap.h>
#include <sof/lib/watchdog.h>
#include <sof/lib/cpu-clk-manager.h>
#include <sof/schedule/edf_schedule.h>
#include <sof/schedule/dp_schedule.h>
#include <sof/schedule/twb_schedule.h>
#include <sof/schedule/ll_schedule.h>
#include <sof/schedule/ll_schedule_domain.h>
#include <sof/trace/trace.h>
#include <ipc/header.h>
#include <ipc/info.h>
#include <kernel/abi.h>
#include <rtos/clk.h>
#include <sof/lib/cpu.h>

#include <sof_versions.h>
#include <stdint.h>

static const struct sof_ipc_fw_ready ready
	__section(".fw_ready") = {
	.hdr = {
		.cmd = SOF_IPC_FW_READY,
		.size = sizeof(struct sof_ipc_fw_ready),
	},
	.version = {
		.hdr.size = sizeof(struct sof_ipc_fw_version),
		.micro = SOF_MICRO,
		.minor = SOF_MINOR,
		.major = SOF_MAJOR,
/* opt-in; reproducible build by default */
#if BLD_COUNTERS
		.build = SOF_BUILD, /* See version-build-counter.cmake */
		.date = __DATE__,
		.time = __TIME__,
#else /* BLD_COUNTERS */
		.build = -1,
		.date = "dtermin.\0",
		.time = "fwready.\0",
#endif /* BLD_COUNTERS */
		.tag = SOF_TAG,
		.abi_version = SOF_ABI_VERSION,
		.src_hash = SOF_SRC_HASH,
	},
	.flags = DEBUG_SET_FW_READY_FLAGS,
};

int platform_boot_complete(uint32_t boot_message)
{
	struct ipc_cmd_hdr header;

	/* get any IPC specific boot message and optional data */
	ipc_boot_complete_msg(&header, 0);

	struct ipc_msg msg = {
		.header = header.pri,
		.extension = header.ext,
		.tx_size = sizeof(ready),
		.tx_data = (void *)&ready,
	};

	/* send fimrware ready message. */
	return ipc_platform_send_msg(&msg);
}

#ifdef CONFIG_PM
static struct pm_notifier pm_state_notifier = {
	.state_entry = cpu_notify_state_entry,
	.state_exit = cpu_notify_state_exit,
};
#endif

/* Runs on the primary core only */
int platform_init(struct sof *sof)
{
	int ret;

	trace_point(TRACE_BOOT_PLATFORM_CLOCK);
	platform_clock_init(sof);

#if CONFIG_KCPS_DYNAMIC_CLOCK_CONTROL
	kcps_budget_init();

	ret = core_kcps_adjust(cpu_get_id(), PRIMARY_CORE_BASE_CPS_USAGE);
	if (ret < 0)
		return ret;
#endif

	trace_point(TRACE_BOOT_PLATFORM_SCHED);
	scheduler_init_edf();

	/* init low latency timer domain and scheduler. Any failure is fatal */
	/* clk is ignored on Zephyr so pass 0 */
	sof->platform_timer_domain = zephyr_domain_init(0);
	ret = scheduler_init_ll(sof->platform_timer_domain);
	if (ret < 0)
		return ret;

#if CONFIG_ZEPHYR_DP_SCHEDULER
	ret = scheduler_dp_init();
	if (ret < 0)
		return ret;
#endif /* CONFIG_ZEPHYR_DP_SCHEDULER */

#if CONFIG_ZEPHYR_TWB_SCHEDULER
	ret = scheduler_twb_init();
	if (ret < 0)
		return ret;
#endif

	/* init the system agent */
	trace_point(TRACE_BOOT_PLATFORM_AGENT);
	sa_init(sof, CONFIG_SYSTICK_PERIOD);

	/* init DMACs */
	trace_point(TRACE_BOOT_PLATFORM_DMA);
	ret = dmac_init(sof);
	if (ret < 0)
		return ret;

#ifdef CONFIG_PM
	/* register power states entry / exit notifiers */
	pm_notifier_register(&pm_state_notifier);
#endif

	/* initialize the host IPC mechanisms */
	trace_point(TRACE_BOOT_PLATFORM_IPC);
	ipc_init(sof);
	idc_init();

	watchdog_init();

	/* show heap status */
	heap_trace_all(1);

	return 0;
}

int platform_context_save(struct sof *sof)
{
	return 0;
}
