// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Lech Betlej <lech.betlej@linux.intel.com>

/**
 * \file audio/selector.c
 * \brief Audio channel selection component. In case 1 output channel is
 * \brief selected in topology the component provides the selected channel on
 * \brief output. In case 2 or 4 channels are selected on output the component
 * \brief works in a passthrough mode.
 * \authors Lech Betlej <lech.betlej@linux.intel.com>
 */

#include <sof/audio/component.h>
#include <sof/audio/pipeline.h>
#include <sof/audio/selector.h>
#include <sof/common.h>
#include <sof/debug/panic.h>
#include <sof/drivers/ipc.h>
#include <sof/lib/alloc.h>
#include <sof/lib/cache.h>
#include <sof/lib/memory.h>
#include <sof/list.h>
#include <sof/string.h>
#include <sof/trace/trace.h>
#include <ipc/control.h>
#include <ipc/stream.h>
#include <ipc/topology.h>
#include <kernel/abi.h>
#include <user/trace.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

/**
 * \brief Validates channel count and index and sets channel count.
 * \details If input data is not supported trace error is displayed and
 * \details input data is modified to safe values.
 * \param[in,out] cd Selector component private data.
 * \param[in] in_channels Number of input channels to the module.
 * \param[in] out_channels Number of output channels to the module.
 * \param[in] ch_idx Index of a channel to be provided on output.
 * \return Status.
 */
static int sel_set_channel_values(struct comp_data *cd, uint32_t in_channels,
				  uint32_t out_channels, uint32_t ch_idx)
{
	/* verify input channels */
	if (in_channels != SEL_SOURCE_2CH && in_channels != SEL_SOURCE_4CH) {
		trace_selector_error("sel_set_channel_values() error: "
				     "in_channels = %u", in_channels);
		return -EINVAL;
	}

	/* verify output channels */
	if (out_channels != SEL_SINK_1CH && out_channels != SEL_SINK_2CH &&
	    out_channels != SEL_SINK_4CH) {
		trace_selector_error("sel_set_channel_values() error: "
				     "out_channels = %u", out_channels);
		return -EINVAL;
	}

	/* verify proper channels for passthrough mode */
	if (out_channels != SEL_SINK_1CH && in_channels != out_channels) {
		trace_selector_error("sel_set_channel_values() error: "
				     "in_channels = %u, out_channels = %u",
				     in_channels, out_channels);
		return -EINVAL;
	}

	if (ch_idx > (SEL_SOURCE_4CH - 1)) {
		trace_selector_error("sel_set_channel_values() error: "
				     "ch_idx = %u", in_channels);
		return -EINVAL;
	}

	cd->config.in_channels_count = in_channels;
	cd->config.out_channels_count = out_channels;
	cd->config.sel_channel = ch_idx;

	return 0;
}

/**
 * \brief Creates selector component.
 * \param[in,out] data Selector base component device.
 * \return Pointer to selector base component device.
 */
static struct comp_dev *selector_new(struct sof_ipc_comp *comp)
{
	struct sof_ipc_comp_process *ipc_process =
		(struct sof_ipc_comp_process *)comp;
	size_t bs = ipc_process->size;
	struct comp_dev *dev;
	struct comp_data *cd;
	int ret;

	trace_selector("selector_new()");

	if (IPC_IS_SIZE_INVALID(ipc_process->config)) {
		IPC_SIZE_ERROR_TRACE(TRACE_CLASS_SELECTOR, ipc_process->config);
		return NULL;
	}

	dev = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM,
		      COMP_SIZE(struct sof_ipc_comp_process));
	if (!dev)
		return NULL;

	ret = memcpy_s(&dev->comp, sizeof(struct sof_ipc_comp_process), comp,
		       sizeof(struct sof_ipc_comp_process));
	assert(!ret);

	cd = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(*cd));
	if (!cd) {
		rfree(dev);
		return NULL;
	}

	comp_set_drvdata(dev, cd);

	ret = memcpy_s(&cd->config, sizeof(cd->config), ipc_process->data, bs);
	assert(!ret);

	/* verification of initial parameters */
	ret = sel_set_channel_values(cd, cd->config.in_channels_count,
				     cd->config.out_channels_count,
				     cd->config.sel_channel);
	if (ret < 0) {
		rfree(cd);
		rfree(dev);
		return NULL;
	}

	dev->state = COMP_STATE_READY;
	return dev;
}

