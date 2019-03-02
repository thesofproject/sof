/*
 * Copyright (c) 2019, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the Intel Corporation nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Lech Betlej <lech.betlej@linux.intel.com>
 */

/**
 * \file audio/selector.c
 * \brief Audio channel selection component. In case 1 output channel is
 * \brief selected in topology the component provides the selected channel on
 * \brief ouput. In case 2 or 4 channels are selected on output the component
 * \brief works in a passthrough mode.
 * \authors Lech Betlej <lech.betlej@linux.intel.com>
 */

#include <stddef.h>
#include <errno.h>
#include <sof/sof.h>
#include <sof/lock.h>
#include <sof/list.h>
#include <sof/stream.h>
#include <sof/alloc.h>
#include <sof/work.h>
#include <sof/clk.h>
#include <sof/ipc.h>
#include "selector.h"
#include <sof/math/numbers.h>

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
				     "ch_idx = %u", ch_idx);
		return -EINVAL;
	}

	cd->in_channels_count = in_channels;
	cd->out_channels_count = out_channels;
	cd->sel_channel = ch_idx;

	return 0;
}

/**
 * \brief Creates selector component.
 * \param[in,out] data Selector base component device.
 * \return Pointer to selector base component device.
 */
static struct comp_dev *selector_new(struct sof_ipc_comp *comp)
{
	struct comp_dev *dev;
	struct sof_ipc_comp_process *sel;
	struct sof_ipc_comp_process *ipc_sel =
		(struct sof_ipc_comp_process *)comp;
	struct comp_data *cd;

	trace_selector("selector_new()");

	if (IPC_IS_SIZE_INVALID(ipc_sel->config)) {
		IPC_SIZE_ERROR_TRACE(TRACE_CLASS_SELECTOR, ipc_sel->config);
		return NULL;
	}

	dev = rzalloc(RZONE_RUNTIME, SOF_MEM_CAPS_RAM,
		      COMP_SIZE(struct sof_ipc_comp_process));
	if (!dev)
		return NULL;

	sel = (struct sof_ipc_comp_process *)&dev->comp;
	memcpy(sel, ipc_sel, sizeof(struct sof_ipc_comp_process));

	cd = rzalloc(RZONE_RUNTIME, SOF_MEM_CAPS_RAM, sizeof(*cd));
	if (!cd) {
		rfree(dev);
		return NULL;
	}

	comp_set_drvdata(dev, cd);

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

	trace_selector("selector_free()");

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
static int selector_params(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct comp_buffer *sourceb, *sinkb;
	struct comp_dev *source, *sink;

	trace_selector("selector_params()");

	/* selector component has only 1 source and 1 sink buffer */
	sourceb = list_first_item(&dev->bsource_list, struct comp_buffer,
				  sink_list);

	sinkb = list_first_item(&dev->bsink_list, struct comp_buffer,
				source_list);

	/* verification of channel counts */
	/*
	 * FIXME: where should sel_channel come from?
	 * from a kcontrol or ipc params
	 */
	ret = sel_set_channel_values(cd, sourceb->source->params.channels,
				     sinkb->sink->params.channels,
				     cd->sel_channel);
	if (ret < 0) {
		trace_selector_error("selector_params() error: ",
				     "invalid params for selector"
				     "input ch count %u output ch count %u"
				     "selected channel %u",
				     sourceb->source->params.channels,
				     sinkb->sink->params.channels,
				     cd->sel_channel);

		return -EINVAL;
	}

	/* set input/output channel counts based on source/sink params */
	cd->out_channels_count = sinkb->sink->params.channels;
	cd->in_channels_count = sourceb->source->params.channels;

	/* rewrite channels number for other components */
	if (dev->params.direction == SOF_IPC_STREAM_PLAYBACK)
		dev->params.channels = cd->out_channels_count;
	else
		dev->params.channels = cd->in_channels_count;

	return 0;
}


/**
 * \brief Sets selector control command.
 * \param[in,out] dev Selector base component device.
 * \param[in,out] cdata Control command data.
 * \return Error code.
 */
static int selector_ctrl_set_cmd(struct comp_dev *dev,
				 struct sof_ipc_ctrl_data *cdata)
{
	/* TODO add implementation aligned with SW assumptions */
	trace_selector("selector_ctrl_set_cmd(), cdata->cmd = %u", cdata->cmd);

	return 0;
}

/**
 * \brief Gets selector control command.
 * \param[in,out] dev Selector base component device.
 * \param[in,out] cdata Selector command data.
 * \param[in] size Command data size.
 * \return Error code.
 */
static int selector_ctrl_get_cmd(struct comp_dev *dev,
				 struct sof_ipc_ctrl_data *cdata, int size)
{
	/* TODO add implementation aligned with SW assumptions */
	cdata->cmd = SOF_CTRL_CMD_ENUM;
	trace_selector("selector_ctrl_get_cmd(), cdata->cmd = %u",
		       cdata->index);

	return 0;
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

	trace_selector("selector_cmd()");

