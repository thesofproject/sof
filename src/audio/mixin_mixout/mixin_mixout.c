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

	dev->ipc_config.frame_fmt = valid_fmt;
	mod->skip_src_buffer_invalidate = true;

	mod->max_sinks = MIXIN_MAX_SINKS;
	return 0;
}

static int mixout_init(struct processing_module *mod)
{
	struct module_source_info __sparse_cache *mod_source_info;
	struct comp_dev *dev = mod->dev;
	struct mixout_data *mo_data;
	enum sof_ipc_frame frame_fmt, valid_fmt;

	comp_dbg(dev, "mixout_new()");

	mo_data = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(*mo_data));
	if (!mo_data)
		return -ENOMEM;

	mod_source_info = module_source_info_acquire(mod->source_info);
	mod_source_info->private = mo_data;
	module_source_info_release(mod_source_info);

	audio_stream_fmt_conversion(mod->priv.cfg.base_cfg.audio_fmt.depth,
				    mod->priv.cfg.base_cfg.audio_fmt.valid_bit_depth,
				    &frame_fmt, &valid_fmt,
				    mod->priv.cfg.base_cfg.audio_fmt.s_type);

	dev->ipc_config.frame_fmt = valid_fmt;
	mod->skip_sink_buffer_writeback = true;

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
	struct module_source_info __sparse_cache *mod_source_info;

	comp_dbg(mod->dev, "mixout_free()");

	mod_source_info = module_source_info_acquire(mod->source_info);
	rfree(mod_source_info->private);
	mod_source_info->private = NULL;
	module_source_info_release(mod_source_info);

	return 0;
}

static int mix_and_remap(struct comp_dev *dev, const struct mixin_data *mixin_data,
			 uint16_t sink_index, struct audio_stream __sparse_cache *sink,
			 uint32_t start_frame, uint32_t mixed_frames,
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
		/* Mix streams. mix_channel() is reused here to mix streams, not individual
		 * channels. To do so, (multichannel) stream is treated as single channel:
		 * channel count is passed as 1, channel index is 0, frame indices (start_frame
		 * and mixed_frame) and frame count are multiplied by real stream channel count.
		 */
		mixin_data->normal_mix_channel(sink, start_frame * audio_stream_get_channels(sink),
					       mixed_frames * audio_stream_get_channels(sink),
					       source,
					       frame_count * audio_stream_get_channels(sink),
					       sink_config->gain);
	} else if (sink_config->mixer_mode == IPC4_MIXER_CHANNEL_REMAPPING_MODE) {
		int i;

		for (i = 0; i < audio_stream_get_channels(sink); i++) {
			uint8_t source_channel =
				(sink_config->output_channel_map >> (i * 4)) & 0xf;

			if (source_channel == 0xf) {
				mixin_data->mute_channel(sink, i, start_frame, mixed_frames,
							 frame_count);
			} else {
				if (source_channel >= audio_stream_get_channels(source)) {
					comp_err(dev, "Out of range chmap: 0x%x, src channels: %u",
						 sink_config->output_channel_map,
						 audio_stream_get_channels(source));
					return -EINVAL;
				}
				mixin_data->remap_mix_channel(sink, i,
							      audio_stream_get_channels(sink),
							      start_frame, mixed_frames,
							      source, source_channel,
							      audio_stream_get_channels(source),
							      frame_count, sink_config->gain);
			}
		}
	} else {
		comp_err(dev, "Unexpected mixer mode: %d", sink_config->mixer_mode);
		return -EINVAL;
	}

	return 0;
}

/* mix silence into stream, i.e. set not yet mixed data in stream to zero */
static void silence(struct audio_stream __sparse_cache *stream, uint32_t start_frame,
		    uint32_t mixed_frames, uint32_t frame_count)
{
	uint32_t skip_mixed_frames;
	uint8_t *ptr;
	uint32_t size;
	int n;

	assert(mixed_frames >= start_frame);
	skip_mixed_frames = mixed_frames - start_frame;

	if (frame_count <= skip_mixed_frames)
		return;

	size = audio_stream_period_bytes(stream, frame_count - skip_mixed_frames);
	ptr = (uint8_t *)audio_stream_get_wptr(stream) +
		audio_stream_period_bytes(stream, mixed_frames);

	while (size) {
		ptr = audio_stream_wrap(stream, ptr);
		n = MIN(audio_stream_bytes_without_wrap(stream, ptr), size);
		memset(ptr, 0, n);
		size -= n;
		ptr += n;
	}
}

