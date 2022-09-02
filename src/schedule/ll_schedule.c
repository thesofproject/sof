// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>
//         Tomasz Lauda <tomasz.lauda@linux.intel.com>

#include <rtos/atomic.h>
#include <sof/audio/pipeline.h>
#include <sof/common.h>
#include <rtos/interrupt.h>
#include <rtos/timer.h>
#include <rtos/alloc.h>
#include <rtos/clk.h>
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
#include <rtos/spinlock.h>
#include <ipc/topology.h>

#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

LOG_MODULE_REGISTER(ll_schedule, CONFIG_SOF_LOG_LEVEL);

/* 4f9c3ec7-7b55-400c-86b3-502b4420e625 */
DECLARE_SOF_UUID("ll-schedule", ll_sched_uuid, 0x4f9c3ec7, 0x7b55, 0x400c,
		 0x86, 0xb3, 0x50, 0x2b, 0x44, 0x20, 0xe6, 0x25);

DECLARE_TR_CTX(ll_tr, SOF_UUID(ll_sched_uuid), LOG_LEVEL_INFO);

/*
 *        LL Scheduler Task State Transition Diagram
 *
 *         schedule_task() +---------+
 *      +------------------|  INIT   |
 *      |                  +---------+
 *      |
 *      v
 * +--------+ is_pending() +---------+ is_pending() +----------+
 * | QUEUED |------------->| PENDING |<-------------|RESCHEDULE|
 * +--------+              +---------+              +----------+
 *                              |                         ^
 *                     execute()|                         |
 *                              v                         |
 *                         +---------+     task_run()     |
 *                         | RUNNING |--------------------+
 *                         +---------+     reschedule
 *                              |
 *                    task_run()|
 *                    completed v
 *                         +---------+
 *                         |COMPLETED|
 *                         +---------+
 */

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

#if CONFIG_PERFORMANCE_COUNTERS
static void perf_ll_sched_trace(struct perf_cnt_data *pcd, int ignored)
{
	tr_info(&ll_tr, "perf ll_work peak plat %u cpu %u",
		(uint32_t)((pcd)->plat_delta_peak),
		(uint32_t)((pcd)->cpu_delta_peak));
}

#if CONFIG_PERFORMANCE_COUNTERS_RUN_AVERAGE
static void perf_avg_ll_sched_trace(struct perf_cnt_data *pcd, int ignored)
{
	tr_info(&ll_tr, "perf ll_work cpu avg %u (current peak %u)",
		(uint32_t)((pcd)->cpu_delta_sum),
		(uint32_t)((pcd)->cpu_delta_peak));
}
#endif

#endif

