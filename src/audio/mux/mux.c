// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Artur Kloniecki <arturx.kloniecki@linux.intel.com>

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/component.h>
#include <sof/audio/data_blob.h>
#include <sof/audio/ipc-config.h>
#include <sof/common.h>
#include <sof/ipc/msg.h>
#include <rtos/alloc.h>
#include <rtos/init.h>
#include <sof/lib/uuid.h>
#include <sof/list.h>
#include <sof/math/numbers.h>
#include <sof/platform.h>
#include <rtos/string.h>
#include <sof/trace/trace.h>
#include <sof/ut.h>
#include <ipc/control.h>
#include <ipc/topology.h>
#include <user/trace.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

#include "mux.h"

LOG_MODULE_REGISTER(muxdemux, CONFIG_SOF_LOG_LEVEL);

/*
 * Check that we are not configuring routing matrix for mixing.
 *
 * In mux case this means, that muxed streams' configuration matrices don't
 * have 1's in corresponding matrix indices. Also single stream matrix can't
 * have 1's in same column as that corresponds to mixing also.
 */
bool mux_mix_check(struct sof_mux_config *cfg)
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

static int mux_demux_common_init(struct processing_module *mod, enum sof_comp_type type)
{
	struct module_data *module_data = &mod->priv;
	struct comp_dev *dev = mod->dev;
	struct module_config *cfg = &module_data->cfg;
	struct comp_data *cd;
	int ret;

	comp_dbg(dev, "mux_init()");

	if (cfg->size > MUX_BLOB_MAX_SIZE) {
		comp_err(dev, "blob size %zu exceeds %zu",
			 cfg->size, MUX_BLOB_MAX_SIZE);
		return -EINVAL;
	}

	cd = rzalloc(SOF_MEM_FLAG_USER,
		     sizeof(*cd) + MUX_BLOB_STREAMS_SIZE);
	if (!cd)
		return -ENOMEM;

	cd->model_handler = comp_data_blob_handler_new(dev);
	if (!cd->model_handler) {
		comp_err(dev, "comp_data_blob_handler_new() failed.");
		ret = -ENOMEM;
		goto err;
	}

	module_data->private = cd;
	ret = comp_init_data_blob(cd->model_handler, cfg->size, cfg->init_data);
	if (ret < 0) {
		comp_err(dev, "comp_init_data_blob() failed.");
		goto err_init;
	}

	mod->verify_params_flags = BUFF_PARAMS_CHANNELS;
	mod->no_pause = true;
	cd->comp_type = type;
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

	return mux_demux_common_init(mod, SOF_COMP_MUX);
}

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

	comp_err(dev, "couldn't find configuration for connected pipeline %u",
		 pipe_id);
	return -EINVAL;
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

#if !CONFIG_COMP_MUX_MODULE
static int demux_init(struct processing_module *mod)
{
	mod->max_sinks = MUX_MAX_STREAMS;

	return mux_demux_common_init(mod, SOF_COMP_DEMUX);
}

