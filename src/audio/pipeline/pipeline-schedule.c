// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>

#include <sof/audio/buffer.h>
#include <sof/audio/component_ext.h>
#include <sof/audio/pipeline.h>
#include <sof/drivers/interrupt.h>
#include <sof/lib/agent.h>
#include <sof/list.h>
#include <sof/schedule/ll_schedule.h>
#include <sof/schedule/schedule.h>
#include <sof/schedule/task.h>
#include <sof/spinlock.h>
#include <sof/string.h>
#include <ipc/header.h>
#include <ipc/stream.h>
#include <ipc/topology.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* f11818eb-e92e-4082-82a3-dc54c604ebb3 */
DECLARE_SOF_UUID("pipe-task", pipe_task_uuid, 0xf11818eb, 0xe92e, 0x4082,
		 0x82,  0xa3, 0xdc, 0x54, 0xc6, 0x04, 0xeb, 0xb3);

static void pipeline_schedule_cancel(struct pipeline *p)
{
	schedule_task_cancel(p->pipe_task);

	/* enable system agent panic, when there are no longer
	 * DMA driven pipelines
	 */
	if (!pipeline_is_timer_driven(p))
		sa_set_panic_on_delay(true);
}

static enum task_state pipeline_task(void *arg)
{
	struct pipeline *p = arg;
	int err;

	pipe_dbg(p, "pipeline_task()");

	/* are we in xrun ? */
	if (p->xrun_bytes) {
		/* try to recover */
		err = pipeline_xrun_recover(p);
		if (err < 0)
			/* skip copy if still in xrun */
			return SOF_TASK_STATE_COMPLETED;
	}

	err = pipeline_copy(p);
	if (err < 0) {
		/* try to recover */
		err = pipeline_xrun_recover(p);
		if (err < 0) {
			pipe_err(p, "pipeline_task(): xrun recover failed! pipeline will be stopped!");
			/* failed - host will stop this pipeline */
			return SOF_TASK_STATE_COMPLETED;
		}
	}

	pipe_dbg(p, "pipeline_task() sched");

	return SOF_TASK_STATE_RESCHEDULE;
}

static struct task *pipeline_task_init(struct pipeline *p, uint32_t type,
				       enum task_state (*func)(void *data))
{
	struct pipeline_task *task = NULL;

	task = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM,
		       sizeof(*task));
	if (!task)
		return NULL;

	if (schedule_task_init_ll(&task->task, SOF_UUID(pipe_task_uuid), type,
				  p->priority, func,
				  p, p->core, 0) < 0) {
		rfree(task);
		return NULL;
	}

	task->sched_comp = p->sched_comp;
	task->registrable = p == p->sched_comp->pipeline;

	return &task->task;
}

int pipeline_schedule_config(struct pipeline *p, uint32_t sched_id,
			     uint32_t core, uint32_t period,
			     uint32_t period_mips, uint32_t frames_per_sched,
			     uint32_t time_domain)
{
	p->sched_id = sched_id;
	p->core = core;
	p->period = period;
	p->period_mips = period_mips;
	p->frames_per_sched = frames_per_sched;
	p->time_domain = time_domain;
	return 0;
}

void pipeline_schedule_triggered(struct pipeline_walk_context *ctx,
				 int cmd)
{
	struct list_item *tlist;
	struct pipeline *p;
	uint32_t flags;

	irq_local_disable(flags);

	list_for_item(tlist, &ctx->pipelines) {
		p = container_of(tlist, struct pipeline, list);

		switch (cmd) {
		case COMP_TRIGGER_PAUSE:
		case COMP_TRIGGER_STOP:
			pipeline_schedule_cancel(p);
			p->status = COMP_STATE_PAUSED;
			break;
		case COMP_TRIGGER_RELEASE:
		case COMP_TRIGGER_START:
			pipeline_schedule_copy(p, 0);
			p->xrun_bytes = 0;
			p->status = COMP_STATE_ACTIVE;
			break;
		case COMP_TRIGGER_SUSPEND:
		case COMP_TRIGGER_RESUME:
		default:
			break;
		}
	}

	irq_local_enable(flags);
}

int pipeline_comp_task_init(struct pipeline *p)
{
	uint32_t type;

	/* initialize task if necessary */
	if (!p->pipe_task) {
		/* right now we always consider pipeline as a low latency
		 * component, but it may change in the future
		 */
		type = pipeline_is_timer_driven(p) ? SOF_SCHEDULE_LL_TIMER :
			SOF_SCHEDULE_LL_DMA;

		p->pipe_task = pipeline_task_init(p, type, pipeline_task);
		if (!p->pipe_task) {
			pipe_err(p, "pipeline_comp_task_init(): task init failed");
			return -ENOMEM;
		}
	}

	return 0;
}

void pipeline_comp_trigger_sched_comp(struct pipeline *p,
				      struct comp_dev *comp,
				      struct pipeline_walk_context *ctx)
{
	/* only required by the scheduling component or sink component
	 * on pipeline without one
	 */
	if (dev_comp_id(p->sched_comp) != dev_comp_id(comp) &&
	    (pipeline_id(p) == pipeline_id(p->sched_comp->pipeline) ||
	     dev_comp_id(p->sink_comp) != dev_comp_id(comp)))
		return;

	/* add for later schedule */
	list_item_append(&p->list, &ctx->pipelines);
}

/* notify pipeline that this component requires buffers emptied/filled */
void pipeline_schedule_copy(struct pipeline *p, uint64_t start)
{
	/* disable system agent panic for DMA driven pipelines */
	if (!pipeline_is_timer_driven(p))
		sa_set_panic_on_delay(false);

	schedule_task(p->pipe_task, start, p->period);
}
