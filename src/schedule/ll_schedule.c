// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>
//         Tomasz Lauda <tomasz.lauda@linux.intel.com>

#include <sof/atomic.h>
#include <sof/common.h>
#include <sof/drivers/interrupt.h>
#include <sof/drivers/timer.h>
#include <sof/lib/alloc.h>
#include <sof/lib/clk.h>
#include <sof/lib/cpu.h>
#include <sof/lib/memory.h>
#include <sof/lib/notifier.h>
#include <sof/lib/perf_cnt.h>
#include <sof/lib/uuid.h>
#include <sof/list.h>
#include <sof/platform.h>
#include <sof/schedule/ll_schedule.h>
#include <sof/schedule/ll_schedule_domain.h>
#include <sof/schedule/schedule.h>
#include <sof/schedule/task.h>
#include <sof/spinlock.h>
#include <ipc/topology.h>

#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* 4f9c3ec7-7b55-400c-86b3-502b4420e625 */
DECLARE_SOF_UUID("ll-schedule", ll_sched_uuid, 0x4f9c3ec7, 0x7b55, 0x400c,
		 0x86, 0xb3, 0x50, 0x2b, 0x44, 0x20, 0xe6, 0x25);

DECLARE_TR_CTX(ll_tr, SOF_UUID(ll_sched_uuid), LOG_LEVEL_INFO);

/* one instance of data allocated per core */
struct ll_schedule_data {
	struct list_item tasks;			/* list of ll tasks */
	atomic_t num_tasks;			/* number of ll tasks */
#if CONFIG_PERFORMANCE_COUNTERS
	struct perf_cnt_data pcd;
#endif
	struct ll_schedule_domain *domain;	/* scheduling domain */
};

static const struct scheduler_ops schedule_ll_ops;

#define perf_ll_sched_trace(pcd, ll_sched)			\
	tr_info(&ll_tr, "perf ll_work peak plat %u cpu %u",	\
		(uint32_t)((pcd)->plat_delta_peak),		\
		(uint32_t)((pcd)->cpu_delta_peak))

static bool schedule_ll_is_pending(struct ll_schedule_data *sch)
{
	struct ll_schedule_domain *domain = sch->domain;
	struct list_item *tlist;
	struct task *task;
	uint32_t pending_count = 0;
	struct comp_dev *sched_comp;

	spin_lock(&domain->lock);

	do {
		sched_comp = NULL;

		/* mark each valid task as pending */
		list_for_item(tlist, &sch->tasks) {
			task = container_of(tlist, struct task, list);

			if (domain_is_pending(domain, task, &sched_comp)) {
				task->state = SOF_TASK_STATE_PENDING;
				pending_count++;
			}
		}
	} while (sched_comp);

	spin_unlock(&domain->lock);

	return pending_count > 0;
}

static void schedule_ll_task_update_start(struct ll_schedule_data *sch,
					  struct task *task)
{
	struct ll_task_pdata *pdata = ll_sch_get_pdata(task);
	uint64_t next;

	next = sch->domain->ticks_per_ms * pdata->period / 1000;

	task->start += next;
}

static void schedule_ll_tasks_execute(struct ll_schedule_data *sch)
{
	struct ll_schedule_domain *domain = sch->domain;
	struct list_item *wlist;
	struct list_item *tlist;
	struct task *task;
	int cpu = cpu_get_id();
	int count;

	/* check each task in the list for pending */
	list_for_item_safe(wlist, tlist, &sch->tasks) {
		task = container_of(wlist, struct task, list);

		if (task->state != SOF_TASK_STATE_PENDING)
			continue;

		tr_dbg(&ll_tr, "task %p %pU being started...", task, task->uid);

		task->state = task_run(task);

		spin_lock(&domain->lock);

		/* do we need to reschedule this task */
		if (task->state == SOF_TASK_STATE_COMPLETED) {
			list_item_del(&task->list);
			atomic_sub(&domain->total_num_tasks, 1);

			/* don't enable irq, if no more tasks to do */
			count = atomic_sub(&sch->num_tasks, 1);
			if (count == 1)
				domain->registered[cpu] = false;
			tr_info(&ll_tr, "task complete %p %pU", task, task->uid);
			tr_info(&ll_tr, "num_tasks %d total_num_tasks %d",
				atomic_read(&sch->num_tasks),
				atomic_read(&domain->total_num_tasks));
		} else {
			/* update task's start time */
			schedule_ll_task_update_start(sch, task);
			tr_dbg(&ll_tr, "task %p uid %pU finished, next period ticks %u, domain->next_tick %u",
			       task, task->uid, (uint32_t)task->start,
			       (uint32_t)domain->next_tick);
		}


		spin_unlock(&domain->lock);
	}
}

