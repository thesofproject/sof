// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 Intel Corporation. All rights reserved.

#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/audio/format.h>
#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/mixer.h>
#include <sof/audio/pipeline.h>
#include <sof/audio/ipc-config.h>
#include <sof/common.h>
#include <sof/compiler_attributes.h>
#include <rtos/panic.h>
#include <sof/ipc/msg.h>
#include <rtos/alloc.h>
#include <rtos/init.h>
#include <sof/lib/memory.h>
#include <sof/lib/uuid.h>
#include <sof/list.h>
#include <sof/math/numbers.h>
#include <sof/platform.h>
#include <rtos/string.h>
#include <sof/trace/trace.h>
#include <sof/ut.h>
#include <ipc/stream.h>
#include <ipc/topology.h>
#include <ipc4/base-config.h>
#include <ipc4/mixin_mixout.h>
#include <user/trace.h>
#include <stddef.h>
#include <stdint.h>

LOG_MODULE_REGISTER(mixer, CONFIG_SOF_LOG_LEVEL);

/* mixin 39656eb2-3b71-4049-8d3f-f92cd5c43c09 */
DECLARE_SOF_RT_UUID("mix_in", mixin_uuid, 0x39656eb2, 0x3b71, 0x4049,
		    0x8d, 0x3f, 0xf9, 0x2c, 0xd5, 0xc4, 0x3c, 0x09);
DECLARE_TR_CTX(mixin_tr, SOF_UUID(mixin_uuid), LOG_LEVEL_INFO);

/* mixout 3c56505a-24d7-418f-bddc-c1f5a3ac2ae0 */
DECLARE_SOF_RT_UUID("mix_out", mixout_uuid, 0x3c56505a, 0x24d7, 0x418f,
		    0xbd, 0xdc, 0xc1, 0xf5, 0xa3, 0xac, 0x2a, 0xe0);
DECLARE_TR_CTX(mixout_tr, SOF_UUID(mixout_uuid), LOG_LEVEL_INFO);

#define MIXIN_MAX_SINKS IPC4_MIXIN_MODULE_MAX_OUTPUT_QUEUES
#define MIXOUT_MAX_SOURCES IPC4_MIXOUT_MODULE_MAX_INPUT_QUEUES

/*
 * Unfortunately, if we have to support a topology with a single mixin
 * connected to multiple mixouts, we cannot use simple implementation as in
 * mixer component. We either need to use intermediate buffer between mixin and
 * mixout, or use a more complex implementation as described below.
 *
 * This implementation does not use buffer between mixin and mixout. Mixed
 * data is written directly to mixout sink buffer. Most of the mixing is done
 * by mixins in mixin_process(). Simply speaking, if no data present in mixout
 * sink -- mixin just copies its source data to mixout sink. If mixout sink
 * has some data (written there previously by some other mixin) -- mixin reads
 * data from mixout sink, mixes it with its source data and writes back to
 * mixout sink.
 *
 * Such implementation has less buffer reads/writes than simple implementation
 * using intermediate buffer between mixin and mixout.
 */

struct mixin_sink_config {
	enum ipc4_mixer_mode mixer_mode;
	uint32_t output_channel_count;
	uint32_t output_channel_map;
	/* Gain as described in struct ipc4_mixer_mode_sink_config */
	uint16_t gain;
};

/* mixin component private data */
struct mixin_data {
	normal_mix_func normal_mix_channel;
	remap_mix_func remap_mix_channel;
	mute_func mute_channel;
	struct mixin_sink_config sink_config[MIXIN_MAX_SINKS];
};

/* mixout component private data. This can be accessed from different cores. */
struct mixout_data {
	/* number of currently mixed frames in mixout sink buffer */
	uint32_t mixed_frames;
	/*
	 * Source data is consumed by mixins in mixin_process() but sink data cannot be
	 * immediately produced. Sink data is produced by mixout in mixout_process() after
	 * ensuring all connected mixins have mixed their data into mixout sink buffer.
	 * So for each connected mixin, mixout keeps knowledge of data already consumed
	 * by mixin but not yet produced in mixout.
	 */
	uint32_t pending_frames[MIXOUT_MAX_SOURCES];

