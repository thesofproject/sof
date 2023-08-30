// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>

#include <sof/audio/buffer.h>
#include <sof/audio/component_ext.h>
#include <sof/audio/pipeline.h>
#include <sof/lib/memory.h>
#include <sof/lib/mm_heap.h>
#include <sof/compiler_attributes.h>
#include <sof/list.h>
#include <rtos/spinlock.h>
#include <ipc/stream.h>
#include <ipc/topology.h>
#include <ipc4/error_status.h>
#include <ipc4/module.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

LOG_MODULE_DECLARE(pipe, CONFIG_SOF_LOG_LEVEL);

static int pipeline_comp_params_neg(struct comp_dev *current,
				    struct comp_buffer *calling_buf,
				    struct pipeline_walk_context *ctx,
				    int dir)
{
	struct pipeline_data *ppl_data = ctx->comp_data;
	int err = 0;

	pipe_dbg(current->pipeline, "pipeline_comp_params_neg(), current->comp.id = %u, dir = %u",
		 dev_comp_id(current), dir);

	/* check if 'current' is already configured */
	switch (current->state) {
	case COMP_STATE_INIT:
	case COMP_STATE_READY:
		/*
		 * Negotiation only happen when the current component has > 1
		 * source or sink, we are propagating the params to branched
		 * buffers, and the subsequent component's .params() or .prepare()
		 * should be responsible for calibrating if needed. For example,
		 * a component who has different channels input/output buffers
		 * should explicitly configure the channels of the branched buffers.
		 */
		err = buffer_set_params(calling_buf, &ppl_data->params->params,
					BUFFER_UPDATE_FORCE);
		break;
	default:
		/* return 0 if params matches */
		if (!buffer_params_match(calling_buf,
					 &ppl_data->params->params,
					 BUFF_PARAMS_FRAME_FMT |
					 BUFF_PARAMS_RATE)) {
			/*
			 * parameters conflict with an active pipeline,
			 * drop an error and reject the .params() command.
			 */
			pipe_err(current->pipeline,
				 "pipeline_comp_params_neg(): params conflict with existing active pipeline!");
			err = -EINVAL;
		}
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
		.incoming = calling_buf,
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

/* save params changes made by component */
static void pipeline_update_buffer_pcm_params(struct comp_buffer *buffer,
					      void *data)
{
	struct sof_ipc_stream_params *params = data;
	int i;

	params->buffer_fmt = audio_stream_get_buffer_fmt(&buffer->stream);
	params->frame_fmt = audio_stream_get_frm_fmt(&buffer->stream);
	params->rate = audio_stream_get_rate(&buffer->stream);
	params->channels = audio_stream_get_channels(&buffer->stream);
	for (i = 0; i < SOF_IPC_MAX_CHANNELS; i++)
		params->chmap[i] = buffer->chmap[i];
}

/* fetch hardware stream parameters from DAI  */
static int pipeline_comp_hw_params(struct comp_dev *current,
				   struct comp_buffer *calling_buf,
				   struct pipeline_walk_context *ctx, int dir)
{
	struct pipeline_data *ppl_data = ctx->comp_data;
	int ret;

	pipe_dbg(current->pipeline, "pipeline_comp_hw_params(), current->comp.id = %u, dir = %u",
		 dev_comp_id(current), dir);

	ret = pipeline_for_each_comp(current, ctx, dir);
	if (ret < 0)
		return ret;

	/* Fetch hardware stream parameters from DAI component */
	if (dev_comp_type(current) == SOF_COMP_DAI) {
		ret = comp_dai_get_hw_params(current,
					     &ppl_data->params->params, dir);
		if (ret < 0) {
			pipe_err(current->pipeline,
				 "pipeline_comp_hw_params(): failed getting DAI parameters: %d",
				 ret);
			return ret;
		}
	}

	return ret;
}

/* propagate hw_params to buffers in pipeline. */
static int pipeline_comp_hw_params_buf(struct comp_dev *current,
				       struct comp_buffer *calling_buf,
				       struct pipeline_walk_context *ctx, int dir)
{
	struct pipeline_data *ppl_data = ctx->comp_data;
	int ret;

	ret = pipeline_for_each_comp(current, ctx, dir);
	if (ret < 0)
		return ret;
	/* set buffer parameters */
	if (calling_buf) {
		ret = buffer_set_params(calling_buf, &ppl_data->params->params,
					BUFFER_UPDATE_IF_UNSET);
		if (ret < 0)
			pipe_err(current->pipeline,
				 "pipeline_comp_hw_params(): buffer_set_params(): %d", ret);
	}

	return ret;
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
	struct pipeline_data data = {
		.start = host,
		.params = &hw_params,
	};
	struct pipeline_walk_context hw_param_ctx = {
		.comp_func = pipeline_comp_hw_params,
		.comp_data = &data,
		.skip_incomplete = true,
	};
	struct pipeline_walk_context buf_param_ctx = {
		.comp_func = pipeline_comp_hw_params_buf,
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

	ret = hw_param_ctx.comp_func(host, NULL, &hw_param_ctx, dir);
	if (ret < 0) {
		pipe_err(p, "pipeline_params(): ret = %d, dev->comp.id = %u",
			 ret, dev_comp_id(host));
		return ret;
	}

	ret = buf_param_ctx.comp_func(host, NULL, &buf_param_ctx, dir);
	if (ret < 0) {
		pipe_err(p, "pipeline_params(): ret = %d, dev->comp.id = %u",
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

#if CONFIG_DEBUG_HEAP
	/* show heap status update with this pipeline run */
	heap_trace_all(0);
#endif
	return ret;
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
		/* ipc4 module is only prepared in its parent pipeline */
		if (IPC4_MOD_ID(current->ipc_config.id))
			return 0;

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

	if (current->ipc_config.proc_domain == COMP_PROCESSING_DOMAIN_LL) {
		/* init a task for LL module, DP task has been created in during init_instance */
		err = pipeline_comp_ll_task_init(current->pipeline);
		if (err < 0)
			return err;
	}

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

	pipe_dbg(p, "pipe prepare");

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
