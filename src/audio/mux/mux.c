// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Artur Kloniecki <arturx.kloniecki@linux.intel.com>

#if CONFIG_COMP_MUX

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/component.h>
#include <sof/audio/data_blob.h>
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

#define MUX_BLOB_STREAMS_SIZE	(MUX_MAX_STREAMS * sizeof(struct mux_stream_data))
#define MUX_BLOB_MAX_SIZE	(sizeof(struct sof_mux_config) + MUX_BLOB_STREAMS_SIZE)

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

#if CONFIG_IPC_MAJOR_3
static int mux_set_values(struct processing_module *mod)
{
	struct comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	struct sof_mux_config *cfg = &cd->config;
	unsigned int i;
	unsigned int j;

	comp_dbg(dev, "mux_set_values()");

	/* check if number of streams configured doesn't exceed maximum */
	if (cfg->num_streams > MUX_MAX_STREAMS) {
		comp_err(dev, "mux_set_values(): configured number of streams (%u) exceeds maximum = "
			    META_QUOTE(MUX_MAX_STREAMS), cfg->num_streams);
		return -EINVAL;
	}

	/* check if all streams configured have distinct IDs */
	for (i = 0; i < cfg->num_streams; i++) {
		for (j = i + 1; j < cfg->num_streams; j++) {
			if (cfg->streams[i].pipeline_id ==
				cfg->streams[j].pipeline_id) {
				comp_err(dev, "mux_set_values(): multiple configured streams have same pipeline ID = %u",
					 cfg->streams[i].pipeline_id);
				return -EINVAL;
			}
		}
	}

	for (i = 0; i < cfg->num_streams; i++) {
		for (j = 0 ; j < PLATFORM_MAX_CHANNELS; j++) {
			if (popcount(cfg->streams[i].mask[j]) > 1) {
				comp_err(dev, "mux_set_values(): mux component is not able to mix channels");
				return -EINVAL;
			}
		}
	}

	if (dev->ipc_config.type == SOF_COMP_MUX) {
		if (mux_mix_check(cfg))
			comp_err(dev, "mux_set_values(): mux component is not able to mix channels");
	}

	for (i = 0; i < cfg->num_streams; i++) {
		cd->config.streams[i].pipeline_id = cfg->streams[i].pipeline_id;
		for (j = 0; j < PLATFORM_MAX_CHANNELS; j++)
			cd->config.streams[i].mask[j] = cfg->streams[i].mask[j];
	}

	cd->config.num_streams = cfg->num_streams;

	if (dev->ipc_config.type == SOF_COMP_MUX)
		mux_prepare_look_up_table(mod);
	else
		demux_prepare_look_up_table(mod);

	if (dev->state > COMP_STATE_INIT) {
		if (dev->ipc_config.type == SOF_COMP_MUX)
			cd->mux = mux_get_processing_function(mod);
		else
			cd->demux = demux_get_processing_function(mod);
	}

	return 0;
}
#endif /* CONFIG_IPC_MAJOR_3 */

#if CONFIG_IPC_MAJOR_4
static int build_config(struct processing_module *mod)
{
	struct comp_dev *dev = mod->dev;
	struct comp_data *cd = module_get_private_data(mod);
	int mask = 1;
	int i;

	dev->ipc_config.type = SOF_COMP_MUX;
	cd->config.num_streams = MUX_MAX_STREAMS;

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
		comp_err(dev, "build_config(): mux component is not able to mix channels");
		return -EINVAL;
	}
	return 0;
}
#endif

static int mux_demux_common_init(struct processing_module *mod)
{
	struct module_data *module_data = &mod->priv;
	struct comp_dev *dev = mod->dev;
	struct module_config *cfg = &module_data->cfg;
	struct comp_data *cd;
	int ret;

	comp_dbg(dev, "mux_init()");

	if (cfg->size > MUX_BLOB_MAX_SIZE) {
		comp_err(dev, "mux_init(): blob size %d exceeds %d", cfg->size, MUX_BLOB_MAX_SIZE);
		return -EINVAL;
	}

	cd = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM,
		     sizeof(*cd) + MUX_BLOB_STREAMS_SIZE);
	if (!cd)
		return -ENOMEM;

	cd->model_handler = comp_data_blob_handler_new(dev);
	if (!cd->model_handler) {
		comp_err(dev, "mux_init(): comp_data_blob_handler_new() failed.");
		ret = -ENOMEM;
		goto err;
	}

	module_data->private = cd;
	ret = comp_init_data_blob(cd->model_handler, cfg->size, cfg->init_data);
	if (ret < 0) {
		comp_err(dev, "mux_init(): comp_init_data_blob() failed.");
		goto err_init;
	}

	mod->verify_params_flags = BUFF_PARAMS_CHANNELS;
	mod->no_pause = true;
	return 0;

