// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>

#include <sof/audio/buffer.h>
#include <sof/audio/component_ext.h>
#include <sof/audio/pipeline.h>
#include <rtos/interrupt.h>
#include <sof/lib/agent.h>
#include <sof/list.h>
#include <sof/schedule/ll_schedule.h>
#include <sof/schedule/dp_schedule.h>
#include <sof/schedule/schedule.h>
#include <sof/audio/module_adapter/module/generic.h>
#include <rtos/task.h>
#include <rtos/spinlock.h>
#include <rtos/string.h>
#include <ipc/header.h>
#include <ipc/stream.h>
#include <ipc/topology.h>
#include <ipc4/module.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

LOG_MODULE_DECLARE(pipe, CONFIG_SOF_LOG_LEVEL);

/* f11818eb-e92e-4082-82a3-dc54c604ebb3 */
DECLARE_SOF_UUID("pipe-task", pipe_task_uuid, 0xf11818eb, 0xe92e, 0x4082,
		 0x82,  0xa3, 0xdc, 0x54, 0xc6, 0x04, 0xeb, 0xb3);

#if CONFIG_ZEPHYR_DP_SCHEDULER

/* ee755917-96b9-4130-b49e-37b9d0501993 */
DECLARE_SOF_UUID("dp-task", dp_task_uuid, 0xee755917, 0x96b9, 0x4130,
		 0xb4,  0x9e, 0x37, 0xb9, 0xd0, 0x50, 0x19, 0x93);

/**
 * current static stack size for each DP component
 * TODO: to be taken from module manifest
 */
#define TASK_DP_STACK_SIZE 8192

/**
 * \brief a priority of the DP threads in the system.
 */
#define ZEPHYR_DP_THREAD_PRIORITY (CONFIG_NUM_PREEMPT_PRIORITIES - 1)

#endif /* CONFIG_ZEPHYR_DP_SCHEDULER */

static void pipeline_schedule_cancel(struct pipeline *p)
{
	schedule_task_cancel(p->pipe_task);

	/* enable system agent panic, when there are no longer
	 * DMA driven pipelines
	 */
	sa_set_panic_on_delay(true);
}

static enum task_state pipeline_task_cmd(struct pipeline *p,
					 struct sof_ipc_reply *reply)
{
	struct comp_dev *host = p->trigger.host;
	int err, cmd = p->trigger.cmd;

	if (!host) {
		int ret;

		p->trigger.cmd = COMP_TRIGGER_NO_ACTION;

		switch (cmd) {
		case COMP_TRIGGER_STOP:
		case COMP_TRIGGER_PAUSE:
			/* if the trigger was aborted, keep the task running */
			if (p->trigger.aborted) {
				ret = SOF_TASK_STATE_RUNNING;
				break;
			}

			/*
			 * if there's a trigger pending, keep the task running until it is executed
			 */
			if (p->trigger.pending)
				ret = SOF_TASK_STATE_RUNNING;
			else
				ret = SOF_TASK_STATE_COMPLETED;
			break;
		case COMP_TRIGGER_PRE_START:
		case COMP_TRIGGER_PRE_RELEASE:
			if (p->status == COMP_STATE_ACTIVE) {
				ret = SOF_TASK_STATE_RUNNING;
				break;
			}
			p->status = COMP_STATE_ACTIVE;
			COMPILER_FALLTHROUGH;
		default:
			ret = SOF_TASK_STATE_RESCHEDULE;
			break;
		}

		p->trigger.aborted = false;
		return ret;
	}

