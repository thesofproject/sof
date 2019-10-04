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
#include <sof/lib/cache.h>
#include <sof/lib/clk.h>
#include <sof/lib/cpu.h>
#include <sof/lib/notifier.h>
#include <sof/list.h>
#include <sof/platform.h>
#include <sof/schedule/ll_schedule.h>
#include <sof/schedule/ll_schedule_domain.h>
#include <sof/schedule/schedule.h>
#include <sof/schedule/task.h>
#include <sof/spinlock.h>
#include <ipc/topology.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct ll_schedule_data {
	struct list_item tasks;			/* list of ll tasks */
	atomic_t num_tasks;			/* number of ll tasks */
	struct notifier notifier;		/* notify frequency changes */
	struct ll_schedule_domain *domain;	/* scheduling domain */
};

struct scheduler_ops schedule_ll_ops;

static bool schedule_ll_is_pending(struct ll_schedule_data *sch)
{
	struct list_item *tlist;
	struct task *task;
	uint32_t pending_count = 0;

	/* mark each valid task as pending */
	list_for_item(tlist, &sch->tasks) {
		task = container_of(tlist, struct task, list);

		if (domain_is_pending(sch->domain, task)) {
			task->state = SOF_TASK_STATE_PENDING;
			pending_count++;
		}
	}

	return pending_count > 0;
}

static void schedule_ll_task_update_start(struct ll_schedule_data *sch,
					  struct task *task)
{
	struct ll_task_pdata *pdata = ll_sch_get_pdata(task);
	uint64_t next;

	next = sch->domain->ticks_per_ms * pdata->period / 1000;

	if (task->flags & SOF_SCHEDULE_FLAG_SYNC)
		task->start += next;
	else
		task->start = next + sch->domain->last_tick;
}

static void schedule_ll_tasks_execute(struct ll_schedule_data *sch)
{
	struct list_item *wlist;
	struct list_item *tlist;
	struct task *task;
	int cpu = cpu_get_id();

	/* check each task in the list for pending */
	list_for_item_safe(wlist, tlist, &sch->tasks) {
		task = container_of(wlist, struct task, list);

		/* run task if its pending and remove from the list */
		if (task->state == SOF_TASK_STATE_PENDING) {
			task->state = task->run(task->data);

			/* do we need to reschedule this task */
			if (task->state == SOF_TASK_STATE_COMPLETED) {
				list_item_del(&task->list);
				atomic_sub(&sch->domain->total_num_tasks, 1);

				/* don't enable irq, if no more tasks to do */
				if (!atomic_sub(&sch->num_tasks, 1))
					sch->domain->registered[cpu] = false;
			} else {
				/* update task's start time */
				schedule_ll_task_update_start(sch, task);
			}
		}
	}
}

static void schedule_ll_clients_enable(struct ll_schedule_data *sch)
{
	int i;

	for (i = 0; i < PLATFORM_CORE_COUNT; i++) {
		if (sch->domain->registered[i]) {
			atomic_add(&sch->domain->num_clients, 1);
			domain_enable(sch->domain, i);
		}
	}
}

static void schedule_ll_clients_reschedule(struct ll_schedule_data *sch)
{
	/* rearm only if there is work to do */
	if (atomic_read(&sch->domain->total_num_tasks)) {
		domain_set(sch->domain, sch->domain->last_tick);
		schedule_ll_clients_enable(sch);
	}
}

static void schedule_ll_tasks_run(void *data)
{
	struct ll_schedule_data *sch = data;
	uint32_t num_clients;
	uint32_t flags;

	domain_disable(sch->domain, cpu_get_id());

	irq_local_disable(flags);

	spin_lock(sch->domain->lock);

	/* clear domain only if all clients are done */
	num_clients = atomic_sub(&sch->domain->num_clients, 1);
	if (!num_clients)
		domain_clear(sch->domain);

	spin_unlock(sch->domain->lock);

	/* run tasks if there are any pending */
	if (schedule_ll_is_pending(sch))
		schedule_ll_tasks_execute(sch);

	spin_lock(sch->domain->lock);

	/* reschedule only if all clients are done */
	if (!num_clients)
		schedule_ll_clients_reschedule(sch);

	spin_unlock(sch->domain->lock);

	irq_local_enable(flags);
}