	void (*mix_func)(struct comp_dev *dev, struct audio_stream __sparse_cache *sink,
			 const struct audio_stream __sparse_cache **sources, uint32_t count,
			 uint32_t frames);
};

static int mixin_init(struct processing_module *mod)
{
	struct module_data *mod_data = &mod->priv;
	struct comp_dev *dev = mod->dev;
	struct mixin_data *md;
	int i;
	enum sof_ipc_frame frame_fmt, valid_fmt;

	comp_dbg(dev, "mixin_init()");

	md = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(*md));
	if (!md)
		return -ENOMEM;

	mod_data->private = md;

	for (i = 0; i < MIXIN_MAX_SINKS; i++) {
		md->sink_config[i].mixer_mode = IPC4_MIXER_NORMAL_MODE;
		md->sink_config[i].gain = IPC4_MIXIN_UNITY_GAIN;
	}

	audio_stream_fmt_conversion(mod->priv.cfg.base_cfg.audio_fmt.depth,
				    mod->priv.cfg.base_cfg.audio_fmt.valid_bit_depth,
				    &frame_fmt, &valid_fmt,
				    mod->priv.cfg.base_cfg.audio_fmt.s_type);

	dev->ipc_config.frame_fmt = frame_fmt;
	dev->bypass_capable = true;

	return 0;
}

static int mixout_init(struct processing_module *mod)
{
	struct module_data *mod_data = &mod->priv;
	struct comp_dev *dev = mod->dev;
	struct mixout_data *mo_data;
	enum sof_ipc_frame frame_fmt, valid_fmt;

	comp_dbg(dev, "mixout_new()");

	mo_data = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(*mo_data));
	if (!mo_data)
		return -ENOMEM;

	mod_data->private = mo_data;

	audio_stream_fmt_conversion(mod->priv.cfg.base_cfg.audio_fmt.depth,
				    mod->priv.cfg.base_cfg.audio_fmt.valid_bit_depth,
				    &frame_fmt, &valid_fmt,
				    mod->priv.cfg.base_cfg.audio_fmt.s_type);

	dev->ipc_config.frame_fmt = frame_fmt;

	return 0;
}

static int mixin_free(struct processing_module *mod)
{
	struct mixin_data *md = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;

	comp_dbg(dev, "mixin_free()");
	rfree(md);

	return 0;
}

static int mixout_free(struct processing_module *mod)
{
	struct mixout_data *md = module_get_private_data(mod);

	comp_dbg(mod->dev, "mixout_free()");
	rfree(md);

	return 0;
}

static int mixin_mixout_process_single_source(struct input_stream_buffer *input_buffer,
					      struct output_stream_buffer *output_buffer)
{
	uint32_t bytes;

	audio_stream_copy(input_buffer->data, 0, output_buffer->data, 0,
			  input_buffer->size * audio_stream_get_channels(input_buffer->data));

	/*
	 * mixout does not modify the format or number of channels, so the number of
	 * bytes consumed and produced are identical
	 */
	bytes = input_buffer->size * audio_stream_frame_bytes(input_buffer->data);
	input_buffer->consumed = bytes;
	output_buffer->size = bytes;

	return 0;
}

static int mixin_process(struct processing_module *mod,
			 struct input_stream_buffer *input_buffers, int num_input_buffers,
			 struct output_stream_buffer *output_buffers, int num_output_buffers)
{
	int i;

	if (num_input_buffers != 1)
		return 0;

	for (i = 0; i < num_output_buffers; i++)
		mixin_mixout_process_single_source(&input_buffers[0], &output_buffers[i]);

	return 0;
}

/* mixout just calls xxx_produce() on data mixed into its sink buffer by
 * mixins.
 */
