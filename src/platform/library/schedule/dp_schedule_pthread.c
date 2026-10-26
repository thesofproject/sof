// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation. All rights reserved.

#define _GNU_SOURCE

#include <sof/audio/component.h>
#include <sof/audio/module_adapter/module/generic.h>
#include <rtos/task.h>
#include <sof/schedule/schedule.h>
#include <sof/schedule/dp_schedule.h>
#include <sof/schedule/ll_schedule_domain.h>
#include <platform/lib/ll_schedule.h>
#include <platform/lib/telemetry_sched.h>
#include <sof/lib/cpu.h>
#include <sof/lib/memory.h>
#include <sof/list.h>
#include <sof/trace/trace.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

#include "host_schedule.h"

SOF_DEFINE_REG_UUID(dp_sched);

DECLARE_TR_CTX(dp_tr, SOF_UUID(dp_sched_uuid), LOG_LEVEL_INFO);

struct scheduler_dp_data {
	struct list_item tasks;		/* list of active DP tasks */
	pthread_mutex_t lock;		/* mutex protecting tasks list and events */
	pthread_cond_t tick_done_cond;	/* signalled when running DP tasks finish a tick */
	uint32_t running_count;		/* number of DP tasks currently running in this tick */
};

struct task_dp_pdata {
	pthread_t thread_id;
	bool thread_created;
	pthread_cond_t cond;
	uint32_t event;
	struct processing_module *mod;
	uint32_t ll_cycles_to_start;
};

/*
 * Go through all DP tasks and evaluate their readiness.
 * Must be called with dp_sch->lock held.
 */
static void scheduler_dp_recalculate_thread(struct scheduler_dp_data *dp_sch)
{
	struct list_item *tlist;
	struct task *curr_task;
	struct task_dp_pdata *pdata;

	list_for_item(tlist, &dp_sch->tasks) {
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
				pdata->event |= DP_TASK_EVENT_PROCESS;
				dp_sch->running_count++;
				pthread_cond_signal(&pdata->cond);
			}
		}
	}
}

void scheduler_dp_ll_tick(void)
{
	struct scheduler_dp_data *dp_sch = scheduler_get_data(SOF_SCHEDULE_DP);

	if (!dp_sch)
		return;

	pthread_mutex_lock(&dp_sch->lock);
	scheduler_dp_recalculate_thread(dp_sch);
	while (dp_sch->running_count > 0)
		pthread_cond_wait(&dp_sch->tick_done_cond, &dp_sch->lock);
	pthread_mutex_unlock(&dp_sch->lock);
}

static void *dp_thread_fn(void *arg)
{
	struct task *task = arg;
	struct task_dp_pdata *pdata = task->priv_data;
	struct scheduler_dp_data *dp_sch = scheduler_get_data(SOF_SCHEDULE_DP);
	enum task_state state;
	bool task_stop = false;
	uint32_t event;

	host_cpu_set_id(task->core);

	pthread_mutex_lock(&dp_sch->lock);

	do {
		while (pdata->event == 0)
			pthread_cond_wait(&pdata->cond, &dp_sch->lock);

		event = pdata->event;
		pdata->event = 0;

		if (event & DP_TASK_EVENT_PROCESS) {
			state = task->state;
			if (task->state == SOF_TASK_STATE_RUNNING) {
				uint64_t t0, t1;
				uint32_t core_dp_cnt = 0;
				struct list_item *it;

				list_for_item(it, &dp_sch->tasks) {
					struct task *t = container_of(it, struct task, list);

					if (t->core == task->core)
						core_dp_cnt++;
				}
				pthread_mutex_unlock(&dp_sch->lock);
				t0 = host_telem_timestamp_cycles();
				state = task_run(task);
				t1 = host_telem_timestamp_cycles();
				host_telem_core_dp_run(task->core, core_dp_cnt,
						       (t1 > t0) ? (t1 - t0) : 1);
				pthread_mutex_lock(&dp_sch->lock);
			}

			if (task->state == SOF_TASK_STATE_RUNNING) {
				task->state = state;
				switch (state) {
				case SOF_TASK_STATE_RESCHEDULE:
					task->state = SOF_TASK_STATE_QUEUED;
					break;
				case SOF_TASK_STATE_CANCEL:
				case SOF_TASK_STATE_COMPLETED:
					list_item_del(&task->list);
					break;
				default:
					list_item_del(&task->list);
					task->state = SOF_TASK_STATE_CANCEL;
					break;
				}
			}

			task_stop = task->state == SOF_TASK_STATE_COMPLETED ||
				    task->state == SOF_TASK_STATE_CANCEL;

			if (dp_sch->running_count > 0) {
				dp_sch->running_count--;
				if (dp_sch->running_count == 0)
					pthread_cond_broadcast(&dp_sch->tick_done_cond);
			}
		}

		if (event & DP_TASK_EVENT_CANCEL)
			task_stop = true;
	} while (!task_stop);

	pthread_mutex_unlock(&dp_sch->lock);

	if (task->state == SOF_TASK_STATE_COMPLETED)
		task_complete(task);

	return NULL;
}