static void schedule_ll_clients_enable(struct ll_schedule_data *sch)
{
	struct ll_schedule_domain *domain = sch->domain;
	int i;

	for (i = 0; i < CONFIG_CORE_COUNT; i++) {
		if (domain->registered[i] && !domain->enabled[i]) {
			atomic_add(&domain->num_clients, 1);
			domain_enable(domain, i);
			domain->enabled[i] = true;
		}
	}
}

static void schedule_ll_clients_reschedule(struct ll_schedule_data *sch)
{
	struct list_item *wlist;
	struct list_item *tlist;
	struct task *task;
	struct task *task_take_dbg = NULL;
	uint64_t next_tick = UINT64_MAX;

	/* rearm only if there is work to do */
	if (atomic_read(&sch->domain->total_num_tasks)) {
		/* traverse to set timer according to the earliest task */
		list_for_item_safe(wlist, tlist, &sch->tasks) {
			task = container_of(wlist, struct task, list);

			/* update to use the earlier tick */
			if (task->start < next_tick) {
				next_tick = task->start;
				task_take_dbg = task;
			}
		}

		tr_dbg(&ll_tr,
		       "schedule_ll_clients_reschedule next_tick %u task_take %p",
		       (unsigned int)next_tick, task_take_dbg);
		domain_set(sch->domain, next_tick);
	}

}

static void schedule_ll_tasks_run(void *data)
{
	struct ll_schedule_data *sch = data;
	struct ll_schedule_domain *domain = sch->domain;
#ifndef __ZEPHYR__
	uint32_t num_clients;
#endif
	uint32_t flags;

	tr_dbg(&ll_tr, "timer interrupt on core %d, at %u, previous next_tick %u",
	       cpu_get_id(),
	       (unsigned int)platform_timer_get_atomic(timer_get()),
	       (unsigned int)domain->next_tick);

	domain_disable(domain, cpu_get_id());

	irq_local_disable(flags);

	spin_lock(&domain->lock);

	domain->enabled[cpu_get_id()] = false;

#ifndef __ZEPHYR__
	/* clear domain only if all clients are done */
	/* TODO: no need for atomic operations,
	 * already protected by spin_lock
	 */
	num_clients = atomic_sub(&domain->num_clients, 1) - 1;
	if (!num_clients)
		domain_clear(domain);
#endif


	spin_unlock(&domain->lock);

	perf_cnt_init(&sch->pcd);

	notifier_event(sch, NOTIFIER_ID_LL_PRE_RUN,
		       NOTIFIER_TARGET_CORE_LOCAL, NULL, 0);

	/* run tasks if there are any pending */
	if (schedule_ll_is_pending(sch))
		schedule_ll_tasks_execute(sch);

	notifier_event(sch, NOTIFIER_ID_LL_POST_RUN,
		       NOTIFIER_TARGET_CORE_LOCAL, NULL, 0);

	perf_cnt_stamp(&sch->pcd, perf_ll_sched_trace, sch);

	spin_lock(&domain->lock);

	schedule_ll_clients_reschedule(sch);

	/* re-enable domain only if all clients are done */
#ifndef __ZEPHYR__
	/* Under Zephyr each client has to be enabled separately */
	if (!num_clients)
#endif
		schedule_ll_clients_enable(sch);

	spin_unlock(&domain->lock);

	irq_local_enable(flags);
}