static int mixout_process(struct processing_module *mod,
			  struct input_stream_buffer *input_buffers, int num_input_buffers,
			  struct output_stream_buffer *output_buffers, int num_output_buffers)
{
	/* if there's only one active source, simply copy the source samples to the sink */
	if (num_input_buffers == 1 & num_output_buffers == 1 && input_buffers[0].size)
		return mixin_mixout_process_single_source(&input_buffers[0], &output_buffers[0]);

	const struct audio_stream __sparse_cache *sources_stream[PLATFORM_MAX_STREAMS];
	struct mixout_data *md;
	int sources_indices[PLATFORM_MAX_STREAMS];
	uint32_t frames = mod->dev->frames;
	uint32_t bytes;
	uint32_t j = 0;
	int i;

	comp_dbg(mod->dev, "mixout_process()");

	if (num_input_buffers > MIXOUT_MAX_SOURCES) {
		comp_err(mod->dev, "Number of input buffers: %d exceeds max\n", num_input_buffers);
		return -EINVAL;
	}

	if (num_output_buffers != 1) {
		comp_err(mod->dev, "Invalid number of output buffers: %d\n", num_output_buffers);
		return -EINVAL;
	}

	for (i = 0; i < num_input_buffers; i++) {
		if (input_buffers[i].size == 0)
			continue;

		sources_stream[j] = mod->input_buffers[i].data;
		sources_indices[j++] = i;
		frames = MIN(frames, input_buffers[i].size);
	}

	/* generate silence if no data available from any of the sources */
	if (!j) {
		/*
		 * Generate silence when sources are inactive. When
		 * sources change to active, additionally keep
		 * generating silence until at least one of the
		 * sources start to have data available (frames!=0).
		 */
		bytes = mod->dev->frames * audio_stream_frame_bytes(mod->output_buffers[0].data);
		if (!audio_stream_set_zero(mod->output_buffers[0].data, bytes))
			mod->output_buffers[0].size = bytes;

		return 0;
	}

	/* mix streams */
	md = module_get_private_data(mod);
	md->mix_func(mod->dev, mod->output_buffers[0].data, sources_stream, j, frames);

	/*
	 * mixin and mixout do not modify the format or number of channels, so the number of
	 * bytes consumed and produced are identical
	 */
	bytes = frames * audio_stream_frame_bytes(mod->output_buffers[0].data);
	mod->output_buffers[0].size = bytes;

	/* update source buffer consumed bytes */
	for (i = 0; i < j; i++)
		mod->input_buffers[sources_indices[i]].consumed = bytes;

	return 0;
}

static int mixin_reset(struct processing_module *mod)
{
	struct mixin_data *mixin_data = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;

	comp_dbg(dev, "mixin_reset()");

	mixin_data->normal_mix_channel = NULL;
	mixin_data->remap_mix_channel = NULL;
	mixin_data->mute_channel = NULL;

	return 0;
}

static int mixout_reset(struct processing_module *mod)
{
	struct comp_dev *dev = mod->dev;
	struct list_item *blist;

	comp_dbg(dev, "mixout_reset()");

	/* FIXME: move this to module_adapter_reset() */
	if (dev->pipeline->source_comp->direction == SOF_IPC_STREAM_PLAYBACK) {
		list_for_item(blist, &dev->bsource_list) {
			struct comp_buffer *source;
			struct comp_buffer __sparse_cache *source_c;
			bool stop;

			/* FIXME: this is racy and implicitly protected by serialised IPCs */
			source = container_of(blist, struct comp_buffer, sink_list);
			source_c = buffer_acquire(source);
			stop = (dev->pipeline == source_c->source->pipeline &&
					source_c->source->state > COMP_STATE_PAUSED);
			buffer_release(source_c);

			if (stop)
				/* should not reset the downstream components */
				return PPL_STATUS_PATH_STOP;
		}
	}

	return 0;
}

