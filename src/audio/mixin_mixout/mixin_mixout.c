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

struct mixin_sink_config {
	enum ipc4_mixer_mode mixer_mode;
	uint32_t output_channel_count;
	uint32_t output_channel_map;
	/* Gain as described in struct ipc4_mixer_mode_sink_config */
	uint16_t gain;
};

/* mixin component private data */
struct mixin_data {
	struct mixin_sink_config sink_config[MIXIN_MAX_SINKS];

	void (*gain_func)(struct audio_stream *stream, uint32_t sample_count,
			  uint16_t gain_mul, uint8_t gain_shift);
};

/* mixout component private data */
struct mixout_data {
	void (*mix_func)(const struct audio_stream *source, struct audio_stream *sink,
			 uint32_t sample_count);
};

static int mixin_init(struct processing_module *mod)
{
	struct module_data *mod_data = &mod->priv;
	struct comp_dev *dev = mod->dev;
	struct mixin_data *md;
	int i;

	comp_dbg(dev, "mixin_init()");

	md = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(*md));
	if (!md)
		return -ENOMEM;

	mod_data->private = md;

	for (i = 0; i < MIXIN_MAX_SINKS; i++) {
		md->sink_config[i].mixer_mode = IPC4_MIXER_NORMAL_MODE;
		md->sink_config[i].gain = IPC4_MIXIN_UNITY_GAIN;
	}

	mod->max_sinks = MIXIN_MAX_SINKS;
	return 0;
}

static int mixout_init(struct processing_module *mod)
{
	struct comp_dev *dev = mod->dev;
	struct module_data *mod_data = &mod->priv;
	struct mixout_data *mixout_data;

	comp_dbg(dev, "mixout_new()");

	mixout_data = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(*mixout_data));
	if (!mixout_data)
		return -ENOMEM;

	mod_data->private = mixout_data;

	mod->max_sources = MIXOUT_MAX_SOURCES;
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

static int mixin_process(struct processing_module *mod,
			 struct input_stream_buffer *input_buffers, int num_input_buffers,
			 struct output_stream_buffer *output_buffers, int num_output_buffers)
{
	struct mixin_data *mixin_data = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	uint32_t avail_frames, free_frames;
	uint16_t sinks_ids[MIXIN_MAX_SINKS];
	bool full_sinks[MIXIN_MAX_SINKS];
	uint32_t bytes_to_copy, samples_to_copy, frames_to_copy;
	int i;
	const struct mixin_sink_config *sink_config;

	comp_dbg(dev, "mixin_process()");

	if (num_input_buffers != 1) {
		comp_err(dev, "Invalid input buffer count: %d", num_input_buffers);
		return -EINVAL;
	}

	if (num_output_buffers > MIXIN_MAX_SINKS) {
		comp_err(dev, "Invalid output buffer count: %d", num_output_buffers);
		return -EINVAL;
	}

	/* first, let's find out how many frames can be now processed --
	 * it is a nimimal value among frames available in source buffer
	 * and frames free in each connected sink buffer.
	 */
	avail_frames = input_buffers[0].size;
	if (avail_frames == 0)
		return 0;

	free_frames = INT32_MAX;

	for (i = 0; i < num_output_buffers; i++) {
		struct comp_buffer __sparse_cache *sink_c;
		uint32_t frames;

		sink_c = attr_container_of(output_buffers[i].data,
					   struct comp_buffer __sparse_cache,
					   stream, __sparse_cache);
		sinks_ids[i] = IPC4_SRC_QUEUE_ID(sink_c->id);

		if (sinks_ids[i] >= MIXIN_MAX_SINKS) {
			comp_err(dev, "Sink index out of range: %u, max sink count: %u",
				 (uint32_t)sinks_ids[i], MIXIN_MAX_SINKS);
			return -EINVAL;
		}

		/* TODO: add proper explanation here! */
		if (!sink_c->hw_params_configured) {
			comp_err(dev, "Uninitialized mixin sink buffer!");
			return -EINVAL;
		}

		frames = audio_stream_get_free_frames(output_buffers[i].data);

		/* Just ignore full sinks and copy data to other connected mixouts */
		full_sinks[i] = (frames == 0);
		if (frames == 0) {
			/* Seems, module_adapter is already doing same thing for us. However,
			 * probably, its better to remove such "if" in module_adapter and
			 * uncomment "if" below.
			 */
#if 0
			if (sink_c->sink && sink_c->sink->state == COMP_STATE_ACTIVE)
				return 0;
#endif
			continue;
		}

		free_frames = MIN(frames, free_frames);
	}

