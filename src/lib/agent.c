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

#include <sof/drivers/timer.h>
#include <sof/lib/agent.h>
#include <sof/lib/alloc.h>
#include <sof/lib/clk.h>
#include <sof/lib/memory.h>
#include <sof/lib/uuid.h>
#include <sof/debug/panic.h>
#include <sof/platform.h>
#include <sof/schedule/ll_schedule.h>
#include <sof/schedule/schedule.h>
#include <sof/schedule/task.h>
#include <sof/sof.h>
#include <sof/trace/trace.h>
#include <ipc/topology.h>
#include <ipc/trace.h>
#include <user/trace.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __ZEPHYR__
#include <kernel.h>
#endif

/* 5276b491-5b64-464e-8984-dc228ef9e6a1 */
DECLARE_SOF_UUID("sa", sa_uuid, 0x5276b491, 0x5b64, 0x464e,
		 0x89, 0x84, 0xdc, 0x22, 0x8e, 0xf9, 0xe6, 0xa1);

DECLARE_TR_CTX(sa_tr, SOF_UUID(sa_uuid), LOG_LEVEL_INFO);

#define perf_sa_trace(pcd, sa)					\
	tr_info(&sa_tr, "perf sys_load peak plat %u cpu %u",	\
		(uint32_t)((pcd)->plat_delta_peak),		\
		(uint32_t)((pcd)->cpu_delta_peak))

/* c63c4e75-8f61-4420-9319-1395932efa9e */
DECLARE_SOF_UUID("agent-work", agent_work_task_uuid, 0xc63c4e75, 0x8f61, 0x4420,
		 0x93, 0x19, 0x13, 0x95, 0x93, 0x2e, 0xfa, 0x9e);

static enum task_state validate(void *data)
{
	struct sa *sa = data;
	uint64_t current;
	uint64_t delta;

	current = platform_timer_get(timer_get());
	delta = current - sa->last_check;

	perf_cnt_stamp(&sa->pcd, perf_sa_trace, sa);

#if CONFIG_AGENT_PANIC_ON_DELAY
	/* panic timeout */
	if (sa->panic_on_delay && delta > sa->panic_timeout)
		panic(SOF_IPC_PANIC_IDLE);
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

	sof->sa = rzalloc(SOF_MEM_ZONE_SYS_SHARED, 0, SOF_MEM_CAPS_RAM, sizeof(*sof->sa));

	/* set default timeouts */
#ifdef __ZEPHYR__
	ticks = k_us_to_cyc_ceil64(timeout);
#else
	ticks = clock_ms_to_ticks(PLATFORM_DEFAULT_CLOCK, 1) * timeout / 1000;
#endif

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

	schedule_task_init_ll(&sof->sa->work, SOF_UUID(agent_work_task_uuid),
			      SOF_SCHEDULE_LL_TIMER,
			      SOF_TASK_PRI_HIGH, validate, sof->sa, 0, 0);

	schedule_task(&sof->sa->work, 0, timeout);

	/* set last check time to now to give time for boot completion */
	sof->sa->last_check = platform_timer_get(timer_get());

}
