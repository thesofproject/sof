// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Artur Kloniecki <arturx.kloniecki@linux.intel.com>

#if CONFIG_COMP_MUX

#include <sof/audio/component.h>
#include <sof/audio/mux.h>
#include <sof/audio/ipc-config.h>
#include <sof/common.h>
#include <sof/ipc/msg.h>
#include <rtos/alloc.h>
#include <rtos/init.h>
#include <sof/lib/memory.h>
#include <sof/lib/uuid.h>
#include <sof/list.h>
#include <sof/math/numbers.h>
#include <sof/platform.h>
#include <rtos/string.h>
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

LOG_MODULE_REGISTER(muxdemux, CONFIG_SOF_LOG_LEVEL);
#if CONFIG_IPC_MAJOR_3
/* c607ff4d-9cb6-49dc-b678-7da3c63ea557 */
DECLARE_SOF_RT_UUID("mux", mux_uuid, 0xc607ff4d, 0x9cb6, 0x49dc,
		     0xb6, 0x78, 0x7d, 0xa3, 0xc6, 0x3e, 0xa5, 0x57);
#else
/* 64ce6e35-857a-4878-ace8-e2a2f42e3069 */
DECLARE_SOF_RT_UUID("mux", mux_uuid, 0x64ce6e35, 0x857a, 0x4878,
		    0xac, 0xe8, 0xe2, 0xa2, 0xf4, 0x2e, 0x30, 0x69);
#endif
DECLARE_TR_CTX(mux_tr, SOF_UUID(mux_uuid), LOG_LEVEL_INFO);

/* c4b26868-1430-470e-a089-15d1c77f851a */
DECLARE_SOF_RT_UUID("demux", demux_uuid, 0xc4b26868, 0x1430, 0x470e,
		 0xa0, 0x89, 0x15, 0xd1, 0xc7, 0x7f, 0x85, 0x1a);

DECLARE_TR_CTX(demux_tr, SOF_UUID(demux_uuid), LOG_LEVEL_INFO);

/*
 * Check that we are not configuring routing matrix for mixing.
 *
 * In mux case this means, that muxed streams' configuration matrices don't
 * have 1's in corresponding matrix indices. Also single stream matrix can't
 * have 1's in same column as that corresponds to mixing also.
 */
static bool mux_mix_check(struct sof_mux_config *cfg)
{
	bool channel_set;
	int i;
	int j;
	int k;

	/* check for single matrix mixing, i.e multiple column bits are not set */
	for (i = 0 ; i < cfg->num_streams; i++) {
		for (j = 0 ; j < PLATFORM_MAX_CHANNELS; j++) {
			channel_set = false;
			for (k = 0 ; k < PLATFORM_MAX_CHANNELS; k++) {
				if (cfg->streams[i].mask[k] & (1 << j)) {
					if (!channel_set)
						channel_set = true;
					else
						return true;
				}
			}
		}
	}

	/* check for inter matrix mixing, i.e corresponding bits are not set */
	for (i = 0 ; i < PLATFORM_MAX_CHANNELS; i++) {
		for (j = 0 ; j < PLATFORM_MAX_CHANNELS; j++) {
			channel_set = false;
			for (k = 0 ; k < cfg->num_streams; k++) {
				if (cfg->streams[k].mask[i] & (1 << j)) {
					if (!channel_set)
						channel_set = true;
					else
						return true;
				}
			}
		}
	}

	return false;
}

static int mux_set_values(struct comp_dev *dev, struct comp_data *cd,
			  struct sof_mux_config *cfg)
{
	unsigned int i;
	unsigned int j;

	comp_dbg(dev, "mux_set_values()");

	/* check if number of streams configured doesn't exceed maximum */
	if (cfg->num_streams > MUX_MAX_STREAMS) {
		comp_cl_err(&comp_mux, "mux_set_values(): configured number of streams (%u) exceeds maximum = "
			    META_QUOTE(MUX_MAX_STREAMS), cfg->num_streams);
		return -EINVAL;
	}