	if (free_frames == INT32_MAX)
		return 0;

	frames_to_copy = MIN(avail_frames, free_frames);
	bytes_to_copy = audio_stream_period_bytes(input_buffers[0].data, frames_to_copy);

	input_buffers[0].consumed = bytes_to_copy;

	for (i = 0; i < num_output_buffers; i++) {
		if (full_sinks[i])
			continue;

		sink_config = &mixin_data->sink_config[sinks_ids[i]];

		if (sink_config->mixer_mode != IPC4_MIXER_NORMAL_MODE) {
			comp_err(dev, "Unsupported mixer mode: %d", sink_config->mixer_mode);
			return -EINVAL;
		}

		samples_to_copy =
			frames_to_copy * audio_stream_get_channels(output_buffers[i].data);

		audio_stream_copy(input_buffers[0].data, 0, output_buffers[i].data, 0,
				  samples_to_copy);

		if (sink_config->gain != IPC4_MIXIN_UNITY_GAIN) {
			assert(mixin_data->gain_func);
			mixin_data->gain_func(output_buffers[i].data, samples_to_copy,
					      sink_config->gain, IPC4_MIXIN_GAIN_SHIFT);
		}

		output_buffers[i].size = bytes_to_copy;
	}

	return 0;
}

static int mixout_process(struct processing_module *mod,
			  struct input_stream_buffer *input_buffers, int num_input_buffers,
			  struct output_stream_buffer *output_buffers, int num_output_buffers)
{
	struct comp_dev *dev = mod->dev;
	struct mixout_data *mixout_data = module_get_private_data(mod);
	uint32_t avail_frames, free_frames;
	int i;
	bool empty_sources[MIXOUT_MAX_SOURCES];
	uint32_t samples_to_mix, bytes_to_mix;
	bool first_source;

	comp_dbg(dev, "mixout_process()");

	if (num_output_buffers != 1) {
		comp_err(dev, "Invalid output buffer count: %d", num_output_buffers);
		return -EINVAL;
	}

	avail_frames = INT32_MAX;

	assert(num_input_buffers <= MIXOUT_MAX_SOURCES);

	for (i = 0; i < num_input_buffers; i++) {
		uint32_t frames = input_buffers[i].size;

		/* Just ignore empty sources */
		empty_sources[i] = (frames == 0);
		if (frames == 0)
			continue;

		avail_frames = MIN(frames, avail_frames);
	}

	if (avail_frames == INT32_MAX)
		return 0;

	free_frames = audio_stream_get_free_frames(output_buffers[0].data);
	samples_to_mix = MIN(free_frames, avail_frames) *
		audio_stream_get_channels(output_buffers[0].data);
	bytes_to_mix = samples_to_mix * audio_stream_sample_bytes(output_buffers[0].data);

	first_source = true;

	for (i = 0; i < num_input_buffers; i++) {
		if (empty_sources[i])
			continue;

		if (first_source) {
			first_source = false;
			audio_stream_copy(input_buffers[i].data, 0,
					  output_buffers[0].data, 0, samples_to_mix);
		} else {
			assert(mixout_data->mix_func);
			mixout_data->mix_func(input_buffers[i].data,
					      output_buffers[0].data, samples_to_mix);
		}

		input_buffers[i].consumed = bytes_to_mix;
	}

	output_buffers[0].size = bytes_to_mix;

	return 0;
}

static void apply_gain_s16(struct audio_stream *stream, uint32_t sample_count,
			   uint16_t gain_mul, uint8_t gain_shift)
{
	int16_t *ptr = stream->w_ptr;

	while (sample_count > 0) {
		uint32_t i, n;

		ptr = audio_stream_wrap(stream, ptr);
		n = MIN(audio_stream_samples_without_wrap_s16(stream, ptr), sample_count);

		for (i = 0; i < n; i++) {
			*ptr = ((int32_t)*ptr * gain_mul) >> gain_shift;
			ptr++;
		}

		sample_count -= n;
	}
}

