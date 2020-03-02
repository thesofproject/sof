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
struct schedulers *schedulers;
static struct sof sof;

struct sof *sof_get(void)
{
	return &sof;
}

struct schedulers **arch_schedulers_get(void)
{
	return &schedulers;
}

void platform_dai_timestamp(struct comp_dev *dai,
	struct sof_ipc_stream_posn *posn)
{
	(void)dai;
	(void)posn;
}

int schedule_task_init(struct task *task, uint32_t uid, uint16_t type,
		       uint16_t priority, enum task_state (*run)(void *data),
		       void *data, uint16_t core, uint32_t flags)
{
	(void)task;
	(void)uid;
	(void)type;
	(void)priority;
	(void)run;
	(void)data;
	(void)core;
	(void)flags;

	return 0;
}

int schedule_task_init_ll(struct task *task, uint32_t uid, uint16_t type,
			  uint16_t priority, enum task_state (*run)(void *data),
			  void *data, uint16_t core, uint32_t flags)
{
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

struct ipc_comp_dev *ipc_get_comp_by_id(struct ipc *ipc, uint32_t id)
{
	(void)ipc;
	(void)id;

	return NULL;
}

struct ipc_comp_dev *ipc_get_comp_by_ppl_id(struct ipc *ipc, uint16_t type,
					    uint32_t ppl_id)
{
	(void)ipc;
	(void)type;
	(void)ppl_id;

	return NULL;
}

void heap_trace_all(int force)
{
	(void)force;
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

void ipc_msg_send(struct ipc_msg *msg, void *data, bool high_priority)
{
	(void)msg;
	(void)data;
	(void)high_priority;
}