static bool schedule_ll_is_pending(struct ll_schedule_data *sch)
{
	struct ll_schedule_domain *domain = sch->domain;
	struct list_item *tlist;
	struct task *task;
	uint32_t pending_count = 0;
	struct comp_dev *sched_comp;
	k_spinlock_key_t key;

	key = k_spin_lock(&domain->lock);

	do {
		sched_comp = NULL;

		/* mark each valid task as pending */
		list_for_item(tlist, &sch->tasks) {
			task = container_of(tlist, struct task, list);

			/*
			 * only tasks queued or waiting for reschedule are
			 * pending for scheduling
			 */
			if (task->state != SOF_TASK_STATE_QUEUED &&
			    task->state != SOF_TASK_STATE_RESCHEDULE)
				continue;

			if (domain_is_pending(domain, task, &sched_comp)) {
				task->state = SOF_TASK_STATE_PENDING;
				pending_count++;
			}
		}
	} while (sched_comp);

	k_spin_unlock(&domain->lock, key);

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

/* caller should hold the domain lock */
static void schedule_ll_task_done(struct ll_schedule_data *sch,
				  struct task *task)
{
	/* Remove from the task list, schedule_task_cancel() won't handle it again */
	list_item_del(&task->list);

	/* unregister the task */
	domain_unregister(sch->domain, task, atomic_sub(&sch->num_tasks, 1) - 1);

	tr_info(&ll_tr, "task complete %p %pU", task, task->uid);
	tr_info(&ll_tr, "num_tasks %ld total_num_tasks %ld",
		atomic_read(&sch->num_tasks),
		atomic_read(&sch->domain->total_num_tasks));
}

/* perf measurement windows size 2^x */
#define CHECKS_WINDOW_SIZE	10

#ifdef CONFIG_SCHEDULE_LOG_CYCLE_STATISTICS
static inline void dsp_load_check(struct task *task, uint32_t cycles0, uint32_t cycles1)
{
	uint32_t diff;

	if (cycles1 > cycles0)
		diff = cycles1 - cycles0;
	else
		diff = UINT32_MAX - cycles0 + cycles1;

	task->cycles_sum += diff;

	if (task->cycles_max < diff)
		task->cycles_max = diff;

	if (++task->cycles_cnt == 1 << CHECKS_WINDOW_SIZE) {
		task->cycles_sum >>= CHECKS_WINDOW_SIZE;
		tr_info(&ll_tr, "task %p %pU avg %u, max %u", task, task->uid,
			task->cycles_sum, task->cycles_max);
		task->cycles_sum = 0;
		task->cycles_max = 0;
		task->cycles_cnt = 0;
	}
}
#endif

static void schedule_ll_tasks_execute(struct ll_schedule_data *sch)
{
	struct ll_schedule_domain *domain = sch->domain;
	struct list_item *wlist;
	struct task *task;
	k_spinlock_key_t key;

	/* check each task in the list for pending */
	wlist = sch->tasks.next;

	/*
	 * Cannot use list_for_item(_safe)() because the task can cancel some
	 * other tasks, removing them from the list. This happens, e.g. when
	 * a pipeline task terminates a DMIC task.
	 */
	while (wlist != &sch->tasks) {
#ifdef CONFIG_SCHEDULE_LOG_CYCLE_STATISTICS
		uint32_t cycles0, cycles1;
#endif
		task = list_item(wlist, struct task, list);

		if (task->state != SOF_TASK_STATE_PENDING) {
			wlist = task->list.next;
			continue;
		}

		tr_dbg(&ll_tr, "task %p %pU being started...", task, task->uid);

#ifdef CONFIG_SCHEDULE_LOG_CYCLE_STATISTICS
		cycles0 = (uint32_t)sof_cycle_get_64();
#endif
		task->state = SOF_TASK_STATE_RUNNING;

		/*
		 * The running task might cancel other tasks, which then get
		 * removed from the list
		 */
		task->state = task_run(task);

		wlist = task->list.next;

		key = k_spin_lock(&domain->lock);

		/* do we need to reschedule this task */
		if (task->state == SOF_TASK_STATE_COMPLETED) {
			schedule_ll_task_done(sch, task);
		} else {
			/* update task's start time */
			schedule_ll_task_update_start(sch, task);
			tr_dbg(&ll_tr, "task %p uid %pU finished, next period ticks %u, domain->next_tick %u",
			       task, task->uid, (uint32_t)task->start,
			       (uint32_t)domain->next_tick);
		}

		k_spin_unlock(&domain->lock, key);

#ifdef CONFIG_SCHEDULE_LOG_CYCLE_STATISTICS
		cycles1 = (uint32_t)sof_cycle_get_64();
		dsp_load_check(task, cycles0, cycles1);
#endif
	}
}

static void schedule_ll_client_reschedule(struct ll_schedule_data *sch)
{
	struct list_item *tlist;
	struct task *task;
	struct task *task_take = NULL;
	uint64_t next_tick = sch->domain->new_target_tick;

	/* rearm only if there is work to do */
	if (atomic_read(&sch->domain->total_num_tasks)) {
		/* traverse to set timer according to the earliest task */
		list_for_item(tlist, &sch->tasks) {
			task = container_of(tlist, struct task, list);

			/* only check tasks asked for rescheduling */
			if (task->state != SOF_TASK_STATE_RESCHEDULE)
				continue;

			/* update to use the earlier tick */
			if (task->start < next_tick) {
				next_tick = task->start;
				task_take = task;
			}
		}

		tr_dbg(&ll_tr,
		       "schedule_ll_clients_reschedule next_tick %u task_take %p",
		       (unsigned int)next_tick, task_take);

		/* update the target_tick */
		if (task_take)
			sch->domain->new_target_tick = next_tick;
	}

}

static void schedule_ll_tasks_run(void *data)
{
	struct ll_schedule_data *sch = data;
	struct ll_schedule_domain *domain = sch->domain;
	k_spinlock_key_t key;
	uint32_t flags;
	uint32_t core = cpu_get_id();

	tr_dbg(&ll_tr, "timer interrupt on core %d, at %u, previous next_tick %u",
	       core,
	       (unsigned int)sof_cycle_get_64_atomic(),
	       (unsigned int)domain->next_tick);

	irq_local_disable(flags);
	key = k_spin_lock(&domain->lock);

	/* disable domain on current core until tasks are finished */
	domain_disable(domain, core);

	if (!atomic_read(&domain->enabled_cores)) {
		/* clear the domain/interrupts */
		domain_clear(domain);
	}

	k_spin_unlock(&domain->lock, key);

	perf_cnt_init(&sch->pcd);

	/* run tasks if there are any pending */
	if (schedule_ll_is_pending(sch))
		schedule_ll_tasks_execute(sch);

	notifier_event(sch, NOTIFIER_ID_LL_POST_RUN,
		       NOTIFIER_TARGET_CORE_LOCAL, NULL, 0);

	perf_cnt_stamp(&sch->pcd, perf_ll_sched_trace, 0 /* ignored */);
	perf_cnt_average(&sch->pcd, perf_avg_ll_sched_trace, 0 /* ignored */);

	key = k_spin_lock(&domain->lock);

	/* reset the new_target_tick for the first core */
	if (domain->new_target_tick < sof_cycle_get_64_atomic())
		domain->new_target_tick = UINT64_MAX;

	/* update the new_target_tick according to tasks on current core */
	schedule_ll_client_reschedule(sch);

	/* set the next interrupt according to the new_target_tick */
	if (domain->new_target_tick < domain->next_tick) {
		domain_set(domain, domain->new_target_tick);
		tr_dbg(&ll_tr, "tasks on core %d done, new_target_tick %u set",
		       core, (unsigned int)domain->new_target_tick);
	}

	/* tasks on current core finished, re-enable domain on it */
	if (atomic_read(&sch->num_tasks))
		domain_enable(domain, core);

	k_spin_unlock(&domain->lock, key);

	irq_local_enable(flags);
}

static int schedule_ll_domain_set(struct ll_schedule_data *sch,
				  struct task *task, uint64_t start,
				  uint64_t period, struct task *reference)
{
	struct ll_schedule_domain *domain = sch->domain;
	int core = cpu_get_id();
	uint64_t task_start_us;
	uint64_t task_start_ticks;
	uint64_t task_start;
	uint64_t offset;
	k_spinlock_key_t key;
	int ret;

	key = k_spin_lock(&domain->lock);

	ret = domain_register(domain, task, &schedule_ll_tasks_run, sch);
	if (ret < 0) {
		tr_err(&ll_tr, "schedule_ll_domain_set: cannot register domain %d",
		       ret);
		goto done;
	}

	tr_dbg(&ll_tr, "task->start %u next_tick %u",
	       (unsigned int)task->start,
	       (unsigned int)domain->next_tick);

	task_start_us = period ? period : start;
	task_start_ticks = domain->ticks_per_ms * task_start_us / 1000;
	task_start = task_start_ticks + sof_cycle_get_64_atomic();

	if (reference) {
		task->start = reference->start;
	} else if (domain->next_tick == UINT64_MAX) {
		/* first task, set domain */
		domain_set(domain, task_start);
		task->start = domain->next_tick;
	} else if (!period) {
		/* one shot task, set domain if it is earlier */
		task->start = task_start;
		if (task->start < domain->next_tick)
			domain_set(domain, task_start);
	} else if (task_start + task_start_ticks < domain->next_tick) {
		/*
		 * Earlier periodic task, try to make it cadence-aligned with the existed task.
		 * In this case task_start_ticks is the number of ticks per period.
		 */
		offset = (domain->next_tick - task_start) % task_start_ticks;
		task_start += offset;
		domain_set(domain, task_start);
		task->start = domain->next_tick;
	} else {
		/* later periodic task, simplify and cover it by the coming interrupt */
		task->start = domain->next_tick;
	}

	/* increase task number of the core */
	atomic_add(&sch->num_tasks, 1);

	/* make sure enable domain on the core */
	domain_enable(domain, core);

	tr_info(&ll_tr, "new added task->start %u at %u",
		(unsigned int)task->start,
		(unsigned int)sof_cycle_get_64_atomic());
	tr_info(&ll_tr, "num_tasks %ld total_num_tasks %ld",
		atomic_read(&sch->num_tasks),
		atomic_read(&domain->total_num_tasks));

done:
	k_spin_unlock(&domain->lock, key);

	return ret;
}

static void schedule_ll_domain_clear(struct ll_schedule_data *sch,
				     struct task *task)
{
	struct ll_schedule_domain *domain = sch->domain;
	k_spinlock_key_t key;

	key = k_spin_lock(&domain->lock);

	/*
	 * Decrement the number of tasks on the core.
	 * Disable domain on the core if needed
	 */
	if (atomic_sub(&sch->num_tasks, 1) == 1)
		domain_disable(domain, cpu_get_id());

	/* unregister the task */
	domain_unregister(domain, task, atomic_read(&sch->num_tasks));

	tr_info(&ll_tr, "num_tasks %ld total_num_tasks %ld",
		atomic_read(&sch->num_tasks),
		atomic_read(&domain->total_num_tasks));

	k_spin_unlock(&domain->lock, key);
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

static void schedule_ll_task_insert_before(struct task *task, struct task *before)
{
	list_item_append(&task->list, &before->list);
}

static void schedule_ll_task_insert_after(struct task *task, struct task *after)
{
	list_item_prepend(&task->list, &after->list);
}

static int schedule_ll_task_common(struct ll_schedule_data *sch, struct task *task,
				   uint64_t start, uint64_t period,
				   struct task *reference, bool before)
{
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
				if (pdata->period < reg_pdata->period) {
					tr_err(&ll_tr,
					       "schedule_ll_task(): registrable task has a period longer than current task");
					ret = -EINVAL;
					goto out;
				}

				pdata->ratio = period / reg_pdata->period;
			}
		}
	}

	/* insert task into the list */
	if (!reference)
		schedule_ll_task_insert(task, &sch->tasks);
	else if (before)
		schedule_ll_task_insert_before(task, reference);
	else
		schedule_ll_task_insert_after(task, reference);
	task->state = SOF_TASK_STATE_QUEUED;

	/* set schedule domain */
	ret = schedule_ll_domain_set(sch, task, start, period, reference);
	if (ret < 0) {
		list_item_del(&task->list);
		goto out;
	}