err_init:
	comp_data_blob_handler_free(cd->model_handler);

err:
	rfree(cd);
	return ret;
}

static int mux_init(struct processing_module *mod)
{
	mod->max_sources = MUX_MAX_STREAMS;

	return mux_demux_common_init(mod);
}

static int demux_init(struct processing_module *mod)
{
	mod->max_sinks = MUX_MAX_STREAMS;

	return mux_demux_common_init(mod);
}
#if CONFIG_IPC_MAJOR_4
/* In ipc4 case param is figured out by module config so we need to first
 * set up param then verify param. BTW for IPC3 path, the param is sent by
 * host driver.
 */
static void set_mux_params(struct processing_module *mod)
{
	struct sof_ipc_stream_params *params = mod->stream_params;
	struct comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	struct comp_buffer *sink, *source;
	struct list_item *source_list;
	int j;
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
		audio_stream_init_alignment_constants(byte_align, frame_align_req,
						      &sink->stream);

		if (!sink->hw_params_configured) {
			ipc4_update_buffer_format(sink, &cd->md.output_format);
			params->frame_fmt = audio_stream_get_frm_fmt(&sink->stream);
		}
	}

	/* update each source format */
	if (!list_is_empty(&dev->bsource_list)) {
		struct ipc4_audio_format *audio_fmt;

		list_for_item(source_list, &dev->bsource_list)
		{
			source = container_of(source_list, struct comp_buffer, sink_list);
			audio_stream_init_alignment_constants(byte_align, frame_align_req,
							      &source->stream);
			j = source->id;
			cd->config.streams[j].pipeline_id = source->pipeline_id;
			if (j == BASE_CFG_QUEUED_ID)
				audio_fmt = &cd->md.base_cfg.audio_fmt;
			else
				audio_fmt = &cd->md.reference_format;

			ipc4_update_buffer_format(source, audio_fmt);
		}
	}

	mux_prepare_look_up_table(mod);
}
#endif /* CONFIG_IPC_MAJOR_4 */


static int mux_free(struct processing_module *mod)
{
	struct comp_data *cd = module_get_private_data(mod);

	comp_dbg(mod->dev, "mux_free()");

	comp_data_blob_handler_free(cd->model_handler);
	rfree(cd);
	return 0;
}

static int get_stream_index(struct comp_dev *dev, struct comp_data *cd, uint32_t pipe_id)
{
	int idx;

	for (idx = 0; idx < MUX_MAX_STREAMS; idx++)
		if (cd->config.streams[idx].pipeline_id == pipe_id)
			return idx;

	comp_err(dev, "get_stream_index(): couldn't find configuration for connected pipeline %u",
		 pipe_id);
	return -EINVAL;
}

static struct mux_look_up *get_lookup_table(struct comp_dev *dev, struct comp_data *cd,
					    uint32_t pipe_id)
{
	int i;

	for (i = 0; i < MUX_MAX_STREAMS; i++)
		if (cd->config.streams[i].pipeline_id == pipe_id)
			return &cd->lookup[i];

	comp_err(dev, "get_lookup_table(): couldn't find configuration for connected pipeline %u",
		 pipe_id);
	return 0;
}

static void mux_prepare_active_look_up(struct comp_data *cd,
				       struct audio_stream *sink,
				       const struct audio_stream **sources)
{
	const struct audio_stream *source;
	int elem;
	int active_elem = 0;

	/* init pointers */
	for (elem = 0; elem < cd->lookup[0].num_elems; elem++) {
		source = sources[cd->lookup[0].copy_elem[elem].stream_id];
		if (!source)
			continue;

		if (cd->lookup[0].copy_elem[elem].in_ch >= audio_stream_get_channels(source) ||
		    cd->lookup[0].copy_elem[elem].out_ch >= audio_stream_get_channels(sink))
			continue;

		cd->active_lookup.copy_elem[active_elem] = cd->lookup[0].copy_elem[elem];
		active_elem++;
	}

	cd->active_lookup.num_elems = active_elem;
}

static void demux_prepare_active_look_up(struct comp_data *cd,
					 struct audio_stream *sink,
					 const struct audio_stream *source,
					 struct mux_look_up *look_up)
{
	int elem;
	int active_elem = 0;