	/* check if all streams configured have distinct IDs */
	for (i = 0; i < cfg->num_streams; i++) {
		for (j = i + 1; j < cfg->num_streams; j++) {
			if (cfg->streams[i].pipeline_id ==
				cfg->streams[j].pipeline_id) {
				comp_cl_err(&comp_mux, "mux_set_values(): multiple configured streams have same pipeline ID = %u",
					    cfg->streams[i].pipeline_id);
				return -EINVAL;
			}
		}
	}

	for (i = 0; i < cfg->num_streams; i++) {
		for (j = 0 ; j < PLATFORM_MAX_CHANNELS; j++) {
			if (popcount(cfg->streams[i].mask[j]) > 1) {
				comp_cl_err(&comp_mux, "mux_set_values(): mux component is not able to mix channels");
				return -EINVAL;
			}
		}
	}

	if (dev->ipc_config.type == SOF_COMP_MUX) {
		if (mux_mix_check(cfg))
			comp_cl_err(&comp_mux, "mux_set_values(): mux component is not able to mix channels");
	}

	for (i = 0; i < cfg->num_streams; i++) {
		cd->config.streams[i].pipeline_id = cfg->streams[i].pipeline_id;
		for (j = 0; j < PLATFORM_MAX_CHANNELS; j++)
			cd->config.streams[i].mask[j] = cfg->streams[i].mask[j];
	}

	cd->config.num_streams = cfg->num_streams;

	if (dev->ipc_config.type == SOF_COMP_MUX)
		mux_prepare_look_up_table(dev);
	else
		demux_prepare_look_up_table(dev);

	if (dev->state > COMP_STATE_INIT) {
		if (dev->ipc_config.type == SOF_COMP_MUX)
			cd->mux = mux_get_processing_function(dev);
		else
			cd->demux = demux_get_processing_function(dev);
	}

	return 0;
}

#if CONFIG_IPC_MAJOR_3
static struct comp_dev *mux_new(const struct comp_driver *drv,
				const struct comp_ipc_config *config,
				const void *spec)
{
	const struct ipc_config_process *ipc_process = spec;
	size_t bs = ipc_process->size;
	struct comp_dev *dev;
	struct comp_data *cd;
	int ret;

	comp_cl_info(&comp_mux, "mux_new()");

	dev = comp_alloc(drv, sizeof(*dev));
	if (!dev)
		return NULL;
	dev->ipc_config = *config;

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
#else
static int build_config(struct comp_data *cd, struct comp_dev *dev)
{
	dev->ipc_config.type = SOF_COMP_MUX;
	cd->config.num_streams = MUX_MAX_STREAMS;
	int mask = 1;
	int i;

	/* clear masks */
	for (i = 0; i < cd->config.num_streams; i++)
		memset(cd->config.streams[i].mask, 0, sizeof(cd->config.streams[i].mask));

	/* Setting masks for streams */
	for (i = 0; i < cd->md.base_cfg.audio_fmt.channels_count; i++) {
		cd->config.streams[0].mask[i] = mask;
		mask <<= 1;
	}

	for (i = 0; i < cd->md.reference_format.channels_count; i++) {
		cd->config.streams[1].mask[i] = mask;
		mask <<= 1;
	}

	/* validation of matrix mixing */
	if (mux_mix_check(&cd->config)) {
		comp_cl_err(&comp_mux, "build_config(): mux component is not able to mix channels");
		return -EINVAL;
	}
	return 0;
}

static struct comp_dev *mux_new(const struct comp_driver *drv,
				const struct comp_ipc_config *config,
				const void *spec)
{
	const struct mux_data *ipc_process = spec;
	struct comp_dev *dev;
	struct comp_data *cd;
	int ret;

	comp_cl_dbg(&comp_mux, "mux_new()");

	dev = comp_alloc(drv, sizeof(*dev));
	if (!dev)
		return NULL;
	dev->ipc_config = *config;

	cd = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(*cd));
	if (!cd) {
		rfree(dev);
		return NULL;
	}

	comp_set_drvdata(dev, cd);

	ret = memcpy_s(&cd->md, sizeof(cd->md), ipc_process, sizeof(*ipc_process));
	assert(!ret);
	ret = build_config(cd, dev);

	if (ret < 0) {
		rfree(cd);
		rfree(dev);
		return NULL;
	}

	dev->state = COMP_STATE_READY;
	return dev;
}

