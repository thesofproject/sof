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
#include <sof/ipc/msg.h>
#include <sof/lib/alloc.h>
#include <sof/lib/memory.h>
#include <sof/lib/uuid.h>
#include <sof/list.h>
#include <sof/string.h>
#include <sof/trace/trace.h>
#include <sof/ut.h>
#include <ipc/control.h>
#include <ipc/stream.h>
#include <ipc/topology.h>
#include <kernel/abi.h>
#include <user/trace.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

static const struct comp_driver comp_selector;

/* 55a88ed5-3d18-46ca-88f1-0ee6eae9930f */
DECLARE_SOF_RT_UUID("selector", selector_uuid, 0x55a88ed5, 0x3d18, 0x46ca,
		 0x88, 0xf1, 0x0e, 0xe6, 0xea, 0xe9, 0x93, 0x0f);

DECLARE_TR_CTX(selector_tr, SOF_UUID(selector_uuid), LOG_LEVEL_INFO);

/**
 * \brief Creates selector component.
 * \param[in,out] data Selector base component device.
 * \return Pointer to selector base component device.
 */
static struct comp_dev *selector_new(const struct comp_driver *drv,
				     struct sof_ipc_comp *comp)
{
	struct sof_ipc_comp_process *ipc_process =
		(struct sof_ipc_comp_process *)comp;
	size_t bs = ipc_process->size;
	struct comp_dev *dev;
	struct comp_data *cd;
	int ret;

	comp_cl_info(&comp_selector, "selector_new()");

	dev = comp_alloc(drv, COMP_SIZE(struct sof_ipc_comp_process));
	if (!dev)
		return NULL;

	ret = memcpy_s(COMP_GET_IPC(dev, sof_ipc_comp_process),
		       sizeof(struct sof_ipc_comp_process), comp,
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

	comp_info(dev, "selector_free()");

	rfree(cd);
	rfree(dev);
}

static int selector_verify_params(struct comp_dev *dev,
				  struct sof_ipc_stream_params *params)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct comp_buffer *buffer;
	struct comp_buffer *sinkb;
	uint32_t in_channels;
	uint32_t out_channels;
	uint32_t flags = 0;

	comp_dbg(dev, "selector_verify_params()");

	sinkb = list_first_item(&dev->bsink_list, struct comp_buffer,
				source_list);

	/* check whether params->channels (received from driver) are equal to
	 * cd->config.in_channels_count (PLAYBACK) or
	 * cd->config.out_channels_count (CAPTURE) set during creating selector
	 * component in selector_new() or in selector_ctrl_set_data().
	 * cd->config.in/out_channels_count = 0 means that it can vary.
	 */
	if (dev->direction == SOF_IPC_STREAM_PLAYBACK) {
		/* fetch sink buffer for playback */
		buffer = list_first_item(&dev->bsink_list, struct comp_buffer,
					 source_list);
		if (cd->config.in_channels_count &&
		    cd->config.in_channels_count != params->channels) {
			comp_err(dev, "selector_verify_params(): src in_channels_count does not match pcm channels");
			return -EINVAL;
		}
		in_channels = cd->config.in_channels_count;

		buffer_lock(buffer, &flags);

		/* if cd->config.out_channels_count are equal to 0
		 * (it can vary), we set params->channels to sink buffer
		 * channels, which were previosly set in
		 * pipeline_comp_hw_params()
		 */
		out_channels = cd->config.out_channels_count ?
			cd->config.out_channels_count : buffer->stream.channels;
		params->channels = out_channels;
	} else {
		/* fetch source buffer for capture */
		buffer = list_first_item(&dev->bsource_list, struct comp_buffer,
					 sink_list);
		if (cd->config.out_channels_count &&
		    cd->config.out_channels_count != params->channels) {
			comp_err(dev, "selector_verify_params(): src in_channels_count does not match pcm channels");
			return -EINVAL;
		}
		out_channels = cd->config.out_channels_count;

		buffer_lock(buffer, &flags);

		/* if cd->config.in_channels_count are equal to 0
		 * (it can vary), we set params->channels to source buffer
		 * channels, which were previosly set in
		 * pipeline_comp_hw_params()
		 */
		in_channels = cd->config.in_channels_count ?
			cd->config.in_channels_count : buffer->stream.channels;
		params->channels = in_channels;
	}

