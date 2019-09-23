// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2017 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Tomasz Lauda <tomasz.lauda@linux.intel.com>

#include <sof/common.h>
#include <sof/debug/panic.h>
#include <sof/drivers/interrupt.h>
#include <sof/drivers/timer.h>
#include <sof/lib/alloc.h>
#include <sof/lib/clk.h>
#include <sof/list.h>
#include <sof/platform.h>
#include <sof/schedule/edf_schedule.h>
#include <sof/schedule/schedule.h>
#include <sof/schedule/task.h>
#include <sof/sof.h>
#include <ipc/topology.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

struct edf_schedule_data {
	struct list_item list;	/* list of tasks in priority queue */
	uint32_t clock;
	int irq;
};

struct scheduler_ops schedule_edf_ops;

static void schedule_edf_task_complete(void *data, struct task *task);
static void schedule_edf_task_running(void *data, struct task *task);
static void schedule_edf(void *data);

static void schedule_edf_task_run(struct task *task, void *data)
{
	while (1) {
		/* execute task run function and remove task from the list
		 * only if completed
		 */
		if (task->run(task->data) == SOF_TASK_STATE_COMPLETED)
			schedule_edf_task_complete(data, task);

		/* find new task for execution */
		schedule_edf(data);
	}
}

static void edf_scheduler_run(void *data)
{
	struct edf_schedule_data *edf_sch = data;
	uint64_t current = platform_timer_get(platform_timer);
	int priority_next = SOF_TASK_PRI_IDLE;
	uint64_t delta_next = UINT64_MAX;
	struct edf_task_pdata *edf_pdata;
	struct task *task_next = NULL;
	struct list_item *tlist;
	struct task *task;
	uint64_t deadline;
	uint64_t delta;
	uint32_t flags;

	tracev_edf_sch("edf_scheduler_run()");

	irq_local_disable(flags);

	/* find next task to run */
	list_for_item(tlist, &edf_sch->list) {
		task = container_of(tlist, struct task, list);

		if (task->state != SOF_TASK_STATE_QUEUED &&
		    task->state != SOF_TASK_STATE_RUNNING &&
		    task->state != SOF_TASK_STATE_RESCHEDULE)
			continue;

		edf_pdata = edf_sch_get_pdata(task);

		deadline = edf_pdata->deadline;

		if (current >= deadline &&
		    !(task->flags & SOF_SCHEDULE_FLAG_IDLE)) {
			/* task needs to be scheduled ASAP */
			task_next = task;
			break;
		}

		delta = deadline - current;

		/* get highest priority */
		if (task->priority < priority_next) {
			priority_next = task->priority;
			delta_next = delta;
			task_next = task;
		} else if (task->priority == priority_next) {
			/* get earliest deadline */
			if (delta <= delta_next) {
				delta_next = delta;
				task_next = task;
			}
		}
	}

	irq_local_enable(flags);

	/* having next task is mandatory */
	assert(task_next);

	schedule_edf_task_running(data, task_next);
}

static void schedule_edf_task(void *data, struct task *task, uint64_t start,
			      uint64_t period)
{
	struct edf_schedule_data *edf_sch = data;
	struct edf_task_pdata *edf_pdata = edf_sch_get_pdata(task);
	uint32_t flags;
	uint64_t current;
	uint64_t ticks_per_ms;

	irq_local_disable(flags);

	/* not enough MCPS to complete */
	if (task->state == SOF_TASK_STATE_QUEUED ||
	    task->state == SOF_TASK_STATE_RUNNING) {
		trace_edf_sch_error("schedule_edf_task(), task already queued "
				    "or running %d, deadline = %u ",
				    task->state, edf_pdata->deadline);
		irq_local_enable(flags);
		return;
	}

	/* get current time */
	current = platform_timer_get(platform_timer);

	ticks_per_ms = clock_ms_to_ticks(edf_sch->clock, 1);

	/* calculate start time */
	task->start = start ? task->start + ticks_per_ms * start / 1000 :
		current;

	/* calculate deadline */
	edf_pdata->deadline = task->start + ticks_per_ms * period / 1000;

	/* add task to the list */
	list_item_append(&task->list, &edf_sch->list);

	task->state = SOF_TASK_STATE_QUEUED;

	irq_local_enable(flags);

	schedule_edf(data);
}

