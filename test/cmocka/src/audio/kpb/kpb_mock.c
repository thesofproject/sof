// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Marcin Rajwa <marcin.rajwa@linux.intel.com>

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <malloc.h>
#include <cmocka.h>

#include <sof/lib/alloc.h>
#include <sof/lib/notifier.h>
#include <sof/lib/pm_runtime.h>
#include <sof/audio/component.h>
#include <sof/drivers/timer.h>
#include <sof/schedule/edf_schedule.h>
#include <sof/schedule/ll_schedule.h>
#include <sof/schedule/schedule.h>
#include <mock_trace.h>
#include <sof/lib/clk.h>

TRACE_IMPL()

static struct sof sof;

void pipeline_xrun(struct pipeline *p, struct comp_dev *dev, int32_t bytes)
{
}

int comp_set_state(struct comp_dev *dev, int cmd)
{
	return 0;
}

int schedule_task_init(struct task *task, uint32_t uid, uint16_t type,
		       uint16_t priority, enum task_state (*run)(void *data),
		       void *data, uint16_t core, uint32_t flags)
{
	return 0;
}

int schedule_task_init_edf(struct task *task, uint32_t uid,
			   const struct task_ops *ops, void *data,
			   uint16_t core, uint32_t flags)
{
	return 0;
}

int schedule_task_init_ll(struct task *task, uint32_t uid, uint16_t type,
			  uint16_t priority, enum task_state (*run)(void *data),
			  void *data, uint16_t core, uint32_t flags)
{
	return 0;
}

uint64_t platform_timer_get(struct timer *timer)
{
	(void)timer;

	return 0;
}

uint64_t arch_timer_get_system(struct timer *timer)
{
	(void)timer;

	return 0;
}

uint64_t clock_ms_to_ticks(int clock, uint64_t ms)
{
	(void)clock;
	(void)ms;

	return 0;
}

struct schedulers **arch_schedulers_get(void)
{
	return NULL;
}

void pm_runtime_enable(enum pm_runtime_context context, uint32_t index)
{
	(void)context;
	(void)index;
}

void pm_runtime_disable(enum pm_runtime_context context, uint32_t index)
{
	(void)context;
	(void)index;
}

struct sof *sof_get(void)
{
	return &sof;
}

int comp_verify_params(struct comp_dev *dev, uint32_t flag,
		       struct sof_ipc_stream_params *params)
{
	return 0;
}
