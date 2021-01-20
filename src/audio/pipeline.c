// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>

#include <sof/audio/buffer.h>
#include <sof/audio/component_ext.h>
#include <sof/audio/pipeline.h>
#include <sof/debug/panic.h>
#include <sof/drivers/interrupt.h>
#include <sof/drivers/ipc.h>
#include <sof/drivers/timer.h>
#include <sof/lib/agent.h>
#include <sof/lib/alloc.h>
#include <sof/lib/clk.h>
#include <sof/lib/mailbox.h>
#include <sof/lib/mm_heap.h>
#include <sof/lib/uuid.h>
#include <sof/list.h>
#include <sof/math/numbers.h>
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

/* generic pipeline data used by pipeline_comp_* functions */
struct pipeline_data {
	struct comp_dev *start;
	struct sof_ipc_pcm_params *params;
	struct sof_ipc_stream_posn *posn;
	struct pipeline *p;
	int cmd;
};

/* 4e934adb-b0ec-4d33-a086-c1022f921321 */
DECLARE_SOF_RT_UUID("pipe", pipe_uuid, 0x4e934adb, 0xb0ec, 0x4d33,
		 0xa0, 0x86, 0xc1, 0x02, 0x2f, 0x92, 0x13, 0x21);

DECLARE_TR_CTX(pipe_tr, SOF_UUID(pipe_uuid), LOG_LEVEL_INFO);

/* f11818eb-e92e-4082-82a3-dc54c604ebb3 */
DECLARE_SOF_UUID("pipe-task", pipe_task_uuid, 0xf11818eb, 0xe92e, 0x4082,
		 0x82,  0xa3, 0xdc, 0x54, 0xc6, 0x04, 0xeb, 0xb3);

static SHARED_DATA struct pipeline_posn pipeline_posn;

void pipeline_posn_init(struct sof *sof)
{
	sof->pipeline_posn = platform_shared_get(&pipeline_posn,
						 sizeof(pipeline_posn));
	spinlock_init(&sof->pipeline_posn->lock);
	platform_shared_commit(sof->pipeline_posn, sizeof(*sof->pipeline_posn));
}

static enum task_state pipeline_task(void *arg);
static void pipeline_schedule_cancel(struct pipeline *p);

/* create new pipeline - returns pipeline id or negative error */
struct pipeline *pipeline_new(struct sof_ipc_pipe_new *pipe_desc,
			      struct comp_dev *cd)
{
	struct sof_ipc_stream_posn posn;
	struct pipeline *p;
	int ret;

	pipe_cl_info("pipeline new pipe_id %d period %d priority %d",
		     pipe_desc->pipeline_id, pipe_desc->period,
		     pipe_desc->priority);

	/* allocate new pipeline */
	p = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(*p));
	if (!p) {
		pipe_cl_err("pipeline_new(): Out of Memory");
		return NULL;
	}

	/* init pipeline */
	p->sched_comp = cd;
	p->status = COMP_STATE_INIT;
	ret = memcpy_s(&p->tctx, sizeof(struct tr_ctx), &pipe_tr,
		       sizeof(struct tr_ctx));
	assert(!ret);
	ret = memcpy_s(&p->ipc_pipe, sizeof(p->ipc_pipe),
		       pipe_desc, sizeof(*pipe_desc));
	assert(!ret);

	ret = pipeline_posn_offset_get(&p->posn_offset);
	if (ret < 0) {
		pipe_err(p, "pipeline_new(): pipeline_posn_offset_get failed %d",
			 ret);
		rfree(p);
		return NULL;
	}

	/* just for retrieving valid ipc_msg header */
	ipc_build_stream_posn(&posn, SOF_IPC_STREAM_TRIG_XRUN,
			      p->ipc_pipe.comp_id);

	p->msg = ipc_msg_init(posn.rhdr.hdr.cmd, sizeof(posn));
	if (!p->msg) {
		pipe_err(p, "pipeline_new(): ipc_msg_init failed");
		rfree(p);
		return NULL;
	}

	return p;
}

int pipeline_connect(struct comp_dev *comp, struct comp_buffer *buffer,
		     int dir)
{
	uint32_t flags;

	if (dir == PPL_CONN_DIR_COMP_TO_BUFFER)
		comp_info(comp, "connect buffer %d as sink", buffer->id);
	else
		comp_info(comp, "connect buffer %d as source", buffer->id);

	irq_local_disable(flags);
	list_item_prepend(buffer_comp_list(buffer, dir),
			  comp_buffer_list(comp, dir));
	buffer_set_comp(buffer, comp, dir);
	comp_writeback(comp);
	irq_local_enable(flags);

	return 0;
}

