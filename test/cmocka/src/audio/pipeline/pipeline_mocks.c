// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Author: Jakub Dabek <jakub.dabek@linux.intel.com>

#include "pipeline_mocks.h"

#include <mock_trace.h>

TRACE_IMPL()

struct ipc *_ipc;
struct timer *platform_timer;

void platform_dai_timestamp(struct comp_dev *dai,
	struct sof_ipc_stream_posn *posn)
{
	(void)dai;
	(void)posn;
}

void schedule_task_free(struct task *task)
{
	(void)task;
	task->state = SOF_TASK_STATE_FREE;
	task->func = NULL;
	task->data = NULL;
}

void schedule_task(struct task *task, uint64_t start, uint64_t period)
{
	(void)task;
	(void)start;
	(void)period;
}

int schedule_task_init(struct task *task, uint16_t type, uint16_t priority,
		       enum task_state (*func)(void *data), void *data,
		       uint16_t core, uint32_t xflags)
{
	(void)task;
	(void)type;
	(void)priority;
	(void)func;
	(void)data;
	(void)core;
	(void)xflags;

	return 0;
}

int schedule_task_cancel(struct task *task)
{
	(void)task;
	return 0;
}

void rfree(void *ptr)
{
	(void)ptr;
}

void platform_host_timestamp(struct comp_dev *host,
	struct sof_ipc_stream_posn *posn)
{
	(void)host;
	(void)posn;
}

int ipc_stream_send_xrun(struct comp_dev *cdev,
	struct sof_ipc_stream_posn *posn)
{
	return 0;
}

void cpu_power_down_core(void) { }

void notifier_notify(void) { }

struct ipc_comp_dev *ipc_get_comp(struct ipc *ipc, uint32_t id)
{
	(void)ipc;
	(void)id;

	return NULL;
}

void heap_trace_all(int force)
{
	(void)force;
}

void __panic(uint32_t p, char *filename, uint32_t linenum)
{
	(void)p;
	(void)filename;
	(void)linenum;
}

uint64_t platform_timer_get(struct timer *timer)
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