/* Most of the mixing is done here on mixin side. mixin mixes its source data
 * into each connected mixout sink buffer. Basically, if mixout sink buffer has
 * no data, mixin copies its source data into mixout sink buffer. If mixout sink
 * buffer has some data (written there by other mixin), mixin reads mixout sink
 * buffer data, mixes it with its source data and writes back to mixout sink
 * buffer. So after all mixin mixin_process() calls, mixout sink buffer contains
 * mixed data. Every mixin calls xxx_consume() on its processed source data, but
 * they do not call xxx_produce(). That is done on mixout side in mixout_process().
 *
 * Since there is no garantie that mixout processing is done in time we have
 * to account for a possibility having not yet produced data in mixout sink
 * buffer that was written there on previous run(s) of mixin_process(). So for each
 * mixin <--> mixout pair we track consumed_yet_not_produced data amount.
 * That value is also used in mixout_process() to calculate how many data was
 * actually mixed and so xxx_produce() is called for that amount.
 */
static int mixin_process(struct processing_module *mod,
			 struct input_stream_buffer *input_buffers, int num_input_buffers,
			 struct output_stream_buffer *output_buffers, int num_output_buffers)
{
	struct mixin_data *mixin_data = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	uint32_t source_avail_frames, sinks_free_frames;
	struct comp_dev *active_mixouts[MIXIN_MAX_SINKS];
	uint16_t sinks_ids[MIXIN_MAX_SINKS];
	uint32_t bytes_to_consume_from_source_buf;
	uint32_t frames_to_copy;
	int source_index;
	int i, ret;

	comp_dbg(dev, "mixin_process()");

	source_avail_frames = audio_stream_get_avail_frames(input_buffers[0].data);
	sinks_free_frames = INT32_MAX;

	/* block mixin pipeline until at least one mixout pipeline started */
	if (num_output_buffers == 0)
		return 0;

	if (num_output_buffers > MIXIN_MAX_SINKS) {
		comp_err(dev, "mixin_process(): Invalid output buffer count %d",
			 num_output_buffers);
		return -EINVAL;
	}

	/* first, let's find out how many frames can be now processed --
	 * it is a nimimal value among frames available in source buffer
	 * and frames free in each connected mixout sink buffer.
	 */
	for (i = 0; i < num_output_buffers; i++) {
		struct comp_buffer __sparse_cache *unused_in_between_buf_c;
		struct comp_dev *mixout;
		uint16_t sink_id;
		struct comp_buffer *sink;
		struct mixout_data *mixout_data;
		struct processing_module *mixout_mod;
		struct module_source_info __sparse_cache *mod_source_info;
		struct comp_buffer __sparse_cache *sink_c;
		uint32_t free_frames, pending_frames;

		/* unused buffer between mixin and mixout */
		unused_in_between_buf_c = attr_container_of(output_buffers[i].data,
							    struct comp_buffer __sparse_cache,
							    stream, __sparse_cache);
		mixout = unused_in_between_buf_c->sink;
		sink_id = IPC4_SRC_QUEUE_ID(unused_in_between_buf_c->id);

		active_mixouts[i] = mixout;
		sinks_ids[i] = sink_id;

		sink = list_first_item(&mixout->bsink_list, struct comp_buffer, source_list);

		mixout_mod = comp_get_drvdata(mixout);
		mod_source_info = module_source_info_acquire(mixout_mod->source_info);
		mixout_data = mod_source_info->private;
		source_index = find_module_source_index(mod_source_info, dev);
		if (source_index < 0) {
			comp_err(dev, "No source info");
			module_source_info_release(mod_source_info);
			return -EINVAL;
		}

		sink_c = buffer_acquire(sink);

		/* Normally this should never happen as we checked above
		 * that mixout is in active state and so its sink buffer
		 * should be already initialized in mixout .params().
		 */
		if (!sink_c->hw_params_configured) {
			comp_err(dev, "Uninitialized mixout sink buffer!");
			buffer_release(sink_c);
			module_source_info_release(mod_source_info);
			return -EINVAL;
		}

		free_frames = audio_stream_get_free_frames(&sink_c->stream);

		/* mixout sink buffer may still have not yet produced data -- data
		 * consumed and written there by mixin on previous mixin_process() run.
		 * We do NOT want to overwrite that data.
		 */
		pending_frames = mixout_data->pending_frames[source_index];
		assert(free_frames >= pending_frames);
		sinks_free_frames = MIN(sinks_free_frames, free_frames - pending_frames);

		buffer_release(sink_c);
		module_source_info_release(mod_source_info);
	}