struct pipeline_walk_context {
	int (*comp_func)(struct comp_dev *, struct comp_buffer *,
			 struct pipeline_walk_context *, int);
	void *comp_data;
	void (*buff_func)(struct comp_buffer *, void *);
	void *buff_data;
	/**< pipelines to be scheduled after trigger walk */
	struct list_item pipelines;
	/*
	 * If this flag is set, pipeline_for_each_comp() will skip all
	 * incompletely initialised components, i.e. those, whose .pipeline ==
	 * NULL. Such components should not be skipped during initialisation
	 * and clean up, but they should be skipped during streaming.
	 */
	bool skip_incomplete;
};

/* Generic method for walking the graph upstream or downstream.
 * It requires function pointer for recursion.
 */
static int pipeline_for_each_comp(struct comp_dev *current,
				  struct pipeline_walk_context *ctx,
				  int dir)
{
	struct list_item *buffer_list = comp_buffer_list(current, dir);
	struct list_item *clist;
	struct comp_buffer *buffer;
	struct comp_dev *buffer_comp;
	uint32_t flags;

	/* run this operation further */
	list_for_item(clist, buffer_list) {
		buffer = buffer_from_list(clist, struct comp_buffer, dir);

		/* don't go back to the buffer which already walked */
		if (buffer->walking)
			continue;

		/* execute operation on buffer */
		if (ctx->buff_func)
			ctx->buff_func(buffer, ctx->buff_data);

		buffer_comp = buffer_get_comp(buffer, dir);

		/* don't go further if this component is not connected */
		if (!buffer_comp ||
		    (ctx->skip_incomplete && !buffer_comp->pipeline))
			continue;

		/* continue further */
		if (ctx->comp_func) {
			buffer_lock(buffer, &flags);
			buffer->walking = true;
			buffer_unlock(buffer, flags);

			int err = ctx->comp_func(buffer_comp, buffer,
						 ctx, dir);
			buffer_lock(buffer, &flags);
			buffer->walking = false;
			buffer_unlock(buffer, flags);
			if (err < 0)
				return err;
		}
	}

	return 0;
}

static int pipeline_comp_complete(struct comp_dev *current,
				  struct comp_buffer *calling_buf,
				  struct pipeline_walk_context *ctx, int dir)
{
	struct pipeline_data *ppl_data = ctx->comp_data;

	pipe_dbg(ppl_data->p, "pipeline_comp_complete(), current->comp.id = %u, dir = %u",
		 dev_comp_id(current), dir);

	if (!comp_is_single_pipeline(current, ppl_data->start)) {
		pipe_dbg(ppl_data->p, "pipeline_comp_complete(), current is from another pipeline");
		return 0;
	}

	/* complete component init */
	current->pipeline = ppl_data->p;
	current->period = ppl_data->p->ipc_pipe.period;
	current->priority = ppl_data->p->ipc_pipe.priority;

	pipeline_for_each_comp(current, ctx, dir);

	return 0;
}

int pipeline_complete(struct pipeline *p, struct comp_dev *source,
		      struct comp_dev *sink)
{
	struct pipeline_data data;
	struct pipeline_walk_context walk_ctx = {
		.comp_func = pipeline_comp_complete,
		.comp_data = &data,
	};

#if !UNIT_TEST && !CONFIG_LIBRARY
	int freq = clock_get_freq(cpu_get_id());
#else
	int freq = 0;
#endif

	pipe_info(p, "pipeline complete, clock freq %dHz", freq);

	/* check whether pipeline is already completed */
	if (p->status != COMP_STATE_INIT) {
		pipe_err(p, "pipeline_complete(): Pipeline already completed");
		return -EINVAL;
	}

	data.start = source;
	data.p = p;

	/* now walk downstream from source component and
	 * complete component task and pipeline initialization
	 */
	walk_ctx.comp_func(source, NULL, &walk_ctx, PPL_DIR_DOWNSTREAM);

	p->source_comp = source;
	p->sink_comp = sink;
	p->status = COMP_STATE_READY;

	/* show heap status */
	heap_trace_all(0);

	return 0;
}

static int pipeline_comp_free(struct comp_dev *current,
			      struct comp_buffer *calling_buf,
			      struct pipeline_walk_context *ctx, int dir)
{
	struct pipeline_data *ppl_data = ctx->comp_data;
	uint32_t flags;

	pipe_dbg(current->pipeline, "pipeline_comp_free(), current->comp.id = %u, dir = %u",
		 dev_comp_id(current), dir);

	if (!comp_is_single_pipeline(current, ppl_data->start)) {
		pipe_dbg(current->pipeline, "pipeline_comp_free(), current is from another pipeline");
		return 0;
	}