out:
	irq_local_enable(flags);

	return ret;
}

static int schedule_ll_task(void *data, struct task *task, uint64_t start,
			    uint64_t period)
{
	struct ll_schedule_data *sch = data;

	return schedule_ll_task_common(sch, task, start, period, NULL, false);
}

static int schedule_ll_task_before(void *data, struct task *task, uint64_t start,
				   uint64_t period, struct task *before)
{
	struct ll_schedule_data *sch = data;

	return schedule_ll_task_common(sch, task, start, period, before, true);
}

static int schedule_ll_task_after(void *data, struct task *task, uint64_t start,
				  uint64_t period, struct task *after)
{
	struct ll_schedule_data *sch = data;

	return schedule_ll_task_common(sch, task, start, period, after, false);
}

int schedule_task_init_ll(struct task *task,
			  const struct sof_uuid_entry *uid, uint16_t type,
			  uint16_t priority, enum task_state (*run)(void *data),
			  void *data, uint16_t core, uint32_t flags)
{
	struct ll_task_pdata *ll_pdata;
	int ret;

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

			/* remove work from list */
			task->state = SOF_TASK_STATE_CANCEL;
			list_item_del(&task->list);

			break;
		}
	}

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

	time += sof_cycle_get_64_atomic();

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

static void scheduler_free_ll(void *data, uint32_t flags)
{
	struct ll_schedule_data *sch = data;
	uint32_t irq_flags;

	irq_local_disable(irq_flags);

	domain_unregister(sch->domain, NULL, 0);

	if (!(flags & SOF_SCHEDULER_FREE_IRQ_ONLY))
		notifier_unregister(sch, NULL,
				    NOTIFIER_CLK_CHANGE_ID(sch->domain->clk));

	irq_local_enable(irq_flags);
}

static void ll_scheduler_recalculate_tasks(struct ll_schedule_data *sch,
					   struct clock_notify_data *clk_data)
{
	uint64_t current = sof_cycle_get_64_atomic();
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
		sch->domain->ticks_per_ms = k_ms_to_cyc_ceil64(1);
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
	.schedule_task_before	= schedule_ll_task_before,
	.schedule_task_after	= schedule_ll_task_after,
	.schedule_task_free	= schedule_ll_task_free,
	.schedule_task_cancel	= schedule_ll_task_cancel,
	.reschedule_task	= reschedule_ll_task,
	.scheduler_free		= scheduler_free_ll,
	.scheduler_restore	= NULL,
	.schedule_task_running	= NULL,
	.schedule_task_complete	= NULL,
};