static void base_module_cfg_to_stream_params(const struct ipc4_base_module_cfg *base_cfg,
					     struct sof_ipc_stream_params *params);
static int mixin_prepare(struct processing_module *mod,
			 struct sof_source __sparse_cache **sources, int num_of_sources,
			 struct sof_sink __sparse_cache **sinks, int num_of_sinks)
{
	struct sof_ipc_stream_params *params = mod->stream_params;
	struct mixin_data *md = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	struct list_item *blist;
	int ret;

	comp_dbg(dev, "mixin_prepare()");

	base_module_cfg_to_stream_params(&mod->priv.cfg.base_cfg, params);

	list_for_item(blist, &dev->bsink_list) {
		struct comp_buffer *sink;
		struct comp_buffer __sparse_cache *sink_c;
		enum sof_ipc_frame frame_fmt, valid_fmt;
		uint16_t sink_id;

		sink = buffer_from_list(blist, PPL_DIR_DOWNSTREAM);
		sink_c = buffer_acquire(sink);

		audio_stream_set_channels(&sink_c->stream,
					  mod->priv.cfg.base_cfg.audio_fmt.channels_count);

		/* Applying channel remapping may produce sink stream with channel count
		 * different from source channel count.
		 */
		sink_id = IPC4_SRC_QUEUE_ID(sink_c->id);
		if (sink_id >= MIXIN_MAX_SINKS) {
			comp_err(dev, "mixin_prepare(): Sink index out of range: %u, max sink count: %u",
				 (uint32_t)sink_id, MIXIN_MAX_SINKS);
			buffer_release(sink_c);
			return -EINVAL;
		}
		if (md->sink_config[sink_id].mixer_mode == IPC4_MIXER_CHANNEL_REMAPPING_MODE)
			audio_stream_set_channels(&sink_c->stream,
						  md->sink_config[sink_id].output_channel_count);

		/* comp_verify_params() does not modify valid_sample_fmt (a BUG?),
		 * let's do this here
		 */
		audio_stream_fmt_conversion(mod->priv.cfg.base_cfg.audio_fmt.depth,
					    mod->priv.cfg.base_cfg.audio_fmt.valid_bit_depth,
					    &frame_fmt, &valid_fmt,
					    mod->priv.cfg.base_cfg.audio_fmt.s_type);

		audio_stream_set_frm_fmt(&sink_c->stream, frame_fmt);
		audio_stream_set_valid_fmt(&sink_c->stream, valid_fmt);

		buffer_release(sink_c);
	}

	/* use BUFF_PARAMS_CHANNELS to skip updating channel count */
	ret = comp_verify_params(dev, BUFF_PARAMS_CHANNELS, params);
	if (ret < 0) {
		comp_err(dev, "mixin_prepare(): comp_verify_params() failed!");
		return -EINVAL;
	}

	return 0;
}

static void base_module_cfg_to_stream_params(const struct ipc4_base_module_cfg *base_cfg,
					     struct sof_ipc_stream_params *params)
{
	enum sof_ipc_frame frame_fmt, valid_fmt;
	int i;

	memset(params, 0, sizeof(struct sof_ipc_stream_params));
	params->channels = base_cfg->audio_fmt.channels_count;
	params->rate = base_cfg->audio_fmt.sampling_frequency;
	params->sample_container_bytes = base_cfg->audio_fmt.depth / 8;
	params->sample_valid_bytes = base_cfg->audio_fmt.valid_bit_depth / 8;
	params->buffer_fmt = base_cfg->audio_fmt.interleaving_style;
	params->buffer.size = base_cfg->obs * 2;

	audio_stream_fmt_conversion(base_cfg->audio_fmt.depth,
				    base_cfg->audio_fmt.valid_bit_depth,
				    &frame_fmt, &valid_fmt,
				    base_cfg->audio_fmt.s_type);
	params->frame_fmt = frame_fmt;