static void apply_gain_s32(struct audio_stream *stream, uint32_t sample_count,
			   uint16_t gain_mul, uint8_t gain_shift)
{
	int32_t *ptr = stream->w_ptr;

	while (sample_count > 0) {
		uint32_t i, n;

		ptr = audio_stream_wrap(stream, ptr);
		n = MIN(audio_stream_samples_without_wrap_s32(stream, ptr), sample_count);

		for (i = 0; i < n; i++) {
			*ptr = ((int64_t)*ptr * gain_mul) >> gain_shift;
			ptr++;
		}

		sample_count -= n;
	}
}

static void mix_s16(const struct audio_stream *source, struct audio_stream *sink,
		    uint32_t sample_count)
{
	int16_t *src = source->r_ptr;
	int16_t *dst = sink->w_ptr;

	while (sample_count > 0) {
		uint32_t i, n;

		src = audio_stream_wrap(source, src);
		dst = audio_stream_wrap(sink, dst);

		n = MIN(audio_stream_samples_without_wrap_s16(source, src),
		audio_stream_samples_without_wrap_s16(sink, dst));
		n = MIN(n, sample_count);

		for (i = 0; i < n; i++) {
			*dst = sat_int16((int32_t)*src + (int32_t)*dst);
			src++;
			dst++;
		}

		sample_count -= n;
	}
}

static void mix_s24(const struct audio_stream *source, struct audio_stream *sink,
		    uint32_t sample_count)
{
	int32_t *src = source->r_ptr;
	int32_t *dst = sink->w_ptr;

	while (sample_count > 0) {
		uint32_t i, n;

		src = audio_stream_wrap(source, src);
		dst = audio_stream_wrap(sink, dst);

		n = MIN(audio_stream_samples_without_wrap_s32(source, src),
		audio_stream_samples_without_wrap_s32(sink, dst));
		n = MIN(n, sample_count);

		for (i = 0; i < n; i++) {
			*dst = sat_int24(*src + *dst);
			src++;
			dst++;
		}

		sample_count -= n;
	}
}

static void mix_s32(const struct audio_stream *source, struct audio_stream *sink,
		    uint32_t sample_count)
{
	int32_t *src = source->r_ptr;
	int32_t *dst = sink->w_ptr;

	while (sample_count > 0) {
		uint32_t i, n;

		src = audio_stream_wrap(source, src);
		dst = audio_stream_wrap(sink, dst);

		n = MIN(audio_stream_samples_without_wrap_s32(source, src),
		audio_stream_samples_without_wrap_s32(sink, dst));
		n = MIN(n, sample_count);

		for (i = 0; i < n; i++) {
			*dst = sat_int32((int64_t)*src + (int64_t)*dst);
			src++;
			dst++;
		}

		sample_count -= n;
	}
}

static int mixin_reset(struct processing_module *mod)
{
	struct mixin_data *mixin_data = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;

	comp_dbg(dev, "mixin_reset()");

	mixin_data->gain_func = NULL;

	return 0;
}

