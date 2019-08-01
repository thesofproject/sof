// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2017 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>

#include <sof/common.h>
#include <sof/drivers/interrupt.h>
#include <sof/drivers/interrupt-map.h>
#include <sof/drivers/timer.h>
#include <sof/lib/alloc.h>
#include <sof/lib/clk.h>
#include <sof/list.h>
#include <sof/platform.h>
#include <sof/schedule/edf_schedule.h>
#include <sof/schedule/schedule.h>
#include <sof/schedule/task.h>
#include <sof/spinlock.h>
#include <ipc/topology.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct edf_schedule_data {
	spinlock_t lock;
	struct list_item list;	/* list of tasks in priority queue */
	struct list_item idle_list; /* list of queued idle tasks */
	uint32_t clock;
	int irq;
};

#define SLOT_ALIGN_TRIES	10

static void schedule_edf(void);
static void schedule_edf_task(struct task *task, uint64_t start,
			      uint64_t period, uint32_t flags);
static int schedule_edf_task_cancel(struct task *task);
static void schedule_edf_task_complete(struct task *task);
static void schedule_edf_task_running(struct task *task);
static int schedule_edf_task_init(struct task *task, uint32_t xflags);
static void schedule_edf_task_free(struct task *task);
static int edf_scheduler_init(void);
static void edf_scheduler_free(void);
static void edf_schedule_idle(void);

/*
 * Simple rescheduler to calculate tasks new start time and deadline if
 * previous deadline was missed. Tries to align at first with current task
 * timing, but will just add onto current if too far behind current.
 * XRUNs will be propagated up to the host if we have to reschedule.
 */
static inline void edf_reschedule(struct task *task, uint64_t current)
{
	int i;
	struct edf_task_pdata *edf_pdata;
	uint64_t delta;

	edf_pdata = edf_sch_get_pdata(task);
	delta = (edf_pdata->deadline - task->start) << 1;

	/* try and align task with current scheduling slots */
	for (i = 0; i < SLOT_ALIGN_TRIES; i++) {
		task->start += delta;

		if (task->start > current + delta) {
			edf_pdata->deadline = task->start + delta;
			return;
		}
	}

	/* task has slipped a lot, so just add delay to current */
	task->start = current + delta;
	edf_pdata->deadline = task->start + delta;
}

/*
 * Find the first non running task with the earliest deadline.
 * TODO: Reduce cache invalidations by checking if the currently
 * running task AND the earliest queued task will both complete before their
 * deadlines. If so, then schedule the earlier queued task after the currently
 * running task has completed.
 */
static inline struct task *edf_get_next(uint64_t current, struct task *ignore)
{
	struct edf_schedule_data *sch =
		(*arch_schedule_get_data())->edf_sch_data;
	struct list_item *clist;
	struct list_item *tlist;
	uint64_t next_delta = UINT64_MAX;
	int next_priority = SOF_TASK_PRI_LOW;
	uint64_t delta;
	uint64_t deadline;
	int reschedule = 0;
	struct task *edf_task_next = NULL;
	struct task *edf_task;
	struct edf_task_pdata *edf_pdata;

	/* any tasks in the scheduler ? */
	if (list_is_empty(&sch->list))
		return NULL;

	/* check every queued or running task in list */
	list_for_item_safe(clist, tlist, &sch->list) {
		edf_task = container_of(clist, struct task, list);

		/* only check queued tasks */
		if (edf_task->state != SOF_TASK_STATE_QUEUED)
			continue;

		/* ignore the ignored tasks */
		if (edf_task == ignore)
			continue;

		edf_pdata = edf_sch_get_pdata(edf_task);

		deadline = edf_pdata->deadline;

		if (current < deadline) {
			delta = deadline - current;

			/* get highest priority */
			if (edf_task->priority < next_priority) {
				next_priority = edf_task->priority;
				next_delta = delta;
				edf_task_next = edf_task;
			} else if (edf_task->priority == next_priority) {
				/* get earliest deadline */
				if (delta < next_delta) {
					next_delta = delta;
					edf_task_next = edf_task;
				}
			}
		} else {
			/* missed scheduling - will be rescheduled */
			trace_edf_sch("edf_get_next(), "
				   "missed scheduling - will be rescheduled");

			/* have we already tried to reschedule ? */
			if (!reschedule) {
				reschedule++;
				trace_edf_sch("edf_get_next(), "
					       "didn't try to reschedule yet");
				edf_reschedule(edf_task, current);
			} else {
				/* reschedule failed */
				list_item_del(&edf_task->list);
				edf_task->state = SOF_TASK_STATE_CANCEL;
				trace_edf_sch_error("edf_get_next(), "
						     "task cancelled");
			}
		}
	}

	return edf_task_next;
}

