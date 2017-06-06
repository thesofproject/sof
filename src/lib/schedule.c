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
#include <reef/reef.h>
#include <reef/lock.h>
#include <reef/list.h>
#include <reef/stream.h>
#include <reef/alloc.h>
#include <reef/debug.h>
#include <reef/clock.h>
#include <reef/schedule.h>
#include <platform/timer.h>
#include <platform/clk.h>
#include <reef/audio/component.h>
#include <reef/audio/pipeline.h>
#include <arch/task.h>

struct schedule_data {
	spinlock_t lock;
	struct list_item list;	/* list of tasks in priority queue */
	uint32_t clock;
};

static struct schedule_data *sch;

/* get time delta */
static inline uint32_t schedule_time_diff(uint32_t current, uint32_t pipe_time)
{
	uint32_t max = MAX_INT;

	/* does work run in next cycle ? */
	if (pipe_time < current) {
		max -= current;
		max += pipe_time;
		return max;
	} else
		return pipe_time - current;
}

/*
 * Find the task with the earliest deadline. This may be the running task or a
 * queued task. TODO: Reduce cache invalidations by checking if the currently
 * running task AND the earliest queued task will both complete before their
 * deadlines. If so, then schedule the earlier queued task after the currently
 * running task has completed.
 */
static struct task *edf_get_next(void)
{
	struct task *task, *next_task = NULL;
	struct list_item *clist;
	uint32_t next_delta = MAX_INT, current, delta;
 
	/* get the current time */
	current = platform_timer_get(NULL);

	/* check every queued or running task in list */
	list_for_item(clist, &sch->list) {
		task = container_of(clist, struct task, list);

		/* get earliest deadline */
		delta = schedule_time_diff(current, task->deadline);
		if (delta < next_delta) {
			next_delta = delta;
			next_task = task;
		}
	}

	return next_task;
}

/*
 * EDF Scheduler - Earliest Deadline First Scheduler.
 *
 * Schedule task with the earliest deadline from task list.
 * Can run in IRQ context.
 */
void schedule_edf(void)
{
	struct task *task;
	uint32_t flags;

	tracev_pipe("EDF");

	/* get next component scheduled  */
	spin_lock_irq(&sch->lock, flags);

	/* get next task to be scheduled */
	task = edf_get_next();
	if (task == NULL)
		goto out;

	/* is task currently running ? */
	if (task->state == TASK_STATE_RUNNING)
		goto out;

	arch_run_task(task);

out:
	spin_unlock_irq(&sch->lock, flags);

	interrupt_clear(PLATFORM_SCHEDULE_IRQ);
}

/* Add a new task to the scheduler to be run */
int schedule_task_del(struct task *task)
{
	uint32_t flags;
	int ret = 0;

	/* add task to list */
	spin_lock_irq(&sch->lock, flags);

	/* is task already running ? */
	if (task->state == TASK_STATE_RUNNING) {
		ret = -EAGAIN;
		goto out;
	}

	list_item_del(&task->list);
	task->state = TASK_STATE_COMPLETED;

out:
	spin_unlock_irq(&sch->lock, flags);
	return ret;
}

/* Add a new task to the scheduler to be run */
void schedule_task(struct task *task, uint32_t deadline, uint16_t priority,
		void *data)
{
	uint32_t flags, time, current, ticks;

	/* get the current time */
	current = platform_timer_get(NULL);

	/* calculate deadline - TODO: include MIPS */
	ticks = clock_us_to_ticks(sch->clock, deadline);
	time = current + ticks - PLATFORM_SCHEDULE_COST;

	/* add task to list */
	spin_lock_irq(&sch->lock, flags);
	task->deadline = time;
	task->priority = priority;
	task->sdata = data;
	list_item_prepend(&task->list, &sch->list);
	task->state = TASK_STATE_QUEUED;
	spin_unlock_irq(&sch->lock, flags);
}

/* Remove a task from the scheduler when complete */
void schedule_task_complete(struct task *task)
{
	uint32_t flags;

	spin_lock_irq(&sch->lock, flags);
	list_item_del(&task->list);
	task->state = TASK_STATE_COMPLETED;
	spin_unlock_irq(&sch->lock, flags);
}

void scheduler_run(void *unused)
{
	/* EDF is only scheduler supported atm */
	schedule_edf();
}

/* run the scheduler */
void schedule(void)
{
	/* the scheduler is run in IRQ context */
	interrupt_set(PLATFORM_SCHEDULE_IRQ);
}

/* Initialise the scheduler */
int scheduler_init(struct reef *reef)
{
	trace_pipe("ScI");

	sch = rzalloc(RZONE_RUNTIME, RFLAGS_NONE, sizeof(*sch));
	list_init(&sch->list);
	spinlock_init(&sch->lock);
	sch->clock = PLATFORM_SCHED_CLOCK;

	/* configure scheduler interrupt */
	interrupt_register(PLATFORM_SCHEDULE_IRQ, scheduler_run, NULL);
	interrupt_enable(PLATFORM_SCHEDULE_IRQ);

	return 0;
}