	/* complete component free */
	current->pipeline = NULL;

	pipeline_for_each_comp(current, ctx, dir);

	/* disconnect source from buffer */
	irq_local_disable(flags);
	list_item_del(comp_buffer_list(current, dir));
	irq_local_enable(flags);

	return 0;
}

/* pipelines must be inactive */
int pipeline_free(struct pipeline *p)
{
	struct pipeline_data data;
	struct pipeline_walk_context walk_ctx = {
		.comp_func = pipeline_comp_free,
		.comp_data = &data,
	};

	pipe_info(p, "pipeline_free()");

	/* make sure we are not in use */
	if (p->source_comp) {
		if (p->source_comp->state > COMP_STATE_READY) {
			pipe_err(p, "pipeline_free(): Pipeline in use, %u, %u",
				 dev_comp_id(p->source_comp),
				 p->source_comp->state);
			return -EBUSY;
		}

		data.start = p->source_comp;

		/* disconnect components */
		walk_ctx.comp_func(p->source_comp, NULL, &walk_ctx,
				   PPL_DIR_DOWNSTREAM);
	}

	/* remove from any scheduling */
	if (p->pipe_task) {
		schedule_task_free(p->pipe_task);
		rfree(p->pipe_task);
	}

	ipc_msg_free(p->msg);

	pipeline_posn_offset_put(p->posn_offset);

	/* now free the pipeline */
	rfree(p);

	/* show heap status */
	heap_trace_all(0);

	return 0;
}

/* save params changes made by component */
static void pipeline_update_buffer_pcm_params(struct comp_buffer *buffer,
					      void *data)
{
	struct sof_ipc_stream_params *params = data;
	int i;

	params->buffer_fmt = buffer->buffer_fmt;
	params->frame_fmt = buffer->stream.frame_fmt;
	params->rate = buffer->stream.rate;
	params->channels = buffer->stream.channels;
	for (i = 0; i < SOF_IPC_MAX_CHANNELS; i++)
		params->chmap[i] = buffer->chmap[i];
}

/* fetch hardware stream parameters from DAI and propagate them to the remaining
 * buffers in pipeline.
 */
static int pipeline_comp_hw_params(struct comp_dev *current,
				   struct comp_buffer *calling_buf,
				   struct pipeline_walk_context *ctx, int dir)
{
	struct pipeline_data *ppl_data = ctx->comp_data;
	uint32_t flags = 0;
	int ret;

	pipe_dbg(current->pipeline, "pipeline_comp_hw_params(), current->comp.id = %u, dir = %u",
		 dev_comp_id(current), dir);

	pipeline_for_each_comp(current, ctx, dir);

	/* Fetch hardware stream parameters from DAI component */
	if (dev_comp_type(current) == SOF_COMP_DAI) {
		ret = comp_dai_get_hw_params(current,
					     &ppl_data->params->params, dir);
		if (ret < 0) {
			pipe_err(current->pipeline, "pipeline_find_dai_comp(): comp_dai_get_hw_params() error.");
			return ret;
		}
	}

	/* set buffer parameters */
	if (calling_buf) {
		buffer_lock(calling_buf, &flags);
		buffer_set_params(calling_buf, &ppl_data->params->params,
				  BUFFER_UPDATE_IF_UNSET);
		buffer_unlock(calling_buf, flags);
	}

	return 0;
}

static int pipeline_comp_params_neg(struct comp_dev *current,
				    struct comp_buffer *calling_buf,
				    struct pipeline_walk_context *ctx,
				    int dir)
{
	struct pipeline_data *ppl_data = ctx->comp_data;
	uint32_t flags = 0;
	int err = 0;

	pipe_dbg(current->pipeline, "pipeline_comp_params_neg(), current->comp.id = %u, dir = %u",
		 dev_comp_id(current), dir);

	/* check if 'current' is already configured */
	if (current->state != COMP_STATE_INIT &&
	    current->state != COMP_STATE_READY) {
		/* return 0 if params matches */
		if (buffer_params_match(calling_buf,
					&ppl_data->params->params,
					BUFF_PARAMS_FRAME_FMT |
					BUFF_PARAMS_RATE))
			return 0;
		/*
		 * the param is conflict with an active pipeline,
		 * drop an error and reject the .params() command.
		 */
		pipe_err(current->pipeline, "pipeline_comp_params_neg(): params conflict with existed active pipeline!");
		return -EINVAL;
	}

	/*
	 * Negotiation only happen when the current component has > 1
	 * source or sink, we are propagating the params to branched
	 * buffers, and the subsequent component's .params() or .prepare()
	 * should be responsible for calibrating if needed. For example,
	 * a component who has different channels input/output buffers
	 * should explicitly configure the channels of the branched buffers.
	 */
	if (calling_buf) {
		buffer_lock(calling_buf, &flags);
		err = buffer_set_params(calling_buf,
					&ppl_data->params->params,
					BUFFER_UPDATE_FORCE);
		buffer_unlock(calling_buf, flags);
	}

	return err;
}

