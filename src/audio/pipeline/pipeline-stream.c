// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>

#include <sof/audio/buffer.h>
#include <sof/audio/component_ext.h>
#include <sof/audio/pipeline.h>
#include <sof/list.h>
#include <sof/spinlock.h>
#include <sof/string.h>
#include <ipc/stream.h>
#include <ipc/topology.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

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
int pipeline_copy(struct pipeline *p)
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
	posn->timestamp_ns = p->period * 1000;
}
