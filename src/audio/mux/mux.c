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
#include <sof/lib/memory.h>
#include <sof/lib/uuid.h>
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

static const struct comp_driver comp_mux;

/* c4b26868-1430-470e-a089-15d1c77f851a */
DECLARE_SOF_UUID("mux", mux_uuid, 0xc4b26868, 0x1430, 0x470e,
		 0xa0, 0x89, 0x15, 0xd1, 0xc7, 0x7f, 0x85, 0x1a);

static int mux_set_values(struct comp_dev *dev, struct comp_data *cd,
			  struct sof_mux_config *cfg)
{
	uint8_t i;
	uint8_t j;

	/* check if number of streams configured doesn't exceed maximum */
	if (cfg->num_streams > MUX_MAX_STREAMS) {
		comp_cl_err(&comp_mux, "mux_set_values() error: configured number of streams (%u) exceeds maximum = "
			    META_QUOTE(MUX_MAX_STREAMS), cfg->num_streams);
		return -EINVAL;
	}

	/* check if all streams configured have distinct IDs */
	for (i = 0; i < cfg->num_streams; i++) {
		for (j = i + 1; j < cfg->num_streams; j++) {
			if (cfg->streams[i].pipeline_id ==
				cfg->streams[j].pipeline_id) {
				comp_cl_err(&comp_mux, "mux_set_values() error: multiple configured streams have same pipeline ID = %u",
					    cfg->streams[i].pipeline_id);
				return -EINVAL;
			}
		}
	}

	/* check if number of channels per stream doesn't exceed maximum */
	for (i = 0; i < cfg->num_streams; i++) {
		if (cfg->streams[i].num_channels > PLATFORM_MAX_CHANNELS) {
			comp_cl_err(&comp_mux, "mux_set_values() error: configured number of channels for stream %u exceeds platform maximum = "
				    META_QUOTE(PLATFORM_MAX_CHANNELS), i);
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

	if (dev->comp.type == SOF_COMP_MUX)
		cd->mux = mux_get_processing_function(dev);
	else
		cd->demux = demux_get_processing_function(dev);

	return 0;
}

static struct comp_dev *mux_new(const struct comp_driver *drv,
				struct sof_ipc_comp *comp)
{
	struct sof_ipc_comp_process *ipc_process =
		(struct sof_ipc_comp_process *)comp;
	size_t bs = ipc_process->size;
	struct comp_dev *dev;
	struct comp_data *cd;
	int ret;

	comp_cl_info(&comp_mux, "mux_new()");

	dev = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM,
		      COMP_SIZE(struct sof_ipc_comp_process));
	if (!dev)
		return NULL;
	dev->drv = drv;

	dev->size = COMP_SIZE(struct sof_ipc_comp_process);

	memcpy_s(COMP_GET_IPC(dev, sof_ipc_comp_process),
		 sizeof(struct sof_ipc_comp_process),
		 comp, sizeof(struct sof_ipc_comp_process));

	cd = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM,
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
	ret = mux_set_values(dev, cd, &cd->config);

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

	comp_info(dev, "mux_free()");

	rfree(cd);
	rfree(dev);
}

static uint8_t get_stream_index(struct comp_data *cd, uint32_t pipe_id)
{
	int i;

	for (i = 0; i < MUX_MAX_STREAMS; i++)
		if (cd->config.streams[i].pipeline_id == pipe_id)
			return i;

	comp_cl_err(&comp_mux, "get_stream_index() error: couldn't find configuration for connected pipeline %u",
		    pipe_id);

	return 0;
}

static int mux_verify_params(struct comp_dev *dev,
			     struct sof_ipc_stream_params *params)
{
	int ret;

	comp_dbg(dev, "mux_verify_params()");

	ret = comp_verify_params(dev, BUFF_PARAMS_CHANNELS, params);
	if (ret < 0) {
		comp_err(dev, "mux_verify_params() error: comp_verify_params() failed.");
		return ret;
	}