/* In ipc4 case param is figured out by module config so we need to first
 * set up param then verify param. BTW for IPC3 path, the param is sent by
 * host driver.
 */
static void set_mux_params(struct comp_dev *dev,
			   struct sof_ipc_stream_params *params)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct comp_buffer *sink, *source;
	struct comp_buffer __sparse_cache *sink_c, *source_c;
	struct list_item *source_list;
	int i, j, valid_bit_depth;
	const uint32_t byte_align = 1;
	const uint32_t frame_align_req = 1;

	params->direction = dev->direction;
	params->channels =  cd->md.base_cfg.audio_fmt.channels_count;
	params->rate = cd->md.base_cfg.audio_fmt.sampling_frequency;
	params->sample_container_bytes = cd->md.base_cfg.audio_fmt.depth / 8;
	params->sample_valid_bytes = cd->md.base_cfg.audio_fmt.valid_bit_depth / 8;
	params->buffer_fmt = cd->md.base_cfg.audio_fmt.interleaving_style;
	params->buffer.size = cd->md.base_cfg.ibs;
	params->no_stream_position = 1;

	/* There are two input pins and one output pin in the mux.
	 * For the first input we assign parameters from base_cfg,
	 * for the second from reference_format
	 * and for sink output_format.
	 */

	/* update sink format */
	if (!list_is_empty(&dev->bsink_list)) {
		sink = list_first_item(&dev->bsink_list, struct comp_buffer, source_list);
		sink_c = buffer_acquire(sink);
		audio_stream_init_alignment_constants(byte_align, frame_align_req,
						      &sink_c->stream);

		if (!sink_c->hw_params_configured) {
			struct ipc4_audio_format out_fmt;

			out_fmt = cd->md.output_format;
			sink_c->stream.channels = out_fmt.channels_count;
			sink_c->stream.rate = out_fmt.sampling_frequency;
			audio_stream_fmt_conversion(out_fmt.depth,
						    out_fmt.valid_bit_depth,
						    &sink_c->stream.frame_fmt,
						    &sink_c->stream.valid_sample_fmt,
						    out_fmt.s_type);

			sink_c->buffer_fmt = out_fmt.interleaving_style;
			params->frame_fmt = sink_c->stream.frame_fmt;

			for (i = 0; i < SOF_IPC_MAX_CHANNELS; i++)
				sink_c->chmap[i] = (out_fmt.ch_map >> i * 4) & 0xf;

			sink_c->hw_params_configured = true;
		}
		buffer_release(sink_c);
	}

	/* update each source format */
	if (!list_is_empty(&dev->bsource_list)) {
		list_for_item(source_list, &dev->bsource_list)
		{
			source = container_of(source_list, struct comp_buffer, sink_list);
			source_c = buffer_acquire(source);
			audio_stream_init_alignment_constants(byte_align, frame_align_req,
							      &source_c->stream);
			j = source_c->id;
			cd->config.streams[j].pipeline_id = source_c->pipeline_id;
			valid_bit_depth = cd->md.base_cfg.audio_fmt.valid_bit_depth;
			if (j == BASE_CFG_QUEUED_ID) {
				source_c->stream.channels =
						cd->md.base_cfg.audio_fmt.channels_count;
				source_c->stream.rate =
						cd->md.base_cfg.audio_fmt.sampling_frequency;
				audio_stream_fmt_conversion(cd->md.base_cfg.audio_fmt.depth,
							    valid_bit_depth,
							    &source_c->stream.frame_fmt,
							    &source_c->stream.valid_sample_fmt,
							    cd->md.base_cfg.audio_fmt.s_type);

				source_c->buffer_fmt = cd->md.base_cfg.audio_fmt.interleaving_style;

				for (i = 0; i < SOF_IPC_MAX_CHANNELS; i++)
					source_c->chmap[i] =
						(cd->md.base_cfg.audio_fmt.ch_map >> i * 4) & 0xf;
			} else {
				/* set parameters for reference input channels */
				source_c->stream.channels =
						cd->md.reference_format.channels_count;
				source_c->stream.rate = cd->md.reference_format.sampling_frequency;
				audio_stream_fmt_conversion(cd->md.reference_format.depth,
							    cd->md.reference_format.valid_bit_depth,
							    &source_c->stream.frame_fmt,
							    &source_c->stream.valid_sample_fmt,
							    cd->md.reference_format.s_type);
				source_c->buffer_fmt = cd->md.reference_format.interleaving_style;

				for (i = 0; i < SOF_IPC_MAX_CHANNELS; i++)
					source_c->chmap[i] =
						(cd->md.reference_format.ch_map >> i * 4) & 0xf;
			}
			source_c->hw_params_configured = true;
			buffer_release(source_c);
		}
	}

	mux_prepare_look_up_table(dev);
}