static int schedule_ll_domain_set(struct ll_schedule_data *sch,
				  struct task *task, uint64_t start,
				  uint64_t period)
{
	struct ll_schedule_domain *domain = sch->domain;
	int core = cpu_get_id();
	unsigned int total;
	uint64_t task_start_us;
	uint64_t task_start_ticks;
	uint64_t task_start;
	uint64_t offset;
	bool registered;
	int ret;

	ret = domain_register(domain, period, task, &schedule_ll_tasks_run,
			      sch);
	if (ret < 0) {
		tr_err(&ll_tr, "schedule_ll_domain_set: cannot register domain %d",
		       ret);
		return ret;
	}

	spin_lock(&domain->lock);

	tr_dbg(&ll_tr, "task->start %u next_tick %u",
	       (unsigned int)task->start,
	       (unsigned int)domain->next_tick);

	task_start_us = period ? period : start;
	task_start_ticks = domain->ticks_per_ms * task_start_us / 1000;
	task_start = task_start_ticks + platform_timer_get_atomic(timer_get());

	if (domain->next_tick == UINT64_MAX) {
		/* first task, set domain */
		domain_set(domain, task_start);
		task->start = domain->next_tick;
	} else if (!period) {
		/* one shot task, set domain if it is earlier */
		task->start = task_start;
		if (task->start < domain->next_tick)
			domain_set(domain, task_start);
	} else if (task_start < domain->next_tick) {
		/* earlier periodic task, try to make it cadence-aligned with the existed task */
		offset = (domain->next_tick - task_start) %
			 (domain->ticks_per_ms * period / 1000);
		task_start = task_start - task_start_ticks + offset;
		domain_set(domain, task_start);
		task->start = domain->next_tick;
	} else {
		/* later periodic task, simplify and cover it by the coming interrupt */
#ifdef __ZEPHYR__
		/*
		 * Under Zephyr domain_set() must be called for each client, if the next
		 * tick is already set by a different client in the same domain, this
		 * will just set the new client to the same tick time
		 */
		domain_set(domain, domain->next_tick);
#endif
		task->start = domain->next_tick;
	}

	registered = domain->registered[core];
	total = atomic_add(&sch->num_tasks, 1);
	if (total == 0)
		domain->registered[core] = true;

	total = atomic_add(&domain->total_num_tasks, 1);
	if (total == 0 || !registered) {
		/* First task on core: count and enable it */
		atomic_add(&domain->num_clients, 1);
		domain_enable(domain, core);
		domain->enabled[core] = true;
	}

	tr_info(&ll_tr, "new added task->start %u at %u",
		(unsigned int)task->start,
		(unsigned int)platform_timer_get_atomic(timer_get()));
	tr_info(&ll_tr, "num_tasks %d total_num_tasks %d",
		atomic_read(&sch->num_tasks),
		atomic_read(&domain->total_num_tasks));


	spin_unlock(&domain->lock);

	return 0;
}

static void schedule_ll_domain_clear(struct ll_schedule_data *sch,
				     struct task *task)
{
	struct ll_schedule_domain *domain = sch->domain;
	int count;

	spin_lock(&domain->lock);

	atomic_sub(&domain->total_num_tasks, 1);

	count = atomic_sub(&sch->num_tasks, 1);
	if (count == 1) {
		domain->registered[cpu_get_id()] = false;

		if (atomic_read(&domain->num_clients)) {
			count = atomic_sub(&domain->num_clients, 1);
			if (count == 1)
				domain_clear(domain);
		}
	}

	tr_info(&ll_tr, "num_tasks %d total_num_tasks %d",
		atomic_read(&sch->num_tasks),
		atomic_read(&domain->total_num_tasks));


	spin_unlock(&domain->lock);

	domain_unregister(domain, task, atomic_read(&sch->num_tasks));
}

static void schedule_ll_task_insert(struct task *task, struct list_item *tasks)
{
	struct list_item *tlist;
	struct task *curr_task;

	/* tasks are added into the list from highest to lowest priority
	 * and tasks with the same priority should be served on
	 * a first-come-first-serve basis
	 */
	list_for_item(tlist, tasks) {
		curr_task = container_of(tlist, struct task, list);
		if (task->priority < curr_task->priority) {
			list_item_append(&task->list, &curr_task->list);
			return;
		}
	}

	/* if task has not been added, means that it has the lowest
	 * priority and should be added at the end of the list
	 */
	list_item_append(&task->list, tasks);
}