static int schedule_edf_task_init(void *data, struct task *task)
{
	struct edf_task_pdata *edf_pdata = NULL;

	if (edf_sch_get_pdata(task))
		return -EEXIST;

	edf_pdata = rzalloc(RZONE_SYS_RUNTIME, SOF_MEM_CAPS_RAM,
			    sizeof(*edf_pdata));
	if (!edf_pdata) {
		trace_edf_sch_error("schedule_edf_task_init() error: alloc "
				    "failed");
		return -ENOMEM;
	}

	edf_sch_set_pdata(task, edf_pdata);

	if (task_context_alloc(&edf_pdata->ctx) < 0)
		goto error;
	if (task_context_init(edf_pdata->ctx, &schedule_edf_task_run,
			      task, data, task->core, NULL, 0) < 0)
		goto error;

	/* flush for slave core */
	if (cpu_is_slave(task->core))
		dcache_writeback_invalidate_region(edf_pdata,
						   sizeof(*edf_pdata));
	return 0;

error:
	trace_edf_sch_error("schedule_edf_task_init() error: init "
			    "context failed");
	if (edf_pdata->ctx)
		task_context_free(edf_pdata->ctx);
	rfree(edf_pdata);
	edf_sch_set_pdata(task, NULL);
	return -EINVAL;
}

static void schedule_edf_task_running(void *data, struct task *task)
{
	struct edf_task_pdata *edf_pdata = edf_sch_get_pdata(task);
	uint32_t flags;

	tracev_edf_sch("schedule_edf_task_running()");

	irq_local_disable(flags);

	task_context_set(edf_pdata->ctx);
	task->state = SOF_TASK_STATE_RUNNING;

	irq_local_enable(flags);
}

static void schedule_edf_task_complete(void *data, struct task *task)
{
	uint32_t flags;

	tracev_edf_sch("schedule_edf_task_complete()");

	irq_local_disable(flags);

	/* execute task complete function if exists */
	if (task->complete)
		task->complete(task->data);

	task->state = SOF_TASK_STATE_COMPLETED;
	list_item_del(&task->list);

	irq_local_enable(flags);
}

static void schedule_edf_task_cancel(void *data, struct task *task)
{
	uint32_t flags;

	tracev_edf_sch("schedule_edf_task_cancel()");

	irq_local_disable(flags);

	/* cancel and delete only if queued */
	if (task->state == SOF_TASK_STATE_QUEUED) {
		task->state = SOF_TASK_STATE_CANCEL;
		list_item_del(&task->list);
	}

	irq_local_enable(flags);
}

static void schedule_edf_task_free(void *data, struct task *task)
{
	struct edf_task_pdata *edf_pdata = edf_sch_get_pdata(task);
	uint32_t flags;

	irq_local_disable(flags);

	task->state = SOF_TASK_STATE_FREE;

	task_context_free(edf_pdata->ctx);
	edf_pdata->ctx = NULL;
	rfree(edf_pdata);
	edf_sch_set_pdata(task, NULL);

	irq_local_enable(flags);
}

int scheduler_init_edf(void)
{
	struct edf_schedule_data *edf_sch;

	trace_edf_sch("edf_scheduler_init()");

	edf_sch = rzalloc(RZONE_SYS, SOF_MEM_CAPS_RAM, sizeof(*edf_sch));
	list_init(&edf_sch->list);
	edf_sch->clock = PLATFORM_DEFAULT_CLOCK;

	scheduler_init(SOF_SCHEDULE_EDF, &schedule_edf_ops, edf_sch);

	/* initialize main task context before enabling interrupt */
	task_main_init();

	/* configure EDF scheduler interrupt */
	edf_sch->irq = interrupt_get_irq(PLATFORM_SCHEDULE_IRQ,
					 PLATFORM_SCHEDULE_IRQ_NAME);
	if (edf_sch->irq < 0)
		return edf_sch->irq;

	interrupt_register(edf_sch->irq, edf_scheduler_run, edf_sch);
	interrupt_enable(edf_sch->irq, edf_sch);

	return 0;
}

static void scheduler_free_edf(void *data)
{
	struct edf_schedule_data *edf_sch = data;
	uint32_t flags;

	irq_local_disable(flags);

	/* disable and unregister EDF scheduler interrupt */
	interrupt_disable(edf_sch->irq, edf_sch);
	interrupt_unregister(edf_sch->irq, edf_sch);

	/* free main task context */
	task_main_free();

	list_item_del(&edf_sch->list);

	irq_local_enable(flags);
}

static void schedule_edf(void *data)
{
	struct edf_schedule_data *edf_sch = data;

	interrupt_set(edf_sch->irq);
}

struct scheduler_ops schedule_edf_ops = {
	.schedule_task		= schedule_edf_task,
	.schedule_task_init	= schedule_edf_task_init,
	.schedule_task_running	= schedule_edf_task_running,
	.schedule_task_complete = schedule_edf_task_complete,
	.reschedule_task	= NULL,
	.schedule_task_cancel	= schedule_edf_task_cancel,
	.schedule_task_free	= schedule_edf_task_free,
	.scheduler_free		= scheduler_free_edf,
	.scheduler_run		= schedule_edf
};
