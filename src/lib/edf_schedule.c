/*
 * Copyright (c) 2017, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the Intel Corporation nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#include <stdint.h>
#include <stddef.h>
#include <errno.h>
#include <sof/sof.h>
#include <sof/lock.h>
#include <sof/list.h>
#include <sof/stream.h>
#include <sof/alloc.h>
#include <sof/debug.h>
#include <sof/clk.h>
#include <sof/edf_schedule.h>
#include <sof/ll_schedule.h>
#include <platform/timer.h>
#include <platform/clk.h>
#include <sof/audio/component.h>
#include <sof/drivers/timer.h>
#include <sof/task.h>

struct edf_schedule_data {
	spinlock_t lock;
	struct list_item list;	/* list of tasks in priority queue */
	uint32_t clock;
};

#define SLOT_ALIGN_TRIES	10

static void schedule_edf(void);
static void schedule_edf_task(struct task *task, uint64_t start,
			      uint64_t deadline, uint32_t flags);
static int schedule_edf_task_cancel(struct task *task);
static void schedule_edf_task_complete(struct task *task);
static void schedule_edf_task_running(struct task *task);
static int schedule_edf_task_init(struct task *task, uint32_t xflags);
static void schedule_edf_task_free(struct task *task);
static int edf_scheduler_init(void);
static void edf_scheduler_free(void);

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
	if (list_is_empty(&sch->list)) {
		return NULL;
	}

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

	interrupt_clear(PLATFORM_SCHEDULE_IRQ);

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

static int _sch_edf_task(struct task *task, uint64_t start,
			 uint64_t deadline)
{
	struct edf_schedule_data *sch =
		(*arch_schedule_get_data())->edf_sch_data;
	uint32_t flags;
	uint64_t current;
	uint64_t ticks_per_ms;
	struct edf_task_pdata *edf_pdata;

	edf_pdata = edf_sch_get_pdata(task);

	tracev_edf_sch("_schedule_edf_task()");

	spin_lock_irq(&sch->lock, flags);

	/* is task already pending ? - not enough MIPS to complete ? */
	if (task->state == SOF_TASK_STATE_PENDING) {
		trace_edf_sch("_schedule_edf_task(), task already pending");
		spin_unlock_irq(&sch->lock, flags);
		return 0;
	}

	/* is task already queued ? - not enough MIPS to complete ? */
	if (task->state == SOF_TASK_STATE_QUEUED) {
		trace_edf_sch("_schedule_edf_task(), task already queued");
		spin_unlock_irq(&sch->lock, flags);
		return 0;
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
	edf_pdata->deadline = task->start + ticks_per_ms * deadline / 1000;

	/* add task to list */
	list_item_append(&task->list, &sch->list);
	task->state = SOF_TASK_STATE_QUEUED;
	spin_unlock_irq(&sch->lock, flags);

	return 1;
}

/*
 * Add a new task to the scheduler to be run and define a scheduling
 * deadline in time for the task to be ran. Do not invoke the scheduler
 * immediately to run task, but wait intil schedule is next called.
 *
 * deadline is in microseconds relative to start.
 */
static void schedule_edf_task_idle(struct task *task, uint64_t deadline)
{
	_sch_edf_task(task, 0, deadline);
}

/*
 * Add a new task to the scheduler to be run and define a scheduling
 * window in time for the task to be ran. i.e. task will run between start and
 * deadline times.
 *
 * start is in microseconds relative to last task start time.
 * deadline is in microseconds relative to start.
 */
static void schedule_edf_task_normal(struct task *task, uint64_t start,
				     uint64_t deadline)
{
	int need_sched;

	need_sched = _sch_edf_task(task, start, deadline);

	/* need to run scheduler if task not already running */
	if (need_sched) {
		/* rerun scheduler */
		schedule_edf();
	}
}

static void schedule_edf_task(struct task *task, uint64_t start,
			      uint64_t deadline, uint32_t flags)
{
	if (flags & SOF_SCHEDULE_FLAG_IDLE)
		schedule_edf_task_idle(task, deadline);
	else
		schedule_edf_task_normal(task, start, deadline);
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
		trace_error(TRACE_CLASS_EDF, "unexpected task state %d "
			"at edf_task completion",
			task->state);
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

	tracev_edf_sch("schedule_edf()");

	spin_lock_irq(&sch->lock, flags);

	/* make sure we have a queued task in the list first before we
	   start scheduling as contexts switches are not free. */
	list_for_item(tlist, &sch->list) {
		edf_task = container_of(tlist, struct task, list);

		/* schedule if we find any queued tasks */
		if (edf_task->state == SOF_TASK_STATE_QUEUED) {
			spin_unlock_irq(&sch->lock, flags);
			goto schedule;
		}
	}

	/* no task to schedule */
	spin_unlock_irq(&sch->lock, flags);
	return;

schedule:
	/* TODO: detect current IRQ context and call edf_scheduler_run if both
	 * current context matches scheduler context. saves a DSP context
	 * switch.
	 */

	/* the scheduler is run in IRQ context */
	interrupt_set(PLATFORM_SCHEDULE_IRQ);
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
	spinlock_init(&sch->lock);
	sch->clock = PLATFORM_SCHED_CLOCK;

	/* configure scheduler interrupt */
	interrupt_register(PLATFORM_SCHEDULE_IRQ, IRQ_AUTO_UNMASK,
			   edf_scheduler_run, NULL);
	interrupt_enable(PLATFORM_SCHEDULE_IRQ);

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
	interrupt_disable(PLATFORM_SCHEDULE_IRQ);
	interrupt_unregister(PLATFORM_SCHEDULE_IRQ);

	/* free arch tasks */
	arch_free_tasks();

	list_item_del(&sch->list);

	spin_unlock_irq(&sch->lock, flags);
}

static int schedule_edf_task_init(struct task *task, uint32_t xflags)
{
	struct edf_task_pdata *edf_pdata;
	(void)xflags;

	if (edf_sch_get_pdata(task))
		return -EEXIST;

	edf_pdata = rzalloc(RZONE_SYS_RUNTIME | RZONE_FLAG_UNCACHED,
			    SOF_MEM_CAPS_RAM, sizeof(*edf_pdata));

	if (!edf_pdata) {
		trace_edf_sch_error("schedule_edf_task_init() error:"
				    "alloc failed");
		return -ENOMEM;
	}

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