/*
 * EDF Scheduler - Earliest Deadline First Scheduler.
 *
 * Schedule task with the earliest deadline from task list.
 * Can run in IRQ context.
 */
static struct task *sch_edf(void)
{
	struct edf_schedule_data *sch =
		(*arch_schedule_get_data())->edf_sch_data;
	uint64_t current;
	uint32_t flags;

	struct task *task;
	struct task *future_task = NULL;

	tracev_edf_sch("sch_edf()");

	interrupt_clear(sch->irq);

	while (!list_is_empty(&sch->list)) {
		spin_lock_irq(&sch->lock, flags);

		/* get the current time */
		current = platform_timer_get(platform_timer);

		/* get next task to be scheduled */
		task = edf_get_next(current, NULL);
		spin_unlock_irq(&sch->lock, flags);

		/* any tasks ? */
		if (!task)
			return NULL;

		/* can task be started now ? */
		if (task->start <= current) {
			/* yes, run current task */
			task->start = current;

			/* init task for running */
			spin_lock_irq(&sch->lock, flags);
			task->state = SOF_TASK_STATE_PENDING;
			list_item_del(&task->list);
			spin_unlock_irq(&sch->lock, flags);

			/* now run task at correct run level */
			if (run_task(task) < 0) {
				trace_edf_sch_error("sch_edf() error");
				break;
			}
		} else {
			/* no, then schedule wake up */
			future_task = task;
			break;
		}
	}

	/* tell caller about future task */
	return future_task;
}

/* cancel and delete task from scheduler - won't stop it if already running */
static int schedule_edf_task_cancel(struct task *task)
{
	struct edf_schedule_data *sch =
		(*arch_schedule_get_data())->edf_sch_data;
	uint32_t flags;
	int ret = 0;

	tracev_edf_sch("schedule_edf_task_cancel()");

	spin_lock_irq(&sch->lock, flags);

	/* check current task state, delete it if it is queued
	 * if it is already running, nothing we can do about it atm
	 */
	if (task->state == SOF_TASK_STATE_QUEUED) {
		/* delete task */
		task->state = SOF_TASK_STATE_CANCEL;
		list_item_del(&task->list);
	}

	spin_unlock_irq(&sch->lock, flags);

	return ret;
}

static void schedule_edf_task(struct task *task, uint64_t start,
			      uint64_t period, uint32_t flags)
{
	struct edf_schedule_data *sch =
		(*arch_schedule_get_data())->edf_sch_data;
	uint64_t current;
	uint64_t ticks_per_ms;
	struct edf_task_pdata *edf_pdata;
	uint32_t lock_flags;
	bool need_sched;

	edf_pdata = edf_sch_get_pdata(task);

	/* invalidate if slave core */
	if (cpu_is_slave(task->core))
		dcache_invalidate_region(edf_pdata, sizeof(*edf_pdata));

	tracev_edf_sch("schedule_edf_task()");

	spin_lock_irq(&sch->lock, lock_flags);

	/* is task already pending ? - not enough MIPS to complete ? */
	if (task->state == SOF_TASK_STATE_PENDING) {
		trace_edf_sch("schedule_edf_task(), task already pending");
		spin_unlock_irq(&sch->lock, lock_flags);
		return;
	}

	/* is task already queued ? - not enough MIPS to complete ? */
	if (task->state == SOF_TASK_STATE_QUEUED) {
		trace_edf_sch("schedule_edf_task(), task already queued");
		spin_unlock_irq(&sch->lock, lock_flags);
		return;
	}

	/* get the current time */
	current = platform_timer_get(platform_timer);

	ticks_per_ms = clock_ms_to_ticks(sch->clock, 1);

	/* calculate start time - TODO: include MIPS */
	if (start == 0)
		task->start = current;
	else
		task->start = task->start + ticks_per_ms * start / 1000 -
			PLATFORM_SCHEDULE_COST;

	/* calculate deadline - TODO: include MIPS */
	edf_pdata->deadline = task->start + ticks_per_ms * period / 1000;

	/* add task to the proper list */
	if (flags & SOF_SCHEDULE_FLAG_IDLE) {
		list_item_append(&task->list, &sch->idle_list);
		need_sched = false;
	} else {
		list_item_append(&task->list, &sch->list);
		need_sched = true;
	}

	task->state = SOF_TASK_STATE_QUEUED;
	spin_unlock_irq(&sch->lock, lock_flags);

	if (need_sched) {
		/* rerun scheduler */
		schedule_edf();
	}
}