	/* init pointers */
	for (elem = 0; elem < look_up->num_elems; elem++) {
		if (look_up->copy_elem[elem].in_ch >= audio_stream_get_channels(source) ||
		    look_up->copy_elem[elem].out_ch >= audio_stream_get_channels(sink))
			continue;

		cd->active_lookup.copy_elem[active_elem] = look_up->copy_elem[elem];
		active_elem++;
	}

	cd->active_lookup.num_elems = active_elem;
}

/* process and copy stream data from source to sink buffers */
static int demux_process(struct processing_module *mod,
			 struct input_stream_buffer *input_buffers, int num_input_buffers,
			 struct output_stream_buffer *output_buffers, int num_output_buffers)
{
	struct comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	struct list_item *clist;
	struct comp_buffer *sink;
	struct audio_stream *sinks_stream[MUX_MAX_STREAMS] = { NULL };
	struct mux_look_up *look_ups[MUX_MAX_STREAMS] = { NULL };
	int frames;
	int sink_bytes;
	int source_bytes;
	int i;

	comp_dbg(dev, "demux_process()");

	/* align sink streams with their respective configurations */
	list_for_item(clist, &dev->bsink_list) {
		sink = container_of(clist, struct comp_buffer, source_list);
		if (sink->sink->state == dev->state) {
			i = get_stream_index(dev, cd, sink->pipeline_id);
			/* return if index wrong */
			if (i < 0) {
				return i;
			}

			look_ups[i] = get_lookup_table(dev, cd, sink->pipeline_id);
			sinks_stream[i] = &sink->stream;
		}
	}

	/* if there are no sinks active, then sinks[] is also empty */
	if (num_output_buffers == 0)
		return 0;

	frames = input_buffers[0].size;
	source_bytes = frames * audio_stream_frame_bytes(mod->input_buffers[0].data);
	sink_bytes = frames * audio_stream_frame_bytes(mod->output_buffers[0].data);

	/* produce output, one sink at a time */
	for (i = 0; i < num_output_buffers; i++) {
		demux_prepare_active_look_up(cd, sinks_stream[i],
					     input_buffers[0].data, look_ups[i]);
		cd->demux(dev, sinks_stream[i], input_buffers[0].data, frames, &cd->active_lookup);
		mod->output_buffers[i].size = sink_bytes;
	}

	/* Update consumed */
	mod->input_buffers[0].consumed = source_bytes;
	return 0;
}

/* process and copy stream data from source to sink buffers */
static int mux_process(struct processing_module *mod,
		       struct input_stream_buffer *input_buffers, int num_input_buffers,
		       struct output_stream_buffer *output_buffers, int num_output_buffers)
{
	struct comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	struct comp_buffer *source;
	struct list_item *clist;
	const struct audio_stream *sources_stream[MUX_MAX_STREAMS] = { NULL };
	int frames = 0;
	int sink_bytes;
	int source_bytes;
	int i, j;

	comp_dbg(dev, "mux_process()");

	/* align source streams with their respective configurations */
	j = 0;
	list_for_item(clist, &dev->bsource_list) {
		source = container_of(clist, struct comp_buffer, sink_list);
		if (source->source->state == dev->state) {
			if (frames)
				frames = MIN(frames, input_buffers[j].size);
			else
				frames = input_buffers[j].size;

			i = get_stream_index(dev, cd, source->pipeline_id);
			/* return if index wrong */
			if (i < 0) {
				return i;
			}

			sources_stream[i] = &source->stream;
		}
		j++;
	}

	/* check if there are any sources active */
	if (num_input_buffers == 0)
		return 0;

	source_bytes = frames * audio_stream_frame_bytes(mod->input_buffers[0].data);
	sink_bytes = frames * audio_stream_frame_bytes(mod->output_buffers[0].data);
	mux_prepare_active_look_up(cd, output_buffers[0].data, &sources_stream[0]);

	/* produce output */
	cd->mux(dev, output_buffers[0].data, &sources_stream[0], frames, &cd->active_lookup);

	/* Update consumed and produced */
	j = 0;
	list_for_item(clist, &dev->bsource_list) {
		source = container_of(clist, struct comp_buffer, sink_list);
		if (source->source->state == dev->state)
			mod->input_buffers[j].consumed = source_bytes;
		j++;
	}
	mod->output_buffers[0].size = sink_bytes;
	return 0;
}

static int mux_reset(struct processing_module *mod)
{
	struct list_item *blist;
	struct comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	int dir = dev->pipeline->source_comp->direction;

	comp_dbg(dev, "mux_reset()");

	if (dir == SOF_IPC_STREAM_PLAYBACK) {
		list_for_item(blist, &dev->bsource_list) {
			struct comp_buffer *source = container_of(blist, struct comp_buffer,
								  sink_list);
			int state = source->source->state;

			/* only mux the sources with the same state with mux */
			if (state > COMP_STATE_READY)
				/* should not reset the downstream components */
				return PPL_STATUS_PATH_STOP;
		}
	}

	cd->mux = NULL;
	cd->demux = NULL;
	return 0;
}

