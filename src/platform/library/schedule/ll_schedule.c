// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019-2026 Intel Corporation. All rights reserved.
//
// Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>

#include <sof/audio/component.h>
#include <rtos/task.h>
#include <sof/schedule/schedule.h>
#include <sof/schedule/dp_schedule.h>
#include <platform/lib/ll_schedule.h>
#include <sof/schedule/ll_schedule_domain.h>
#include <platform/lib/telemetry_sched.h>
#include <sof/lib/cpu.h>
#include <rtos/wait.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

/* Single-threaded cooperative LL scheduler for Xtensa simulator (xt-run) */

SOF_DEFINE_REG_UUID(ll_sched_lib);

DECLARE_TR_CTX(ll_tr, SOF_UUID(ll_sched_lib_uuid), LOG_LEVEL_INFO);

static int host_current_core_id;

/* list of all tasks */
static struct list_item sched_list;

int arch_cpu_get_id(void)
{
	return host_current_core_id;
}

void host_cpu_set_id(int id)
{
	host_current_core_id = id;
}

void schedule_ll_run_tasks(void)
{
	struct list_item *tlist, *tlist_;
	struct task *task;
	enum task_state state;
	uint64_t core_cycles[CONFIG_CORE_COUNT] = { 0 };
	uint32_t core_tasks[CONFIG_CORE_COUNT] = { 0 };
	int c;

	/* list empty then return */
	if (list_is_empty(&sched_list))
		fprintf(stdout, "LL scheduler thread exit - list empty\n");
	else
		host_telem_ll_tick_begin();

	/* iterate through the task list */
	list_for_item_safe(tlist, tlist_, &sched_list) {
		task = container_of(tlist, struct task, list);

		/* only run queued tasks */
		if (task->state == SOF_TASK_STATE_QUEUED) {
			uint64_t t0, t1;
			int core = (task->core < CONFIG_CORE_COUNT) ? task->core : 0;

			task->state = SOF_TASK_STATE_RUNNING;
			host_cpu_set_id(task->core);

			t0 = host_telem_timestamp_cycles();
			state = task_run(task);
			t1 = host_telem_timestamp_cycles();

			core_tasks[core]++;
			core_cycles[core] += (t1 > t0) ? (t1 - t0) : 1;

			if (state == SOF_TASK_STATE_COMPLETED) {
				task->state = SOF_TASK_STATE_COMPLETED;
				list_item_del(&task->list);
			} else if (task->state == SOF_TASK_STATE_RUNNING) {
				task->state = SOF_TASK_STATE_QUEUED;
			}
		}
	}

	for (c = 0; c < CONFIG_CORE_COUNT; c++) {
		if (core_tasks[c] > 0)
			host_telem_core_ll_tick(c, core_tasks[c], core_cycles[c]);
	}

	host_cpu_set_id(0);

#if CONFIG_ZEPHYR_DP_SCHEDULER
	scheduler_dp_ll_tick();
#endif
}

/* schedule new LL task */
static int schedule_ll_task(void *data, struct task *task, uint64_t start,
			    uint64_t period)
{
	if (task->state == SOF_TASK_STATE_QUEUED ||
	    task->state == SOF_TASK_STATE_RUNNING)
		return 0;

	/* add task to list */
	list_item_prepend(&task->list, &sched_list);
	task->state = SOF_TASK_STATE_QUEUED;
	task->start = 0;

	return 0;
}

#if CONFIG_SOF_BOOT_TEST_STANDALONE || CONFIG_LIBRARY
static void ll_scheduler_free(void *data, uint32_t flags)
{
	free(data);
}
#endif

/* TODO: scheduler free and cancel APIs can merge as part of Zephyr */
static int schedule_ll_task_cancel(void *data, struct task *task)
{
	if (task->state == SOF_TASK_STATE_QUEUED ||
	    task->state == SOF_TASK_STATE_RUNNING)
		list_item_del(&task->list);

	/* delete task */
	task->state = SOF_TASK_STATE_CANCEL;

	return 0;
}

/* TODO: scheduler free and cancel APIs can merge as part of Zephyr */
static int schedule_ll_task_free(void *data, struct task *task)
{
	if (task->state == SOF_TASK_STATE_QUEUED ||
	    task->state == SOF_TASK_STATE_RUNNING)
		list_item_del(&task->list);

	task->state = SOF_TASK_STATE_FREE;

	return 0;
}

static struct scheduler_ops schedule_ll_ops = {
	.schedule_task		= schedule_ll_task,
	.schedule_task_running	= NULL,
	.reschedule_task	= NULL,
	.schedule_task_cancel	= schedule_ll_task_cancel,
	.schedule_task_free	= schedule_ll_task_free,
#if CONFIG_SOF_BOOT_TEST_STANDALONE || CONFIG_LIBRARY
	.scheduler_free		= ll_scheduler_free,
#endif
};

int schedule_task_init_ll(struct task *task,
			  const struct sof_uuid_entry *uid, uint16_t type,
			  int16_t priority, enum task_state (*run)(void *data),
			  void *data, uint16_t core, uint32_t flags)
{
	return schedule_task_init(task, uid, SOF_SCHEDULE_LL_TIMER, priority, run,
				  data, core, flags);
}

/* initialize scheduler */
int scheduler_init_ll(struct ll_schedule_domain *domain)
{
	tr_info(&ll_tr, "ll_scheduler_init()");

	list_init(&sched_list);
	scheduler_init(SOF_SCHEDULE_LL_TIMER, &schedule_ll_ops, NULL);

	return 0;
}