static int scheduler_dp_task_stop(void *data, struct task *task)
{
	struct scheduler_dp_data *dp_sch = data;
	struct task_dp_pdata *pdata = task->priv_data;
	pthread_t tid = 0;
	bool join_thread = false;

	pthread_mutex_lock(&dp_sch->lock);

	while (dp_sch->running_count > 0)
		pthread_cond_wait(&dp_sch->tick_done_cond, &dp_sch->lock);

	if (task->state == SOF_TASK_STATE_QUEUED ||
	    task->state == SOF_TASK_STATE_RUNNING)
		list_item_del(&task->list);

	task->state = SOF_TASK_STATE_CANCEL;
	pdata->event |= DP_TASK_EVENT_CANCEL;
	pthread_cond_signal(&pdata->cond);

	if (pdata->thread_created) {
		tid = pdata->thread_id;
		pdata->thread_created = false;
		join_thread = true;
	}

	pthread_mutex_unlock(&dp_sch->lock);

	if (join_thread)
		pthread_join(tid, NULL);

	return 0;
}

static int scheduler_dp_task_free(void *data, struct task *task)
{
	struct task_dp_pdata *pdata = task->priv_data;

	scheduler_dp_task_stop(data, task);
	pthread_cond_destroy(&pdata->cond);
	task->state = SOF_TASK_STATE_FREE;
	rfree(task);

	return 0;
}

static int scheduler_dp_task_shedule(void *data, struct task *task, uint64_t start,
				     uint64_t period)
{
	struct scheduler_dp_data *dp_sch = data;
	struct task_dp_pdata *pdata = task->priv_data;
	int ret;

	pthread_mutex_lock(&dp_sch->lock);

	if (task_is_active(task)) {
		pthread_mutex_unlock(&dp_sch->lock);
		return 0;
	}

	if (task->state != SOF_TASK_STATE_INIT &&
	    task->state != SOF_TASK_STATE_CANCEL &&
	    task->state != SOF_TASK_STATE_COMPLETED) {
		pthread_mutex_unlock(&dp_sch->lock);
		return -EINVAL;
	}

	if (!pdata->thread_created) {
		int host_cpu;

		pdata->event = 0;
		ret = pthread_create(&pdata->thread_id, NULL, dp_thread_fn, task);
		if (ret != 0) {
			pthread_mutex_unlock(&dp_sch->lock);
			return -ret;
		}
		host_cpu = host_pin_thread_to_dsp_core(pdata->thread_id, task->core);
		pdata->thread_created = true;
		tr_info(&dp_tr, "DP thread created for task %p on DSP core %d (host CPU %d)",
			task, task->core, host_cpu);
	}

	task->state = SOF_TASK_STATE_QUEUED;
	list_item_prepend(&task->list, &dp_sch->tasks);
	pdata->mod->dp_startup_delay = true;
	pdata->ll_cycles_to_start = 0;

	pthread_mutex_unlock(&dp_sch->lock);
	return 0;
}

static void dp_scheduler_free(void *data, uint32_t flags)
{
	struct scheduler_dp_data *dp_sch = data;

	if (!dp_sch)
		return;

	pthread_mutex_destroy(&dp_sch->lock);
	pthread_cond_destroy(&dp_sch->tick_done_cond);
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
	pthread_mutex_init(&dp_sch->lock, NULL);
	pthread_cond_init(&dp_sch->tick_done_cond, NULL);

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
	pthread_cond_init(&pdata->cond, NULL);
	pdata->mod = mod;

	task_memory->task.ops.complete = ops->complete;
	task_memory->task.ops.get_deadline = ops->get_deadline;
	task_memory->task.state = SOF_TASK_STATE_INIT;
	task_memory->task.core = core;
	task_memory->task.priv_data = pdata;

	ret = pthread_create(&pdata->thread_id, NULL, dp_thread_fn, &task_memory->task);
	if (ret != 0) {
		tr_err(&dp_tr, "scheduler_dp_task_init: pthread_create failed: %d", ret);
		pthread_cond_destroy(&pdata->cond);
		rfree(task_memory);
		return -ret;
	}
	ret = host_pin_thread_to_dsp_core(pdata->thread_id, core);
	pdata->thread_created = true;
	tr_info(&dp_tr, "DP thread initialized for module %#x on DSP core %d (host CPU %d)",
		dev_comp_id(mod->dev), core, ret);

	*task = &task_memory->task;
	return 0;
}

void scheduler_get_task_info_dp(struct scheduler_props *scheduler_props,
				uint32_t *data_off_size)
{
	(void)scheduler_props;
	(void)data_off_size;
}
