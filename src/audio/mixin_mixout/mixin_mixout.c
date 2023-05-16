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
	void (* mix_func)(const struct audio_stream *source, struct audio_stream *sink,
		      uint32_t sample_count);
};

static int mixin_init(struct processing_module *mod)
{
	struct module_data *mod_data = &mod->priv;
	struct comp_dev *dev = mod->dev;
	struct mixin_data *md;
	int i;
	enum sof_ipc_frame __sparse_cache frame_fmt, valid_fmt;

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
	mod->simple_copy = true;

	return 0;
}

static int mixout_init(struct processing_module *mod)
{
	struct comp_dev *dev = mod->dev;
	struct module_data *mod_data = &mod->priv;
	struct mixout_data *mo_data;
	enum sof_ipc_frame __sparse_cache frame_fmt, valid_fmt;

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
	mod->simple_copy = true;

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

/* Currently only does copy with gain. Remapping is not implemented. */
static int copy_and_remap(struct comp_dev *dev, const struct mixin_data *mixin_data,
			 uint16_t sink_index, struct audio_stream __sparse_cache *sink,
			 const struct audio_stream __sparse_cache *source, uint32_t frame_count)
{
	const struct mixin_sink_config *sink_config;

	if (sink_index >= MIXIN_MAX_SINKS) {
		comp_err(dev, "Sink index out of range: %u, max sinks count: %u",
			 (uint32_t)sink_index, MIXIN_MAX_SINKS);
		return -EINVAL;
	}

	sink_config = &mixin_data->sink_config[sink_index];

	if (sink_config->mixer_mode == IPC4_MIXER_NORMAL_MODE) {
		audio_stream_copy(source, 0, sink, 0, frame_count * sink->channels);

		if (sink_config->gain != IPC4_MIXIN_UNITY_GAIN)
			mixin_data->gain_func(sink, frame_count * sink->channels,
					      sink_config->gain, IPC4_MIXIN_GAIN_SHIFT);
	} else if (sink_config->mixer_mode == IPC4_MIXER_CHANNEL_REMAPPING_MODE) {
		comp_err(dev, "Remapping mode is not implemented!");
		return -EINVAL;
	} else {
		comp_err(dev, "Unexpected mixer mode: %d", sink_config->mixer_mode);
		return -EINVAL;
	}

	return 0;
}

static int mixin_process(struct processing_module *mod,
			 struct input_stream_buffer *input_buffers, int num_input_buffers,
			 struct output_stream_buffer *output_buffers, int num_output_buffers)
{
	struct mixin_data *mixin_data = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	uint32_t source_avail_frames, sinks_free_frames;
	uint16_t sinks_ids[MIXIN_MAX_SINKS];
	uint32_t bytes_to_consume_from_source_buf;
	uint32_t frames_to_copy;
	int i, ret;

	comp_dbg(dev, "mixin_process()");

	source_avail_frames = audio_stream_get_avail_frames(input_buffers[0].data);
	sinks_free_frames = INT32_MAX;

	/* block mixin pipeline until at least one mixout pipeline started */
	if (num_output_buffers == 0)
		return 0;

	if (num_output_buffers >= MIXIN_MAX_SINKS) {
		comp_err(dev, "mixin_process(): Invalid output buffer count %d",
			 num_output_buffers);
		return -EINVAL;
	}

	/* first, let's find out how many frames can be now processed --
	 * it is a nimimal value among frames available in source buffer
	 * and frames free in each connected mixin sink buffer.
	 */
	for (i = 0; i < num_output_buffers; i++) {
		struct comp_buffer __sparse_cache *sink_c;

		sink_c = attr_container_of(output_buffers[i].data,
							    struct comp_buffer __sparse_cache,
							    stream, __sparse_cache);
		sinks_ids[i] = IPC4_SRC_QUEUE_ID(sink_c->id);

		/* Normally this should never happen as we checked above
		 * that mixin is in active state and so its sink buffer
		 * should be already initialized in mixin .params().
		 */
		if (!sink_c->hw_params_configured) {
			comp_err(dev, "Uninitialized mixin sink buffer!");
			return -EINVAL;
		}

		sinks_free_frames = MIN(audio_stream_get_free_frames(output_buffers[i].data),
								     sinks_free_frames);
	}

	if (source_avail_frames > 0) {
		frames_to_copy = MIN(source_avail_frames, sinks_free_frames);
		bytes_to_consume_from_source_buf =
			audio_stream_period_bytes(input_buffers[0].data, frames_to_copy);
		input_buffers[0].consumed = bytes_to_consume_from_source_buf;
	} else {
		/* if source does not produce any data -- do NOT block mixing but generate
		 * silence as that source output.
		 *
		 * here frames_to_copy is silence size.
		 */
		/* FIXME: This will not work with 44.1 kHz !!! */
		frames_to_copy = MIN(dev->frames, sinks_free_frames);
	}

	for (i = 0; i < num_output_buffers; i++) {
		/* if source does not produce any data but mixin is in active state -- generate
		 * silence instead of that source data
		 */
		if (source_avail_frames == 0) {
			/* generate silence */
			audio_stream_set_zero(output_buffers[i].data,
					      audio_stream_period_bytes(output_buffers[i].data,
										frames_to_copy));
		} else {
			ret = copy_and_remap(dev, mixin_data, sinks_ids[i], output_buffers[i].data,
					    input_buffers[0].data, frames_to_copy);
			if (ret < 0) {
				return ret;
			}
		}

		output_buffers[i].size = audio_stream_period_bytes(output_buffers[i].data,
								   frames_to_copy);
	}

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

static int mixout_process(struct processing_module *mod,
			  struct input_stream_buffer *input_buffers, int num_input_buffers,
			  struct output_stream_buffer *output_buffers, int num_output_buffers)
{
	struct comp_dev *dev = mod->dev;
	struct mixout_data *md = module_get_private_data(mod);
	uint32_t avail_samples = INT32_MAX;
	int i;

	comp_dbg(dev, "mixout_process()");

	for (i = 0; i < num_input_buffers; i++) {
		avail_samples = MIN(audio_stream_get_avail_samples(input_buffers[i].data),
				    avail_samples);
	}

	if (avail_samples > 0 && avail_samples < INT32_MAX) {
		uint32_t samples_to_mix = MIN(audio_stream_get_free_samples(output_buffers[0].data), avail_samples);
		uint32_t bytes_to_mix = samples_to_mix * audio_stream_sample_bytes(output_buffers[0].data);

		audio_stream_copy(input_buffers[0].data, 0, output_buffers[0].data, 0, samples_to_mix);
		input_buffers[0].consumed = bytes_to_mix;

		for (i = 1; i < num_input_buffers; i++) {
			md->mix_func(input_buffers[i].data, output_buffers[0].data, samples_to_mix);
			input_buffers[i].consumed = bytes_to_mix;
		}

		output_buffers[0].size = bytes_to_mix;
	} else {
		/* FIXME: That will not work with 44.1 kHz !!! */
		uint32_t sink_bytes = MIN(audio_stream_get_free_frames(output_buffers[0].data), dev->frames) * audio_stream_frame_bytes(output_buffers[0].data);
		if (!audio_stream_set_zero(output_buffers[0].data, sink_bytes))
			output_buffers[0].size = sink_bytes;
		else
			output_buffers[0].size = 0;
	}

	return 0;
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

static void base_module_cfg_to_stream_params(const struct ipc4_base_module_cfg *base_cfg,
					     struct sof_ipc_stream_params *params);

/* params are derived from base config for ipc4 path */
static int mixin_params(struct processing_module *mod)
{
	struct sof_ipc_stream_params *params = mod->stream_params;
	struct mixin_data *md = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	struct list_item *blist;
	int ret;

	comp_dbg(dev, "mixin_params()");

	base_module_cfg_to_stream_params(&mod->priv.cfg.base_cfg, params);

	list_for_item(blist, &dev->bsink_list) {
		struct comp_buffer *sink;
		struct comp_buffer __sparse_cache *sink_c;
		uint16_t sink_id;

		sink = buffer_from_list(blist, PPL_DIR_DOWNSTREAM);
		sink_c = buffer_acquire(sink);

		sink_c->stream.channels = mod->priv.cfg.base_cfg.audio_fmt.channels_count;

		/* Applying channel remapping may produce sink stream with channel count
		 * different from source channel count.
		 */
		sink_id = IPC4_SRC_QUEUE_ID(sink_c->id);
		if (sink_id >= MIXIN_MAX_SINKS) {
			comp_err(dev, "Sink index out of range: %u, max sink count: %u",
				 (uint32_t)sink_id, MIXIN_MAX_SINKS);
			buffer_release(sink_c);
			return -EINVAL;
		}
		if (md->sink_config[sink_id].mixer_mode == IPC4_MIXER_CHANNEL_REMAPPING_MODE)
			sink_c->stream.channels = md->sink_config[sink_id].output_channel_count;

		/* comp_verify_params() does not modify valid_sample_fmt (a BUG?),
		 * let's do this here
		 */
		audio_stream_fmt_conversion(mod->priv.cfg.base_cfg.audio_fmt.depth,
					    mod->priv.cfg.base_cfg.audio_fmt.valid_bit_depth,
					    &sink_c->stream.frame_fmt,
					    &sink_c->stream.valid_sample_fmt,
					    mod->priv.cfg.base_cfg.audio_fmt.s_type);

		buffer_release(sink_c);
	}

	/* use BUFF_PARAMS_CHANNELS to skip updating channel count */
	ret = comp_verify_params(dev, BUFF_PARAMS_CHANNELS, params);
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
static int mixin_prepare(struct processing_module *mod)
{
	struct mixin_data *md = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	struct comp_buffer *sink;
	struct comp_buffer __sparse_cache *sink_c;
	enum sof_ipc_frame fmt;
	int ret;

	comp_info(dev, "mixin_prepare()");

	ret = mixin_params(mod);
	if (ret < 0)
		return ret;

	sink = list_first_item(&dev->bsink_list, struct comp_buffer, source_list);
	sink_c = buffer_acquire(sink);
	fmt = sink_c->stream.valid_sample_fmt;
	buffer_release(sink_c);

	/* currently inactive so setup mixer */
	switch (fmt) {
	case SOF_IPC_FRAME_S16_LE:
		md->gain_func = apply_gain_s16;
		break;
	case SOF_IPC_FRAME_S24_4LE:
	case SOF_IPC_FRAME_S32_LE:
		md->gain_func = apply_gain_s32;
		break;
	default:
		comp_err(dev, "unsupported data format %d", fmt);
		return -EINVAL;
	}

	return 0;
}

static void base_module_cfg_to_stream_params(const struct ipc4_base_module_cfg *base_cfg,
					     struct sof_ipc_stream_params *params)
{
	enum sof_ipc_frame __sparse_cache frame_fmt, valid_fmt;
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
	enum sof_ipc_frame __sparse_cache dummy;
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
				    &dummy, &sink_c->stream.valid_sample_fmt,
				    mod->priv.cfg.base_cfg.audio_fmt.s_type);

	sink_stream_size = sink_c->stream.size;

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

static int mixout_prepare(struct processing_module *mod)
{
	struct comp_dev *dev = mod->dev;
	struct mixout_data *md = module_get_private_data(mod);
	struct comp_buffer *sink;
	struct comp_buffer __sparse_cache *sink_c;
	enum sof_ipc_frame fmt;
	int ret;

	ret = mixout_params(mod);
	if (ret < 0)
		return ret;

	comp_dbg(dev, "mixout_prepare()");

	sink = list_first_item(&dev->bsink_list, struct comp_buffer, source_list);
	sink_c = buffer_acquire(sink);
	fmt = sink_c->stream.valid_sample_fmt;
	buffer_release(sink_c);

	/* currently inactive so setup mixout */
	switch (fmt) {
	case SOF_IPC_FRAME_S16_LE:
		md->mix_func = mix_s16;
		break;
	case SOF_IPC_FRAME_S24_4LE:
		md->mix_func = mix_s24;
		break;
	case SOF_IPC_FRAME_S32_LE:
		md->mix_func = mix_s32;
		break;
	default:
		comp_err(dev, "unsupported data format %d", fmt);
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
	.process = mixin_process,
	.set_configuration = mixin_set_config,
	.reset = mixin_reset,
	.free = mixin_free
};

DECLARE_MODULE_ADAPTER(mixin_interface, mixin_uuid, mixin_tr);
SOF_MODULE_INIT(mixin, sys_comp_module_mixin_interface_init);

static struct module_interface mixout_interface = {
	.init  = mixout_init,
	.prepare = mixout_prepare,
	.process = mixout_process,
	.reset = mixout_reset,
	.free = mixout_free
};

DECLARE_MODULE_ADAPTER(mixout_interface, mixout_uuid, mixout_tr);
SOF_MODULE_INIT(mixout, sys_comp_module_mixout_interface_init);