static int pipeline_comp_params(struct comp_dev *current,
				struct comp_buffer *calling_buf,
				struct pipeline_walk_context *ctx, int dir)
{
	struct pipeline_data *ppl_data = ctx->comp_data;
	struct pipeline_walk_context param_neg_ctx = {
		.comp_func = pipeline_comp_params_neg,
		.comp_data = ppl_data,
		.skip_incomplete = true,
	};
	int stream_direction = ppl_data->params->params.direction;
	int end_type;
	int err;

	pipe_dbg(current->pipeline, "pipeline_comp_params(), current->comp.id = %u, dir = %u",
		 dev_comp_id(current), dir);

	if (!comp_is_single_pipeline(current, ppl_data->start)) {
		/* If pipeline connected to the starting one is in improper
		 * direction (CAPTURE towards DAI, PLAYBACK towards HOST),
		 * stop propagation of parameters not to override their config.
		 * Direction param of the pipeline can not be trusted at this
		 * point, as it might not be configured yet, hence checking
		 * for endpoint component type.
		 */
		end_type = comp_get_endpoint_type(current->pipeline->sink_comp);
		if (stream_direction == SOF_IPC_STREAM_PLAYBACK) {
			if (end_type == COMP_ENDPOINT_HOST ||
			    end_type == COMP_ENDPOINT_NODE)
				return 0;
		}

		if (stream_direction == SOF_IPC_STREAM_CAPTURE) {
			if (end_type == COMP_ENDPOINT_DAI ||
			    end_type == COMP_ENDPOINT_NODE)
				return 0;
		}
	}

	/* don't do any params if current is running */
	if (current->state == COMP_STATE_ACTIVE)
		return 0;

	/* do params negotiation with other branches(opposite direction) */
	err = pipeline_for_each_comp(current, &param_neg_ctx, !dir);
	if (err < 0 || err == PPL_STATUS_PATH_STOP)
		return err;

	/* set comp direction */
	current->direction = ppl_data->params->params.direction;

	err = comp_params(current, &ppl_data->params->params);
	if (err < 0 || err == PPL_STATUS_PATH_STOP)
		return err;

	return pipeline_for_each_comp(current, ctx, dir);
}

/* Send pipeline component params from host to endpoints.
 * Params always start at host (PCM) and go downstream for playback and
 * upstream for capture.
 *
 * Playback params can be re-written by upstream components. e.g. upstream SRC
 * can change sample rate for all downstream components regardless of sample
 * rate from host.
 *
 * Capture params can be re-written by downstream components.
 *
 * Params are always modified in the direction of host PCM to DAI.
 */
int pipeline_params(struct pipeline *p, struct comp_dev *host,
		    struct sof_ipc_pcm_params *params)
{
	struct sof_ipc_pcm_params hw_params;
	struct pipeline_data data;
	struct pipeline_walk_context hw_param_ctx = {
		.comp_func = pipeline_comp_hw_params,
		.comp_data = &data,
		.skip_incomplete = true,
	};
	struct pipeline_walk_context param_ctx = {
		.comp_func = pipeline_comp_params,
		.comp_data = &data,
		.buff_func = pipeline_update_buffer_pcm_params,
		.buff_data = &params->params,
		.skip_incomplete = true,
	};
	int dir = params->params.direction;
	int ret;

	pipe_info(p, "pipe params dir %d frame_fmt %d buffer_fmt %d rate %d",
		  params->params.direction, params->params.frame_fmt,
		  params->params.buffer_fmt, params->params.rate);
	pipe_info(p, "pipe params stream_tag %d channels %d sample_valid_bytes %d sample_container_bytes %d",
		  params->params.stream_tag, params->params.channels,
		  params->params.sample_valid_bytes,
		  params->params.sample_container_bytes);

	/* settin hw params */
	data.start = host;
	data.params = &hw_params;

	ret = hw_param_ctx.comp_func(host, NULL, &hw_param_ctx, dir);
	if (ret < 0) {
		pipe_err(p, "pipeline_prepare(): ret = %d, dev->comp.id = %u",
			 ret, dev_comp_id(host));
		return ret;
	}

	/* setting pcm params */
	data.params = params;
	data.start = host;