	/* Set buffer params */
	buffer_set_params(buffer, params, BUFFER_UPDATE_FORCE);

	/* set component period frames */
	component_set_period_frames(dev, sinkb->stream.rate);

	buffer_unlock(buffer, flags);

	/* verify input channels */
	switch (in_channels) {
	case SEL_SOURCE_2CH:
	case SEL_SOURCE_4CH:
		break;
	default:
		comp_err(dev, "selector_verify_params(): in_channels = %u"
			 , in_channels);
		return -EINVAL;
	}

	/* verify output channels */
	switch (out_channels) {
	case SEL_SINK_1CH:
		break;
	case SEL_SINK_2CH:
	case SEL_SINK_4CH:
		/* verify proper channels for passthrough mode */
		if (in_channels != out_channels) {
			comp_err(dev, "selector_verify_params(): in_channels = %u, out_channels = %u"
				 , in_channels, out_channels);
			return -EINVAL;
		}
		break;
	default:
		comp_err(dev, "selector_verify_params(): out_channels = %u"
			 , out_channels);
		return -EINVAL;
	}

	if (cd->config.sel_channel > (SEL_SOURCE_4CH - 1)) {
		comp_err(dev, "selector_verify_params(): ch_idx = %u"
			 , cd->config.sel_channel);
		return -EINVAL;
	}

	return 0;
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
	int err;

	comp_info(dev, "selector_params()");

	err = selector_verify_params(dev, params);
	if (err < 0) {
		comp_err(dev, "selector_params(): pcm params verification failed.");
		return -EINVAL;
	}

	return 0;
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
		comp_info(dev, "selector_ctrl_set_data(), SOF_CTRL_CMD_BINARY");

		cfg = (struct sof_sel_config *)
		      ASSUME_ALIGNED(cdata->data->data, 4);