	err = pipeline_trigger_run(p, host, cmd);
	if (err < 0) {
		pipe_err(p, "pipeline_task_cmd(): failed to trigger components: %d", err);
		reply->error = err;
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
			/* comp_trigger() interrupted trigger propagation or an xrun occurred */
			if (p->trigger.aborted && p->status == COMP_STATE_PAUSED) {
				p->status = COMP_STATE_ACTIVE;
				/*
				 * the pipeline aborted a STOP or a PAUSE
				 * command, proceed with copying
				 */
				err = SOF_TASK_STATE_RUNNING;
			} else {
				err = SOF_TASK_STATE_COMPLETED;
			}
		} else if (p->trigger.cmd != cmd) {
			/* PRE stage completed */
			if (p->trigger.delay)
				return SOF_TASK_STATE_RESCHEDULE;
			/* No delay: the final stage has already run too */
			err = SOF_TASK_STATE_RESCHEDULE;
		} else if (p->status == COMP_STATE_PAUSED) {
			/* reset the pipeline components for IPC4 after the STOP trigger */
			if (cmd == COMP_TRIGGER_STOP && IPC4_MOD_ID(host->ipc_config.id)) {
				err = pipeline_reset(host->pipeline, host);
				if (err < 0)
					reply->error = err;
			}
			err = SOF_TASK_STATE_COMPLETED;
		} else {
			p->status = COMP_STATE_ACTIVE;
			err = SOF_TASK_STATE_RUNNING;
		}
	}

	p->trigger.cmd = COMP_TRIGGER_NO_ACTION;

	ipc_msg_reply(reply);

	return err;
}

static enum task_state pipeline_task(void *arg)
{
	struct sof_ipc_reply reply = {
		.hdr.cmd = SOF_IPC_GLB_REPLY,
		.hdr.size = sizeof(reply),
	};
	struct pipeline *p = arg;
	int err;

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

	if (p->trigger.cmd != COMP_TRIGGER_NO_ACTION) {
		/* Process an offloaded command */
		err = pipeline_task_cmd(p, &reply);
		if (err != SOF_TASK_STATE_RUNNING)
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
				p->trigger.pending = true;
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
			if (pipeline_is_timer_driven(p)) {
				/*
				 * Use the first of connected pipelines to
				 * trigger, mark all other connected pipelines
				 * active immediately.
				 */
				p->trigger.cmd = cmd;
				p->trigger.pending = true;
				p->trigger.host = ppl_data->start;
				ppl_data->start = NULL;
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

int pipeline_comp_ll_task_init(struct pipeline *p)
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

#if CONFIG_ZEPHYR_DP_SCHEDULER
static enum task_state dp_task_run(void *data)
{
	struct processing_module *mod = data;

	module_process_sink_src(mod, mod->sources, mod->num_of_sources,
				mod->sinks, mod->num_of_sinks);

	return SOF_TASK_STATE_RESCHEDULE;
}

int pipeline_comp_dp_task_init(struct comp_dev *comp)
{
	int ret;
	struct processing_module *mod = comp_get_drvdata(comp);
	struct task_ops ops  = {
		.run		= dp_task_run,
		.get_deadline	= NULL,
		.complete	= NULL
	};

	if (!comp->task) {
		ret = scheduler_dp_task_init(&comp->task,
					     SOF_UUID(dp_task_uuid),
					     &ops,
					     mod,
					     comp->ipc_config.core,
					     TASK_DP_STACK_SIZE,
					     ZEPHYR_DP_THREAD_PRIORITY);
		if (ret < 0)
			return ret;
	}

	return 0;
}
#endif /* CONFIG_ZEPHYR_DP_SCHEDULER */

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

	/*
	 * With connected pipelines some pipelines can be re-used for multiple
	 * streams. E.g. if playback pipelines A and B are connected on a mixer,
	 * belonging to pipeline C, leading to a DAI, if A is already streaming,
	 * when we attempt to start B, we don't need to schedule pipeline C -
	 * it's already running.
	 */
	if (task_is_active(p->pipe_task))
		return;

	if (p->sched_next && task_is_active(p->sched_next->pipe_task))
		schedule_task_before(p->pipe_task, start, p->period,
				     p->sched_next->pipe_task);
	else if (p->sched_prev && task_is_active(p->sched_prev->pipe_task))
		schedule_task_after(p->pipe_task, start, p->period,
				    p->sched_prev->pipe_task);
	else
		schedule_task(p->pipe_task, start, p->period);
}