static int mux_get_attribute(struct comp_dev *dev, uint32_t type, void *value)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	switch (type) {
	case COMP_ATTR_BASE_CONFIG:
		*(struct ipc4_base_module_cfg *)value = cd->md.base_cfg;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}
#endif

static void mux_free(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	comp_dbg(dev, "mux_free()");

	rfree(cd);
	rfree(dev);
}

static int get_stream_index(struct comp_data *cd, uint32_t pipe_id)
{
	int idx;

	for (idx = 0; idx < MUX_MAX_STREAMS; idx++)
		if (cd->config.streams[idx].pipeline_id == pipe_id)
			return idx;

	comp_cl_err(&comp_mux, "get_stream_index(): couldn't find configuration for connected pipeline %u",
		    pipe_id);

	return -EINVAL;
}

static struct mux_look_up *get_lookup_table(struct comp_data *cd,
					    uint32_t pipe_id)
{
	int i;

	for (i = 0; i < MUX_MAX_STREAMS; i++)
		if (cd->config.streams[i].pipeline_id == pipe_id)
			return &cd->lookup[i];

	comp_cl_err(&comp_mux, "get_lookup_table(): couldn't find configuration for connected pipeline %u",
		    pipe_id);

	return 0;
}

/* set component audio stream parameters */
static int mux_params(struct comp_dev *dev,
		      struct sof_ipc_stream_params *params)
{
	int ret;

	comp_dbg(dev, "mux_params()");
#if CONFIG_IPC_MAJOR_4
	set_mux_params(dev, params);
#endif
	ret = comp_verify_params(dev, BUFF_PARAMS_CHANNELS, params);
	if (ret < 0)
		comp_err(dev, "mux_params(): comp_verify_params() failed.");

	return ret;
}