/* Remove a task from the scheduler when complete */
static void schedule_edf_task_complete(struct task *task)
{
	struct edf_schedule_data *sch =
		(*arch_schedule_get_data())->edf_sch_data;
	uint32_t flags;

	tracev_edf_sch("schedule_edf_task_complete()");

	spin_lock_irq(&sch->lock, flags);

	/* Some high priority HW based IRQ handlers can reschedule tasks
	 * immediately. i.e. before the task context can change task state
	 * back to COMPLETED. Check here to make sure we dont clobber
	 * task->state for regular non IRQ users.
	 */
	switch (task->state) {
	case SOF_TASK_STATE_RUNNING:
		task->state = SOF_TASK_STATE_COMPLETED;
		break;
	case SOF_TASK_STATE_QUEUED:
	case SOF_TASK_STATE_PENDING:
		/* nothing to do here, high priority IRQ has scheduled us */
		break;
	default:
		trace_edf_sch_error("unexpected task state %d at edf_task "
				    "completion", task->state);
		task->state = SOF_TASK_STATE_COMPLETED;
		break;
	}
	spin_unlock_irq(&sch->lock, flags);
}

/* Update task state to running */
static void schedule_edf_task_running(struct task *task)
{
	struct edf_schedule_data *sch =
		(*arch_schedule_get_data())->edf_sch_data;
	uint32_t flags;

	tracev_edf_sch("schedule_edf_task_running()");

	spin_lock_irq(&sch->lock, flags);
	task->state = SOF_TASK_STATE_RUNNING;
	spin_unlock_irq(&sch->lock, flags);
}

static void edf_scheduler_run(void *unused)
{
	tracev_edf_sch("edf_scheduler_run()");

	sch_edf();
}

/* run the scheduler */
static void schedule_edf(void)
{
	struct edf_schedule_data *sch =
		(*arch_schedule_get_data())->edf_sch_data;
	struct list_item *tlist;
	struct task *edf_task;
	uint32_t flags;
	uint64_t current;

	tracev_edf_sch("schedule_edf()");

	spin_lock_irq(&sch->lock, flags);

	/* make sure we have a queued task in the list first before we
	 * start scheduling as contexts switches are not free.
	 */
	list_for_item(tlist, &sch->list) {
		edf_task = container_of(tlist, struct task, list);

		/* schedule if we find any queued tasks
		 * which reached its deadline
		 */
		current = platform_timer_get(platform_timer);
		if (edf_task->state == SOF_TASK_STATE_QUEUED &&
		    edf_task->start <= current) {
			spin_unlock_irq(&sch->lock, flags);
			goto schedule;
		}
	}

	/* no task to schedule */
	spin_unlock_irq(&sch->lock, flags);

	/* There is no prioritized task to handle at the moment.
	 * Let's check if we have any idle task awaiting for execution.
	 */
	edf_schedule_idle();
	return;

schedule:
	/* TODO: detect current IRQ context and call edf_scheduler_run if both
	 * current context matches scheduler context. saves a DSP context
	 * switch.
	 */

	/* the scheduler is run in IRQ context */
	interrupt_set(sch->irq);
}