	ret = param_ctx.comp_func(host, NULL, &param_ctx, dir);
	if (ret < 0) {
		pipe_err(p, "pipeline_params(): ret = %d, host->comp.id = %u",
			 ret, dev_comp_id(host));
	}

	return ret;
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
				  p->ipc_pipe.priority, func,
				  p, p->ipc_pipe.core, 0) < 0) {
		rfree(task);
		return NULL;
	}

	task->sched_comp = p->sched_comp;
	task->registrable = p == p->sched_comp->pipeline;

	return &task->task;
}

static int pipeline_comp_task_init(struct pipeline *p)
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
			pipe_err(p, "pipeline_prepare(): task init failed");
			return -ENOMEM;
		}
	}

	return 0;
}

static int pipeline_comp_prepare(struct comp_dev *current,
				 struct comp_buffer *calling_buf,
				 struct pipeline_walk_context *ctx, int dir)
{
	struct pipeline_data *ppl_data = ctx->comp_data;
	int stream_direction = dir;
	int end_type;
	int err;

	pipe_dbg(current->pipeline, "pipeline_comp_prepare(), current->comp.id = %u, dir = %u",
		 dev_comp_id(current), dir);

	if (!comp_is_single_pipeline(current, ppl_data->start)) {
		/* If pipeline connected to the starting one is in improper
		 * direction (CAPTURE towards DAI, PLAYBACK towards HOST),
		 * stop propagation. Direction param of the pipeline can not be
		 * trusted at this point, as it might not be configured yet,
		 * hence checking for endpoint component type.
		 */
		end_type = comp_get_endpoint_type(current->pipeline->sink_comp);
		if (stream_direction == SOF_IPC_STREAM_PLAYBACK) {
			if (end_type == COMP_ENDPOINT_HOST ||
			    end_type == COMP_ENDPOINT_NODE)
				return 0;
		}

		if (stream_direction == SOF_IPC_STREAM_CAPTURE) {
			if (end_type == COMP_ENDPOINT_DAI ||
			    end_type == COMP_ENDPOINT_NODE)
				return 0;
		}
	}

	err = pipeline_comp_task_init(current->pipeline);
	if (err < 0)
		return err;

	err = comp_prepare(current);
	if (err < 0 || err == PPL_STATUS_PATH_STOP)
		return err;

	return pipeline_for_each_comp(current, ctx, dir);
}

/* prepare the pipeline for usage */
int pipeline_prepare(struct pipeline *p, struct comp_dev *dev)
{
	struct pipeline_data ppl_data;
	struct pipeline_walk_context walk_ctx = {
		.comp_func = pipeline_comp_prepare,
		.comp_data = &ppl_data,
		.buff_func = buffer_reset_pos,
		.skip_incomplete = true,
	};
	int ret;

	pipe_info(p, "pipe prepare");

	ppl_data.start = dev;

	ret = walk_ctx.comp_func(dev, NULL, &walk_ctx, dev->direction);
	if (ret < 0) {
		pipe_err(p, "pipeline_prepare(): ret = %d, dev->comp.id = %u",
			 ret, dev_comp_id(dev));
		return ret;
	}

	p->status = COMP_STATE_PREPARE;

	return ret;
}

