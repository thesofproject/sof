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
#include <sof/lib/mm_heap.h>
#include <sof/lib/watchdog.h>
#include <sof/lib/cpu-clk-manager.h>
#include <sof/schedule/edf_schedule.h>
#include <sof/schedule/ll_schedule.h>
#include <sof/schedule/ll_schedule_domain.h>
#include <sof/trace/trace.h>
#include <ipc/header.h>
#include <ipc/info.h>
#include <kernel/abi.h>
#include <rtos/clk.h>
#include <zephyr/pm/pm.h>

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

/* Value to be determined experimentaly */
#define BASE_CPS_USAGE 10000

extern void ace_mark_thread_as_ready(void);

/* notifier called after every power state transition */
static void notify_pm_state_exit(enum pm_state state)
{
	if (state == PM_STATE_SOFT_OFF)	{
		if (!cpu_is_primary(arch_proc_id()))
			ace_mark_thread_as_ready();
	}
}

static struct pm_notifier pm_state_notifier = {
	.state_entry = NULL,
	.state_exit = notify_pm_state_exit,
};

/* Runs on the primary core only */
int platform_init(struct sof *sof)
{
	int ret;

	trace_point(TRACE_BOOT_PLATFORM_CLOCK);
	platform_clock_init(sof);
	kcps_budget_init();
	core_kcps_adjust(0, BASE_CPS_USAGE);

	trace_point(TRACE_BOOT_PLATFORM_SCHED);
	scheduler_init_edf();

	/* init low latency timer domain and scheduler */
	sof->platform_timer_domain = zephyr_domain_init(PLATFORM_DEFAULT_CLOCK);
	scheduler_init_ll(sof->platform_timer_domain);

	/* init the system agent */
	trace_point(TRACE_BOOT_PLATFORM_AGENT);
	sa_init(sof, CONFIG_SYSTICK_PERIOD);

	/* init DMACs */
	trace_point(TRACE_BOOT_PLATFORM_DMA);
	ret = dmac_init(sof);
	if (ret < 0)
		return ret;

	/* register power states entry / exit notifiers */
	pm_notifier_register(&pm_state_notifier);

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
