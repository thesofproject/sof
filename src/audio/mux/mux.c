// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Artur Kloniecki <arturx.kloniecki@linux.intel.com>

#include <config.h>

#if CONFIG_COMP_MUX

#include <sof/audio/component.h>
#include <sof/audio/mux.h>
#include <sof/common.h>
#include <sof/drivers/ipc.h>
#include <sof/lib/alloc.h>
#include <sof/list.h>
#include <sof/math/numbers.h>
#include <sof/platform.h>
#include <sof/string.h>
#include <sof/trace/preproc.h>
#include <sof/trace/trace.h>
#include <sof/ut.h>
#include <ipc/control.h>
#include <ipc/topology.h>
#include <user/trace.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

static int mux_set_values(struct comp_data *cd, struct sof_mux_config *cfg)
{
	uint8_t i;
	uint8_t j;

	/* check if number of streams configured doesn't exceed maximum */
	if (cfg->num_streams > MUX_MAX_STREAMS) {
		trace_mux_error("mux_set_values() error: configured number of "
				"streams (%u) exceeds maximum = "
				META_QUOTE(MUX_MAX_STREAMS),
				cfg->num_streams);
		return -EINVAL;
	}

	/* check if all streams configured have distinct IDs */
	for (i = 0; i < cfg->num_streams; i++) {
		for (j = i + 1; j < cfg->num_streams; j++) {
			if (cfg->streams[i].pipeline_id ==
				cfg->streams[j].pipeline_id) {
				trace_mux_error("mux_set_values() error: "
						"multiple configured streams "
						"have same pipeline ID = %u",
						cfg->streams[i].pipeline_id);
				return -EINVAL;
			}
		}
	}

	/* check if number of channels per stream doesn't exceed maximum */
	for (i = 0; i < cfg->num_streams; i++) {
		if (cfg->streams[i].num_channels > PLATFORM_MAX_CHANNELS) {
			trace_mux_error("mux_set_values() error: configured "
					"number of channels for stream %u "
					"exceeds platform maximum = "
					META_QUOTE(PLATFORM_MAX_CHANNELS),
					i);
			return -EINVAL;
		}
	}

	cd->config.num_channels = cfg->num_channels;
	cd->config.frame_format = cfg->frame_format;

	for (i = 0; i < cfg->num_streams; i++) {
		cd->config.streams[i].num_channels = cfg->streams[i].num_channels;
		cd->config.streams[i].pipeline_id = cfg->streams[i].pipeline_id;
		for (j = 0; j < cfg->streams[i].num_channels; j++)
			cd->config.streams[i].mask[j] = cfg->streams[i].mask[j];
	}

	return 0;
}

static struct comp_dev *mux_new(struct sof_ipc_comp *comp)
{
	struct sof_ipc_comp_process *ipc_process =
		(struct sof_ipc_comp_process *)comp;
	size_t bs = ipc_process->size;
	struct comp_dev *dev;
	struct comp_data *cd;
	int ret;

	trace_mux("mux_new()");

	if (IPC_IS_SIZE_INVALID(ipc_process->config)) {
		IPC_SIZE_ERROR_TRACE(TRACE_CLASS_MUX, ipc_process->config);
		return NULL;
	}

	dev = rzalloc(RZONE_RUNTIME, SOF_MEM_CAPS_RAM,
		      COMP_SIZE(struct sof_ipc_comp_process));
	if (!dev)
		return NULL;

	memcpy(&dev->comp, comp, sizeof(struct sof_ipc_comp_process));

	cd = rzalloc(RZONE_RUNTIME, SOF_MEM_CAPS_RAM,
		     sizeof(*cd) + MUX_MAX_STREAMS * sizeof(struct mux_stream_data));
	if (!cd) {
		rfree(dev);
		return NULL;
	}

	comp_set_drvdata(dev, cd);

	memcpy_s(&cd->config, sizeof(struct sof_mux_config) +
		 MUX_MAX_STREAMS * sizeof(struct mux_stream_data),
		 ipc_process->data, bs);

	/* verification of initial parameters */
	ret = mux_set_values(cd, &cd->config);

	if (ret < 0) {
		rfree(cd);
		rfree(dev);
		return NULL;
	}

	dev->state = COMP_STATE_READY;
	return dev;
}

static void mux_free(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	trace_mux("mux_free()");

	rfree(cd);
	rfree(dev);
}