	for (i = 0; i < SOF_IPC_MAX_CHANNELS; i++)
		params->chmap[i] = (base_cfg->audio_fmt.ch_map >> i * 4) & 0xf;
}

static int mixout_params(struct processing_module *mod)
{
	struct sof_ipc_stream_params *params = mod->stream_params;
	struct comp_buffer *sink;
	struct comp_buffer __sparse_cache *sink_c;
	struct comp_dev *dev = mod->dev;
	enum sof_ipc_frame frame_fmt, valid_fmt;
	uint32_t sink_period_bytes, sink_stream_size;
	int ret;

	comp_dbg(dev, "mixout_params()");

	base_module_cfg_to_stream_params(&mod->priv.cfg.base_cfg, params);

	ret = comp_verify_params(dev, 0, params);
	if (ret < 0) {
		comp_err(dev, "mixout_params(): comp_verify_params() failed!");
		return -EINVAL;
	}

	sink = list_first_item(&dev->bsink_list, struct comp_buffer, source_list);
	sink_c = buffer_acquire(sink);

	/* comp_verify_params() does not modify valid_sample_fmt (a BUG?), let's do this here */
	audio_stream_fmt_conversion(mod->priv.cfg.base_cfg.audio_fmt.depth,
				    mod->priv.cfg.base_cfg.audio_fmt.valid_bit_depth,
				    &frame_fmt, &valid_fmt,
				    mod->priv.cfg.base_cfg.audio_fmt.s_type);

	audio_stream_set_valid_fmt(&sink_c->stream, valid_fmt);
	audio_stream_set_channels(&sink_c->stream, params->channels);

	sink_stream_size = audio_stream_get_size(&sink_c->stream);

	/* calculate period size based on config */
	sink_period_bytes = audio_stream_period_bytes(&sink_c->stream,
						      dev->frames);
	buffer_release(sink_c);

	if (sink_period_bytes == 0) {
		comp_err(dev, "mixout_params(): period_bytes = 0");
		return -EINVAL;
	}

	if (sink_stream_size < sink_period_bytes) {
		comp_err(dev, "mixout_params(): sink buffer size %d is insufficient < %d",
			 sink_stream_size, sink_period_bytes);
		return -ENOMEM;
	}

	return 0;
}

static int mixout_prepare(struct processing_module *mod,
			  struct sof_source __sparse_cache **sources, int num_of_sources,
			  struct sof_sink __sparse_cache **sinks, int num_of_sinks)
{
	struct mixout_data *md = module_get_private_data(mod);
	struct comp_buffer __sparse_cache *sink_c;
	struct comp_dev *dev = mod->dev;
	struct comp_buffer *sink;
	int ret;

	ret = mixout_params(mod);
	if (ret < 0)
		return ret;

	comp_dbg(dev, "mixout_prepare()");

	sink = list_first_item(&dev->bsink_list, struct comp_buffer,
			       source_list);
	sink_c = buffer_acquire(sink);
	md->mix_func = mixer_get_processing_function(dev, sink_c);
	buffer_release(sink_c);

	if (!md->mix_func) {
		comp_err(dev, "mixout_prepare(): no mix function");
		return -EINVAL;
	}

	return 0;
}

