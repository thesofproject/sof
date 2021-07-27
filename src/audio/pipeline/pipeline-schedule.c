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
	sa_set_panic_on_delay(true);
}

static enum task_state pipeline_task(void *arg)
{
	struct sof_ipc_reply reply = {
		.hdr.cmd = SOF_IPC_GLB_REPLY,
		.hdr.size = sizeof(reply),
	};
	struct pipeline *p = arg;
	int err, cmd = p->trigger.cmd;

	pipe_dbg(p, "pipeline_task()");

	/* are we in xrun ? */
	if (p->xrun_bytes) {
		/*
		 * This happens when one of the connected pipelines runs into an xrun even before
		 * this pipeline task gets a chance to run. But the host is still waiting for a
		 * trigger IPC response. So, send an error response to prevent it from getting
		 * timed out. No point triggering the pipeline in this case. It will be stopped
		 * anyway by the host.
		 */
		if (p->trigger.cmd != COMP_TRIGGER_NO_ACTION) {
			struct sof_ipc_reply reply = {
				.hdr.cmd = SOF_IPC_GLB_REPLY,
				.hdr.size = sizeof(reply),
				.error = -EPIPE,
			};

			p->trigger.cmd = COMP_TRIGGER_NO_ACTION;

			ipc_msg_reply(&reply);
		}

		/* try to recover */
		err = pipeline_xrun_recover(p);
		if (err < 0)
			/* skip copy if still in xrun */
			return SOF_TASK_STATE_COMPLETED;
	}

	if (p->trigger.delay) {
		p->trigger.delay--;
		return SOF_TASK_STATE_RESCHEDULE;
	}

	if (cmd != COMP_TRIGGER_NO_ACTION) {
		if (!p->trigger.host) {
			p->trigger.cmd = COMP_TRIGGER_NO_ACTION;
			return p->status == COMP_STATE_PAUSED ? SOF_TASK_STATE_COMPLETED :
				SOF_TASK_STATE_RESCHEDULE;
		}

		/* First pipeline task run for either START or RELEASE: PRE stage */
		err = pipeline_trigger_run(p, p->trigger.host, cmd);
		if (err < 0) {
			pipe_err(p, "pipeline_task(): failed to trigger components: %d", err);
			reply.error = err;
			err = SOF_TASK_STATE_COMPLETED;
		} else {
			switch (cmd) {
			case COMP_TRIGGER_START:
			case COMP_TRIGGER_RELEASE:
				p->status = COMP_STATE_ACTIVE;
				break;
			case COMP_TRIGGER_PRE_START:
			case COMP_TRIGGER_PRE_RELEASE:
				p->status = COMP_STATE_PRE_ACTIVE;
				break;
			case COMP_TRIGGER_STOP:
			case COMP_TRIGGER_PAUSE:
				p->status = COMP_STATE_PAUSED;
			}

			if (err == PPL_STATUS_PATH_STOP) {
				pipe_warn(p, "pipeline_task(): stopping for xrun");
				err = SOF_TASK_STATE_COMPLETED;
			} else if (p->trigger.cmd != cmd) {
				/* PRE stage completed */
				if (p->trigger.delay)
					return SOF_TASK_STATE_RESCHEDULE;
				/* No delay: the final stage has already run too */
			} else if (p->status == COMP_STATE_PAUSED && !p->trigger.aborted) {
				err = SOF_TASK_STATE_COMPLETED;
			} else {
				p->status = COMP_STATE_ACTIVE;
				err = SOF_TASK_STATE_RESCHEDULE;
			}
		}

		p->trigger.cmd = COMP_TRIGGER_NO_ACTION;

		ipc_msg_reply(&reply);

		return err;
	}

	if (p->status == COMP_STATE_PAUSED)
		/*
		 * One of pipelines, being stopped, but not the one, that
		 * triggers all components
		 */
		return SOF_TASK_STATE_COMPLETED;

	/*
	 * The first execution of the pipeline task above has triggered all
	 * pipeline components. Subsequent iterations actually perform data
	 * copying below.
	 */
	err = pipeline_copy(p);
	if (err < 0) {
		/* try to recover */
		err = pipeline_xrun_recover(p);
		if (err < 0) {
			pipe_err(p, "pipeline_task(): xrun recovery failed! pipeline is stopped.");
			/* failed - host will stop this pipeline */
			return SOF_TASK_STATE_COMPLETED;
		}
	}

	pipe_dbg(p, "pipeline_task() sched");

	return SOF_TASK_STATE_RESCHEDULE;
}

static struct task *pipeline_task_init(struct pipeline *p, uint32_t type)
{
	struct pipeline_task *task = NULL;

	task = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM,
		       sizeof(*task));
	if (!task)
		return NULL;

	if (schedule_task_init_ll(&task->task, SOF_UUID(pipe_task_uuid), type,
				  p->priority, pipeline_task,
				  p, p->core, 0) < 0) {
		rfree(task);
		return NULL;
	}

	task->sched_comp = p->sched_comp;
	task->registrable = p == p->sched_comp->pipeline;

	return &task->task;
}

void pipeline_schedule_config(struct pipeline *p, uint32_t sched_id,
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
}

/* trigger connected pipelines: either immediately or schedule them */
void pipeline_schedule_triggered(struct pipeline_walk_context *ctx,
				 int cmd)
{
	struct pipeline_data *ppl_data = ctx->comp_data;
	struct list_item *tlist;
	struct pipeline *p;
	uint32_t flags;
	bool first_pipe = true;

	/*
	 * Interrupts have to be disabled while adding tasks to or removing them
	 * from the scheduler list. Without that scheduling can begin
	 * immediately before all pipelines achieved a consistent state.
	 */
	irq_local_disable(flags);

	switch (cmd) {
	case COMP_TRIGGER_PAUSE:
	case COMP_TRIGGER_STOP:
		list_for_item(tlist, &ctx->pipelines) {
			p = container_of(tlist, struct pipeline, list);
			if (pipeline_is_timer_driven(p) &&
			    p->status != COMP_STATE_PAUSED) {
				/*
				 * Paused pipelines have their tasks stopped
				 * already, use a running pipeline to trigger
				 * components.
				 */
				p->trigger.cmd = cmd;
				p->trigger.host = ppl_data->start;
				ppl_data->start = NULL;
			} else {
				pipeline_schedule_cancel(p);
				p->status = COMP_STATE_PAUSED;
			}
		}
		break;
	case COMP_TRIGGER_PRE_RELEASE:
	case COMP_TRIGGER_PRE_START:
		list_for_item(tlist, &ctx->pipelines) {
			p = container_of(tlist, struct pipeline, list);
			p->xrun_bytes = 0;
			if (pipeline_is_timer_driven(p) && first_pipe) {
				/*
				 * Use the first of connected pipelines to
				 * trigger, mark all other connected pipelines
				 * active immediately.
				 */
				p->trigger.cmd = cmd;
				p->trigger.host = ppl_data->start;
				first_pipe = false;
			} else {
				p->status = COMP_STATE_ACTIVE;
			}
			pipeline_schedule_copy(p, 0);
		}
		break;
	case COMP_TRIGGER_XRUN:
		list_for_item(tlist, &ctx->pipelines) {
			p = container_of(tlist, struct pipeline, list);
			if (!p->xrun_bytes)
				/*
				 * the exact number of xrun bytes is unused,
				 * just make it non-0
				 */
				p->xrun_bytes = 1;
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

		p->pipe_task = pipeline_task_init(p, type);
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