/* set component audio stream parameters */
static int mux_params(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	trace_mux("mux_params()");

	cd->config.num_channels = dev->params.channels;
	cd->config.frame_format = dev->params.frame_fmt;

	return 0;
}

static int mux_ctrl_set_cmd(struct comp_dev *dev,
			    struct sof_ipc_ctrl_data *cdata)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct sof_mux_config *cfg;
	int ret = 0;

	trace_mux("mux_ctrl_set_cmd(), cdata->cmd = 0x%08x", cdata->cmd);

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_BINARY:
		cfg = (struct sof_mux_config *)cdata->data->data;

		ret = mux_set_values(cd, cfg);
		break;
	default:
		trace_mux_error("mux_ctrl_set_cmd() error: invalid cdata->cmd ="
				" 0x%08x", cdata->cmd);
		ret = -EINVAL;
		break;
	}

	return ret;
}

/* used to pass standard and bespoke commands (with data) to component */
static int mux_cmd(struct comp_dev *dev, int cmd, void *data,
		   int max_data_size)
{
	struct sof_ipc_ctrl_data *cdata = data;

	trace_mux("mux_cmd() cmd = 0x%08x", cmd);

	switch (cmd) {
	case COMP_CMD_SET_DATA:
		return mux_ctrl_set_cmd(dev, cdata);
	default:
		return -EINVAL;
	}
}

static uint8_t get_stream_index(struct comp_data *cd, uint32_t pipe_id)
{
	int i;

	for (i = 0; i < MUX_MAX_STREAMS; i++) {
		if (cd->config.streams[i].pipeline_id == pipe_id)
			return i;
	}
	trace_mux_error("get_stream_index() error: couldn't find configuration "
			"for connected pipeline %u", pipe_id);
	return 0;
}

/* process and copy stream data from source to sink buffers */
static int demux_copy(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct comp_buffer *source;
	struct comp_buffer *sink;
	struct comp_buffer *sinks[MUX_MAX_STREAMS] = { NULL };
	struct list_item *clist;
	uint32_t num_sinks = 0;
	uint32_t i = 0;
	uint32_t frames = -1;
	uint32_t source_bytes;
	uint32_t sinks_bytes[MUX_MAX_STREAMS] = { 0 };

	tracev_mux("demux_copy()");

	// align sink streams with their respective configurations
	list_for_item(clist, &dev->bsink_list) {
		sink = container_of(clist, struct comp_buffer, source_list);
		if (sink->sink->state == dev->state) {
			num_sinks++;
			i = get_stream_index(cd, sink->pipeline_id);
			sinks[i] = sink;
		}
	}

	/* if there are no sinks active */
	if (num_sinks == 0)
		return 0;

	source = list_first_item(&dev->bsource_list, struct comp_buffer,
				 sink_list);

	/* check if source is active */
	if (source->source->state != dev->state)
		return 0;

	for (i = 0; i < MUX_MAX_STREAMS; i++) {
		if (!sinks[i])
			continue;
		frames = MIN(frames, comp_avail_frames(source, sinks[i]));
	}

	source_bytes = frames * comp_frame_bytes(source->source);
	for (i = 0; i < MUX_MAX_STREAMS; i++) {
		if (!sinks[i])
			continue;
		sinks_bytes[i] = frames * comp_frame_bytes(sinks[i]->sink);
	}

	/* produce output, one sink at a time */
	for (i = 0; i < MUX_MAX_STREAMS; i++) {
		if (!sinks[i])
			continue;

		cd->demux(dev, sinks[i], source, frames, &cd->config.streams[i]);
	}

	/* update components */
	for (i = 0; i < MUX_MAX_STREAMS; i++) {
		if (!sinks[i])
			continue;
		comp_update_buffer_produce(sinks[i], sinks_bytes[i]);
	}
	comp_update_buffer_consume(source, source_bytes);

	return 0;
}