/**
 * \brief Frees selector component.
 * \param[in,out] dev Selector base component device.
 */
static void selector_free(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	trace_selector_with_ids(dev, "selector_free()");

	rfree(cd);
	rfree(dev);
}

/**
 * \brief Sets selector component audio stream parameters.
 * \param[in,out] dev Selector base component device.
 * \return Error code.
 *
 * All done in prepare since we need to know source and sink component params.
 */
static int selector_params(struct comp_dev *dev,
			   struct sof_ipc_stream_params *params)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct comp_buffer *sinkb;
	struct comp_buffer *sourceb;

	trace_selector_with_ids(dev, "selector_params()");

	sourceb = list_first_item(&dev->bsource_list, struct comp_buffer,
				  sink_list);
	sinkb = list_first_item(&dev->bsink_list, struct comp_buffer,
				source_list);

	/* rewrite channels number for other components */
	if (dev->direction == SOF_IPC_STREAM_PLAYBACK)
		sinkb->stream.channels = cd->config.out_channels_count;
	else
		sourceb->stream.channels = cd->config.in_channels_count;

	return PPL_STATUS_PATH_STOP;
}

/**
 * \brief Sets selector control command.
 * \param[in,out] dev Selector base component device.
 * \param[in,out] cdata Control command data.
 * \return Error code.
 */
static int selector_ctrl_set_data(struct comp_dev *dev,
				  struct sof_ipc_ctrl_data *cdata)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct sof_sel_config *cfg;
	int ret = 0;

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_BINARY:
		trace_selector_with_ids(dev, "selector_ctrl_set_data(), "
					"SOF_CTRL_CMD_BINARY");

		cfg = (struct sof_sel_config *)
		      ASSUME_ALIGNED(cdata->data->data, 4);

		/* Just copy the configuration & verify input params.*/
		ret = sel_set_channel_values(cd, cfg->in_channels_count,
					     cfg->out_channels_count,
					     cfg->sel_channel);
		break;
	default:
		trace_selector_error_with_ids(dev, "selector_ctrl_set_cmd() "
					      "error: invalid cdata->cmd = %u",
					      cdata->cmd);
		ret = -EINVAL;
		break;
	}

	return ret;
}

/**
 * \brief Gets selector control command.
 * \param[in,out] dev Selector base component device.
 * \param[in,out] cdata Selector command data.
 * \param[in] size Command data size.
 * \return Error code.
 */
static int selector_ctrl_get_data(struct comp_dev *dev,
				  struct sof_ipc_ctrl_data *cdata, int size)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	int ret = 0;

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_BINARY:
		trace_selector_with_ids(dev, "selector_ctrl_get_data(), "
					"SOF_CTRL_CMD_BINARY");

		/* Copy back to user space */
		ret = memcpy_s(cdata->data->data, ((struct sof_abi_hdr *)
			       (cdata->data))->size, &cd->config,
			       sizeof(cd->config));
		assert(!ret);

		cdata->data->abi = SOF_ABI_VERSION;
		cdata->data->size = sizeof(cd->config);
		break;

	default:
		trace_selector_error_with_ids(dev, "selector_ctrl_get_data() "
					      "error: invalid cdata->cmd");
		ret = -EINVAL;
		break;
	}

	return ret;
}

/**
 * \brief Used to pass standard and bespoke commands (with data) to component.
 * \param[in,out] dev Selector base component device.
 * \param[in] cmd Command type.
 * \param[in,out] data Control command data.
 * \param[in] max_data_size Command max data size.
 * \return Error code.
 */