		/* Just set the configuration */
		cd->config.in_channels_count = cfg->in_channels_count;
		cd->config.out_channels_count = cfg->out_channels_count;
		cd->config.sel_channel = cfg->sel_channel;
		break;
	default:
		comp_err(dev, "selector_ctrl_set_cmd(): invalid cdata->cmd = %u",
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
		comp_info(dev, "selector_ctrl_get_data(), SOF_CTRL_CMD_BINARY");

		/* Copy back to user space */
		ret = memcpy_s(cdata->data->data, ((struct sof_abi_hdr *)
			       (cdata->data))->size, &cd->config,
			       sizeof(cd->config));
		assert(!ret);

		cdata->data->abi = SOF_ABI_VERSION;
		cdata->data->size = sizeof(cd->config);
		break;

	default:
		comp_err(dev, "selector_ctrl_get_data(): invalid cdata->cmd");
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
	struct sof_ipc_ctrl_data *cdata = ASSUME_ALIGNED(data, 4);
	int ret = 0;

	comp_info(dev, "selector_cmd()");

	switch (cmd) {
	case COMP_CMD_SET_DATA:
		ret = selector_ctrl_set_data(dev, cdata);
		break;
	case COMP_CMD_GET_DATA:
		ret = selector_ctrl_get_data(dev, cdata, max_data_size);
		break;
	case COMP_CMD_SET_VALUE:
		comp_info(dev, "selector_cmd(), COMP_CMD_SET_VALUE");
		break;
	case COMP_CMD_GET_VALUE:
		comp_info(dev, "selector_cmd(), COMP_CMD_GET_VALUE");
		break;
	default:
		comp_err(dev, "selector_cmd(): invalid command");
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
	struct comp_buffer *sourceb;
	int ret;

	comp_info(dev, "selector_trigger()");

	sourceb = list_first_item(&dev->bsource_list, struct comp_buffer,
				  sink_list);

	ret = comp_set_state(dev, cmd);

	/* TODO: remove in the future after adding support for case when
	 * kpb_init_draining() and kpb_draining_task() are interrupted by
	 * new pipeline_task()
	 */
	return dev_comp_type(sourceb->source) == SOF_COMP_KPB ?
		PPL_STATUS_PATH_STOP : ret;
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
	uint32_t flags = 0;

	comp_dbg(dev, "selector_copy()");

	/* selector component will have 1 source and 1 sink buffer */
	source = list_first_item(&dev->bsource_list, struct comp_buffer,
				 sink_list);
	sink = list_first_item(&dev->bsink_list, struct comp_buffer,
			       source_list);

	if (!source->stream.avail)
		return PPL_STATUS_PATH_STOP;

	buffer_lock(source, &flags);
	buffer_lock(sink, &flags);

	frames = audio_stream_avail_frames(&source->stream, &sink->stream);
	source_bytes = frames * audio_stream_frame_bytes(&source->stream);
	sink_bytes = frames * audio_stream_frame_bytes(&sink->stream);

	buffer_unlock(sink, flags);
	buffer_unlock(source, flags);

	comp_dbg(dev, "selector_copy(), source_bytes = 0x%x, sink_bytes = 0x%x",
		 source_bytes, sink_bytes);

	/* copy selected channels from in to out */
	buffer_invalidate(source, source_bytes);
	cd->sel_func(dev, &sink->stream, &source->stream, frames);
	buffer_writeback(sink, sink_bytes);

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
	struct sof_ipc_comp_config *config = dev_comp_config(dev);
	int ret;

	comp_info(dev, "selector_prepare()");

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
	comp_info(dev, "selector_prepare(): sourceb->schannels = %u",
		  sourceb->stream.channels);
	comp_info(dev, "selector_prepare(): sinkb->channels = %u",
		  sinkb->stream.channels);

	if (sinkb->stream.size < config->periods_sink * cd->sink_period_bytes) {
		comp_err(dev, "selector_prepare(): sink buffer size %d is insufficient < %d * %d",
			 sinkb->stream.size, config->periods_sink, cd->sink_period_bytes);
		ret = -ENOMEM;
		goto err;
	}

	/* validate */
	if (cd->sink_period_bytes == 0) {
		comp_err(dev, "selector_prepare(): cd->sink_period_bytes = 0, dev->frames = %u",
			 dev->frames);
		ret = -EINVAL;
		goto err;
	}

	if (cd->source_period_bytes == 0) {
		comp_err(dev, "selector_prepare(): cd->source_period_bytes = 0, dev->frames = %u",
			 dev->frames);
		ret = -EINVAL;
		goto err;
	}

	cd->sel_func = sel_get_processing_function(dev);
	if (!cd->sel_func) {
		comp_err(dev, "selector_prepare(): invalid cd->sel_func, cd->source_format = %u, cd->sink_format = %u, cd->out_channels_count = %u",
			 cd->source_format, cd->sink_format,
			 cd->config.out_channels_count);
		ret = -EINVAL;
		goto err;
	}

	return 0;

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

	comp_info(dev, "selector_reset()");

	ret = comp_set_state(dev, COMP_TRIGGER_RESET);

	return ret;
}

/** \brief Selector component definition. */
static const struct comp_driver comp_selector = {
	.type	= SOF_COMP_SELECTOR,
	.uid	= SOF_RT_UUID(selector_uuid),
	.tctx	= &selector_tr,
	.ops	= {
		.create		= selector_new,
		.free		= selector_free,
		.params		= selector_params,
		.cmd		= selector_cmd,
		.trigger	= selector_trigger,
		.copy		= selector_copy,
		.prepare	= selector_prepare,
		.reset		= selector_reset,
	},
};

static SHARED_DATA struct comp_driver_info comp_selector_info = {
	.drv = &comp_selector,
};

/** \brief Initializes selector component. */
UT_STATIC void sys_comp_selector_init(void)
{
	comp_register(platform_shared_get(&comp_selector_info,
					  sizeof(comp_selector_info)));
}

DECLARE_MODULE(sys_comp_selector_init);