	if (source_avail_frames > 0) {
		struct comp_buffer __sparse_cache *source_c;

		frames_to_copy = MIN(source_avail_frames, sinks_free_frames);
		bytes_to_consume_from_source_buf =
			audio_stream_period_bytes(input_buffers[0].data, frames_to_copy);
		if (bytes_to_consume_from_source_buf > 0) {
			input_buffers[0].consumed = bytes_to_consume_from_source_buf;
			source_c = attr_container_of(input_buffers[0].data,
						     struct comp_buffer __sparse_cache,
						     stream, __sparse_cache);
			buffer_stream_invalidate(source_c, bytes_to_consume_from_source_buf);
		}
	} else {
		/* if source does not produce any data -- do NOT block mixing but generate
		 * silence as that source output.
		 *
		 * here frames_to_copy is silence size.
		 */
		frames_to_copy = MIN(dev->frames, sinks_free_frames);
	}

	/* iterate over all connected mixouts and mix source data into each mixout sink buffer */
	for (i = 0; i < num_output_buffers; i++) {
		struct comp_dev *mixout;
		struct comp_buffer *sink;
		struct mixout_data *mixout_data;
		struct module_source_info __sparse_cache *mod_source_info;
		struct processing_module *mixout_mod;
		uint32_t start_frame;
		struct comp_buffer __sparse_cache *sink_c;
		uint32_t writeback_size;

		mixout = active_mixouts[i];
		sink = list_first_item(&mixout->bsink_list, struct comp_buffer, source_list);

		mixout_mod = comp_get_drvdata(mixout);
		mod_source_info = module_source_info_acquire(mixout_mod->source_info);
		mixout_data = mod_source_info->private;
		source_index = find_module_source_index(mod_source_info, dev);
		if (source_index < 0) {
			comp_err(dev, "No source info");
			module_source_info_release(mod_source_info);
			return -EINVAL;
		}

		/* Skip data from previous run(s) not yet produced in mixout_process().
		 * Normally start_frame would be 0 unless mixout pipeline has serious
		 * performance problems with processing data on time in mixout.
		 */
		start_frame = mixout_data->pending_frames[source_index];

		sink_c = buffer_acquire(sink);

		/* if source does not produce any data but mixin is in active state -- generate
		 * silence instead of that source data
		 */
		if (source_avail_frames == 0) {
			/* generate silence */
			silence(&sink_c->stream, start_frame, mixout_data->mixed_frames,
				frames_to_copy);
		} else {
			/* basically, if sink buffer has no data -- copy source data there, if
			 * sink buffer has some data (written by another mixin) mix that data
			 * with source data.
			 */
			ret = mix_and_remap(dev, mixin_data, sinks_ids[i], &sink_c->stream,
					    start_frame, mixout_data->mixed_frames,
					    input_buffers[0].data, frames_to_copy);
			if (ret < 0) {
				buffer_release(sink_c);
				module_source_info_release(mod_source_info);
				return ret;
			}
		}

		/* it would be better to writeback memory region starting from start_frame and
		 * of frames_to_copy size (converted to bytes, of course). However, seems
		 * there is no appropreate API. Anyway, start_frame would be 0 most of the time.
		 */
		writeback_size = audio_stream_period_bytes(&sink_c->stream,
							   frames_to_copy + start_frame);
		if (writeback_size > 0)
			buffer_stream_writeback(sink_c, writeback_size);
		buffer_release(sink_c);

		mixout_data->pending_frames[source_index] += frames_to_copy;

		if (frames_to_copy + start_frame > mixout_data->mixed_frames)
			mixout_data->mixed_frames = frames_to_copy + start_frame;

		module_source_info_release(mod_source_info);
	}

	return 0;
}

/* mixout just calls xxx_produce() on data mixed into its sink buffer by
 * mixins.
 */
static int mixout_process(struct processing_module *mod,
			  struct input_stream_buffer *input_buffers, int num_input_buffers,
			  struct output_stream_buffer *output_buffers, int num_output_buffers)
{
	struct module_source_info __sparse_cache *mod_source_info;
	struct comp_dev *dev = mod->dev;
	struct mixout_data *md;
	uint32_t frames_to_produce = INT32_MAX;
	uint32_t pending_frames;
	uint32_t sink_bytes;
	int i;

	comp_dbg(dev, "mixout_process()");