static int schedule_ll_domain_set(struct ll_schedule_data *sch,
				  struct task *task, uint64_t period)
{
	int core = cpu_get_id();
	int ret;

	ret = domain_register(sch->domain, period, task, &schedule_ll_tasks_run,
			      sch);
	if (ret < 0) {
		trace_ll_error("schedule_ll_domain_set() error: cannot "
			       "register domain %d", ret);
		return ret;
	}

	spin_lock(sch->domain->lock);

	if (atomic_add(&sch->num_tasks, 1) == 1)
		sch->domain->registered[core] = true;

	if (atomic_add(&sch->domain->total_num_tasks, 1) == 1) {
		domain_set(sch->domain, platform_timer_get(platform_timer));
		atomic_add(&sch->domain->num_clients, 1);
		domain_enable(sch->domain, core);
	}

	spin_unlock(sch->domain->lock);

	return 0;
}

static void schedule_ll_domain_clear(struct ll_schedule_data *sch)
{
	spin_lock(sch->domain->lock);

	if (!atomic_sub(&sch->domain->total_num_tasks, 1)) {
		domain_clear(sch->domain);
		sch->domain->last_tick = 0;
	}

	if (!atomic_sub(&sch->num_tasks, 1)) {
		sch->domain->registered[cpu_get_id()] = false;

		/* reschedule if there is no more clients waiting */
		if (!atomic_sub(&sch->domain->num_clients, 1)) {
			domain_clear(sch->domain);
			schedule_ll_clients_reschedule(sch);
		}
	}

	spin_unlock(sch->domain->lock);

	domain_unregister(sch->domain, atomic_read(&sch->num_tasks));
}

static void schedule_ll_task_insert(struct task *task, struct list_item *tasks)
{
	struct list_item *tlist;
	struct task *curr_task;

	/* tasks are added into the list in order */
	list_for_item(tlist, tasks) {
		curr_task = container_of(tlist, struct task, list);
		if (task->priority <= curr_task->priority) {
			list_item_append(&task->list, &curr_task->list);
			return;
		}
	}

	/* if task has not been added, means that it has the lowest
	 * priority and should be added at the end of the list
	 */
	list_item_append(&task->list, tasks);
}

static void schedule_ll_task(void *data, struct task *task, uint64_t start,
			     uint64_t period)
{
	struct ll_schedule_data *sch = data;
	struct ll_task_pdata *pdata;
	struct list_item *tlist;
	struct task *curr_task;
	uint32_t flags;

	irq_local_disable(flags);

	/* check if task is already scheduled */
	list_for_item(tlist, &sch->tasks) {
		curr_task = container_of(tlist, struct task, list);

		/* keep original start */
		if (curr_task == task)
			goto out;
	}

	pdata = ll_sch_get_pdata(task);

	/* invalidate if slave core */
	if (cpu_is_slave(task->core))
		dcache_invalidate_region(pdata, sizeof(*pdata));

	pdata->period = period;

	/* insert task into the list */
	schedule_ll_task_insert(task, &sch->tasks);

	/* set schedule domain */
	if (schedule_ll_domain_set(sch, task, period) < 0) {
		list_item_del(&task->list);
		goto out;
	}

	task->start = sch->domain->ticks_per_ms * start / 1000;

	if (task->flags & SOF_SCHEDULE_FLAG_SYNC)
		task->start += platform_timer_get(platform_timer);
	else
		task->start += sch->domain->last_tick;

out:
	irq_local_enable(flags);
}