/* Initialise the scheduler */
static int edf_scheduler_init(void)
{
	trace_edf_sch("edf_scheduler_init()");

	struct schedule_data *sch_data = *arch_schedule_get_data();
	struct edf_schedule_data *sch;

	sch_data->edf_sch_data = rzalloc(RZONE_SYS, SOF_MEM_CAPS_RAM,
					 sizeof(*sch_data->edf_sch_data));

	sch = sch_data->edf_sch_data;

	list_init(&sch->list);
	list_init(&sch->idle_list);
	spinlock_init(&sch->lock);
	sch->clock = PLATFORM_SCHED_CLOCK;

	/* configure scheduler interrupt */
	sch->irq = interrupt_get_irq(PLATFORM_SCHEDULE_IRQ,
				     PLATFORM_SCHEDULE_IRQ_NAME);
	if (sch->irq < 0)
		return sch->irq;
	interrupt_register(sch->irq, IRQ_AUTO_UNMASK, edf_scheduler_run, sch);
	interrupt_enable(sch->irq, sch);

	/* allocate arch tasks */
	int tasks_result = allocate_tasks();

	return tasks_result;
}

/* Frees scheduler */
static void edf_scheduler_free(void)
{
	struct edf_schedule_data *sch =
		(*arch_schedule_get_data())->edf_sch_data;
	uint32_t flags;

	spin_lock_irq(&sch->lock, flags);

	/* disable and unregister scheduler interrupt */
	interrupt_disable(sch->irq, sch);
	interrupt_unregister(sch->irq, sch);

	/* free arch tasks */
	arch_free_tasks();

	list_item_del(&sch->list);
	list_item_del(&sch->idle_list);

	spin_unlock_irq(&sch->lock, flags);
}

static int schedule_edf_task_init(struct task *task, uint32_t xflags)
{
	struct edf_task_pdata *edf_pdata;
	(void)xflags;

	if (edf_sch_get_pdata(task))
		return -EEXIST;

	edf_pdata = rzalloc(RZONE_SYS_RUNTIME, SOF_MEM_CAPS_RAM,
			    sizeof(*edf_pdata));

	if (!edf_pdata) {
		trace_edf_sch_error("schedule_edf_task_init() error:"
				    "alloc failed");
		return -ENOMEM;
	}

	/* flush for slave core */
	if (cpu_is_slave(task->core))
		dcache_writeback_invalidate_region(edf_pdata,
						   sizeof(*edf_pdata));

	edf_sch_set_pdata(task, edf_pdata);

	return 0;
}

static void schedule_edf_task_free(struct task *task)
{
	task->state = SOF_TASK_STATE_FREE;
	task->func = NULL;
	task->data = NULL;

	rfree(edf_sch_get_pdata(task));
	edf_sch_set_pdata(task, NULL);
}

/* scheduler of idle tasks (non prioritized) */
static void edf_schedule_idle(void)
{
	struct edf_schedule_data *sch =
		(*arch_schedule_get_data())->edf_sch_data;
	struct list_item *tlist;
	struct list_item *clist;
	struct task *task;
	uint64_t ret;

	/* Are we on passive level? */
	if (arch_interrupt_get_level() != SOF_IRQ_PASSIVE_LEVEL)
		return;

	/* invoke unprioritized/idle tasks right away */
	list_for_item_safe(clist, tlist, &sch->idle_list) {
		task = container_of(clist, struct task, list);

		/* run task if we find any queued */
		if (task->state == SOF_TASK_STATE_QUEUED) {
			ret = task->func(task->data);

			if (ret == 0) {
				/* task done, remove it from the list */
				task->state = SOF_TASK_STATE_COMPLETED;
				list_item_del(clist);
			}
		}
	}
}

struct scheduler_ops schedule_edf_ops = {
	.schedule_task		= schedule_edf_task,
	.schedule_task_init	= schedule_edf_task_init,
	.schedule_task_running	= schedule_edf_task_running,
	.schedule_task_complete = schedule_edf_task_complete,
	.reschedule_task	= NULL,
	.schedule_task_cancel	= schedule_edf_task_cancel,
	.schedule_task_free	= schedule_edf_task_free,
	.scheduler_init		= edf_scheduler_init,
	.scheduler_free		= edf_scheduler_free,
	.scheduler_run		= schedule_edf
};