	switch (cmd) {
	case COMP_CMD_SET_VALUE:
		return selector_ctrl_set_cmd(dev, cdata);
	case COMP_CMD_GET_VALUE:
		return selector_ctrl_get_cmd(dev, cdata, max_data_size);
	default:
		return -EINVAL;
	}
}

/**
 * \brief Sets component state.
 * \param[in,out] dev Selector base component device.
 * \param[in] cmd Command type.
 * \return Error code.
 */
static int selector_trigger(struct comp_dev *dev, int cmd)
{
	trace_selector("selector_trigger()");

	return comp_set_state(dev, cmd);
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

	tracev_selector("selector_copy()");

	/* selector component will have 1 source and 1 sink buffer */
	source = list_first_item(&dev->bsource_list, struct comp_buffer,
				 sink_list);
	sink = list_first_item(&dev->bsink_list, struct comp_buffer,
			       source_list);

	/* check for underrun */
	if (source->avail == 0) {
		trace_selector_error("selector_copy() error: "
				     "source component buffer has not enough "
				     "data available");
		comp_underrun(dev, source, 0, 0);
		return -EIO;
	}

	/* check for overrun */
	if (sink->free == 0) {
		trace_selector_error("selector_copy() error: "
				     "sink component buffer has not enough "
				     "free bytes for copy");
		comp_overrun(dev, sink, 0, 0);
		return -EIO;
	}

	frames = comp_avail_frames(source, sink);
	source_bytes = frames * comp_frame_bytes(source->source);
	sink_bytes = frames * comp_frame_bytes(sink->sink);

	tracev_selector("selector_copy(), source_bytes = 0x%x, sink_bytes = "
			"0x%x", source_bytes, sink_bytes);

	/* copy selected channels from in to out */
	cd->sel_func(dev, sink, source, frames);

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

	trace_selector("selector_prepare()");

	ret = comp_set_state(dev, COMP_TRIGGER_PREPARE);
	if (ret)
		return ret;

	/* selector component will have 1 source and 1 sink buffer */
	sourceb = list_first_item(&dev->bsource_list, struct comp_buffer,
				  sink_list);
	sinkb = list_first_item(&dev->bsink_list, struct comp_buffer,
				source_list);

	/* get source data format and period bytes */
	comp_set_period_bytes(sourceb->source, dev->frames, &cd->source_format,
			      &cd->source_period_bytes);

	/* get sink data format and period bytes */
	comp_set_period_bytes(sinkb->sink, dev->frames, &cd->sink_format,
			      &cd->sink_period_bytes);

	/* There is an assumption that sink component will report out
	 * proper number of channels [1] for selector to actually
	 * reduce channel count between source and sink
	 */
	trace_selector("selector_prepare(): source->params.channels = %u",
		       sourceb->sink->params.channels);
	trace_selector("selector_prepare(): sink->params.channels = %u",
		       sinkb->sink->params.channels);

	/* validate */
	if (cd->sink_period_bytes == 0) {
		trace_selector_error("selector_prepare() error: "
				     "cd->sink_period_bytes = 0, dev->frames ="
				     " %u, sinkb->sink->frame_bytes = %u",
				     dev->frames, sinkb->sink->frame_bytes);
		ret = -EINVAL;
		goto err;
	}

	if (cd->source_period_bytes == 0) {
		trace_selector_error("selector_prepare() error: "
				     "cd->source_period_bytes = 0, "
				     "dev->frames = %u, "
				     "sourceb->source->frame_bytes = %u",
				     dev->frames,
				     sourceb->source->frame_bytes);
		ret = -EINVAL;
		goto err;
	}

	/* set downstream buffer size */
	ret = buffer_set_size(sinkb, cd->sink_period_bytes *
			      config->periods_sink);
	if (ret < 0) {
		trace_selector_error("selector_prepare() error: "
				     "buffer_set_size() failed");
		goto err;
	}

	cd->sel_func = sel_get_processing_function(dev);
	if (!cd->sel_func) {
		trace_selector_error("selector_prepare() error: "
				     "invalid cd->sel_func, "
				     "cd->source_format = %u, "
				     "cd->sink_format = %u, "
				     "cd->out_channels_count = %u",
				     cd->source_format, cd->sink_format,
				     cd->out_channels_count);
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
	trace_selector("selector_reset()");

	return comp_set_state(dev, COMP_TRIGGER_RESET);
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
		trace_selector("selector_cache(), CACHE_WRITEBACK_INV");

		cd = comp_get_drvdata(dev);

		dcache_writeback_invalidate_region(cd, sizeof(*cd));
		dcache_writeback_invalidate_region(dev, sizeof(*dev));
		break;

	case CACHE_INVALIDATE:
		trace_selector("selector_cache(), CACHE_INVALIDATE");

		dcache_invalidate_region(dev, sizeof(*dev));

		cd = comp_get_drvdata(dev);
		dcache_invalidate_region(cd, sizeof(*cd));
		break;
	}
}

/** \brief Selector component definition. */
struct comp_driver comp_selector = {
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

/** \brief Initializes selector component. */
void sys_comp_selector_init(void)
{
	comp_register(&comp_selector);
}