static int selector_cmd(struct comp_dev *dev, int cmd, void *data,
			int max_data_size)
{
	struct sof_ipc_ctrl_data *cdata = data;
	int ret = 0;

	trace_selector_with_ids(dev, "selector_cmd()");

	switch (cmd) {
	case COMP_CMD_SET_DATA:
		ret = selector_ctrl_set_data(dev, cdata);
		break;
	case COMP_CMD_GET_DATA:
		ret = selector_ctrl_get_data(dev, cdata, max_data_size);
		break;
	case COMP_CMD_SET_VALUE:
		trace_selector_with_ids(dev,
					"selector_cmd(), COMP_CMD_SET_VALUE");
		break;
	case COMP_CMD_GET_VALUE:
		trace_selector_with_ids(dev,
					"selector_cmd(), COMP_CMD_GET_VALUE");
		break;
	default:
		trace_selector_error_with_ids(dev, "selector_cmd() error: "
					      "invalid command");
		ret = -EINVAL;
	}

	return ret;
}

/**
 * \brief Sets component state.
 * \param[in,out] dev Selector base component device.
 * \param[in] cmd Command type.
 * \return Error code.
 */
static int selector_trigger(struct comp_dev *dev, int cmd)
{
	int ret;

	trace_selector_with_ids(dev, "selector_trigger()");

	ret = comp_set_state(dev, cmd);
	return ret == 0 ? PPL_STATUS_PATH_STOP : ret;
}

/**
 * \brief Copies and processes stream data.
 * \param[in,out] dev Selector base component device.
 * \return Error code.
 */
static int selector_copy(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct comp_buffer *sink;
	struct comp_buffer *source;
	uint32_t frames;
	uint32_t source_bytes;
	uint32_t sink_bytes;

	tracev_selector_with_ids(dev, "selector_copy()");

	/* selector component will have 1 source and 1 sink buffer */
	source = list_first_item(&dev->bsource_list, struct comp_buffer,
				 sink_list);
	sink = list_first_item(&dev->bsink_list, struct comp_buffer,
			       source_list);

	frames = audio_stream_avail_frames(&source->stream, &sink->stream);
	source_bytes = frames * audio_stream_frame_bytes(&source->stream);
	sink_bytes = frames * audio_stream_frame_bytes(&sink->stream);

	tracev_selector_with_ids(dev, "selector_copy(), "
				 "source_bytes = 0x%x, "
				 "sink_bytes = 0x%x",
				 source_bytes, sink_bytes);

	/* copy selected channels from in to out */
	cd->sel_func(dev, &sink->stream, &source->stream, frames);

	/* calculate new free and available */
	comp_update_buffer_produce(sink, sink_bytes);
	comp_update_buffer_consume(source, source_bytes);

	return 0;
}

/**
 * \brief Prepares selector component for processing.
 * \param[in,out] dev Selector base component device.
 * \return Error code.
 */