static int schedule_ll_task_init(void *data, struct task *task)
{
	struct ll_task_pdata *ll_pdata;

	if (ll_sch_get_pdata(task))
		return -EEXIST;

	ll_pdata = rzalloc(RZONE_SYS_RUNTIME, SOF_MEM_CAPS_RAM,
			   sizeof(*ll_pdata));

	if (!ll_pdata) {
		trace_ll_error("schedule_ll_task_init() error: alloc failed");
		return -ENOMEM;
	}

	/* flush for slave core */
	if (cpu_is_slave(task->core))
		dcache_writeback_invalidate_region(ll_pdata,
						   sizeof(*ll_pdata));

	ll_sch_set_pdata(task, ll_pdata);

	return 0;
}

static void schedule_ll_task_free(void *data, struct task *task)
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
}

static void schedule_ll_task_cancel(void *data, struct task *task)
{
	struct ll_schedule_data *sch = data;
	struct list_item *tlist;
	struct task *curr_task;
	uint32_t flags;

	irq_local_disable(flags);

	/* check to see if we are scheduled */
	list_for_item(tlist, &sch->tasks) {
		curr_task = container_of(tlist, struct task, list);

		/* found it */
		if (curr_task == task) {
			schedule_ll_domain_clear(sch);
			break;
		}
	}

	/* remove work from list */
	task->state = SOF_TASK_STATE_CANCEL;
	list_item_del(&task->list);

	irq_local_enable(flags);
}

static void reschedule_ll_task(void *data, struct task *task, uint64_t start)
{
	struct ll_schedule_data *sch = data;
	struct list_item *tlist;
	struct task *curr_task;
	uint32_t flags;
	uint64_t time;

	time = sch->domain->ticks_per_ms * start / 1000;

	if (task->flags & SOF_SCHEDULE_FLAG_SYNC)
		time += platform_timer_get(platform_timer);
	else
		time += sch->domain->last_tick;

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

	trace_ll_error("reschedule_ll_task() error: task not found");

out:
	irq_local_enable(flags);
}

static void scheduler_free_ll(void *data)
{
	struct ll_schedule_data *sch = data;
	uint32_t flags;

	irq_local_disable(flags);

	notifier_unregister(&sch->notifier);

	list_item_del(&sch->tasks);

	irq_local_enable(flags);
}

static void ll_scheduler_recalculate_tasks(struct ll_schedule_data *sch,
					   struct clock_notify_data *clk_data)
{
	uint64_t current = platform_timer_get(platform_timer);
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

static void ll_scheduler_notify(int message, void *data, void *event_data)
{
	struct ll_schedule_data *sch = data;
	struct clock_notify_data *clk_data = event_data;
	uint32_t flags;

	irq_local_disable(flags);

	/* we need to recalculate tasks when clock frequency changes */
	if (message == CLOCK_NOTIFY_POST) {
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
	sch = rzalloc(RZONE_SYS, SOF_MEM_CAPS_RAM, sizeof(*sch));
	list_init(&sch->tasks);
	atomic_init(&sch->num_tasks, 0);
	sch->domain = domain;

	/* notification of clock changes */
	sch->notifier.cb = ll_scheduler_notify;
	sch->notifier.cb_data = sch;
	sch->notifier.id = NOTIFIER_CLK_CHANGE_ID(domain->clk);
	notifier_register(&sch->notifier);

	scheduler_init(domain->type, &schedule_ll_ops, sch);

	return 0;
}

struct scheduler_ops schedule_ll_ops = {
	.schedule_task		= schedule_ll_task,
	.schedule_task_init	= schedule_ll_task_init,
	.schedule_task_free	= schedule_ll_task_free,
	.schedule_task_cancel	= schedule_ll_task_cancel,
	.reschedule_task	= reschedule_ll_task,
	.scheduler_free		= scheduler_free_ll,
	.schedule_task_running	= NULL,
	.schedule_task_complete	= NULL,
	.scheduler_run		= NULL
};