static int schedule_ll_task(void *data, struct task *task, uint64_t start,
			    uint64_t period)
{
	struct ll_schedule_data *sch = data;
	struct ll_task_pdata *pdata;
	struct ll_task_pdata *reg_pdata;
	struct list_item *tlist;
	struct task *curr_task;
	struct task *registrable_task = NULL;
	struct pipeline_task *pipe_task;
	uint32_t flags;
	int ret = 0;

	irq_local_disable(flags);

	/* check if task is already scheduled */
	list_for_item(tlist, &sch->tasks) {
		curr_task = container_of(tlist, struct task, list);

		/* keep original start */
		if (curr_task == task)
			goto out;
	}

	pdata = ll_sch_get_pdata(task);

	tr_info(&ll_tr, "task add %p %pU", task, task->uid);
	if (start <= UINT_MAX && period <= UINT_MAX)
		tr_info(&ll_tr, "task params pri %d flags %d start %u period %u",
			task->priority, task->flags,
			(unsigned int)start, (unsigned int)period);
	else
		tr_info(&ll_tr, "task params pri %d flags %d start or period > %u",
			task->priority, task->flags, UINT_MAX);

	pdata->period = period;

	/* for full synchronous domain, calculate ratio and initialize skip_cnt for task */
	if (sch->domain->full_sync) {
		pdata->ratio = 1;
		pdata->skip_cnt = (uint16_t)SOF_TASK_SKIP_COUNT;

		/* get the registrable task */
		list_for_item(tlist, &sch->tasks) {
			curr_task = container_of(tlist, struct task, list);
			pipe_task = pipeline_task_get(curr_task);

			/* registrable task found */
			if (pipe_task->registrable) {
				registrable_task = curr_task;
				break;
			}
		}

		/* we found a registrable task */
		if (registrable_task) {
			reg_pdata = ll_sch_get_pdata(registrable_task);

			/* update ratio for all tasks */
			list_for_item(tlist, &sch->tasks) {
				curr_task = container_of(tlist, struct task, list);
				pdata = ll_sch_get_pdata(curr_task);

				/* the assumption is that the registrable
				 * task has the smallest period
				 */
				if (pdata->period >= reg_pdata->period) {
					pdata->ratio = period / reg_pdata->period;
				} else {
					tr_err(&ll_tr,
					       "schedule_ll_task(): registrable task has a period longer than current task");
					ret = -EINVAL;
					goto out;
				}
			}
		}
	}

	/* insert task into the list */
	schedule_ll_task_insert(task, &sch->tasks);

	/* set schedule domain */
	ret = schedule_ll_domain_set(sch, task, start, period);
	if (ret < 0) {
		list_item_del(&task->list);
		goto out;
	}


out:
	irq_local_enable(flags);

	return ret;
}

int schedule_task_init_ll(struct task *task,
			  const struct sof_uuid_entry *uid, uint16_t type,
			  uint16_t priority, enum task_state (*run)(void *data),
			  void *data, uint16_t core, uint32_t flags)
{
	struct ll_task_pdata *ll_pdata;
	int ret = 0;

	ret = schedule_task_init(task, uid, type, priority, run, data, core,
				 flags);
	if (ret < 0)
		return ret;

	if (ll_sch_get_pdata(task))
		return -EEXIST;

	ll_pdata = rzalloc(SOF_MEM_ZONE_SYS_RUNTIME, 0, SOF_MEM_CAPS_RAM,
			   sizeof(*ll_pdata));

	if (!ll_pdata) {
		tr_err(&ll_tr, "schedule_task_init_ll(): alloc failed");
		return -ENOMEM;
	}

	ll_sch_set_pdata(task, ll_pdata);

	return 0;
}

static int schedule_ll_task_free(void *data, struct task *task)
{
	struct ll_task_pdata *ll_pdata;
	uint32_t flags;

	irq_local_disable(flags);

	/* release the resources */
	task->state = SOF_TASK_STATE_FREE;
	ll_pdata = ll_sch_get_pdata(task);
	rfree(ll_pdata);
	ll_sch_set_pdata(task, NULL);

	irq_local_enable(flags);

	return 0;
}

