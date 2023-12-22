// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>

#include <sof/audio/component_ext.h>
#include <sof/audio/pipeline.h>
#include <ipc/stream.h>
#include <ipc/topology.h>

LOG_MODULE_DECLARE(pipe, CONFIG_SOF_LOG_LEVEL);

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
bool pipeline_should_report_enodata_on_trigger(struct comp_dev *rsrc,
					       struct pipeline_walk_context *ctx,
					       int dir)
{
	struct pipeline_data *ppl_data = ctx->comp_data;
	struct comp_dev *pipe_source = ppl_data->start->pipeline->source_comp;

	/* In IPC3, FW propagates triggers to connected pipelines, so
	 * it can have determistic logic to conclude no data is
	 * available.
	 */
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

	/* source pipeline may not be active since priority is not higher than current one */
	if (rsrc->pipeline->priority <= ppl_data->start->pipeline->priority)
		return false;

	/* if component on which we depend to provide data is inactive, then the
	 * pipeline has no means of providing data
	 */
	if (rsrc->state != COMP_STATE_ACTIVE)
		return true;

	return false;
}

struct dai_data *get_pipeline_dai_device_data(struct comp_dev *dev)
{
	return comp_get_drvdata(dev);
}

int pipeline_is_single_triggered(bool is_single_ppl, bool is_same_sched, struct comp_dev *current,
				 struct pipeline_walk_context *ctx, int dir)
{
	if (!is_single_ppl && !is_same_sched) {
		pipe_dbg(current->pipeline,
			 "pipeline_comp_trigger(), current is from another pipeline");

		if (pipeline_should_report_enodata_on_trigger(current, ctx, dir))
			return -ENODATA;
		/* return EPIPE used to indicate propogation stopped
		 * since it is neither single pipe nor same schedule
		 */
		return -EPIPE;
	}

	return 0;
}