	return 0;
}

/* set component audio stream parameters */
static int mux_params(struct comp_dev *dev,
		      struct sof_ipc_stream_params *params)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct comp_buffer *sinkb;
	int err;

	comp_info(dev, "mux_params()");

	err = mux_verify_params(dev, params);
	if (err < 0) {
		comp_err(dev, "mux_fir_params(): pcm params verification failed.");
		return -EINVAL;
	}

	sinkb = list_first_item(&dev->bsink_list, struct comp_buffer,
				  source_list);

	cd->config.num_channels = sinkb->stream.channels;
	cd->config.frame_format = sinkb->stream.frame_fmt;

	return 0;
}

static int mux_ctrl_set_cmd(struct comp_dev *dev,
			    struct sof_ipc_ctrl_data *cdata)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct sof_mux_config *cfg;
	int ret = 0;

	comp_info(dev, "mux_ctrl_set_cmd(), cdata->cmd = 0x%08x",
		  cdata->cmd);

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_BINARY:
		cfg = (struct sof_mux_config *)
		      ASSUME_ALIGNED(cdata->data->data, 4);

		ret = mux_set_values(dev, cd, cfg);
		break;
	default:
		comp_err(dev, "mux_ctrl_set_cmd() error: invalid cdata->cmd = 0x%08x",
			 cdata->cmd);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int mux_ctrl_get_cmd(struct comp_dev *dev,
			    struct sof_ipc_ctrl_data *cdata, int size)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct sof_mux_config *cfg = &cd->config;
	uint32_t reply_size;
	int ret = 0;

	comp_cl_info(&comp_mux, "mux_ctrl_get_cmd(), cdata->cmd = 0x%08x",
		     cdata->cmd);

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_BINARY:
		/* calculate config size */
		reply_size = sizeof(struct sof_mux_config) + cfg->num_streams *
			sizeof(struct mux_stream_data);

		/* copy back to user space */
		assert(!memcpy_s(cdata->data->data, ((struct sof_abi_hdr *)
				 (cdata->data))->size, cfg, reply_size));

		cdata->data->abi = SOF_ABI_VERSION;
		cdata->data->size = reply_size;
		break;
	default:
		comp_cl_err(&comp_mux, "mux_ctrl_set_cmd() error: invalid cdata->cmd = 0x%08x",
			    cdata->cmd);
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

	comp_info(dev, "mux_cmd() cmd = 0x%08x", cmd);

	switch (cmd) {
	case COMP_CMD_SET_DATA:
		return mux_ctrl_set_cmd(dev, cdata);
	case COMP_CMD_GET_DATA:
		return mux_ctrl_get_cmd(dev, cdata, max_data_size);
	default:
		return -EINVAL;
	}
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
	uint32_t avail;
	uint32_t sinks_bytes[MUX_MAX_STREAMS] = { 0 };
	uint32_t flags = 0;

	comp_dbg(dev, "demux_copy()");

	if (!cd->demux) {
		comp_err(dev, "demux_copy() error: no demux processing function for component.");
		comp_set_state(dev, COMP_TRIGGER_RESET);
		return -EINVAL;
	}

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

	buffer_lock(source, &flags);

	/* check if source is active */
	if (source->source->state != dev->state) {
		buffer_unlock(source, flags);
		return 0;
	}

	for (i = 0; i < MUX_MAX_STREAMS; i++) {
		if (!sinks[i])
			continue;
		buffer_lock(sinks[i], &flags);
		avail = audio_stream_avail_frames(&source->stream,
						  &sinks[i]->stream);
		frames = MIN(frames, avail);
		buffer_unlock(sinks[i], flags);
	}

	buffer_unlock(source, flags);

	source_bytes = frames * audio_stream_frame_bytes(&source->stream);
	for (i = 0; i < MUX_MAX_STREAMS; i++) {
		if (!sinks[i])
			continue;
		sinks_bytes[i] = frames *
				 audio_stream_frame_bytes(&sinks[i]->stream);
	}

	/* produce output, one sink at a time */
	for (i = 0; i < MUX_MAX_STREAMS; i++) {
		if (!sinks[i])
			continue;

		buffer_invalidate(source, source_bytes);
		cd->demux(dev, &sinks[i]->stream, &source->stream, frames,
			  &cd->config.streams[i]);
		buffer_writeback(sinks[i], sinks_bytes[i]);
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
	const struct audio_stream *sources_stream[MUX_MAX_STREAMS] = { NULL };
	struct list_item *clist;
	uint32_t num_sources = 0;
	uint32_t i = 0;
	uint32_t frames = -1;
	uint32_t sources_bytes[MUX_MAX_STREAMS] = { 0 };
	uint32_t sink_bytes;

	comp_dbg(dev, "mux_copy()");

	if (!cd->mux) {
		comp_err(dev, "mux_copy() error: no mux processing function for component.");
		comp_set_state(dev, COMP_TRIGGER_RESET);
		return -EINVAL;
	}

	/* align source streams with their respective configurations */
	list_for_item(clist, &dev->bsource_list) {
		source = container_of(clist, struct comp_buffer, sink_list);
		if (source->source->state == dev->state) {
			num_sources++;
			i = get_stream_index(cd, source->pipeline_id);
			sources[i] = source;
			sources_stream[i] = &source->stream;
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
		frames = MIN(frames,
			     audio_stream_avail_frames(sources_stream[i],
						       &sink->stream));
	}

	for (i = 0; i < MUX_MAX_STREAMS; i++) {
		if (!sources[i])
			continue;
		sources_bytes[i] = frames *
				   audio_stream_frame_bytes(sources_stream[i]);
		buffer_invalidate(sources[i], sources_bytes[i]);
	}
	sink_bytes = frames * audio_stream_frame_bytes(&sink->stream);

	/* produce output */
	cd->mux(dev, &sink->stream, &sources_stream[0], frames,
		&cd->config.streams[0]);
	buffer_writeback(sink, sink_bytes);

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
	comp_info(dev, "mux_reset()");

	return comp_set_state(dev, COMP_TRIGGER_RESET);
}

static int mux_prepare(struct comp_dev *dev)
{
	int ret;

	comp_info(dev, "mux_prepare()");

	ret = comp_set_state(dev, COMP_TRIGGER_PREPARE);
	if (ret) {
		comp_info(dev, "mux_prepare() comp_set_state() returned non-zero.");
		return ret;
	}

	return 0;
}

static int mux_trigger(struct comp_dev *dev, int cmd)
{
	int ret = 0;

	comp_info(dev, "mux_trigger(), command = %u", cmd);

	ret = comp_set_state(dev, cmd);

	if (ret == COMP_STATUS_STATE_ALREADY_SET)
		ret = PPL_STATUS_PATH_STOP;

	return ret;
}

static const struct comp_driver comp_mux = {
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

static SHARED_DATA struct comp_driver_info comp_mux_info = {
	.drv = &comp_mux,
};

static const struct comp_driver comp_demux = {
	.type	= SOF_COMP_DEMUX,
	.uid = SOF_UUID(mux_uuid),
	.ops	= {
		.new		= mux_new,
		.free		= mux_free,
		.params		= mux_params,
		.cmd		= mux_cmd,
		.copy		= demux_copy,
		.prepare	= mux_prepare,
		.reset		= mux_reset,
		.trigger	= mux_trigger,
	},
};

static SHARED_DATA struct comp_driver_info comp_demux_info = {
	.drv = &comp_demux,
};

UT_STATIC void sys_comp_mux_init(void)
{
	comp_register(platform_shared_get(&comp_mux_info,
					  sizeof(comp_mux_info)));
	comp_register(platform_shared_get(&comp_demux_info,
					  sizeof(comp_demux_info)));
}

DECLARE_MODULE(sys_comp_mux_init);

#endif /* CONFIG_COMP_MUX */
