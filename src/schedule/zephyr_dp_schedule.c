// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2023 Intel Corporation. All rights reserved.
 *
 * Author: Marcin Szkudlinski
 */

#include <sof/audio/component.h>
#include <sof/audio/module_adapter/module/generic.h>
#include <sof/llext_manager.h>
#include <rtos/task.h>
#include <rtos/userspace_helper.h>
#include <sof/schedule/dp_schedule.h>
#include <sof/schedule/ll_schedule.h>
#include <sof/schedule/ll_schedule_domain.h>
#include <sof/trace/trace.h>
#include <rtos/wait.h>
#include <rtos/interrupt.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/clock.h>
#include <zephyr/sys/sem.h>
#include <zephyr/sys/mutex.h>
#include <sof/lib/memory.h>
#include <sof/schedule/schedule.h>
#include <ipc4/base_fw.h>

#include <sof/schedule/zephyr_dp_schedule.h>

#include <zephyr/kernel/thread.h>

#include <stdint.h>
#include <string.h>

LOG_MODULE_DECLARE(dp_schedule, CONFIG_SOF_LOG_LEVEL);
extern struct tr_ctx dp_tr;

/* The scheduler locks, the LL tick system call and the scheduler initialization, which
 * run in kernel mode, are in src/kernel/schedule/zephyr_dp_schedule-kernel.c
 */

/* Each per-core instance of DP scheduler has separate structures; hence, locks are per-core.
 *
 * TODO: consider using cpu_get_id() instead of supplying core as a parameter.
 */
unsigned int scheduler_dp_lock(uint16_t core)
{
	k_sem_take(&dp_lock[core], K_FOREVER);
	return core;
}

void scheduler_dp_unlock(unsigned int key)
{
	k_sem_give(&dp_lock[key]);
}

#if CONFIG_SOF_USERSPACE_APPLICATION
int scheduler_dp_task_cancel(void *data, struct task *task)
{
	/* Should never be called */
	k_panic();
	return -EOPNOTSUPP;
}
#endif

int scheduler_dp_task_stop(void *data, struct task *task)
{
	unsigned int lock_key;
	struct scheduler_dp_data *dp_sch = (struct scheduler_dp_data *)data;
	struct task_dp_pdata *pdata = task->priv_data;

	/* this is asyn cancel - mark the task as canceled and remove it from scheduling */
	lock_key = scheduler_dp_lock(task->core);

	task->state = SOF_TASK_STATE_CANCEL;
	list_item_del(&task->list);

	/* if there're no more DP task, stop LL tick source */
	if (list_is_empty(&dp_sch->tasks))
		schedule_task_cancel(&dp_sch->ll_tick_src);

	/* if the task is waiting - let it run and self-terminate */
	k_event_set(pdata->event, DP_TASK_EVENT_CANCEL);

	scheduler_dp_unlock(lock_key);

	/* wait till the task has finished, if there was any task created */
	if (pdata->thread_id)
		k_thread_join(pdata->thread_id, K_FOREVER);

	return 0;
}

int scheduler_dp_task_free(void *data, struct task *task)
{
	struct task_dp_pdata *pdata = task->priv_data;
	int ret;

	scheduler_dp_task_stop(data, task);

	/* the thread should be terminated at this moment,
	 * abort is safe and will ensure no use after free
	 */
	if (pdata->thread_id) {
		k_thread_abort(pdata->thread_id);
		pdata->thread_id = NULL;
	}

	/* free task stack */
	ret = user_stack_free(pdata->p_stack);
	pdata->p_stack = NULL;

	scheduler_dp_internal_free(task);

	return ret;
}

int scheduler_dp_task_shedule(void *data, struct task *task, uint64_t start,
			      uint64_t period)
{
	struct scheduler_dp_data *dp_sch = (struct scheduler_dp_data *)data;
	struct task_dp_pdata *pdata = task->priv_data;
	unsigned int lock_key;

	lock_key = scheduler_dp_lock(task->core);

	if (task_is_active(task)) {
		scheduler_dp_unlock(lock_key);
		tr_dbg(&dp_tr, "DP task already active");
		return 0;
	}

	if (task->state != SOF_TASK_STATE_INIT &&
	    task->state != SOF_TASK_STATE_CANCEL &&
	    task->state != SOF_TASK_STATE_COMPLETED) {
		scheduler_dp_unlock(lock_key);
		return -EINVAL;
	}

	/* if there's no DP tasks scheduled yet, run ll tick source task */
	if (list_is_empty(&dp_sch->tasks))
		schedule_task(&dp_sch->ll_tick_src, 0, 0);

	/* add a task to DP scheduler list */
	task->state = SOF_TASK_STATE_QUEUED;
	list_item_prepend(&task->list, &dp_sch->tasks);

	pdata->mod->dp_startup_delay = true;
	scheduler_dp_unlock(lock_key);

	tr_dbg(&dp_tr, "DP task scheduled with period %u [us]", (uint32_t)period);
	return 0;
}

void scheduler_get_task_info_dp(struct scheduler_props *scheduler_props, uint32_t *data_off_size)
{
	unsigned int lock_key;

	scheduler_props->processing_domain = COMP_PROCESSING_DOMAIN_DP;
#if CONFIG_SOF_USERSPACE_LL
	struct scheduler_dp_data *dp_sch = scheduler_get_user_data(SOF_SCHEDULE_DP);
#else
	struct scheduler_dp_data *dp_sch = scheduler_get_data(SOF_SCHEDULE_DP);
#endif

	lock_key = scheduler_dp_lock(cpu_get_id());
	scheduler_get_task_info(scheduler_props, data_off_size,  &dp_sch->tasks);
	scheduler_dp_unlock(lock_key);
}
