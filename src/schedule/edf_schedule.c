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
#include <sof/lib/uuid.h>
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

/* 77de2074-828c-4044-a40b-420b72749e8b */
DECLARE_SOF_UUID("edf-schedule", edf_sched_uuid, 0x77de2074, 0x828c, 0x4044,
		 0xa4, 0x0b, 0x42, 0x0b, 0x72, 0x74, 0x9e, 0x8b);

DECLARE_TR_CTX(edf_tr, SOF_UUID(edf_sched_uuid), LOG_LEVEL_INFO);

struct edf_schedule_data {
	struct list_item list;	/* list of tasks in priority queue */
	uint32_t clock;
	int irq;
};

static const struct scheduler_ops schedule_edf_ops;

static int schedule_edf_task_complete(void *data, struct task *task);
static int schedule_edf_task_running(void *data, struct task *task);
static void schedule_edf(struct edf_schedule_data *edf_sch);

static void schedule_edf_task_run(struct task *task, void *data)
{
	while (1) {
		/* execute task run function and remove task from the list
		 * only if completed
		 */
		if (task_run(task) == SOF_TASK_STATE_COMPLETED)
			schedule_edf_task_complete(data, task);

		/* find new task for execution */
		schedule_edf(data);
	}
}

static void edf_scheduler_run(void *data)
{
	struct edf_schedule_data *edf_sch = data;
	uint64_t deadline_next = SOF_TASK_DEADLINE_IDLE;
	struct task *task_next = NULL;
	struct list_item *tlist;
	struct task *task;
	uint64_t deadline;
	uint32_t flags;

	tr_dbg(&edf_tr, "edf_scheduler_run()");

	irq_local_disable(flags);

	/* find next task to run */
	list_for_item(tlist, &edf_sch->list) {
		task = container_of(tlist, struct task, list);

		if (task->state != SOF_TASK_STATE_QUEUED &&
		    task->state != SOF_TASK_STATE_RUNNING)
			continue;

		deadline = task_get_deadline(task);

		if (deadline == SOF_TASK_DEADLINE_NOW) {
			/* task needs to be scheduled ASAP */
			task_next = task;
			break;
		}

		/* get earliest deadline */
		if (deadline <= deadline_next) {
			deadline_next = deadline;
			task_next = task;
		}
	}

	irq_local_enable(flags);

	/* having next task is mandatory */
	assert(task_next);

	schedule_edf_task_running(data, task_next);
}

static int schedule_edf_task(void *data, struct task *task, uint64_t start,
			     uint64_t period)
{
	struct edf_schedule_data *edf_sch = data;
	uint32_t flags;
	(void) period; /* not used */
	(void) start; /* not used */

	irq_local_disable(flags);

	/* not enough MCPS to complete */
	if (task->state == SOF_TASK_STATE_QUEUED ||
	    task->state == SOF_TASK_STATE_RUNNING) {
		tr_err(&edf_tr, "schedule_edf_task(), task already queued or running %d",
		       task->state);
		irq_local_enable(flags);
		return -EALREADY;
	}

	/* add task to the list */
	list_item_append(&task->list, &edf_sch->list);

	task->state = SOF_TASK_STATE_QUEUED;

	irq_local_enable(flags);

	schedule_edf(edf_sch);

	return 0;
}

int schedule_task_init_edf(struct task *task, const struct sof_uuid_entry *uid,
			   const struct task_ops *ops,
			   void *data, uint16_t core, uint32_t flags)
{
	struct edf_task_pdata *edf_pdata = NULL;
	int ret = 0;

	ret = schedule_task_init(task, uid, SOF_SCHEDULE_EDF, 0, ops->run, data,
				 core, flags);
	if (ret < 0)
		return ret;

	if (edf_sch_get_pdata(task))
		return -EEXIST;

	edf_pdata = rzalloc(SOF_MEM_ZONE_SYS_RUNTIME, 0, SOF_MEM_CAPS_RAM,
			    sizeof(*edf_pdata));
	if (!edf_pdata) {
		tr_err(&edf_tr, "schedule_task_init_edf(): alloc failed");
		return -ENOMEM;
	}

	edf_sch_set_pdata(task, edf_pdata);

	task->ops.complete = ops->complete;
	task->ops.get_deadline = ops->get_deadline;

	if (task_context_alloc(&edf_pdata->ctx) < 0)
		goto error;
	if (task_context_init(edf_pdata->ctx, &schedule_edf_task_run,
			      task, scheduler_get_data(SOF_SCHEDULE_EDF),
			      task->core, NULL, 0) < 0)
		goto error;

	/* flush for secondary core */
	if (cpu_is_secondary(task->core))
		dcache_writeback_invalidate_region(edf_pdata,
						   sizeof(*edf_pdata));
	return 0;

error:
	tr_err(&edf_tr, "schedule_task_init_edf(): init context failed");
	if (edf_pdata->ctx)
		task_context_free(edf_pdata->ctx);
	rfree(edf_pdata);
	edf_sch_set_pdata(task, NULL);
	return -EINVAL;
}

static int schedule_edf_task_running(void *data, struct task *task)
{
	struct edf_task_pdata *edf_pdata = edf_sch_get_pdata(task);
	uint32_t flags;

	tr_dbg(&edf_tr, "schedule_edf_task_running()");

	irq_local_disable(flags);

	task_context_set(edf_pdata->ctx);
	task->state = SOF_TASK_STATE_RUNNING;

	irq_local_enable(flags);

	return 0;
}

static int schedule_edf_task_complete(void *data, struct task *task)
{
	uint32_t flags;

	tr_dbg(&edf_tr, "schedule_edf_task_complete()");

	irq_local_disable(flags);

	task_complete(task);

	task->state = SOF_TASK_STATE_COMPLETED;
	list_item_del(&task->list);

	irq_local_enable(flags);

	return 0;
}

static int schedule_edf_task_cancel(void *data, struct task *task)
{
	uint32_t flags;

	tr_dbg(&edf_tr, "schedule_edf_task_cancel()");

	irq_local_disable(flags);

	/* cancel and delete only if queued */
	if (task->state == SOF_TASK_STATE_QUEUED) {
		task->state = SOF_TASK_STATE_CANCEL;
		list_item_del(&task->list);
	}

	irq_local_enable(flags);

	return 0;
}

static int schedule_edf_task_free(void *data, struct task *task)
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

	return 0;
}

int scheduler_init_edf(void)
{
	struct edf_schedule_data *edf_sch;

	tr_info(&edf_tr, "edf_scheduler_init()");

	edf_sch = rzalloc(SOF_MEM_ZONE_SYS, 0, SOF_MEM_CAPS_RAM,
			  sizeof(*edf_sch));
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

static void schedule_edf(struct edf_schedule_data *edf_sch)
{
	interrupt_set(edf_sch->irq);
}

static const struct scheduler_ops schedule_edf_ops = {
	.schedule_task		= schedule_edf_task,
	.schedule_task_running	= schedule_edf_task_running,
	.schedule_task_complete = schedule_edf_task_complete,
	.reschedule_task	= NULL,
	.schedule_task_cancel	= schedule_edf_task_cancel,
	.schedule_task_free	= schedule_edf_task_free,
	.scheduler_free		= scheduler_free_edf,
};