static int mux_prepare(struct processing_module *mod,
		       struct sof_source **sources, int num_of_sources,
		       struct sof_sink **sinks, int num_of_sinks)
{
	struct comp_dev *dev = mod->dev;
	struct comp_data *cd = module_get_private_data(mod);
	struct list_item *blist;
	struct comp_buffer *source;
	struct comp_buffer *sink;
	struct sof_mux_config *config;
	size_t blob_size;
	int state;
	int ret;

	comp_dbg(dev, "mux_prepare()");

	config = comp_get_data_blob(cd->model_handler, &blob_size, NULL);
	if (blob_size > MUX_BLOB_MAX_SIZE) {
		comp_err(dev, "mux_prepare(): illegal blob size %d", blob_size);
		return -EINVAL;
	}

	memcpy_s(&cd->config, MUX_BLOB_MAX_SIZE, config, blob_size);

#if CONFIG_IPC_MAJOR_4
	ret = build_config(mod);
#else
	ret = mux_set_values(mod);
#endif
	if (ret < 0)
		return ret;

#if CONFIG_IPC_MAJOR_4
	set_mux_params(mod);
#endif

	if (dev->ipc_config.type == SOF_COMP_MUX)
		cd->mux = mux_get_processing_function(mod);
	else
		cd->demux = demux_get_processing_function(mod);

	if (!cd->mux && !cd->demux) {
		comp_err(dev, "mux_prepare(): Invalid configuration, couldn't find suitable processing function.");
		return -EINVAL;
	}

	/* check each mux source state, set source align to 1 byte, 1 frame */
	list_for_item(blist, &dev->bsource_list) {
		source = container_of(blist, struct comp_buffer, sink_list);
		state = source->source->state;
		audio_stream_init_alignment_constants(1, 1, &source->stream);

		/* only prepare downstream if we have no active sources */
		if (state == COMP_STATE_PAUSED || state == COMP_STATE_ACTIVE)
			return PPL_STATUS_PATH_STOP;
	}

	/* set sink align to 1 byte, 1 frame */
	list_for_item(blist, &dev->bsink_list) {
		sink = container_of(blist, struct comp_buffer, source_list);
		audio_stream_init_alignment_constants(1, 1, &sink->stream);
	}

	/* prepare downstream */
	return 0;
}

static int mux_get_config(struct processing_module *mod,
			  uint32_t config_id, uint32_t *data_offset_size,
			  uint8_t *fragment, size_t fragment_size)
{
	struct sof_ipc_ctrl_data *cdata = (struct sof_ipc_ctrl_data *)fragment;
	struct comp_data *cd = module_get_private_data(mod);

	comp_dbg(mod->dev, "mux_get_config()");

	return comp_data_blob_get_cmd(cd->model_handler, cdata, fragment_size);
}

static int mux_set_config(struct processing_module *mod, uint32_t config_id,
			  enum module_cfg_fragment_position pos, uint32_t data_offset_size,
			  const uint8_t *fragment, size_t fragment_size, uint8_t *response,
			  size_t response_size)
{
	struct comp_data *cd = module_get_private_data(mod);

	comp_dbg(mod->dev, "mux_set_config()");

	return comp_data_blob_set(cd->model_handler, pos, data_offset_size,
				  fragment, fragment_size);
}

static const struct module_interface mux_interface = {
	.init = mux_init,
	.set_configuration = mux_set_config,
	.get_configuration = mux_get_config,
	.prepare = mux_prepare,
	.process_audio_stream = mux_process,
	.reset = mux_reset,
	.free = mux_free,
};

DECLARE_MODULE_ADAPTER(mux_interface, mux_uuid, mux_tr);
SOF_MODULE_INIT(mux, sys_comp_module_mux_interface_init);

static const struct module_interface demux_interface = {
	.init = demux_init,
	.set_configuration = mux_set_config,
	.get_configuration = mux_get_config,
	.prepare = mux_prepare,
	.process_audio_stream = demux_process,
	.reset = mux_reset,
	.free = mux_free,
};

DECLARE_MODULE_ADAPTER(demux_interface, demux_uuid, demux_tr);
SOF_MODULE_INIT(demux, sys_comp_module_demux_interface_init);

#endif /* CONFIG_COMP_MUX */
