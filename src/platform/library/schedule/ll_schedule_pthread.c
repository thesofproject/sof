// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019-2026 Intel Corporation. All rights reserved.
//
// Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>

#define _GNU_SOURCE

#include <sof/audio/component.h>
#include <rtos/task.h>
#include <sof/schedule/schedule.h>
#include <sof/schedule/dp_schedule.h>
#include <platform/lib/ll_schedule.h>
#include <sof/schedule/ll_schedule_domain.h>
#include <platform/lib/telemetry_sched.h>
#include <sof/lib/cpu.h>
#include <rtos/wait.h>
#include <pthread.h>
#include <sched.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>

#include "host_schedule.h"

/* Host multi-threaded LL scheduler */

SOF_DEFINE_REG_UUID(ll_sched_lib);

DECLARE_TR_CTX(ll_tr, SOF_UUID(ll_sched_lib_uuid), LOG_LEVEL_INFO);

struct ll_core_worker {
	int core_id;
	int host_cpu;
	struct list_item sched_list;
	pthread_t thread_id;
	bool thread_created;
	pthread_cond_t wake_cond;
	bool run_tick;
	bool stop;
};

static __thread int host_current_core_id;
static cpu_set_t host_allowed_cpuset;
static bool host_cpuset_initialized;

static struct ll_core_worker ll_workers[CONFIG_CORE_COUNT];
static pthread_mutex_t ll_sched_lock;
static pthread_cond_t ll_tick_done_cond;
static uint32_t ll_running_cores;

int arch_cpu_get_id(void)
{
	return host_current_core_id;
}

void host_cpu_set_id(int id)
{
	host_current_core_id = id;
}

static void init_host_cpuset(void)
{
	if (host_cpuset_initialized)
		return;
	CPU_ZERO(&host_allowed_cpuset);
	if (sched_getaffinity(0, sizeof(host_allowed_cpuset), &host_allowed_cpuset) == 0 &&
	    CPU_COUNT(&host_allowed_cpuset) > 0)
		host_cpuset_initialized = true;
}

int host_pin_thread_to_dsp_core(pthread_t thread, int dsp_core)
{
	cpu_set_t cpuset;
	int num_cpus, target_idx, count = 0;
	int host_cpu = -1;
	int i;

	init_host_cpuset();
	if (!host_cpuset_initialized)
		return -EINVAL;

	num_cpus = CPU_COUNT(&host_allowed_cpuset);
	if (num_cpus <= 0)
		return -EINVAL;

	target_idx = ((dsp_core >= 0) ? dsp_core : 0) % num_cpus;
	for (i = 0; i < CPU_SETSIZE; i++) {
		if (CPU_ISSET(i, &host_allowed_cpuset)) {
			if (count == target_idx) {
				host_cpu = i;
				break;
			}
			count++;
		}
	}

	if (host_cpu < 0)
		return -EINVAL;

	CPU_ZERO(&cpuset);
	CPU_SET(host_cpu, &cpuset);
	if (pthread_setaffinity_np(thread, sizeof(cpuset), &cpuset) != 0)
		return -errno;

	return host_cpu;
}

static void *ll_core_thread_fn(void *arg)
{
	struct ll_core_worker *worker = arg;
	struct list_item *tlist, *tlist_;
	struct task *task;
	enum task_state state;

	host_cpu_set_id(worker->core_id);

	pthread_mutex_lock(&ll_sched_lock);

	while (!worker->stop) {
		while (!worker->run_tick && !worker->stop)
			pthread_cond_wait(&worker->wake_cond, &ll_sched_lock);

		if (worker->stop)
			break;

		worker->run_tick = false;
		pthread_mutex_unlock(&ll_sched_lock);

		uint64_t tick_start = host_telem_timestamp_cycles();
		uint32_t tasks_run = 0;

		/* iterate through this core's LL task list */
		list_for_item_safe(tlist, tlist_, &worker->sched_list) {
			task = container_of(tlist, struct task, list);

			/* only run queued tasks */
			if (task->state == SOF_TASK_STATE_QUEUED) {
				task->state = SOF_TASK_STATE_RUNNING;

				state = task_run(task);
				tasks_run++;

				if (state == SOF_TASK_STATE_COMPLETED) {
					task->state = SOF_TASK_STATE_COMPLETED;
					list_item_del(&task->list);
				} else if (task->state == SOF_TASK_STATE_RUNNING) {
					task->state = SOF_TASK_STATE_QUEUED;
				}
			}
		}

		if (tasks_run > 0)
			host_telem_core_ll_tick(worker->core_id, tasks_run,
						host_telem_timestamp_cycles() - tick_start);

		pthread_mutex_lock(&ll_sched_lock);
		if (ll_running_cores > 0) {
			ll_running_cores--;
			if (ll_running_cores == 0)
				pthread_cond_broadcast(&ll_tick_done_cond);
		}
	}

	pthread_mutex_unlock(&ll_sched_lock);
	return NULL;
}