	mod_source_info = module_source_info_acquire(mod->source_info);
	md = mod_source_info->private;

	/* iterate over all connected mixins to find minimal value of frames they consumed
	 * (i.e., mixed into mixout sink buffer). That is the amount that can/should be
	 * produced now.
	 */
	for (i = 0; i < num_input_buffers; i++) {
		const struct audio_stream __sparse_cache *source_stream;
		struct comp_buffer __sparse_cache *unused_in_between_buf;
		struct comp_dev *source;
		int source_index;

		source_stream = input_buffers[i].data;
		unused_in_between_buf = attr_container_of(source_stream,
							  struct comp_buffer __sparse_cache,
							  stream, __sparse_cache);

		source = unused_in_between_buf->source;

		source_index = find_module_source_index(mod_source_info, source);
		/* this shouldn't happen but skip even if it does and move to the next source */
		if (source_index < 0)
			continue;

		pending_frames = md->pending_frames[source_index];

		if (source->state == COMP_STATE_ACTIVE || pending_frames)
			frames_to_produce = MIN(frames_to_produce, pending_frames);
	}

	if (frames_to_produce > 0 && frames_to_produce < INT32_MAX) {
		for (i = 0; i < num_input_buffers; i++) {
			const struct audio_stream __sparse_cache *source_stream;
			struct comp_buffer __sparse_cache *unused_in_between_buf;
			struct comp_dev *source;
			int source_index;
			uint32_t pending_frames;

			source_stream = input_buffers[i].data;
			unused_in_between_buf = attr_container_of(source_stream,
								  struct comp_buffer __sparse_cache,
								  stream, __sparse_cache);

			source = unused_in_between_buf->source;

			source_index = find_module_source_index(mod_source_info, source);
			if (source_index < 0)
				continue;

			pending_frames = md->pending_frames[source_index];
			if (pending_frames >= frames_to_produce)
				md->pending_frames[source_index] -= frames_to_produce;
			else
				md->pending_frames[source_index] = 0;
		}

		assert(md->mixed_frames >= frames_to_produce);
		md->mixed_frames -= frames_to_produce;

		sink_bytes = frames_to_produce *
				audio_stream_frame_bytes(output_buffers[0].data);
		output_buffers[0].size = sink_bytes;
	} else {
		sink_bytes = dev->frames * audio_stream_frame_bytes(output_buffers[0].data);
		if (!audio_stream_set_zero(output_buffers[0].data, sink_bytes))
			output_buffers[0].size = sink_bytes;
		else
			output_buffers[0].size = 0;
	}

	module_source_info_release(mod_source_info);

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

	/* Buffers between mixins and mixouts are not used (mixin writes data directly to mixout
	 * sink). But, anyway, let's setup these buffers properly just in case.
	 */
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
			comp_err(dev, "Sink index out of range: %u, max sink count: %u",
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
	fmt = audio_stream_get_valid_fmt(&sink_c->stream);
	buffer_release(sink_c);

	/* currently inactive so setup mixer */
	switch (fmt) {
	case SOF_IPC_FRAME_S16_LE:
	case SOF_IPC_FRAME_S24_4LE:
	case SOF_IPC_FRAME_S32_LE:
		md->normal_mix_channel = normal_mix_get_processing_function(fmt);
		md->remap_mix_channel = remap_mix_get_processing_function(fmt);
		md->mute_channel = mute_mix_get_processing_function(fmt);
		break;
	default:
		comp_err(dev, "unsupported data format %d", fmt);
		return -EINVAL;
	}

	if (!md->normal_mix_channel || !md->remap_mix_channel || !md->mute_channel) {
		comp_err(dev, "have not found the suitable processing function");
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
	params->frame_fmt = valid_fmt;

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
	struct module_source_info __sparse_cache *mod_source_info;
	struct comp_dev *dev = mod->dev;
	struct mixout_data *md;
	int ret, i;

	ret = mixout_params(mod);
	if (ret < 0)
		return ret;

	comp_dbg(dev, "mixout_prepare()");

	/*
	 * Since mixout sink buffer stream is reset on .prepare(), let's
	 * reset counters for not yet produced frames in that buffer.
	 */
	mod_source_info = module_source_info_acquire(mod->source_info);
	md = mod_source_info->private;
	md->mixed_frames = 0;

	for (i = 0; i < MIXOUT_MAX_SOURCES; i++)
		md->pending_frames[i] = 0;
	module_source_info_release(mod_source_info);

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