static int selector_prepare(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct comp_buffer *sinkb;
	struct comp_buffer *sourceb;
	struct sof_ipc_comp_config *config = COMP_GET_CONFIG(dev);
	int ret;

	trace_selector_with_ids(dev, "selector_prepare()");

	ret = comp_set_state(dev, COMP_TRIGGER_PREPARE);
	if (ret < 0)
		return ret;

	if (ret == COMP_STATUS_STATE_ALREADY_SET)
		return PPL_STATUS_PATH_STOP;

	/* selector component will have 1 source and 1 sink buffer */
	sourceb = list_first_item(&dev->bsource_list, struct comp_buffer,
				  sink_list);
	sinkb = list_first_item(&dev->bsink_list, struct comp_buffer,
				source_list);

	/* get source data format and period bytes */
	cd->source_format = sourceb->stream.frame_fmt;
	cd->source_period_bytes =
		audio_stream_period_bytes(&sourceb->stream, dev->frames);

	/* get sink data format and period bytes */
	cd->sink_format = sinkb->stream.frame_fmt;
	cd->sink_period_bytes =
		audio_stream_period_bytes(&sinkb->stream, dev->frames);

	/* There is an assumption that sink component will report out
	 * proper number of channels [1] for selector to actually
	 * reduce channel count between source and sink
	 */
	trace_selector_with_ids(dev,
				"selector_prepare(): sourceb->schannels = %u",
				sourceb->stream.channels);
	trace_selector_with_ids(dev,
				"selector_prepare(): sinkb->channels = %u",
				sinkb->stream.channels);

	if (sinkb->stream.size < config->periods_sink * cd->sink_period_bytes) {
		trace_selector_error_with_ids(dev, "selector_prepare() error: "
					      "sink buffer size is insufficient");
		ret = -ENOMEM;
		goto err;
	}

	/* validate */
	if (cd->sink_period_bytes == 0) {
		trace_selector_error_with_ids(dev, "selector_prepare() error: "
				     "cd->sink_period_bytes = 0, dev->frames ="
				     " %u", dev->frames);
		ret = -EINVAL;
		goto err;
	}

	if (cd->source_period_bytes == 0) {
		trace_selector_error_with_ids(dev, "selector_prepare() error: "
				     "cd->source_period_bytes = 0, "
				     "dev->frames = %u", dev->frames);
		ret = -EINVAL;
		goto err;
	}

	cd->sel_func = sel_get_processing_function(dev);
	if (!cd->sel_func) {
		trace_selector_error_with_ids(dev, "selector_prepare() error: "
				     "invalid cd->sel_func, "
				     "cd->source_format = %u, "
				     "cd->sink_format = %u, "
				     "cd->out_channels_count = %u",
				     cd->source_format, cd->sink_format,
				     cd->config.out_channels_count);
		ret = -EINVAL;
		goto err;
	}

	return PPL_STATUS_PATH_STOP;

err:
	comp_set_state(dev, COMP_TRIGGER_RESET);
	return ret;
}

/**
 * \brief Resets selector component.
 * \param[in,out] dev Selector base component device.
 * \return Error code.
 */
static int selector_reset(struct comp_dev *dev)
{
	int ret;

	trace_selector_with_ids(dev, "selector_reset()");

	ret = comp_set_state(dev, COMP_TRIGGER_RESET);
	return ret == 0 ? PPL_STATUS_PATH_STOP : ret;
}

/**
 * \brief Executes cache operation on selector component.
 * \param[in,out] dev Selector base component device.
 * \param[in] cmd Cache command.
 */
static void selector_cache(struct comp_dev *dev, int cmd)
{
	struct comp_data *cd;

	switch (cmd) {
	case CACHE_WRITEBACK_INV:
		trace_selector_with_ids(dev, "selector_cache(), "
					"CACHE_WRITEBACK_INV");

		cd = comp_get_drvdata(dev);

		dcache_writeback_invalidate_region(cd, sizeof(*cd));
		dcache_writeback_invalidate_region(dev, sizeof(*dev));
		break;

	case CACHE_INVALIDATE:
		trace_selector_with_ids(dev, "selector_cache(), "
					"CACHE_INVALIDATE");

		dcache_invalidate_region(dev, sizeof(*dev));

		cd = comp_get_drvdata(dev);
		dcache_invalidate_region(cd, sizeof(*cd));
		break;
	}
}

/** \brief Selector component definition. */
static const struct comp_driver comp_selector = {
	.type	= SOF_COMP_SELECTOR,
	.ops	= {
		.new		= selector_new,
		.free		= selector_free,
		.params		= selector_params,
		.cmd		= selector_cmd,
		.trigger	= selector_trigger,
		.copy		= selector_copy,
		.prepare	= selector_prepare,
		.reset		= selector_reset,
		.cache		= selector_cache,
	},
};

static SHARED_DATA struct comp_driver_info comp_selector_info = {
	.drv = &comp_selector,
};

/** \brief Initializes selector component. */
static void sys_comp_selector_init(void)
{
	comp_register(platform_shared_get(&comp_selector_info,
					  sizeof(comp_selector_info)));
}

DECLARE_MODULE(sys_comp_selector_init);