void schedule_ll_run_tasks(void)
{
	bool any_active = false;
	int c;

	pthread_mutex_lock(&ll_sched_lock);

	for (c = 0; c < CONFIG_CORE_COUNT; c++) {
		if (!list_is_empty(&ll_workers[c].sched_list)) {
			if (!any_active) {
				host_telem_ll_tick_begin();
				any_active = true;
			}
			ll_workers[c].run_tick = true;
			ll_running_cores++;
			pthread_cond_signal(&ll_workers[c].wake_cond);
		}
	}

	if (!any_active)
		fprintf(stdout, "LL scheduler thread exit - list empty\n");

	while (ll_running_cores > 0)
		pthread_cond_wait(&ll_tick_done_cond, &ll_sched_lock);

	pthread_mutex_unlock(&ll_sched_lock);

#if CONFIG_ZEPHYR_DP_SCHEDULER
	scheduler_dp_ll_tick();
#endif
}

/* schedule new LL task */
static int schedule_ll_task(void *data, struct task *task, uint64_t start,
			    uint64_t period)
{
	int core = (task->core < CONFIG_CORE_COUNT) ? task->core : 0;
	struct ll_core_worker *worker = &ll_workers[core];
	int ret;

	pthread_mutex_lock(&ll_sched_lock);

	if (task->state == SOF_TASK_STATE_QUEUED ||
	    task->state == SOF_TASK_STATE_RUNNING) {
		pthread_mutex_unlock(&ll_sched_lock);
		return 0;
	}

	if (!worker->thread_created) {
		worker->stop = false;
		worker->run_tick = false;
		ret = pthread_create(&worker->thread_id, NULL, ll_core_thread_fn, worker);
		if (ret != 0) {
			pthread_mutex_unlock(&ll_sched_lock);
			return -ret;
		}
		worker->host_cpu = host_pin_thread_to_dsp_core(worker->thread_id, core);
		worker->thread_created = true;
		tr_info(&ll_tr, "LL worker thread created for DSP core %d (host CPU %d)",
			core, worker->host_cpu);
	} else if (list_is_empty(&worker->sched_list)) {
		tr_info(&ll_tr, "LL worker thread created for DSP core %d (host CPU %d)",
			core, worker->host_cpu);
	}

	/* add task to core's list */
	list_item_prepend(&task->list, &worker->sched_list);
	task->state = SOF_TASK_STATE_QUEUED;
	task->start = 0;

	pthread_mutex_unlock(&ll_sched_lock);

	return 0;
}

#if CONFIG_SOF_BOOT_TEST_STANDALONE || CONFIG_LIBRARY
static void ll_scheduler_free(void *data, uint32_t flags)
{
	int c;

	for (c = 0; c < CONFIG_CORE_COUNT; c++) {
		pthread_t tid = 0;
		bool join = false;

		pthread_mutex_lock(&ll_sched_lock);
		if (ll_workers[c].thread_created) {
			ll_workers[c].stop = true;
			pthread_cond_signal(&ll_workers[c].wake_cond);
			tid = ll_workers[c].thread_id;
			ll_workers[c].thread_created = false;
			join = true;
		}
		pthread_mutex_unlock(&ll_sched_lock);

		if (join)
			pthread_join(tid, NULL);

		pthread_cond_destroy(&ll_workers[c].wake_cond);
	}

	pthread_mutex_destroy(&ll_sched_lock);
	pthread_cond_destroy(&ll_tick_done_cond);
	free(data);
}
#endif

/* TODO: scheduler free and cancel APIs can merge as part of Zephyr */
static int schedule_ll_task_cancel(void *data, struct task *task)
{
	pthread_mutex_lock(&ll_sched_lock);

	if (task->state == SOF_TASK_STATE_QUEUED ||
	    task->state == SOF_TASK_STATE_RUNNING)
		list_item_del(&task->list);

	/* delete task */
	task->state = SOF_TASK_STATE_CANCEL;

	pthread_mutex_unlock(&ll_sched_lock);

	return 0;
}

/* TODO: scheduler free and cancel APIs can merge as part of Zephyr */
static int schedule_ll_task_free(void *data, struct task *task)
{
	pthread_mutex_lock(&ll_sched_lock);

	if (task->state == SOF_TASK_STATE_QUEUED ||
	    task->state == SOF_TASK_STATE_RUNNING)
		list_item_del(&task->list);

	task->state = SOF_TASK_STATE_FREE;

	pthread_mutex_unlock(&ll_sched_lock);

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
	int c;

	tr_info(&ll_tr, "ll_scheduler_init()");

	pthread_mutex_init(&ll_sched_lock, NULL);
	pthread_cond_init(&ll_tick_done_cond, NULL);

	for (c = 0; c < CONFIG_CORE_COUNT; c++) {
		ll_workers[c].core_id = c;
		ll_workers[c].host_cpu = -1;
		ll_workers[c].thread_created = false;
		ll_workers[c].run_tick = false;
		ll_workers[c].stop = false;
		list_init(&ll_workers[c].sched_list);
		pthread_cond_init(&ll_workers[c].wake_cond, NULL);
	}

	scheduler_init(SOF_SCHEDULE_LL_TIMER, &schedule_ll_ops, NULL);

	return 0;
}