static int mixin_set_config(struct processing_module *mod, uint32_t config_id,
			    enum module_cfg_fragment_position pos, uint32_t data_offset_size,
			    const uint8_t *fragment, size_t fragment_size, uint8_t *response,
			    size_t response_size)
{
	struct mixin_data *mixin_data = module_get_private_data(mod);
	const struct ipc4_mixer_mode_config *cfg;
	struct comp_dev *dev = mod->dev;
	int i;
	uint32_t sink_index;
	uint16_t gain;

	if (config_id != IPC4_MIXER_MODE) {
		comp_err(dev, "mixin_set_config() unsupported param ID: %u", config_id);
		return -EINVAL;
	}

	if (!(pos & MODULE_CFG_FRAGMENT_SINGLE)) {
		comp_err(dev, "mixin_set_config() data is expected to be sent as one chunk");
		return -EINVAL;
	}

	/* for a single chunk data, data_offset_size is size */
	if (data_offset_size < sizeof(struct ipc4_mixer_mode_config)) {
		comp_err(dev, "mixin_set_config() too small data size: %u", data_offset_size);
		return -EINVAL;
	}

	if (data_offset_size > SOF_IPC_MSG_MAX_SIZE) {
		comp_err(dev, "mixin_set_config() too large data size: %u", data_offset_size);
		return -EINVAL;
	}

	cfg = (const struct ipc4_mixer_mode_config *)fragment;

	if (cfg->mixer_mode_config_count < 1 || cfg->mixer_mode_config_count > MIXIN_MAX_SINKS) {
		comp_err(dev, "mixin_set_config() invalid mixer_mode_config_count: %u",
			 cfg->mixer_mode_config_count);
		return -EINVAL;
	}

	if (sizeof(struct ipc4_mixer_mode_config) +
	    (cfg->mixer_mode_config_count - 1) * sizeof(struct ipc4_mixer_mode_sink_config) >
	    data_offset_size) {
		comp_err(dev, "mixin_set_config(): unexpected data size: %u", data_offset_size);
		return -EINVAL;
	}

	for (i = 0; i < cfg->mixer_mode_config_count; i++) {
		sink_index = cfg->mixer_mode_sink_configs[i].output_queue_id;
		if (sink_index >= MIXIN_MAX_SINKS) {
			comp_err(dev, "mixin_set_config(): invalid sink index: %u", sink_index);
			return -EINVAL;
		}

		gain = cfg->mixer_mode_sink_configs[i].gain;
		if (gain > IPC4_MIXIN_UNITY_GAIN)
			gain = IPC4_MIXIN_UNITY_GAIN;
		mixin_data->sink_config[sink_index].gain = gain;

		comp_dbg(dev, "mixin_set_config(): gain 0x%x will be applied for sink %u",
			 gain, sink_index);

		if (cfg->mixer_mode_sink_configs[i].mixer_mode ==
			IPC4_MIXER_CHANNEL_REMAPPING_MODE) {
			uint32_t channel_count =
				cfg->mixer_mode_sink_configs[i].output_channel_count;
			if (channel_count < 1 || channel_count > 8) {
				comp_err(dev, "mixin_set_config(): Invalid output_channel_count %u for sink %u",
					 channel_count, sink_index);
				return -EINVAL;
			}

			mixin_data->sink_config[sink_index].output_channel_count = channel_count;
			mixin_data->sink_config[sink_index].output_channel_map =
				cfg->mixer_mode_sink_configs[i].output_channel_map;

			comp_dbg(dev, "mixin_set_config(): output_channel_count: %u, chmap: 0x%x for sink: %u",
				 channel_count,
				 mixin_data->sink_config[sink_index].output_channel_map,
				 sink_index);
		}

		mixin_data->sink_config[sink_index].mixer_mode =
			cfg->mixer_mode_sink_configs[i].mixer_mode;
	}

	return 0;
}

static struct module_interface mixin_interface = {
	.init  = mixin_init,
	.prepare = mixin_prepare,
	.process_audio_stream = mixin_process,
	.set_configuration = mixin_set_config,
	.reset = mixin_reset,
	.free = mixin_free
};

DECLARE_MODULE_ADAPTER(mixin_interface, mixin_uuid, mixin_tr);
SOF_MODULE_INIT(mixin, sys_comp_module_mixin_interface_init);

static struct module_interface mixout_interface = {
	.init  = mixout_init,
	.prepare = mixout_prepare,
	.process_audio_stream = mixout_process,
	.reset = mixout_reset,
	.free = mixout_free
};

DECLARE_MODULE_ADAPTER(mixout_interface, mixout_uuid, mixout_tr);
SOF_MODULE_INIT(mixout, sys_comp_module_mixout_interface_init);