static struct mux_look_up *get_lookup_table(struct comp_dev *dev, struct comp_data *cd,
					    uint32_t pipe_id)
{
	int i;

	for (i = 0; i < MUX_MAX_STREAMS; i++)
		if (cd->config.streams[i].pipeline_id == pipe_id)
			return &cd->lookup[i];

	comp_err(dev, "couldn't find configuration for connected pipeline %u",
		 pipe_id);
	return 0;
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
	struct comp_buffer *sink;
	struct audio_stream *sinks_stream[MUX_MAX_STREAMS] = { NULL };
	struct mux_look_up *look_ups[MUX_MAX_STREAMS] = { NULL };
	int frames;
	int sink_bytes;
	int source_bytes;
	int i;

	comp_dbg(dev, "demux_process()");

	/* align sink streams with their respective configurations */
	comp_dev_for_each_consumer(dev, sink) {
		if (comp_buffer_get_sink_state(sink) == dev->state) {
			i = get_stream_index(dev, cd, buffer_pipeline_id(sink));
			/* return if index wrong */
			if (i < 0) {
				return i;
			}

			look_ups[i] = get_lookup_table(dev, cd, buffer_pipeline_id(sink));
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
		if (sinks_stream[i]) {
			demux_prepare_active_look_up(cd, sinks_stream[i],
						     input_buffers[0].data, look_ups[i]);
			cd->demux(dev, sinks_stream[i], input_buffers[0].data,
				  frames, &cd->active_lookup);
		}
		mod->output_buffers[i].size = sink_bytes;
	}

	/* Update consumed */
	mod->input_buffers[0].consumed = source_bytes;
	return 0;
}

static int demux_trigger(struct processing_module *mod, int cmd)
{
	/* Check for cross-pipeline sinks: in general foreign
	 * pipelines won't be started synchronously with ours (it's
	 * under control of host software), so output can't be
	 * guaranteed not to overflow.  Always set the
	 * overrun_permitted flag.  These sink components are assumed
	 * responsible for flushing/synchronizing the stream
	 * themselves.
	 */
	if (cmd == COMP_TRIGGER_PRE_START) {
		struct comp_buffer *b;

		comp_dev_for_each_producer(mod->dev, b) {
			if (comp_buffer_get_sink_component(b)->pipeline != mod->dev->pipeline)
				audio_stream_set_overrun(&b->stream, true);
		}
	}

	return module_adapter_set_state(mod, mod->dev, cmd);
}
#endif

/* process and copy stream data from source to sink buffers */
static int mux_process(struct processing_module *mod,
		       struct input_stream_buffer *input_buffers, int num_input_buffers,
		       struct output_stream_buffer *output_buffers, int num_output_buffers)
{
	struct comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	struct comp_buffer *source;
	const struct audio_stream *sources_stream[MUX_MAX_STREAMS] = { NULL };
	int frames = 0;
	int sink_bytes;
	int source_bytes;
	int i, j;

	comp_dbg(dev, "mux_process()");

	/* align source streams with their respective configurations */
	j = 0;
	comp_dev_for_each_producer(dev, source) {
		if (comp_buffer_get_source_state(source) == dev->state) {
			if (frames)
				frames = MIN(frames, input_buffers[j].size);
			else
				frames = input_buffers[j].size;

			i = get_stream_index(dev, cd, buffer_pipeline_id(source));
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
	comp_dev_for_each_producer(dev, source) {
		if (comp_buffer_get_source_state(source) == dev->state)
			mod->input_buffers[j].consumed = source_bytes;
		j++;
	}
	mod->output_buffers[0].size = sink_bytes;
	return 0;
}

static int mux_reset(struct processing_module *mod)
{
	struct comp_buffer *source;
	struct comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	int dir = dev->pipeline->source_comp->direction;

	comp_dbg(dev, "mux_reset()");

	if (dir == SOF_IPC_STREAM_PLAYBACK) {
		comp_dev_for_each_producer(dev, source) {
			int state = comp_buffer_get_source_state(source);

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
	struct sof_mux_config *config;
	size_t blob_size;
	int ret;

	comp_dbg(dev, "mux_prepare()");

	config = comp_get_data_blob(cd->model_handler, &blob_size, NULL);
	if (blob_size > MUX_BLOB_MAX_SIZE) {
		comp_err(dev, "illegal blob size %zu", blob_size);
		return -EINVAL;
	}

	memcpy_s(&cd->config, MUX_BLOB_MAX_SIZE, config, blob_size);

	ret = mux_params(mod);
	if (ret < 0)
		return ret;

	if (cd->comp_type == SOF_COMP_MUX)
		cd->mux = mux_get_processing_function(mod);
	else
		cd->demux = demux_get_processing_function(mod);

	if (!cd->mux && !cd->demux) {
		comp_err(dev, "Invalid configuration, couldn't find suitable processing function.");
		return -EINVAL;
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

#if !CONFIG_COMP_MUX_MODULE
static const struct module_interface demux_interface = {
	.init = demux_init,
	.set_configuration = mux_set_config,
	.get_configuration = mux_get_config,
	.prepare = mux_prepare,
	.process_audio_stream = demux_process,
	.trigger = demux_trigger,
	.reset = mux_reset,
	.free = mux_free,
};
#endif

#if CONFIG_COMP_MUX_MODULE
/* modular: llext dynamic link */

#include <module/module/api_ver.h>
#include <module/module/llext.h>
#include <rimage/sof/user/manifest.h>

SOF_LLEXT_MOD_ENTRY(mux, &mux_interface);

/*
 * The demux entry is removed because mtl.toml doesn't have an entry
 * for it. Once that is fixed, the manifest line below can be
 * re-activated:
 * SOF_LLEXT_MOD_ENTRY(demux, &demux_interface);
 */

static const struct sof_man_module_manifest mod_manifest[] __section(".module") __used = {
	SOF_LLEXT_MODULE_MANIFEST("MUX", mux_llext_entry, 1, SOF_REG_UUID(mux4), 15),
	/*
	 * See comment above for a demux deactivation reason
	 * SOF_LLEXT_MODULE_MANIFEST("DEMUX", demux_llext_entry, 1, SOF_REG_UUID(demux), 15),
	 */
};

SOF_LLEXT_BUILDINFO;

#else

DECLARE_MODULE_ADAPTER(mux_interface, MUX_UUID, mux_tr);
SOF_MODULE_INIT(mux, sys_comp_module_mux_interface_init);

DECLARE_MODULE_ADAPTER(demux_interface, demux_uuid, demux_tr);
SOF_MODULE_INIT(demux, sys_comp_module_demux_interface_init);

#endif
