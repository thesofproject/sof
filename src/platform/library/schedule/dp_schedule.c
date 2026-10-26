// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation. All rights reserved.

#include <sof/audio/component.h>
#include <sof/audio/module_adapter/module/generic.h>
#include <rtos/task.h>
#include <sof/schedule/schedule.h>
#include <sof/schedule/dp_schedule.h>
#include <sof/schedule/ll_schedule_domain.h>
#include <platform/lib/telemetry_sched.h>
#include <sof/lib/cpu.h>
#include <sof/lib/memory.h>
#include <sof/list.h>
#include <sof/trace/trace.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

/* Synchronous standard-C DP scheduler for Xtensa simulator (xt-run) */

SOF_DEFINE_REG_UUID(dp_sched);

DECLARE_TR_CTX(dp_tr, SOF_UUID(dp_sched_uuid), LOG_LEVEL_INFO);

struct scheduler_dp_data {
	struct list_item tasks;		/* list of active DP tasks */
};

struct task_dp_pdata {
	struct processing_module *mod;
	uint32_t ll_cycles_to_start;
};

void scheduler_dp_ll_tick(void)
{
	struct scheduler_dp_data *dp_sch = scheduler_get_data(SOF_SCHEDULE_DP);
	struct list_item *tlist, *tlist_;
	struct task *curr_task;
	struct task_dp_pdata *pdata;
	enum task_state state;

	if (!dp_sch)
		return;

	list_for_item_safe(tlist, tlist_, &dp_sch->tasks) {
		curr_task = container_of(tlist, struct task, list);
		pdata = curr_task->priv_data;
		struct processing_module *mod = pdata->mod;

		/* decrease number of LL ticks/cycles left till the module reaches its deadline */
		if (mod->dp_startup_delay && pdata->ll_cycles_to_start) {
			pdata->ll_cycles_to_start--;
			if (!pdata->ll_cycles_to_start)
				mod->dp_startup_delay = false;
		}

		if (curr_task->state == SOF_TASK_STATE_QUEUED &&
		    mod->dev->state == COMP_STATE_ACTIVE) {
			bool mod_ready;

			mod_ready = module_is_ready_to_process(mod, mod->sources,
							       mod->num_of_sources,
							       mod->sinks,
							       mod->num_of_sinks);
			if (mod_ready) {
				curr_task->state = SOF_TASK_STATE_RUNNING;
				if (mod->dp_startup_delay && !pdata->ll_cycles_to_start) {
					pdata->ll_cycles_to_start =
						module_get_lpt(pdata->mod) / LL_TIMER_PERIOD_US;
					if (!pdata->ll_cycles_to_start)
						pdata->ll_cycles_to_start = 1;
				}

				host_cpu_set_id(curr_task->core);
				{
					uint64_t t0 = host_telem_timestamp_cycles();
					uint64_t t1;

					state = task_run(curr_task);
					t1 = host_telem_timestamp_cycles();
					host_telem_core_dp_run(curr_task->core, 1,
							       (t1 > t0) ? (t1 - t0) : 1);
				}
				host_cpu_set_id(0);

				if (curr_task->state == SOF_TASK_STATE_RUNNING) {
					curr_task->state = state;
					switch (state) {
					case SOF_TASK_STATE_RESCHEDULE:
						curr_task->state = SOF_TASK_STATE_QUEUED;
						break;
					case SOF_TASK_STATE_COMPLETED:
						list_item_del(&curr_task->list);
						task_complete(curr_task);
						break;
					case SOF_TASK_STATE_CANCEL:
						list_item_del(&curr_task->list);
						break;
					default:
						list_item_del(&curr_task->list);
						curr_task->state = SOF_TASK_STATE_CANCEL;
						break;
					}
				}
			}
		}
	}
}

static int scheduler_dp_task_stop(void *data, struct task *task)
{
	(void)data;

	if (task->state == SOF_TASK_STATE_QUEUED ||
	    task->state == SOF_TASK_STATE_RUNNING)
		list_item_del(&task->list);

	task->state = SOF_TASK_STATE_CANCEL;

	return 0;
}

static int scheduler_dp_task_free(void *data, struct task *task)
{
	scheduler_dp_task_stop(data, task);
	task->state = SOF_TASK_STATE_FREE;
	rfree(task);

	return 0;
}

static int scheduler_dp_task_shedule(void *data, struct task *task, uint64_t start,
				     uint64_t period)
{
	struct scheduler_dp_data *dp_sch = data;
	struct task_dp_pdata *pdata = task->priv_data;

	(void)start;
	(void)period;

	if (task_is_active(task))
		return 0;

	if (task->state != SOF_TASK_STATE_INIT &&
	    task->state != SOF_TASK_STATE_CANCEL &&
	    task->state != SOF_TASK_STATE_COMPLETED)
		return -EINVAL;

	task->state = SOF_TASK_STATE_QUEUED;
	list_item_prepend(&task->list, &dp_sch->tasks);
	pdata->mod->dp_startup_delay = true;
	pdata->ll_cycles_to_start = 0;

	return 0;
}

static void dp_scheduler_free(void *data, uint32_t flags)
{
	struct scheduler_dp_data *dp_sch = data;

	(void)flags;
	free(dp_sch);
}

static struct scheduler_ops schedule_dp_ops = {
	.schedule_task		= scheduler_dp_task_shedule,
	.schedule_task_cancel	= scheduler_dp_task_stop,
	.schedule_task_free	= scheduler_dp_task_free,
	.scheduler_free		= dp_scheduler_free,
};

int scheduler_dp_init(void)
{
	struct scheduler_dp_data *dp_sch;

	dp_sch = calloc(1, sizeof(*dp_sch));
	if (!dp_sch)
		return -ENOMEM;

	list_init(&dp_sch->tasks);
	scheduler_init(SOF_SCHEDULE_DP, &schedule_dp_ops, dp_sch);

	return 0;
}

int scheduler_dp_task_init(struct task **task,
			   const struct sof_uuid_entry *uid,
			   const struct task_ops *ops,
			   struct processing_module *mod,
			   uint16_t core,
			   size_t stack_size,
			   uint32_t options)
{
	struct {
		struct task task;
		struct task_dp_pdata pdata;
	} *task_memory;
	struct task_dp_pdata *pdata;
	int ret;

	(void)stack_size;

	task_memory = rzalloc(SOF_MEM_FLAG_USER, sizeof(*task_memory));
	if (!task_memory) {
		tr_err(&dp_tr, "scheduler_dp_task_init: memory alloc failed");
		return -ENOMEM;
	}

	ret = schedule_task_init(&task_memory->task, uid, SOF_SCHEDULE_DP, 0, ops->run,
				 mod, core, options);
	if (ret < 0) {
		tr_err(&dp_tr, "scheduler_dp_task_init: schedule_task_init failed");
		rfree(task_memory);
		return ret;
	}

	pdata = &task_memory->pdata;
	pdata->mod = mod;

	task_memory->task.ops.complete = ops->complete;
	task_memory->task.ops.get_deadline = ops->get_deadline;
	task_memory->task.state = SOF_TASK_STATE_INIT;
	task_memory->task.core = core;
	task_memory->task.priv_data = pdata;

	*task = &task_memory->task;
	return 0;
}

void scheduler_get_task_info_dp(struct scheduler_props *scheduler_props,
				uint32_t *data_off_size)
{
	(void)scheduler_props;
	(void)data_off_size;
}