static int mixout_reset(struct processing_module *mod)
{
	struct mixout_data *mixout_data = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	struct list_item *blist;

	comp_dbg(dev, "mixout_reset()");

	mixout_data->mix_func = NULL;

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

/* FIXME: both mixin_params() and mixout_params() could be removed when
 * mess related to valid_fmt is resolved!
 */
static int mixin_params(struct processing_module *mod)
{
	struct sof_ipc_stream_params *params = mod->stream_params;
	struct comp_dev *dev = mod->dev;
	struct list_item *blist;
	int ret;

	comp_dbg(dev, "mixin_params()");

	ipc4_base_module_cfg_to_stream_params(&mod->priv.cfg.base_cfg, params);

	list_for_item(blist, &dev->bsink_list) {
		struct comp_buffer *sink;
		struct comp_buffer __sparse_cache *sink_c;
		enum sof_ipc_frame frame_fmt, valid_fmt;

		sink = buffer_from_list(blist, PPL_DIR_DOWNSTREAM);
		sink_c = buffer_acquire(sink);

		/* comp_verify_params() does not modify valid_sample_fmt (a BUG?),
		 * let's do this here
		 */
		audio_stream_fmt_conversion(mod->priv.cfg.base_cfg.audio_fmt.depth,
					    mod->priv.cfg.base_cfg.audio_fmt.valid_bit_depth,
					    &frame_fmt, &valid_fmt,
					    mod->priv.cfg.base_cfg.audio_fmt.s_type);

///		audio_stream_set_frm_fmt(&sink_c->stream, frame_fmt);
		audio_stream_set_valid_fmt(&sink_c->stream, valid_fmt);

		buffer_release(sink_c);
	}

	ret = comp_verify_params(dev, 0, params);
	if (ret < 0) {
		comp_err(dev, "mixin_params(): comp_verify_params() failed!");
		return -EINVAL;
	}

	return 0;
}

/*
 * Prepare the mixer. The mixer may already be running at this point with other
 * sources. Make sure we only prepare the "prepared" source streams and not
 * the active or inactive sources.
 *
 * We should also make sure that we propagate the prepare call to downstream
 * if downstream is not currently active.
 */
static int mixin_prepare(struct processing_module *mod,
			 struct sof_source __sparse_cache **sources, int num_of_sources,
			 struct sof_sink __sparse_cache **sinks, int num_of_sinks)
{
	struct mixin_data *mixin_data = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	struct comp_buffer *sink;
	struct comp_buffer __sparse_cache *sink_c;
	enum sof_ipc_frame fmt;
	int ret;

	comp_info(dev, "mixin_prepare()");

	ret = mixin_params(mod);
	if (ret < 0)
		return ret;

	if (list_is_empty(&dev->bsink_list)) {
		comp_err(dev, "At least one sink expected!");
		return -EINVAL;
	}

	sink = list_first_item(&dev->bsink_list, struct comp_buffer, source_list);
	sink_c = buffer_acquire(sink);
	fmt = audio_stream_get_valid_fmt(&sink_c->stream);
	buffer_release(sink_c);

	switch (fmt) {
	case SOF_IPC_FRAME_S16_LE:
		mixin_data->gain_func = apply_gain_s16;
		break;
	case SOF_IPC_FRAME_S24_4LE:
	case SOF_IPC_FRAME_S32_LE:
		mixin_data->gain_func = apply_gain_s32;
		break;
	default:
		comp_err(dev, "Unsupported format: %d", fmt);
		return -EINVAL;
	}

	return 0;
}

static int mixout_params(struct processing_module *mod)
{
	struct sof_ipc_stream_params *params = mod->stream_params;
	struct comp_buffer *sink;
	struct comp_buffer __sparse_cache *sink_c;
	struct comp_dev *dev = mod->dev;
	enum sof_ipc_frame frame_fmt, valid_fmt;
	int ret;

	comp_dbg(dev, "mixout_params()");

	ipc4_base_module_cfg_to_stream_params(&mod->priv.cfg.base_cfg, params);

	ret = comp_verify_params(dev, 0, params);
	if (ret < 0) {
		comp_err(dev, "mixout_params(): comp_verify_params() failed!");
		return -EINVAL;
	}

	if (list_is_empty(&dev->bsink_list)) {
		comp_err(dev, "No sink buffer found!");
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

	buffer_release(sink_c);

	return 0;
}

static int mixout_prepare(struct processing_module *mod,
			  struct sof_source __sparse_cache **sources, int num_of_sources,
			  struct sof_sink __sparse_cache **sinks, int num_of_sinks)
{
	struct comp_dev *dev = mod->dev;
	struct mixout_data *mixout_data = module_get_private_data(mod);
	int ret;
	struct comp_buffer *sink;
	struct comp_buffer __sparse_cache *sink_c;
	enum sof_ipc_frame fmt;

	ret = mixout_params(mod);
	if (ret < 0)
		return ret;

	comp_dbg(dev, "mixout_prepare()");

	if (list_is_empty(&dev->bsink_list)) {
		comp_err(dev, "No sink buffer found!");
		return -EINVAL;
	}

	sink = list_first_item(&dev->bsink_list, struct comp_buffer, source_list);
	sink_c = buffer_acquire(sink);
	fmt = audio_stream_get_valid_fmt(&sink_c->stream);
	buffer_release(sink_c);

	switch (fmt) {
	case SOF_IPC_FRAME_S16_LE:
		mixout_data->mix_func = mix_s16;
		break;
	case SOF_IPC_FRAME_S24_4LE:
		mixout_data->mix_func = mix_s24;
		break;
	case SOF_IPC_FRAME_S32_LE:
		mixout_data->mix_func = mix_s32;
		break;
	default:
		comp_err(dev, "Unsupported format: %d", fmt);
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
