// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2017 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>

/*
 * System Agent - Simple FW Monitor that can notify host drivers in the event
 * of any FW errors. The SA checks if the DSP is still responsive and verifies
 * the stability of the system by checking time elapsed between every timer
 * tick. If the core exceeds the threshold by over 5% then the SA will emit
 * error trace. However if it will be exceeded by over 100% the panic will be
 * called.
 */

#include <rtos/timer.h>
#include <sof/lib/agent.h>
#include <rtos/alloc.h>
#include <rtos/clk.h>
#include <sof/lib/memory.h>
#include <sof/lib/uuid.h>
#include <rtos/panic.h>
#include <sof/platform.h>
#include <sof/schedule/ll_schedule.h>
#include <sof/schedule/schedule.h>
#include <rtos/task.h>
#include <rtos/sof.h>
#include <sof/trace/trace.h>
#include <ipc/topology.h>
#include <ipc/trace.h>
#include <user/trace.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <rtos/kernel.h>

LOG_MODULE_REGISTER(sa, CONFIG_SOF_LOG_LEVEL);

SOF_DEFINE_REG_UUID(sa);

DECLARE_TR_CTX(sa_tr, SOF_UUID(sa_uuid), LOG_LEVEL_INFO);

SOF_DEFINE_REG_UUID(agent_work);

#if CONFIG_PERFORMANCE_COUNTERS
static void perf_sa_trace(struct perf_cnt_data *pcd, int ignored)
{
	tr_info(&sa_tr, "perf sys_load peak plat %u cpu %u",
		(uint32_t)((pcd)->plat_delta_peak),
		(uint32_t)((pcd)->cpu_delta_peak));
}

static void perf_avg_sa_trace(struct perf_cnt_data *pcd, int ignored)
{
	tr_info(&sa_tr, "perf sys_load cpu avg %u (current peak %u)",
		(uint32_t)((pcd)->cpu_delta_sum),
		(uint32_t)((pcd)->cpu_delta_peak));
}

#endif

static enum task_state validate(void *data)
{
	struct sa *sa = data;
	uint64_t current;
	uint64_t delta;

	current = sof_cycle_get_64();
	delta = current - sa->last_check;

	perf_cnt_stamp(&sa->pcd, perf_sa_trace, 0 /* ignored */);
	perf_cnt_average(&sa->pcd, perf_avg_sa_trace, 0 /* ignored */);

#if CONFIG_AGENT_PANIC_ON_DELAY
	/* panic timeout */
	if (sa->panic_on_delay && delta > sa->panic_timeout)
		sof_panic(SOF_IPC_PANIC_IDLE);
#endif

	/* warning timeout */
	if (delta > sa->warn_timeout) {
		if (delta > UINT_MAX)
			tr_warn(&sa_tr, "validate(), ll drift detected, delta > %u", UINT_MAX);
		else
			tr_warn(&sa_tr, "validate(), ll drift detected, delta = %u",
				(unsigned int)delta);
	}

	/* update last_check to current */
	sa->last_check = current;

	return SOF_TASK_STATE_RESCHEDULE;
}

void sa_init(struct sof *sof, uint64_t timeout)
{
	uint64_t ticks;

	if (timeout > UINT_MAX)
		tr_warn(&sa_tr, "sa_init(), timeout > %u", UINT_MAX);
	else
		tr_info(&sa_tr, "sa_init(), timeout = %u", (unsigned int)timeout);

	sof->sa = rzalloc(SOF_MEM_FLAG_USER | SOF_MEM_FLAG_COHERENT, sizeof(*sof->sa));

	/* set default timeouts */
	ticks = k_us_to_cyc_ceil64(timeout);

	/* TODO: change values after minimal drifts will be assured */
	sof->sa->panic_timeout = 2 * ticks;	/* 100% delay */
	sof->sa->warn_timeout = ticks + ticks / 20;	/* 5% delay */

	atomic_init(&sof->sa->panic_cnt, 0);
	sof->sa->panic_on_delay = true;

	if (ticks > UINT_MAX || sof->sa->warn_timeout > UINT_MAX ||
	    sof->sa->panic_timeout > UINT_MAX)
		tr_info(&sa_tr,
			"sa_init(), some of the values are > %u", UINT_MAX);
	else
		tr_info(&sa_tr,
			"sa_init(), ticks = %u, sof->sa->warn_timeout = %u, sof->sa->panic_timeout = %u",
			(unsigned int)ticks, (unsigned int)sof->sa->warn_timeout,
			(unsigned int)sof->sa->panic_timeout);

	schedule_task_init_ll(&sof->sa->work, SOF_UUID(agent_work_uuid),
			      SOF_SCHEDULE_LL_TIMER,
			      SOF_TASK_PRI_HIGH, validate, sof->sa, 0, 0);

	schedule_task(&sof->sa->work, 0, timeout);

	/* set last check time to now to give time for boot completion */
	sof->sa->last_check = sof_cycle_get_64();

}

void sa_exit(struct sof *sof)
{
	schedule_task_cancel(&sof->sa->work);
}