static int mux_ctrl_set_cmd(struct comp_dev *dev,
			    struct sof_ipc_ctrl_data *cdata)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct sof_mux_config *cfg;
	int ret = 0;

	comp_dbg(dev, "mux_ctrl_set_cmd(), cdata->cmd = 0x%08x",
		 cdata->cmd);

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_BINARY:
		cfg = (struct sof_mux_config *)
		      ASSUME_ALIGNED(&cdata->data->data, 4);

		ret = mux_set_values(dev, cd, cfg);
		break;
	default:
		comp_err(dev, "mux_ctrl_set_cmd(): invalid cdata->cmd = 0x%08x",
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
		const int mux_memcpy_err __unused =
			memcpy_s(cdata->data->data, ((struct sof_abi_hdr *)
						     (cdata->data))->size, cfg, reply_size);
		assert(!mux_memcpy_err);

		cdata->data->abi = SOF_ABI_VERSION;
		cdata->data->size = reply_size;
		break;
	default:
		comp_cl_err(&comp_mux, "mux_ctrl_set_cmd(): invalid cdata->cmd = 0x%08x",
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
	struct sof_ipc_ctrl_data *cdata = ASSUME_ALIGNED(data, 4);

	comp_dbg(dev, "mux_cmd() cmd = 0x%08x", cmd);

	switch (cmd) {
	case COMP_CMD_SET_DATA:
		return mux_ctrl_set_cmd(dev, cdata);
	case COMP_CMD_GET_DATA:
		return mux_ctrl_get_cmd(dev, cdata, max_data_size);
	default:
		return -EINVAL;
	}
}

static void mux_prepare_active_look_up(struct comp_dev *dev,
				       struct audio_stream __sparse_cache *sink,
				       const struct audio_stream __sparse_cache **sources)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	const struct audio_stream __sparse_cache *source;
	uint8_t active_elem;
	uint8_t elem;

	cd->active_lookup.num_elems = 0;

	active_elem = 0;

	/* init pointers */
	for (elem = 0; elem < cd->lookup[0].num_elems; elem++) {
		source = sources[cd->lookup[0].copy_elem[elem].stream_id];
		if (!source)
			continue;

		if ((cd->lookup[0].copy_elem[elem].in_ch >
		    (source->channels - 1)) ||
		    (cd->lookup[0].copy_elem[elem].out_ch >
		    (sink->channels - 1)))
			continue;

		cd->active_lookup.copy_elem[active_elem] =
			cd->lookup[0].copy_elem[elem];
		active_elem++;
		cd->active_lookup.num_elems = active_elem;
	}
}

static void demux_prepare_active_look_up(struct comp_dev *dev,
					 struct audio_stream __sparse_cache *sink,
					 const struct audio_stream __sparse_cache *source,
					 struct mux_look_up *look_up)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	uint8_t active_elem;
	uint8_t elem;

	cd->active_lookup.num_elems = 0;

	active_elem = 0;

	/* init pointers */
	for (elem = 0; elem < look_up->num_elems; elem++) {
		if ((look_up->copy_elem[elem].in_ch >
		    (source->channels - 1)) ||
		    (look_up->copy_elem[elem].out_ch >
		    (sink->channels - 1)))
			continue;

		cd->active_lookup.copy_elem[active_elem] =
			look_up->copy_elem[elem];
		active_elem++;
		cd->active_lookup.num_elems = active_elem;
	}
}