/* process and copy stream data from source to sink buffers */
static int mux_copy(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct comp_buffer *sink;
	struct comp_buffer *source;
	struct comp_buffer *sources[MUX_MAX_STREAMS] = { NULL };
	struct list_item *clist;
	uint32_t num_sources = 0;
	uint32_t i = 0;
	uint32_t frames = -1;
	uint32_t sources_bytes[MUX_MAX_STREAMS] = { 0 };
	uint32_t sink_bytes;

	tracev_mux("mux_copy()");

	/* align source streams with their respective configurations */
	list_for_item(clist, &dev->bsource_list) {
		source = container_of(clist, struct comp_buffer, sink_list);
		if (source->source->state == dev->state) {
			num_sources++;
			i = get_stream_index(cd, source->pipeline_id);
			sources[i] = source;
		}
	}

	/* check if there are any sources active */
	if (num_sources == 0)
		return 0;

	sink = list_first_item(&dev->bsink_list, struct comp_buffer,
			       source_list);

	/* check if sink is active */
	if (sink->sink->state != dev->state)
		return 0;

	for (i = 0; i < MUX_MAX_STREAMS; i++) {
		if (!sources[i])
			continue;
		frames = MIN(frames, comp_avail_frames(sources[i], sink));
	}

	for (i = 0; i < MUX_MAX_STREAMS; i++) {
		if (!sources[i])
			continue;
		sources_bytes[i] = frames *
				   comp_frame_bytes(sources[i]->source);
	}
	sink_bytes = frames * comp_frame_bytes(sink->sink);

	/* produce output */
	cd->mux(dev, sink, &sources[0], frames, &cd->config.streams[0]);

	/* update components */
	comp_update_buffer_produce(sink, sink_bytes);
	for (i = 0; i < MUX_MAX_STREAMS; i++) {
		if (!sources[i])
			continue;
		comp_update_buffer_consume(sources[i], sources_bytes[i]);
	}

	return 0;
}

static int mux_reset(struct comp_dev *dev)
{
	trace_mux("mux_reset()");

	return comp_set_state(dev, COMP_TRIGGER_RESET);
}

static int mux_prepare(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int ret;

	trace_mux("mux_prepare()");

	ret = comp_set_state(dev, COMP_TRIGGER_PREPARE);
	if (ret) {
		trace_mux("mux_prepare() comp_set_state() returned non-zero.");
		return ret;
	}

	cd->mux = mux_get_processing_function(dev);
	if (!cd->mux) {
		trace_mux_error("mux_prepare() error: couldn't find appropriate"
				" mux processing function for component.");
		ret = -EINVAL;
		goto err;
	}

	return 0;

err:
	comp_set_state(dev, COMP_TRIGGER_RESET);
	return ret;
}

static int demux_prepare(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int ret;

	trace_mux("demux_prepare()");

	ret = comp_set_state(dev, COMP_TRIGGER_PREPARE);
	if (ret) {
		trace_mux("demux_prepare() comp_set_state() returned non-zero");
		return ret;
	}

	cd->demux = demux_get_processing_function(dev);
	if (!cd->demux) {
		trace_mux_error("demux_prepare() error: couldn't find "
				"appropriate demux processing function for "
				"component.");
		ret = -EINVAL;
		goto err;
	}

	return 0;

err:
	comp_set_state(dev, COMP_TRIGGER_RESET);
	return ret;
}

static int mux_trigger(struct comp_dev *dev, int cmd)
{
	int ret = 0;

	trace_mux("mux_trigger(), command = %u", cmd);

	ret = comp_set_state(dev, cmd);

	if (ret == COMP_STATUS_STATE_ALREADY_SET)
		ret = PPL_STATUS_PATH_STOP;

	return ret;
}

struct comp_driver comp_mux = {
	.type	= SOF_COMP_MUX,
	.ops	= {
		.new		= mux_new,
		.free		= mux_free,
		.params		= mux_params,
		.cmd		= mux_cmd,
		.copy		= mux_copy,
		.prepare	= mux_prepare,
		.reset		= mux_reset,
		.trigger	= mux_trigger,
	},
};

struct comp_driver comp_demux = {
	.type	= SOF_COMP_DEMUX,
	.ops	= {
		.new		= mux_new,
		.free		= mux_free,
		.params		= mux_params,
		.cmd		= mux_cmd,
		.copy		= demux_copy,
		.prepare	= demux_prepare,
		.reset		= mux_reset,
		.trigger	= mux_trigger,
	},
};

UT_STATIC void sys_comp_mux_init(void)
{
	comp_register(&comp_mux);
	comp_register(&comp_demux);
}

DECLARE_MODULE(sys_comp_mux_init);

#endif /* CONFIG_COMP_MUX */
