// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>

#include <sof/audio/component_ext.h>
#include <ipc/topology.h>
#include <sof/ipc/common.h>
#include <sof/audio/buffer.h>
#include <ipc4/base-config.h>
#include <sof/audio/pipeline.h>
#include <sof/lib/cpu.h>
#include <module/module/base.h>
#include "../audio/copier/copier.h"

LOG_MODULE_DECLARE(pipe, CONFIG_SOF_LOG_LEVEL);

/* Playback only: visit connected pipeline to find the dai comp and latency.
 * This function walks down through a pipelines chain looking for the target dai component.
 * Calculates the delay of each pipeline by determining the number of buffered blocks.
 */
struct comp_dev *pipeline_get_dai_comp_latency(uint32_t pipeline_id, uint32_t *latency)
{
	struct ipc_comp_dev *ipc_sink;
	struct ipc_comp_dev *ipc_source;
	struct comp_dev *source;
	struct ipc *ipc = ipc_get();

	*latency = 0;

	/* Walks through the ipc component list and get source endpoint component of the given
	 * pipeline.
	 */
	ipc_source = ipc_get_ppl_src_comp(ipc, pipeline_id);
	if (!ipc_source)
		return NULL;
	source = ipc_source->cd;

	/* Walks through the ipc component list and get sink endpoint component of the given
	 * pipeline. This function returns the first sink. We assume that dai is connected to pin 0.
	 */
	ipc_sink = ipc_get_ppl_sink_comp(ipc, pipeline_id);
	while (ipc_sink) {
		struct comp_buffer *buffer;
		uint64_t input_data, output_data;
		struct ipc4_base_module_cfg input_base_cfg = {.ibs = 0};
		struct ipc4_base_module_cfg output_base_cfg = {.obs = 0};
		int ret;

		/* Calculate pipeline latency */
		input_data = comp_get_total_data_processed(source, 0, true);
		output_data = comp_get_total_data_processed(ipc_sink->cd, 0, false);

		ret = comp_get_attribute(source, COMP_ATTR_BASE_CONFIG, &input_base_cfg);
		if (ret < 0)
			return NULL;

		ret = comp_get_attribute(ipc_sink->cd, COMP_ATTR_BASE_CONFIG, &output_base_cfg);
		if (ret < 0)
			return NULL;

		if (input_data && output_data && input_base_cfg.ibs && output_base_cfg.obs)
			*latency += input_data / input_base_cfg.ibs -
				output_data / output_base_cfg.obs;

		/* If the component doesn't have a sink buffer, it can be a dai. */
		if (list_is_empty(&ipc_sink->cd->bsink_list))
			return dev_comp_type(ipc_sink->cd) == SOF_COMP_DAI ? ipc_sink->cd : NULL;

		/* Get a component connected to our sink buffer - hop to a next pipeline */
		buffer = buffer_from_list(comp_buffer_list(ipc_sink->cd, PPL_DIR_DOWNSTREAM)->next,
					  PPL_DIR_DOWNSTREAM);
		source = buffer_get_comp(buffer, PPL_DIR_DOWNSTREAM);

		/* buffer_comp is in another pipeline and it is not complete */
		if (!source || !source->pipeline)
			return NULL;

		/* As pipeline data is allocated in cached space, continue calculation for next
		 * connected pipeline only if that pipeline is on same core.
		 * This is a workaround, the real solution would be to use something like
		 * process_on_core() to continue calculation on required core. However, as this
		 * "latency feature" seems never used anyway, this workaround could be enough.
		 */
		if (!cpu_is_me(source->ipc_config.core))
			return NULL;

		/* Get a next sink component */
		ipc_sink = ipc_get_ppl_sink_comp(ipc, source->pipeline->pipeline_id);
	}

	return NULL;
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
bool pipeline_should_report_enodata_on_trigger(struct comp_dev *rsrc,
					       struct pipeline_walk_context *ctx,
					       int dir)
{
	/*
	 * In IPC4, host controls state of each pipeline separately,
	 * so FW cannot reliably detect case of no data based on
	 * observing state of src->pipeline here.
	 */
	return false;
}

struct dai_data *get_pipeline_dai_device_data(struct comp_dev *dev)
{
	struct processing_module *mod = comp_get_drvdata(dev);
	struct copier_data *cd = module_get_private_data(mod);

	return cd->dd[0];
}

int pipeline_is_single_triggered(bool is_single_ppl, bool is_same_sched, struct comp_dev *current,
				 struct pipeline_walk_context *ctx, int dir)
{
	if (!is_single_ppl) {
		pipe_dbg(current->pipeline,
			 "pipeline_comp_trigger(), current is from another pipeline");

		/* return EPIPE used to indicate propogation stopped
		 * since it is not single pipe.
		 */
		return -EPIPE;
	}

	return 0;
}