/* process and copy stream data from source to sink buffers */
static int demux_copy(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct comp_buffer *source, *sink;
	struct comp_buffer __sparse_cache *source_c, *sink_c;
	struct comp_buffer __sparse_cache *sinks[MUX_MAX_STREAMS] = { NULL };
	struct mux_look_up *look_ups[MUX_MAX_STREAMS] = { NULL };
	struct mux_look_up *look_up;
	struct list_item *clist;
	uint32_t num_sinks = 0;
	uint32_t frames = -1;
	uint32_t source_bytes;
	uint32_t avail;
	uint32_t sinks_bytes[MUX_MAX_STREAMS] = { 0 };
	int i, ret = 0;

	comp_dbg(dev, "demux_copy()");

	if (!cd->demux) {
		comp_err(dev, "demux_copy(): no demux processing function for component.");
		comp_set_state(dev, COMP_TRIGGER_RESET);
		return -EINVAL;
	}

	// align sink streams with their respective configurations
	list_for_item(clist, &dev->bsink_list) {
		sink = container_of(clist, struct comp_buffer, source_list);
		sink_c = buffer_acquire(sink);

		if (sink_c->sink->state == dev->state) {
			num_sinks++;
			i = get_stream_index(cd, sink_c->pipeline_id);
			/* return if index wrong */
			if (i < 0) {
				ret = i;
				goto out_sink;
			}
			look_up = get_lookup_table(cd, sink_c->pipeline_id);
			sinks[i] = sink_c;
			look_ups[i] = look_up;
		} else {
			buffer_release(sink_c);
		}
	}

	/* if there are no sinks active, then sinks[] is also empty */
	if (num_sinks == 0)
		return 0;

	source = list_first_item(&dev->bsource_list, struct comp_buffer,
				 sink_list);
	source_c = buffer_acquire(source);

	/* check if source is active */
	if (source_c->source->state != dev->state)
		goto out_source;

	for (i = 0; i < MUX_MAX_STREAMS; i++) {
		if (!sinks[i])
			continue;
		avail = audio_stream_avail_frames(&source_c->stream,
						  &sinks[i]->stream);
		frames = MIN(frames, avail);
	}

	source_bytes = frames * audio_stream_frame_bytes(&source_c->stream);
	for (i = 0; i < MUX_MAX_STREAMS; i++)
		if (sinks[i])
			sinks_bytes[i] = frames *
				audio_stream_frame_bytes(&sinks[i]->stream);

	/* produce output, one sink at a time */
	for (i = 0; i < MUX_MAX_STREAMS; i++) {
		if (!sinks[i])
			continue;

		demux_prepare_active_look_up(dev, &sinks[i]->stream,
					     &source_c->stream, look_ups[i]);
		buffer_stream_invalidate(source_c, source_bytes);
		cd->demux(dev, &sinks[i]->stream, &source_c->stream, frames,
			  &cd->active_lookup);
		buffer_stream_writeback(sinks[i], sinks_bytes[i]);
	}

	/* update components */
	for (i = 0; i < MUX_MAX_STREAMS; i++)
		if (sinks[i])
			comp_update_buffer_produce(sinks[i], sinks_bytes[i]);

	comp_update_buffer_consume(source_c, source_bytes);

out_source:
	buffer_release(source_c);
out_sink:
	/* Release buffers in reverse order */
	for (i = MUX_MAX_STREAMS - 1; i >= 0; i--)
		if (sinks[i])
			buffer_release(sinks[i]);

	return ret;
}

/* process and copy stream data from source to sink buffers */
static int mux_copy(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct comp_buffer *source, *sink;
	struct comp_buffer __sparse_cache *source_c, *sink_c;
	struct comp_buffer __sparse_cache *sources[MUX_MAX_STREAMS] = { NULL };
	const struct audio_stream __sparse_cache *sources_stream[MUX_MAX_STREAMS] = { NULL };
	struct list_item *clist;
	uint32_t num_sources = 0;
	uint32_t frames = -1;
	uint32_t max_frames = 0;
	uint32_t sources_bytes[MUX_MAX_STREAMS] = { 0 };
	uint32_t sink_bytes;
	int i;

	comp_dbg(dev, "mux_copy()");

	if (!cd->mux) {
		comp_err(dev, "mux_copy(): no mux processing function for component.");
		comp_set_state(dev, COMP_TRIGGER_RESET);
		return -EINVAL;
	}

	/* align source streams with their respective configurations */
	list_for_item(clist, &dev->bsource_list) {
		source = container_of(clist, struct comp_buffer, sink_list);
		source_c = buffer_acquire(source);

		if (source_c->source->state == dev->state) {
			num_sources++;
			i = get_stream_index(cd, source_c->pipeline_id);
			/* return if index wrong */
			if (i < 0) {
				unsigned int j;

				for (j = 0; j < MUX_MAX_STREAMS; j++)
					if (sources[j])
						buffer_release(sources[j]);
				return i;
			}
			sources[i] = source_c;
			sources_stream[i] = &source_c->stream;
		} else {
			buffer_release(source_c);
		}
	}

	/* check if there are any sources active */
	if (num_sources == 0)
		return 0;

	sink = list_first_item(&dev->bsink_list, struct comp_buffer,
			       source_list);

	sink_c = buffer_acquire(sink);

	/* check if sink is active */
	if (sink_c->sink->state != dev->state)
		goto out;

	for (i = 0; i < MUX_MAX_STREAMS; i++) {
		uint32_t avail_frames;
		if (!sources[i])
			continue;
		avail_frames = audio_stream_avail_frames(sources_stream[i],
							 &sink_c->stream);
		frames = MIN(frames, avail_frames);
		max_frames = MAX(max_frames,avail_frames);
	}

	for (i = 0; i < MUX_MAX_STREAMS; i++) {
		if (!sources[i])
			continue;
		sources_bytes[i] = frames *
				   audio_stream_frame_bytes(sources_stream[i]);
		buffer_stream_invalidate(sources[i], sources_bytes[i]);
	}
	sink_bytes = frames * audio_stream_frame_bytes(&sink_c->stream);

	if(0==sink_bytes)
	{
		if((max_frames!=0) &&
		(num_sources>1))
		{
			sink_bytes = max_frames * audio_stream_frame_bytes(&sink_c->stream);
		}
		comp_warn(dev, "WARN MUX SINK BYTES:%d frames:%d num_src:%d max:%d",sink_bytes,frames,num_sources,max_frames);
	}
	mux_prepare_active_look_up(dev, &sink_c->stream, &sources_stream[0]);

	/* produce output */
	cd->mux(dev, &sink_c->stream, &sources_stream[0], frames,
		&cd->active_lookup);
	buffer_stream_writeback(sink_c, sink_bytes);

	/* update components */
	comp_update_buffer_produce(sink_c, sink_bytes);
	for (i = 0; i < MUX_MAX_STREAMS; i++)
		if (sources[i])
			comp_update_buffer_consume(sources[i], sources_bytes[i]);

out:
	buffer_release(sink_c);

	for (i = MUX_MAX_STREAMS - 1; i >= 0; i--)
		if (sources[i])
			buffer_release(sources[i]);

	return 0;
}