static void pipeline_comp_trigger_sched_comp(struct pipeline *p,
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

/*
 * Check whether pipeline is incapable of acquiring data for capture.
 *
 * If capture START/RELEASE trigger originated on dailess pipeline and reached
 * inactive pipeline as it's source, then we indicate that it's blocked.
 *
 * @param rsrc - component from remote pipeline serving as source to relevant
 *		 pipeline
 * @param ctx - trigger walk context
 * @param dir - trigger direction
 */
static inline bool
pipeline_should_report_enodata_on_trigger(struct comp_dev *rsrc,
					  struct pipeline_walk_context *ctx,
					  int dir)
{
	struct pipeline_data *ppl_data = ctx->comp_data;
	struct comp_dev *pipe_source = ppl_data->start->pipeline->source_comp;

	/* only applies to capture pipelines */
	if (dir != SOF_IPC_STREAM_CAPTURE)
		return false;

	/* only applicable on trigger start/release */
	if (ppl_data->cmd != COMP_TRIGGER_START &&
	    ppl_data->cmd != COMP_TRIGGER_RELEASE)
		return false;

	/* only applies for dailess pipelines */
	if (pipe_source && dev_comp_type(pipe_source) == SOF_COMP_DAI)
		return false;

	/* if component on which we depend to provide data is inactive, then the
	 * pipeline has no means of providing data
	 */
	if (rsrc->state != COMP_STATE_ACTIVE)
		return true;

	return false;
}

static int pipeline_comp_trigger(struct comp_dev *current,
				 struct comp_buffer *calling_buf,
				 struct pipeline_walk_context *ctx, int dir)
{
	struct pipeline_data *ppl_data = ctx->comp_data;
	int is_single_ppl = comp_is_single_pipeline(current, ppl_data->start);
	int is_same_sched =
		pipeline_is_same_sched_comp(current->pipeline,
					    ppl_data->start->pipeline);
	int err;

	pipe_dbg(current->pipeline, "pipeline_comp_trigger(), current->comp.id = %u, dir = %u",
		 dev_comp_id(current), dir);

	/* trigger should propagate to the connected pipelines,
	 * which need to be scheduled together
	 */
	if (!is_single_ppl && !is_same_sched) {
		pipe_dbg(current->pipeline, "pipeline_comp_trigger(), current is from another pipeline");

		if (pipeline_should_report_enodata_on_trigger(current, ctx,
							      dir))
			return -ENODATA;

		return 0;
	}

	/* send command to the component and update pipeline state */
	err = comp_trigger(current, ppl_data->cmd);
	if (err < 0 || err == PPL_STATUS_PATH_STOP)
		return err;

	pipeline_comp_trigger_sched_comp(current->pipeline, current, ctx);

	return pipeline_for_each_comp(current, ctx, dir);
}

/*
 * trigger handler for pipelines in xrun, used for recovery from host only.
 * return values:
 *	0 -- success, further trigger in caller needed.
 *	PPL_STATUS_PATH_STOP -- done, no more further trigger needed.
 *	minus -- failed, caller should return failure.
 */
static int pipeline_xrun_handle_trigger(struct pipeline *p, int cmd)
{
	int ret = 0;

	/* it is expected in paused status for xrun pipeline */
	if (!p->xrun_bytes || p->status != COMP_STATE_PAUSED)
		return 0;

	/* in xrun, handle start/stop trigger */
	switch (cmd) {
	case COMP_TRIGGER_START:
		/* in xrun, prepare before trigger start needed */
		pipe_info(p, "in xrun, prepare it first");
		/* prepare the pipeline */
		ret = pipeline_prepare(p, p->source_comp);
		if (ret < 0) {
			pipe_err(p, "prepare: ret = %d", ret);
			return ret;
		}
		/* now ready for start, clear xrun_bytes */
		p->xrun_bytes = 0;
		break;
	case COMP_TRIGGER_STOP:
		/* in xrun, suppose pipeline is already stopped, ignore it */
		pipe_info(p, "already stopped in xrun");
		/* no more further trigger stop needed */
		ret = PPL_STATUS_PATH_STOP;
		break;
	}

	return ret;
}

static void pipeline_schedule_triggered(struct pipeline_walk_context *ctx,
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

/* trigger pipeline */
int pipeline_trigger(struct pipeline *p, struct comp_dev *host, int cmd)
{
	struct pipeline_data data;
	struct pipeline_walk_context walk_ctx = {
		.comp_func = pipeline_comp_trigger,
		.comp_data = &data,
		.skip_incomplete = true,
	};
	int ret;

	pipe_info(p, "pipe trigger cmd %d", cmd);

	list_init(&walk_ctx.pipelines);

	/* handle pipeline global checks before going into each components */
	if (p->xrun_bytes) {
		ret = pipeline_xrun_handle_trigger(p, cmd);
		if (ret < 0) {
			pipe_err(p, "xrun handle: ret = %d", ret);
			return ret;
		} else if (ret == PPL_STATUS_PATH_STOP)
			/* no further action needed*/
			return 0;
	}

	data.start = host;
	data.cmd = cmd;

	ret = walk_ctx.comp_func(host, NULL, &walk_ctx, host->direction);
	if (ret < 0) {
		pipe_err(p, "pipeline_trigger(): ret = %d, host->comp.id = %u, cmd = %d",
			 ret, dev_comp_id(host), cmd);
	}

	pipeline_schedule_triggered(&walk_ctx, cmd);

	return ret;
}

static int pipeline_comp_reset(struct comp_dev *current,
			       struct comp_buffer *calling_buf,
			       struct pipeline_walk_context *ctx, int dir)
{
	struct pipeline *p = ctx->comp_data;
	int stream_direction = dir;
	int end_type;
	int is_single_ppl = comp_is_single_pipeline(current, p->source_comp);
	int is_same_sched =
		pipeline_is_same_sched_comp(current->pipeline, p);
	int err;

	pipe_dbg(current->pipeline, "pipeline_comp_reset(), current->comp.id = %u, dir = %u",
		 dev_comp_id(current), dir);

	/* reset should propagate to the connected pipelines,
	 * which need to be scheduled together
	 */
	if (!is_single_ppl && !is_same_sched) {
		/* If pipeline connected to the starting one is in improper
		 * direction (CAPTURE towards DAI, PLAYBACK towards HOST),
		 * stop propagation. Direction param of the pipeline can not be
		 * trusted at this point, as it might not be configured yet,
		 * hence checking for endpoint component type.
		 */
		end_type = comp_get_endpoint_type(current->pipeline->sink_comp);
		if (stream_direction == SOF_IPC_STREAM_PLAYBACK) {
			if (end_type == COMP_ENDPOINT_HOST ||
			    end_type == COMP_ENDPOINT_NODE)
				return 0;
		}

		if (stream_direction == SOF_IPC_STREAM_CAPTURE) {
			if (end_type == COMP_ENDPOINT_DAI ||
			    end_type == COMP_ENDPOINT_NODE)
				return 0;
		}
	}

	err = comp_reset(current);
	if (err < 0 || err == PPL_STATUS_PATH_STOP)
		return err;

	return pipeline_for_each_comp(current, ctx, dir);
}

/* reset the whole pipeline */
int pipeline_reset(struct pipeline *p, struct comp_dev *host)
{
	struct pipeline_walk_context walk_ctx = {
		.comp_func = pipeline_comp_reset,
		.comp_data = p,
		.buff_func = buffer_reset_params,
		.skip_incomplete = true,
	};
	int ret;

	pipe_info(p, "pipe reset");

	ret = walk_ctx.comp_func(host, NULL, &walk_ctx, host->direction);
	if (ret < 0) {
		pipe_err(p, "pipeline_reset(): ret = %d, host->comp.id = %u",
			 ret, dev_comp_id(host));
	}

	return ret;
}

static int pipeline_comp_copy(struct comp_dev *current,
			      struct comp_buffer *calling_buf,
			      struct pipeline_walk_context *ctx, int dir)
{
	struct pipeline_data *ppl_data = ctx->comp_data;
	int is_single_ppl = comp_is_single_pipeline(current, ppl_data->start);
	int err;

	pipe_dbg(current->pipeline, "pipeline_comp_copy(), current->comp.id = %u, dir = %u",
		 dev_comp_id(current), dir);

	if (!is_single_ppl) {
		pipe_dbg(current->pipeline, "pipeline_comp_copy(), current is from another pipeline and can't be scheduled together");
		return 0;
	}

	if (!comp_is_active(current)) {
		pipe_dbg(current->pipeline, "pipeline_comp_copy(), current is not active");
		return 0;
	}

	/* copy to downstream immediately */
	if (dir == PPL_DIR_DOWNSTREAM) {
		err = comp_copy(current);
		if (err < 0 || err == PPL_STATUS_PATH_STOP)
			return err;
	}

	err = pipeline_for_each_comp(current, ctx, dir);
	if (err < 0 || err == PPL_STATUS_PATH_STOP)
		return err;

	if (dir == PPL_DIR_UPSTREAM)
		err = comp_copy(current);

	return err;
}

/* Copy data across all pipeline components.
 * For capture pipelines it always starts from source component
 * and continues downstream and for playback pipelines it first
 * copies sink component itself and then goes upstream.
 */
static int pipeline_copy(struct pipeline *p)
{
	struct pipeline_data data;
	struct pipeline_walk_context walk_ctx = {
		.comp_func = pipeline_comp_copy,
		.comp_data = &data,
		.skip_incomplete = true,
	};
	struct comp_dev *start;
	uint32_t dir;
	int ret;

	if (p->source_comp->direction == SOF_IPC_STREAM_PLAYBACK) {
		dir = PPL_DIR_UPSTREAM;
		start = p->sink_comp;
	} else {
		dir = PPL_DIR_DOWNSTREAM;
		start = p->source_comp;
	}

	data.start = start;
	data.p = p;

	ret = walk_ctx.comp_func(start, NULL, &walk_ctx, dir);
	if (ret < 0)
		pipe_err(p, "pipeline_copy(): ret = %d, start->comp.id = %u, dir = %u",
			 ret, dev_comp_id(start), dir);

	return ret;
}

/* Walk the graph to active components in any pipeline to find
 * the first active DAI and return it's timestamp.
 */
static int pipeline_comp_timestamp(struct comp_dev *current,
				   struct comp_buffer *calling_buf,
				   struct pipeline_walk_context *ctx, int dir)
{
	struct pipeline_data *ppl_data = ctx->comp_data;

	if (!comp_is_active(current)) {
		pipe_dbg(current->pipeline, "pipeline_comp_timestamp(), current is not active");
		return 0;
	}

	/* is component a DAI endpoint? */
	if (current != ppl_data->start &&
	    (dev_comp_type(current) == SOF_COMP_DAI ||
	    dev_comp_type(current) == SOF_COMP_SG_DAI)) {
		platform_dai_timestamp(current, ppl_data->posn);
		return -1;
	}

	return pipeline_for_each_comp(current, ctx, dir);
}

/* Get the timestamps for host and first active DAI found. */
void pipeline_get_timestamp(struct pipeline *p, struct comp_dev *host,
			    struct sof_ipc_stream_posn *posn)
{
	struct pipeline_data data;
	struct pipeline_walk_context walk_ctx = {
		.comp_func = pipeline_comp_timestamp,
		.comp_data = &data,
		.skip_incomplete = true,
	};

	platform_host_timestamp(host, posn);

	data.start = host;
	data.posn = posn;

	walk_ctx.comp_func(host, NULL, &walk_ctx, host->direction);

	/* set timestamp resolution */
	posn->timestamp_ns = p->ipc_pipe.period * 1000;
}

static int pipeline_comp_xrun(struct comp_dev *current,
			      struct comp_buffer *calling_buf,
			      struct pipeline_walk_context *ctx, int dir)
{
	struct pipeline_data *ppl_data = ctx->comp_data;

	if (dev_comp_type(current) == SOF_COMP_HOST) {
		/* get host timestamps */
		platform_host_timestamp(current, ppl_data->posn);

		/* send XRUN to host */
		mailbox_stream_write(ppl_data->p->posn_offset, ppl_data->posn,
				     sizeof(*ppl_data->posn));
		ipc_msg_send(ppl_data->p->msg, ppl_data->posn, true);
	}

	return pipeline_for_each_comp(current, ctx, dir);
}

/* Send an XRUN to each host for this component. */
void pipeline_xrun(struct pipeline *p, struct comp_dev *dev,
		   int32_t bytes)
{
	struct pipeline_data data;
	struct pipeline_walk_context walk_ctx = {
		.comp_func = pipeline_comp_xrun,
		.comp_data = &data,
		.skip_incomplete = true,
	};
	struct sof_ipc_stream_posn posn;
	int ret;

	/* don't flood host */
	if (p->xrun_bytes)
		return;

	/* only send when we are running */
	if (dev->state != COMP_STATE_ACTIVE)
		return;

	/* notify all pipeline comps we are in XRUN, and stop copying */
	ret = pipeline_trigger(p, p->source_comp, COMP_TRIGGER_XRUN);
	if (ret < 0)
		pipe_err(p, "pipeline_xrun(): Pipelines notification about XRUN failed, ret = %d",
			 ret);

	memset(&posn, 0, sizeof(posn));
	ipc_build_stream_posn(&posn, SOF_IPC_STREAM_TRIG_XRUN,
			      dev_comp_id(dev));
	p->xrun_bytes = bytes;
	posn.xrun_size = bytes;
	posn.xrun_comp_id = dev_comp_id(dev);
	data.posn = &posn;
	data.p = p;

	walk_ctx.comp_func(dev, NULL, &walk_ctx, dev->direction);
}

#if NO_XRUN_RECOVERY
/* recover the pipeline from a XRUN condition */
static int pipeline_xrun_recover(struct pipeline *p)
{
	return -EINVAL;
}

#else
/* recover the pipeline from a XRUN condition */
static int pipeline_xrun_recover(struct pipeline *p)
{
	int ret;

	pipe_err(p, "pipeline_xrun_recover()");

	/* prepare the pipeline */
	ret = pipeline_prepare(p, p->source_comp);
	if (ret < 0) {
		pipe_err(p, "pipeline_xrun_recover(): pipeline_prepare() failed, ret = %d",
			 ret);
		return ret;
	}

	/* reset xrun status as we already in prepared */
	p->xrun_bytes = 0;

	/* restart pipeline comps */
	ret = pipeline_trigger(p, p->source_comp, COMP_TRIGGER_START);
	if (ret < 0) {
		pipe_err(p, "pipeline_xrun_recover(): pipeline_trigger() failed, ret = %d",
			 ret);
		return ret;
	}

	return 0;
}
#endif

/* notify pipeline that this component requires buffers emptied/filled */
void pipeline_schedule_copy(struct pipeline *p, uint64_t start)
{
	/* disable system agent panic for DMA driven pipelines */
	if (!pipeline_is_timer_driven(p))
		sa_set_panic_on_delay(false);

	schedule_task(p->pipe_task, start, p->ipc_pipe.period);
}

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