static int schedule_ll_task_cancel(void *data, struct task *task)
{
	struct ll_schedule_data *sch = data;
	struct list_item *tlist;
	struct task *curr_task;
	uint32_t flags;

	irq_local_disable(flags);

	tr_info(&ll_tr, "task cancel %p %pU", task, task->uid);

	/* check to see if we are scheduled */
	list_for_item(tlist, &sch->tasks) {
		curr_task = container_of(tlist, struct task, list);

		/* found it */
		if (curr_task == task) {
			schedule_ll_domain_clear(sch, task);
			break;
		}
	}

	/* remove work from list */
	task->state = SOF_TASK_STATE_CANCEL;
	list_item_del(&task->list);

	irq_local_enable(flags);

	return 0;
}

static int reschedule_ll_task(void *data, struct task *task, uint64_t start)
{
	struct ll_schedule_data *sch = data;
	struct list_item *tlist;
	struct task *curr_task;
	uint32_t flags;
	uint64_t time;

	time = sch->domain->ticks_per_ms * start / 1000;

	time += platform_timer_get_atomic(timer_get());

	irq_local_disable(flags);

	/* check to see if we are already scheduled */
	list_for_item(tlist, &sch->tasks) {
		curr_task = container_of(tlist, struct task, list);
		/* found it */
		if (curr_task == task) {
			/* set start time */
			task->start = time;
			goto out;
		}
	}

	tr_err(&ll_tr, "reschedule_ll_task(): task not found");

out:

	irq_local_enable(flags);

	return 0;
}

static void scheduler_free_ll(void *data)
{
	struct ll_schedule_data *sch = data;
	uint32_t flags;

	irq_local_disable(flags);

	notifier_unregister(sch, NULL,
			    NOTIFIER_CLK_CHANGE_ID(sch->domain->clk));

	list_item_del(&sch->tasks);


	irq_local_enable(flags);
}

static void ll_scheduler_recalculate_tasks(struct ll_schedule_data *sch,
					   struct clock_notify_data *clk_data)
{
	uint64_t current = platform_timer_get_atomic(timer_get());
	struct list_item *tlist;
	struct task *task;
	uint64_t delta_ms;

	list_for_item(tlist, &sch->tasks) {
		task = container_of(tlist, struct task, list);
		delta_ms = (task->start - current) /
			clk_data->old_ticks_per_msec;

		task->start = delta_ms ?
			current + sch->domain->ticks_per_ms * delta_ms :
			current + (sch->domain->ticks_per_ms >> 3);
	}
}

static void ll_scheduler_notify(void *arg, enum notify_id type, void *data)
{
	struct ll_schedule_data *sch = arg;
	struct clock_notify_data *clk_data = data;
	uint32_t flags;

	irq_local_disable(flags);

	/* we need to recalculate tasks when clock frequency changes */
	if (clk_data->message == CLOCK_NOTIFY_POST) {
		sch->domain->ticks_per_ms = clock_ms_to_ticks(sch->domain->clk,
							      1);
		ll_scheduler_recalculate_tasks(sch, clk_data);
	}

	irq_local_enable(flags);
}

int scheduler_init_ll(struct ll_schedule_domain *domain)
{
	struct ll_schedule_data *sch;

	/* initialize scheduler private data */
	sch = rzalloc(SOF_MEM_ZONE_SYS, 0, SOF_MEM_CAPS_RAM, sizeof(*sch));
	list_init(&sch->tasks);
	atomic_init(&sch->num_tasks, 0);
	sch->domain = domain;

	/* notification of clock changes */
	notifier_register(sch, NULL, NOTIFIER_CLK_CHANGE_ID(domain->clk),
			  ll_scheduler_notify, 0);

	scheduler_init(domain->type, &schedule_ll_ops, sch);


	return 0;
}

static const struct scheduler_ops schedule_ll_ops = {
	.schedule_task		= schedule_ll_task,
	.schedule_task_free	= schedule_ll_task_free,
	.schedule_task_cancel	= schedule_ll_task_cancel,
	.reschedule_task	= reschedule_ll_task,
	.scheduler_free		= scheduler_free_ll,
	.schedule_task_running	= NULL,
	.schedule_task_complete	= NULL,
};