static int mux_reset(struct comp_dev *dev)
{
	int dir = dev->pipeline->source_comp->direction;
	struct list_item *blist;
	struct comp_data *cd = comp_get_drvdata(dev);

	comp_dbg(dev, "mux_reset()");

	if (dir == SOF_IPC_STREAM_PLAYBACK) {
		list_for_item(blist, &dev->bsource_list) {
			struct comp_buffer *source = container_of(blist, struct comp_buffer,
								  sink_list);
			struct comp_buffer __sparse_cache *source_c = buffer_acquire(source);
			int state = source_c->source->state;

			buffer_release(source_c);

			/* only mux the sources with the same state with mux */
			if (state > COMP_STATE_READY)
				/* should not reset the downstream components */
				return PPL_STATUS_PATH_STOP;
		}
	}

	if (dev->ipc_config.type == SOF_COMP_MUX)
		cd->mux = NULL;
	else
		cd->demux = NULL;

	comp_set_state(dev, COMP_TRIGGER_RESET);
	return 0;
}

static int mux_prepare(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct list_item *blist;
	int ret;

	comp_dbg(dev, "mux_prepare()");

	if (dev->state != COMP_STATE_ACTIVE) {
		if (dev->ipc_config.type == SOF_COMP_MUX)
			cd->mux = mux_get_processing_function(dev);
		else
			cd->demux = demux_get_processing_function(dev);

		if (!cd->mux && !cd->demux) {
			comp_err(dev, "mux_prepare(): Invalid configuration, couldn't find suitable processing function.");
			return -EINVAL;
		}

		ret = comp_set_state(dev, COMP_TRIGGER_PREPARE);
		if (ret) {
			comp_info(dev, "mux_prepare() comp_set_state() returned non-zero.");
			return ret;
		}
	}

	/* check each mux source state */
	list_for_item(blist, &dev->bsource_list) {
		struct comp_buffer *source = container_of(blist, struct comp_buffer,
							  sink_list);
		struct comp_buffer __sparse_cache *source_c = buffer_acquire(source);
		int state = source_c->source->state;

		buffer_release(source_c);

		/* only prepare downstream if we have no active sources */
		if (state == COMP_STATE_PAUSED || state == COMP_STATE_ACTIVE)
			return PPL_STATUS_PATH_STOP;
	}

	/* prepare downstream */
	return 0;
}

static int mux_source_status_count(struct comp_dev *mux, uint32_t status)
{
	struct list_item *blist;
	int count = 0;

	/* count source with state == status */
	list_for_item(blist, &mux->bsource_list) {
		struct comp_buffer *source = container_of(blist, struct comp_buffer,
							  sink_list);
		struct comp_buffer __sparse_cache *source_c = buffer_acquire(source);

		if (source_c->source->state == status)
			count++;
		buffer_release(source_c);
	}

	return count;
}

static int mux_trigger(struct comp_dev *dev, int cmd)
{
	int dir = dev->pipeline->source_comp->direction;
	unsigned int src_n_active, src_n_paused;
	int ret;

	comp_dbg(dev, "mux_trigger(), command = %u", cmd);

	/*
	 * We are in a TRIGGER IPC. IPCs are serialised, while we're processing
	 * this one, no other IPCs can be received until we have replied to the
	 * current one
	 */
	src_n_active = mux_source_status_count(dev, COMP_STATE_ACTIVE);
	src_n_paused = mux_source_status_count(dev, COMP_STATE_PAUSED);
#if CONFIG_IPC_MAJOR_4
	if (dir == SOF_IPC_STREAM_PLAYBACK) {
		switch (cmd) {
		case COMP_TRIGGER_PRE_START:
			if (src_n_active || src_n_paused)
				return PPL_STATUS_PATH_STOP;
			break;
		case COMP_TRIGGER_PRE_RELEASE:
			dev->state = COMP_STATE_PRE_ACTIVE;
			break;
		case COMP_TRIGGER_RELEASE:
			dev->state = COMP_STATE_ACTIVE;
			break;
		default:
			break;
		}
	}
#else
	switch (cmd) {
	case COMP_TRIGGER_PRE_START:
		if (src_n_active || src_n_paused)
			return PPL_STATUS_PATH_STOP;
	}
#endif
	ret = comp_set_state(dev, cmd);
	if (ret < 0)
		return ret;

	if (ret == COMP_STATUS_STATE_ALREADY_SET)
		ret = PPL_STATUS_PATH_STOP;

	/* nothing else to check for capture streams */
	if (dir == SOF_IPC_STREAM_CAPTURE)
		return ret;

	/* don't stop mux if at least one source is active */
	if (src_n_active && (cmd == COMP_TRIGGER_PAUSE || cmd == COMP_TRIGGER_STOP)) {
		dev->state = COMP_STATE_ACTIVE;
		ret = PPL_STATUS_PATH_STOP;
	/* don't stop mux if at least one source is paused */
	} else if (src_n_paused && cmd == COMP_TRIGGER_STOP) {
		dev->state = COMP_STATE_PAUSED;
		ret = PPL_STATUS_PATH_STOP;
	}

	return ret;
}

static const struct comp_driver comp_mux = {
	.type	= SOF_COMP_MUX,
	.uid	= SOF_RT_UUID(mux_uuid),
	.tctx	= &mux_tr,
	.ops	= {
		.create		= mux_new,
		.free		= mux_free,
		.params		= mux_params,
		.cmd		= mux_cmd,
		.copy		= mux_copy,
		.prepare	= mux_prepare,
		.reset		= mux_reset,
		.trigger	= mux_trigger,
#if CONFIG_IPC_MAJOR_4
		.get_attribute = mux_get_attribute,
#endif
	},
};

static SHARED_DATA struct comp_driver_info comp_mux_info = {
	.drv = &comp_mux,
};

static const struct comp_driver comp_demux = {
	.type	= SOF_COMP_DEMUX,
	.uid	= SOF_RT_UUID(demux_uuid),
	.tctx	= &demux_tr,
	.ops	= {
		.create		= mux_new,
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
SOF_MODULE_INIT(mux, sys_comp_mux_init);

#endif /* CONFIG_COMP_MUX */
