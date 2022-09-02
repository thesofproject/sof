// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 Intel Corporation. All rights reserved.

#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/audio/format.h>
#include <sof/audio/mixer.h>
#include <sof/audio/pipeline.h>
#include <sof/audio/ipc-config.h>
#include <sof/common.h>
#include <sof/debug/panic.h>
#include <sof/ipc/msg.h>
#include <rtos/alloc.h>
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

static const struct comp_driver comp_mixin;
static const struct comp_driver comp_mixout;

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
 * by mixins in mixin_copy(). Simply speaking, if no data present in mixout
 * sink -- mixin just copies its source data to mixout sink. If mixout sink
 * has some data (written there previously by some other mixin) -- mixin reads
 * data from mixout sink, mixes it with its source data and writes back to
 * mixout sink.
 *
 * Such implementation has less buffer reads/writes than simple implementation
 * using intermediate buffer between mixin and mixout.
 */

/*
 * Source data is consumed by mixins in mixin_copy() but sink data cannot be
 * immediately produced. Sink data is produced by mixout in mixout_copy() after
 * ensuring all connected mixins have mixed their data into mixout sink buffer.
 * So for each connected mixin, mixout keeps knowledge of data already consumed
 * by mixin but not yet produced in mixout.
 */
struct mixout_source_info {
	struct comp_dev *mixin;
	uint32_t consumed_yet_not_produced_frames;
};

/*
 * Data used by both mixin and mixout: number of currently mixed frames in
 * mixout sink buffer and each mixin consumed data amount (and so mixout should
 * produce the appropreate amount of data).
 * Can be accessed from different cores.
 */
struct mixed_data_info {
	struct coherent c;
	uint32_t mixed_frames;
	struct mixout_source_info source_info[MIXOUT_MAX_SOURCES];
};

__must_check static inline
struct mixed_data_info __sparse_cache *mixed_data_info_acquire(struct mixed_data_info *mdi)
{
	struct coherent __sparse_cache *c = coherent_acquire_thread(&mdi->c, sizeof(*mdi));

	return attr_container_of(c, struct mixed_data_info __sparse_cache, c, __sparse_cache);
}

static inline
void mixed_data_info_release(struct mixed_data_info __sparse_cache *mdi)
{
	coherent_release_thread(&mdi->c, sizeof(*mdi));
}

struct mixin_sink_config {
	enum ipc4_mixer_mode mixer_mode;
	uint32_t output_channel_count;
	uint32_t output_channel_map;
	/* Gain as described in struct ipc4_mixer_mode_sink_config */
	uint16_t gain;
};

/* mixin component private data */
struct mixin_data {
	/* Must be the 1st field, function ipc4_comp_get_base_module_cfg casts components
	 * private data as ipc4_base_module_cfg!
	 */
	struct ipc4_base_module_cfg base_cfg;

	void (*mix_channel)(struct audio_stream __sparse_cache *sink, uint8_t sink_channel_index,
			    uint8_t sink_channel_count, uint32_t start_frame, uint32_t mixed_frames,
			    const struct audio_stream __sparse_cache *source,
			    uint8_t source_channel_index, uint8_t source_channel_count,
			    uint32_t frame_count, uint16_t gain);

	void (*mute_channel)(struct audio_stream __sparse_cache *stream, uint8_t channel_index,
			     uint32_t start_frame, uint32_t mixed_frames, uint32_t frame_count);

	struct mixin_sink_config sink_config[MIXIN_MAX_SINKS];
};

/* mixout component private data */
struct mixout_data {
	/* Must be the 1st field, function ipc4_comp_get_base_module_cfg casts components
	 * private data as ipc4_base_module_cfg!
	 */
	struct ipc4_base_module_cfg base_cfg;
	struct mixed_data_info *mixed_data_info;
};

static struct comp_dev *mixin_new(const struct comp_driver *drv,
				  struct comp_ipc_config *config,
				  void *spec)
{
	struct comp_dev *dev;
	struct mixin_data *md;
	int i;
	enum sof_ipc_frame __sparse_cache frame_fmt, valid_fmt;

	comp_cl_dbg(&comp_mixin, "mixin_new()");

	dev = comp_alloc(drv, sizeof(*dev));
	if (!dev)
		return NULL;

	dev->ipc_config = *config;

	md = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(*md));
	if (!md) {
		rfree(dev);
		return NULL;
	}

	memcpy_s(&md->base_cfg, sizeof(md->base_cfg), spec, sizeof(md->base_cfg));

	for (i = 0; i < MIXIN_MAX_SINKS; i++) {
		md->sink_config[i].mixer_mode = IPC4_MIXER_NORMAL_MODE;
		md->sink_config[i].gain = IPC4_MIXIN_UNITY_GAIN;
	}

	comp_set_drvdata(dev, md);

	audio_stream_fmt_conversion(md->base_cfg.audio_fmt.depth,
				    md->base_cfg.audio_fmt.valid_bit_depth,
				    &frame_fmt, &valid_fmt,
				    md->base_cfg.audio_fmt.s_type);

	dev->ipc_config.frame_fmt = frame_fmt;

	dev->state = COMP_STATE_READY;
	return dev;
}

static struct comp_dev *mixout_new(const struct comp_driver *drv,
				   struct comp_ipc_config *config,
				   void *spec)
{
	struct comp_dev *dev;
	struct mixout_data *md;
	enum sof_ipc_frame __sparse_cache frame_fmt, valid_fmt;

	comp_cl_dbg(&comp_mixout, "mixout_new()");

	dev = comp_alloc(drv, sizeof(*dev));
	if (!dev)
		return NULL;

	dev->ipc_config = *config;

	md = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(*md));
	if (!md) {
		rfree(dev);
		return NULL;
	}

	memcpy_s(&md->base_cfg, sizeof(md->base_cfg), spec, sizeof(md->base_cfg));

	md->mixed_data_info = rzalloc(SOF_MEM_ZONE_RUNTIME_SHARED, 0, SOF_MEM_CAPS_RAM,
				      sizeof(struct mixed_data_info));
	if (!md->mixed_data_info) {
		rfree(md);
		rfree(dev);
		return NULL;
	}

	coherent_init_thread(md->mixed_data_info, c);
	comp_set_drvdata(dev, md);

	audio_stream_fmt_conversion(md->base_cfg.audio_fmt.depth,
				    md->base_cfg.audio_fmt.valid_bit_depth,
				    &frame_fmt, &valid_fmt,
				    md->base_cfg.audio_fmt.s_type);

	dev->ipc_config.frame_fmt = frame_fmt;

	dev->state = COMP_STATE_READY;
	return dev;
}

static void mixin_free(struct comp_dev *dev)
{
	comp_dbg(dev, "mixin_free()");
	rfree(comp_get_drvdata(dev));
	rfree(dev);
}

static void mixout_free(struct comp_dev *dev)
{
	struct mixout_data *mixout_data;

	comp_dbg(dev, "mixout_free()");

	mixout_data = comp_get_drvdata(dev);

	coherent_free_thread(mixout_data->mixed_data_info, c);
	rfree(mixout_data->mixed_data_info);
	rfree(mixout_data);
	rfree(dev);
}

static
struct mixout_source_info *find_mixout_source_info(struct mixed_data_info __sparse_cache *mdi,
						   const struct comp_dev *mixin)
{
	/* mixin == NULL is also a valid input -- this will find first unused entry */
	int i;

	for (i = 0; i < MIXOUT_MAX_SOURCES; i++) {
		if (mdi->source_info[i].mixin == mixin)
			return &mdi->source_info[i];
	}

	return NULL;
}

#if CONFIG_FORMAT_S16LE
/* Instead of using sink->channels and source->channels, sink_channel_count and
 * source_channel_count are supplied as parameters. This is done to reuse the function
 * to also mix an entire stream. In this case the function is called with fake stream
 * parameters: multichannel stream is treated as single channel and so the entire stream
 * contents is mixed.
 */
static void mix_channel_s16(struct audio_stream __sparse_cache *sink, uint8_t sink_channel_index,
			    uint8_t sink_channel_count, uint32_t start_frame, uint32_t mixed_frames,
			    const struct audio_stream __sparse_cache *source,
			    uint8_t source_channel_index, uint8_t source_channel_count,
			    uint32_t frame_count, uint16_t gain)
{
	int16_t *dest, *src;
	uint32_t frames_to_mix, frames_to_copy;

	/* audio_stream_wrap() is required and is done below in a loop */
	dest = (int16_t *)sink->w_ptr + start_frame * sink_channel_count + sink_channel_index;
	src = (int16_t *)source->r_ptr + source_channel_index;

	assert(mixed_frames >= start_frame);
	frames_to_mix = MIN(mixed_frames - start_frame, frame_count);
	frames_to_copy = frame_count - frames_to_mix;

	/* we do not want to use something like audio_stream_frames_without_wrap() here
	 * as it uses stream->channels internally. Also we would like to avoid expensive
	 * division operations.
	 */
	while (frames_to_mix) {
		src = audio_stream_wrap(source, src);
		dest = audio_stream_wrap(sink, dest);

		if (gain == IPC4_MIXIN_UNITY_GAIN)
			while (frames_to_mix && src < (int16_t *)source->end_addr &&
			       dest < (int16_t *)sink->end_addr) {
				*dest = sat_int16((int32_t)*dest + (int32_t)*src);
				src += source_channel_count;
				dest += sink_channel_count;
				frames_to_mix--;
			}
		else
			while (frames_to_mix && src < (int16_t *)source->end_addr &&
			       dest < (int16_t *)sink->end_addr) {
				*dest = sat_int16((int32_t)*dest +
						  q_mults_16x16(*src, gain,
								IPC4_MIXIN_GAIN_SHIFT));
				src += source_channel_count;
				dest += sink_channel_count;
				frames_to_mix--;
			}
	}

	while (frames_to_copy) {
		src = audio_stream_wrap(source, src);
		dest = audio_stream_wrap(sink, dest);

		if (gain == IPC4_MIXIN_UNITY_GAIN)
			while (frames_to_copy && src < (int16_t *)source->end_addr &&
			       dest < (int16_t *)sink->end_addr) {
				*dest = *src;
				src += source_channel_count;
				dest += sink_channel_count;
				frames_to_copy--;
			}
		else
			while (frames_to_copy && src < (int16_t *)source->end_addr &&
			       dest < (int16_t *)sink->end_addr) {
				*dest = (int16_t)q_mults_16x16(*src, gain, IPC4_MIXIN_GAIN_SHIFT);
				src += source_channel_count;
				dest += sink_channel_count;
				frames_to_copy--;
			}
	}
}

static void mute_channel_s16(struct audio_stream __sparse_cache *stream, uint8_t channel_index,
			     uint32_t start_frame, uint32_t mixed_frames, uint32_t frame_count)
{
	uint32_t skip_mixed_frames;
	int16_t *ptr;

	assert(mixed_frames >= start_frame);
	skip_mixed_frames = mixed_frames - start_frame;

	if (frame_count <= skip_mixed_frames)
		return;
	frame_count -= skip_mixed_frames;

	/* audio_stream_wrap() is needed here and it is just below in a loop */
	ptr = (int16_t *)stream->w_ptr + mixed_frames * stream->channels + channel_index;

	while (frame_count) {
		ptr = audio_stream_wrap(stream, ptr);
		while (frame_count && ptr < (int16_t *)stream->end_addr) {
			*ptr = 0;
			ptr += stream->channels;
			frame_count--;
		}
	}
}
#endif	/* CONFIG_FORMAT_S16LE */

#if CONFIG_FORMAT_S24LE
/* Instead of using sink->channels and source->channels, sink_channel_count and
 * source_channel_count are supplied as parameters. This is done to reuse the function
 * to also mix an entire stream. In this case the function is called with fake stream
 * parameters: multichannel stream is treated as single channel and so the entire stream
 * contents is mixed.
 */
static void mix_channel_s24(struct audio_stream __sparse_cache *sink, uint8_t sink_channel_index,
			    uint8_t sink_channel_count, uint32_t start_frame, uint32_t mixed_frames,
			    const struct audio_stream __sparse_cache *source,
			    uint8_t source_channel_index, uint8_t source_channel_count,
			    uint32_t frame_count, uint16_t gain)
{
	int32_t *dest, *src;
	uint32_t frames_to_mix, frames_to_copy;

	/* audio_stream_wrap() is required and is done below in a loop */
	dest = (int32_t *)sink->w_ptr + start_frame * sink_channel_count + sink_channel_index;
	src = (int32_t *)source->r_ptr + source_channel_index;

	assert(mixed_frames >= start_frame);
	frames_to_mix = MIN(mixed_frames - start_frame, frame_count);
	frames_to_copy = frame_count - frames_to_mix;

	/* we do not want to use something like audio_stream_frames_without_wrap() here
	 * as it uses stream->channels internally. Also we would like to avoid expensive
	 * division operations.
	 */
	while (frames_to_mix) {
		src = audio_stream_wrap(source, src);
		dest = audio_stream_wrap(sink, dest);

		if (gain == IPC4_MIXIN_UNITY_GAIN)
			while (frames_to_mix && src < (int32_t *)source->end_addr &&
			       dest < (int32_t *)sink->end_addr) {
				*dest = sat_int24(sign_extend_s24(*dest) + sign_extend_s24(*src));
				src += source_channel_count;
				dest += sink_channel_count;
				frames_to_mix--;
			}
		else
			while (frames_to_mix && src < (int32_t *)source->end_addr &&
			       dest < (int32_t *)sink->end_addr) {
				*dest = sat_int24(sign_extend_s24(*dest) +
						  (int32_t)q_mults_32x32(sign_extend_s24(*src),
						  gain, IPC4_MIXIN_GAIN_SHIFT));
				src += source_channel_count;
				dest += sink_channel_count;
				frames_to_mix--;
			}
	}

	while (frames_to_copy) {
		src = audio_stream_wrap(source, src);
		dest = audio_stream_wrap(sink, dest);

		if (gain == IPC4_MIXIN_UNITY_GAIN)
			while (frames_to_copy && src < (int32_t *)source->end_addr &&
			       dest < (int32_t *)sink->end_addr) {
				*dest = *src;
				src += source_channel_count;
				dest += sink_channel_count;
				frames_to_copy--;
			}
		else
			while (frames_to_copy && src < (int32_t *)source->end_addr &&
			       dest < (int32_t *)sink->end_addr) {
				*dest = (int32_t)q_mults_32x32(sign_extend_s24(*src),
							       gain, IPC4_MIXIN_GAIN_SHIFT);
				src += source_channel_count;
				dest += sink_channel_count;
				frames_to_copy--;
			}
	}
}

#endif	/* CONFIG_FORMAT_S24LE */

#if CONFIG_FORMAT_S32LE
/* Instead of using sink->channels and source->channels, sink_channel_count and
 * source_channel_count are supplied as parameters. This is done to reuse the function
 * to also mix an entire stream. In this case the function is called with fake stream
 * parameters: multichannel stream is treated as single channel and so the entire stream
 * contents is mixed.
 */
static void mix_channel_s32(struct audio_stream __sparse_cache *sink, uint8_t sink_channel_index,
			    uint8_t sink_channel_count, uint32_t start_frame, uint32_t mixed_frames,
			    const struct audio_stream __sparse_cache *source,
			    uint8_t source_channel_index, uint8_t source_channel_count,
			    uint32_t frame_count, uint16_t gain)
{
	int32_t *dest, *src;
	uint32_t frames_to_mix, frames_to_copy;

	/* audio_stream_wrap() is required and is done below in a loop */
	dest = (int32_t *)sink->w_ptr + start_frame * sink_channel_count + sink_channel_index;
	src = (int32_t *)source->r_ptr + source_channel_index;

	assert(mixed_frames >= start_frame);
	frames_to_mix = MIN(mixed_frames - start_frame, frame_count);
	frames_to_copy = frame_count - frames_to_mix;

	/* we do not want to use something like audio_stream_frames_without_wrap() here
	 * as it uses stream->channels internally. Also we would like to avoid expensive
	 * division operations.
	 */
	while (frames_to_mix) {
		src = audio_stream_wrap(source, src);
		dest = audio_stream_wrap(sink, dest);

		if (gain == IPC4_MIXIN_UNITY_GAIN)
			while (frames_to_mix && src < (int32_t *)source->end_addr &&
			       dest < (int32_t *)sink->end_addr) {
				*dest = sat_int32((int64_t)*dest + (int64_t)*src);
				src += source_channel_count;
				dest += sink_channel_count;
				frames_to_mix--;
			}
		else
			while (frames_to_mix && src < (int32_t *)source->end_addr &&
			       dest < (int32_t *)sink->end_addr) {
				*dest = sat_int32((int64_t)*dest +
						  q_mults_32x32(*src, gain, IPC4_MIXIN_GAIN_SHIFT));
				src += source_channel_count;
				dest += sink_channel_count;
				frames_to_mix--;
			}
	}

	while (frames_to_copy) {
		src = audio_stream_wrap(source, src);
		dest = audio_stream_wrap(sink, dest);

		if (gain == IPC4_MIXIN_UNITY_GAIN)
			while (frames_to_copy && src < (int32_t *)source->end_addr &&
			       dest < (int32_t *)sink->end_addr) {
				*dest = *src;
				src += source_channel_count;
				dest += sink_channel_count;
				frames_to_copy--;
			}
		else
			while (frames_to_copy && src < (int32_t *)source->end_addr &&
			       dest < (int32_t *)sink->end_addr) {
				*dest = (int32_t)q_mults_32x32(*src, gain, IPC4_MIXIN_GAIN_SHIFT);
				src += source_channel_count;
				dest += sink_channel_count;
				frames_to_copy--;
			}
	}
}

static void mute_channel_s32(struct audio_stream __sparse_cache *stream, uint8_t channel_index,
			     uint32_t start_frame, uint32_t mixed_frames, uint32_t frame_count)
{
	uint32_t skip_mixed_frames;
	int32_t *ptr;

	assert(mixed_frames >= start_frame);
	skip_mixed_frames = mixed_frames - start_frame;

	if (frame_count <= skip_mixed_frames)
		return;
	frame_count -= skip_mixed_frames;

	/* audio_stream_wrap() is needed here and it is just below in a loop */
	ptr = (int32_t *)stream->w_ptr + mixed_frames * stream->channels + channel_index;

	while (frame_count) {
		ptr = audio_stream_wrap(stream, ptr);
		while (frame_count && ptr < (int32_t *)stream->end_addr) {
			*ptr = 0;
			ptr += stream->channels;
			frame_count--;
		}
	}
}
#endif	/* CONFIG_FORMAT_S32LE */

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
		mixin_data->mix_channel(sink, 0, 1, start_frame * sink->channels,
					mixed_frames * sink->channels, source, 0, 1,
					frame_count * sink->channels, sink_config->gain);
	} else if (sink_config->mixer_mode == IPC4_MIXER_CHANNEL_REMAPPING_MODE) {
		int i;

		for (i = 0; i < sink->channels; i++) {
			uint8_t source_channel =
				(sink_config->output_channel_map >> (i * 4)) & 0xf;

			if (source_channel == 0xf) {
				mixin_data->mute_channel(sink, i, start_frame, mixed_frames,
							 frame_count);
			} else {
				if (source_channel >= source->channels) {
					comp_err(dev, "Out of range chmap: 0x%x, src channels: %u",
						 sink_config->output_channel_map,
						 source->channels);
					return -EINVAL;
				}
				mixin_data->mix_channel(sink, i, sink->channels, start_frame,
							mixed_frames, source, source_channel,
							source->channels, frame_count,
							sink_config->gain);
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
	ptr = (uint8_t *)stream->w_ptr + audio_stream_period_bytes(stream, mixed_frames);

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
 * buffer. So after all mixin mixin_copy() calls, mixout sink buffer contains
 * mixed data. Every mixin calls xxx_consume() on its processed source data, but
 * they do not call xxx_produce(). That is done on mixout side in mixout_copy().
 *
 * Since there is no garantie that mixout processing is done in time we have
 * to account for a possibility having not yet produced data in mixout sink
 * buffer that was written there on previous run(s) of mixin_copy(). So for each
 * mixin <--> mixout pair we track consumed_yet_not_produced data amount.
 * That value is also used in mixout_copy() to calculate how many data was
 * actually mixed and so xxx_produce() is called for that amount.
 */
static int mixin_copy(struct comp_dev *dev)
{
	struct mixin_data *mixin_data;
	struct comp_buffer *source;
	struct comp_buffer __sparse_cache *source_c;
	uint32_t source_avail_frames, sinks_free_frames;
	struct comp_dev *active_mixouts[MIXIN_MAX_SINKS];
	uint16_t sinks_ids[MIXIN_MAX_SINKS];
	int active_mixout_cnt;
	struct list_item *blist;
	uint32_t bytes_to_consume_from_source_buf;
	uint32_t frames_to_copy;
	int i, ret;

	comp_dbg(dev, "mixin_copy()");

	mixin_data = comp_get_drvdata(dev);

	source = list_first_item(&dev->bsource_list, struct comp_buffer, sink_list);
	source_c = buffer_acquire(source);

	source_avail_frames = audio_stream_get_avail_frames(&source_c->stream);
	sinks_free_frames = INT32_MAX;

	/* first, let's find out how many frames can be now processed --
	 * it is a nimimal value among frames available in source buffer
	 * and frames free in each connected mixout sink buffer.
	 */
	active_mixout_cnt = 0;

	list_for_item(blist, &dev->bsink_list) {
		struct comp_buffer *unused_in_between_buf;
		struct comp_buffer __sparse_cache *unused_in_between_buf_c;
		struct comp_dev *mixout;
		uint16_t sink_id;
		struct comp_buffer *sink;
		struct mixout_data *mixout_data;
		struct mixed_data_info __sparse_cache *mixed_data_info;
		struct mixout_source_info *src_info;
		struct comp_buffer __sparse_cache *sink_c;
		uint32_t free_frames;

		/* unused buffer between mixin and mixout */
		unused_in_between_buf = buffer_from_list(blist, struct comp_buffer,
							 PPL_DIR_DOWNSTREAM);
		mixout = buffer_get_comp(unused_in_between_buf, PPL_DIR_DOWNSTREAM);
		unused_in_between_buf_c = buffer_acquire(unused_in_between_buf);
		sink_id = IPC4_SRC_QUEUE_ID(unused_in_between_buf_c->id);
		buffer_release(unused_in_between_buf_c);

		if (comp_get_state(dev, mixout) != COMP_STATE_ACTIVE)
			continue;
		active_mixouts[active_mixout_cnt] = mixout;
		sinks_ids[active_mixout_cnt] = sink_id;
		active_mixout_cnt++;

		sink = list_first_item(&mixout->bsink_list, struct comp_buffer, source_list);

		mixout_data = comp_get_drvdata(mixout);
		mixed_data_info = mixed_data_info_acquire(mixout_data->mixed_data_info);
		src_info = find_mixout_source_info(mixed_data_info, dev);
		if (!src_info) {
			comp_err(dev, "No source info");
			mixed_data_info_release(mixed_data_info);
			buffer_release(source_c);
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
			mixed_data_info_release(mixed_data_info);
			buffer_release(source_c);
			return -EINVAL;
		}

		free_frames = audio_stream_get_free_frames(&sink_c->stream);

		/* mixout sink buffer may still have not yet produced data -- data
		 * consumed and written there by mixin on previous mixin_copy() run.
		 * We do NOT want to overwrite that data.
		 */
		assert(free_frames >= src_info->consumed_yet_not_produced_frames);
		sinks_free_frames = MIN(sinks_free_frames,
					free_frames -
					src_info->consumed_yet_not_produced_frames);

		buffer_release(sink_c);
		mixed_data_info_release(mixed_data_info);
	}

	bytes_to_consume_from_source_buf = 0;
	if (source_avail_frames > 0) {
		if (active_mixout_cnt == 0) {
			/* discard source data */
			comp_update_buffer_consume(source_c,
						   audio_stream_period_bytes(&source_c->stream,
									     source_avail_frames));
			buffer_release(source_c);
			return 0;
		}

		frames_to_copy = MIN(source_avail_frames, sinks_free_frames);
		bytes_to_consume_from_source_buf =
			audio_stream_period_bytes(&source_c->stream, frames_to_copy);
		if (bytes_to_consume_from_source_buf > 0)
			buffer_stream_invalidate(source_c, bytes_to_consume_from_source_buf);
	} else {
		/* if source does not produce any data -- do NOT block mixing but generate
		 * silence as that source output.
		 *
		 * here frames_to_copy is silence size.
		 */
		frames_to_copy = MIN(dev->frames, sinks_free_frames);
	}

	/* iterate over all connected mixouts and mix source data into each mixout sink buffer */
	for (i = 0; i < active_mixout_cnt; i++) {
		struct comp_dev *mixout;
		struct comp_buffer *sink;
		struct mixout_data *mixout_data;
		struct mixed_data_info __sparse_cache *mixed_data_info;
		struct mixout_source_info *src_info;
		uint32_t start_frame;
		struct comp_buffer __sparse_cache *sink_c;
		uint32_t writeback_size;

		mixout = active_mixouts[i];
		sink = list_first_item(&mixout->bsink_list, struct comp_buffer, source_list);

		mixout_data = comp_get_drvdata(mixout);
		mixed_data_info = mixed_data_info_acquire(mixout_data->mixed_data_info);
		src_info = find_mixout_source_info(mixed_data_info, dev);
		if (!src_info) {
			comp_err(dev, "No source info");
			mixed_data_info_release(mixed_data_info);
			buffer_release(source_c);
			return -EINVAL;
		}

		/* Skip data from previous run(s) not yet produced in mixout_copy().
		 * Normally start_frame would be 0 unless mixout pipeline has serious
		 * performance problems with processing data on time in mixout.
		 */
		start_frame = src_info->consumed_yet_not_produced_frames;
		assert(sinks_free_frames >= start_frame);

		sink_c = buffer_acquire(sink);

		/* if source does not produce any data but mixin is in active state -- generate
		 * silence instead of that source data
		 */
		if (source_avail_frames == 0) {
			/* generate silence */
			silence(&sink_c->stream, start_frame, mixed_data_info->mixed_frames,
				frames_to_copy);
		} else {
			/* basically, if sink buffer has no data -- copy source data there, if
			 * sink buffer has some data (written by another mixin) mix that data
			 * with source data.
			 */
			ret = mix_and_remap(dev, mixin_data, sinks_ids[i], &sink_c->stream,
					    start_frame, mixed_data_info->mixed_frames,
					    &source_c->stream, frames_to_copy);
			if (ret < 0) {
				buffer_release(sink_c);
				mixed_data_info_release(mixed_data_info);
				buffer_release(source_c);
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

		src_info->consumed_yet_not_produced_frames += frames_to_copy;

		if (frames_to_copy + start_frame > mixed_data_info->mixed_frames)
			mixed_data_info->mixed_frames = frames_to_copy + start_frame;

		mixed_data_info_release(mixed_data_info);
	}

	if (bytes_to_consume_from_source_buf > 0)
		comp_update_buffer_consume(source_c, bytes_to_consume_from_source_buf);
	buffer_release(source_c);

	return 0;
}

/* mixout just calls xxx_produce() on data mixed into its sink buffer by
 * mixins.
 */
static int mixout_copy(struct comp_dev *dev)
{
	struct mixout_data *mixout_data;
	struct mixed_data_info __sparse_cache *mixed_data_info;
	struct list_item *blist;
	uint32_t frames_to_produce = INT32_MAX;

	comp_dbg(dev, "mixout_copy()");

	mixout_data = comp_get_drvdata(dev);
	mixed_data_info = mixed_data_info_acquire(mixout_data->mixed_data_info);

	/* iterate over all connected mixins to find minimal value of frames they consumed
	 * (i.e., mixed into mixout sink buffer). That is the amount that can/should be
	 * produced now.
	 */
	list_for_item(blist, &dev->bsource_list) {
		struct comp_buffer *unused_in_between_buf;
		struct comp_dev *mixin;
		struct mixout_source_info *src_info;

		unused_in_between_buf = buffer_from_list(blist, struct comp_buffer,
							 PPL_DIR_UPSTREAM);
		mixin = buffer_get_comp(unused_in_between_buf, PPL_DIR_UPSTREAM);

		src_info = find_mixout_source_info(mixed_data_info, mixin);
		if (!src_info) {
			comp_err(dev, "No source info");
			mixed_data_info_release(mixed_data_info);
			return -EINVAL;
		}

		/* Inactive sources should not block other active sources */
		if (comp_get_state(dev, mixin) == COMP_STATE_ACTIVE)
			frames_to_produce = MIN(frames_to_produce,
						src_info->consumed_yet_not_produced_frames);
	}

	if (frames_to_produce > 0 && frames_to_produce < INT32_MAX) {
		int i;
		struct comp_buffer *sink;
		struct comp_buffer __sparse_cache *sink_c;

		for (i = 0; i < MIXOUT_MAX_SOURCES; i++) {
			if (!mixed_data_info->source_info[i].mixin)
				continue;

			if (mixed_data_info->source_info[i].consumed_yet_not_produced_frames >=
				frames_to_produce)
				mixed_data_info->source_info[i].consumed_yet_not_produced_frames -=
					frames_to_produce;
			else
				mixed_data_info->source_info[i].consumed_yet_not_produced_frames =
					0;
		}

		assert(mixed_data_info->mixed_frames >= frames_to_produce);
		mixed_data_info->mixed_frames -= frames_to_produce;

		sink = list_first_item(&dev->bsink_list, struct comp_buffer, source_list);
		sink_c = buffer_acquire(sink);
		/* writeback is already done in mixin while mixing */
		comp_update_buffer_produce(sink_c,
					   audio_stream_period_bytes(&sink_c->stream,
								     frames_to_produce));
		buffer_release(sink_c);
	}

	mixed_data_info_release(mixed_data_info);

	return 0;
}

static int mixin_reset(struct comp_dev *dev)
{
	struct mixin_data *mixin_data;

	comp_dbg(dev, "mixin_reset()");

	mixin_data = comp_get_drvdata(dev);
	mixin_data->mix_channel = NULL;
	mixin_data->mute_channel = NULL;

	comp_set_state(dev, COMP_TRIGGER_RESET);

	return 0;
}

static int mixout_reset(struct comp_dev *dev)
{
	struct mixout_data *mixout_data;
	struct mixed_data_info __sparse_cache *mixed_data_info;
	struct list_item *blist;

	comp_dbg(dev, "mixout_reset()");

	mixout_data = comp_get_drvdata(dev);
	mixed_data_info = mixed_data_info_acquire(mixout_data->mixed_data_info);
	memset(mixed_data_info->source_info, 0, sizeof(mixed_data_info->source_info));
	mixed_data_info_release(mixed_data_info);

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

	comp_set_state(dev, COMP_TRIGGER_RESET);

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
static int mixin_prepare(struct comp_dev *dev)
{
	struct mixin_data *md;
	struct comp_buffer *sink;
	struct comp_buffer __sparse_cache *sink_c;
	enum sof_ipc_frame fmt;
	int ret;

	comp_dbg(dev, "mixin_prepare()");

	if (dev->state == COMP_STATE_ACTIVE)
		return 0;

	md = comp_get_drvdata(dev);

	sink = list_first_item(&dev->bsink_list, struct comp_buffer,
			       source_list);
	sink_c = buffer_acquire(sink);
	fmt = sink_c->stream.valid_sample_fmt;
	buffer_release(sink_c);

	/* currently inactive so setup mixer */
	switch (fmt) {
#if CONFIG_FORMAT_S16LE
	case SOF_IPC_FRAME_S16_LE:
		md->mix_channel = mix_channel_s16;
		md->mute_channel = mute_channel_s16;
		break;
#endif /* CONFIG_FORMAT_S16LE */
#if CONFIG_FORMAT_S24LE
	case SOF_IPC_FRAME_S24_4LE:
		md->mix_channel = mix_channel_s24;
		md->mute_channel = mute_channel_s32;	/* yes, 32 is correct */
		break;
#endif /* CONFIG_FORMAT_S24LE */
#if CONFIG_FORMAT_S32LE
	case SOF_IPC_FRAME_S32_LE:
		md->mix_channel = mix_channel_s32;
		md->mute_channel = mute_channel_s32;
		break;
#endif /* CONFIG_FORMAT_S32LE */
	default:
		comp_err(dev, "unsupported data format");
		return -EINVAL;
	}

	ret = comp_set_state(dev, COMP_TRIGGER_PREPARE);
	if (ret < 0)
		return ret;

	if (ret == COMP_STATUS_STATE_ALREADY_SET)
		return PPL_STATUS_PATH_STOP;

	return 0;
}

static int mixout_prepare(struct comp_dev *dev)
{
	struct mixout_data *md;
	int ret;
	struct list_item *blist;

	comp_dbg(dev, "mixout_prepare()");

	if (dev->state == COMP_STATE_ACTIVE)
		return 0;

	md = comp_get_drvdata(dev);

	list_for_item(blist, &dev->bsource_list) {
		struct comp_buffer *unused_in_between_buf;
		struct comp_dev *mixin;

		unused_in_between_buf = buffer_from_list(blist, struct comp_buffer,
							 PPL_DIR_UPSTREAM);
		mixin = buffer_get_comp(unused_in_between_buf, PPL_DIR_UPSTREAM);

		if (mixin->pipeline && mixin->pipeline->core != dev->pipeline->core) {
			coherent_shared_thread(md->mixed_data_info, c);
			break;
		}
	}

	ret = comp_set_state(dev, COMP_TRIGGER_PREPARE);
	if (ret < 0)
		return ret;

	if (ret == COMP_STATUS_STATE_ALREADY_SET)
		return PPL_STATUS_PATH_STOP;

	return 0;
}

static int mixinout_trigger(struct comp_dev *dev, int cmd)
{
	int ret;

	comp_dbg(dev, "mixinout_trigger()");

	ret = comp_set_state(dev, cmd);

	if (ret == COMP_STATUS_STATE_ALREADY_SET)
		return PPL_STATUS_PATH_STOP;

	return ret;
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

/* params are derived from base config for ipc4 path */
static int mixin_params(struct comp_dev *dev,
			struct sof_ipc_stream_params *params)
{
	struct mixin_data *md;
	struct list_item *blist;
	int ret;

	comp_dbg(dev, "mixin_params()");

	md = comp_get_drvdata(dev);
	base_module_cfg_to_stream_params(&md->base_cfg, params);

	/* Buffers between mixins and mixouts are not used (mixin writes data directly to mixout
	 * sink). But, anyway, let's setup these buffers properly just in case.
	 */
	list_for_item(blist, &dev->bsink_list) {
		struct comp_buffer *sink;
		struct comp_buffer __sparse_cache *sink_c;
		uint16_t sink_id;

		sink = buffer_from_list(blist, struct comp_buffer, PPL_DIR_DOWNSTREAM);
		sink_c = buffer_acquire(sink);

		sink_c->stream.channels = md->base_cfg.audio_fmt.channels_count;

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
		audio_stream_fmt_conversion(md->base_cfg.audio_fmt.depth,
					    md->base_cfg.audio_fmt.valid_bit_depth,
					    &sink_c->stream.frame_fmt,
					    &sink_c->stream.valid_sample_fmt,
					    md->base_cfg.audio_fmt.s_type);

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

static int mixout_params(struct comp_dev *dev,
			 struct sof_ipc_stream_params *params)
{
	struct mixout_data *md;
	int ret;
	struct comp_buffer *sink;
	struct comp_buffer __sparse_cache *sink_c;
	enum sof_ipc_frame __sparse_cache dummy;
	uint32_t sink_period_bytes, sink_stream_size;

	comp_dbg(dev, "mixout_params()");

	md = comp_get_drvdata(dev);
	base_module_cfg_to_stream_params(&md->base_cfg, params);

	ret = comp_verify_params(dev, 0, params);
	if (ret < 0) {
		comp_err(dev, "mixout_params(): comp_verify_params() failed!");
		return -EINVAL;
	}

	sink = list_first_item(&dev->bsink_list, struct comp_buffer,
			       source_list);
	sink_c = buffer_acquire(sink);

	/* comp_verify_params() does not modify valid_sample_fmt (a BUG?), let's do this here */
	audio_stream_fmt_conversion(md->base_cfg.audio_fmt.depth,
				    md->base_cfg.audio_fmt.valid_bit_depth,
				    &dummy, &sink_c->stream.valid_sample_fmt,
				    md->base_cfg.audio_fmt.s_type);

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

static int mixout_bind(struct comp_dev *dev, void *data)
{
	struct ipc4_module_bind_unbind *bu;
	int src_id;
	struct mixout_data *md;
	struct mixed_data_info __sparse_cache *mixed_data_info;

	bu = (struct ipc4_module_bind_unbind *)data;
	src_id = IPC4_COMP_ID(bu->primary.r.module_id, bu->primary.r.instance_id);

	md = comp_get_drvdata(dev);
	mixed_data_info = mixed_data_info_acquire(md->mixed_data_info);

	/*
	 * If dev->ipc_config.id == src_id then we're called for the downstream
	 * link, nothing to do
	 */
	if (dev->ipc_config.id != src_id) {
		/* new mixin -> mixout */
		struct comp_dev *mixin;
		struct mixout_source_info *source_info;

		mixin = ipc4_get_comp_dev(src_id);
		if (!mixin) {
			comp_err(dev, "mixout_bind: no source with ID %d found", src_id);
			mixed_data_info_release(mixed_data_info);
			return -EINVAL;
		}

		source_info = find_mixout_source_info(mixed_data_info, mixin);
		if (source_info) {
			/* this should never happen as source_info should
			 * have been already cleared in minxout_unbind()
			 */
			memset(source_info, 0, sizeof(*source_info));
		}
		source_info = find_mixout_source_info(mixed_data_info, NULL);
		if (!source_info) {
			/* no free space in source_info table */
			comp_err(dev, "Too many mixout inputs!");
			mixed_data_info_release(mixed_data_info);
			return -ENOMEM;
		}
		source_info->mixin = mixin;
		source_info->consumed_yet_not_produced_frames = 0;
	}

	mixed_data_info_release(mixed_data_info);

	return 0;
}

static int mixout_unbind(struct comp_dev *dev, void *data)
{
	struct ipc4_module_bind_unbind *bu;
	int src_id;
	struct mixout_data *md;
	struct mixed_data_info __sparse_cache *mixed_data_info;

	bu = (struct ipc4_module_bind_unbind *)data;
	src_id = IPC4_COMP_ID(bu->primary.r.module_id, bu->primary.r.instance_id);

	md = comp_get_drvdata(dev);
	mixed_data_info = mixed_data_info_acquire(md->mixed_data_info);

	/* mixout -> new sink */
	if (dev->ipc_config.id == src_id) {
		mixed_data_info->mixed_frames = 0;
		memset(mixed_data_info->source_info, 0, sizeof(mixed_data_info->source_info));
	} else { /* new mixin -> mixout */
		struct comp_dev *mixin;
		struct mixout_source_info *source_info;

		mixin = ipc4_get_comp_dev(src_id);
		if (!mixin) {
			comp_err(dev, "mixout_bind: no source with ID %d found", src_id);
			mixed_data_info_release(mixed_data_info);
			return -EINVAL;
		}

		source_info = find_mixout_source_info(mixed_data_info, mixin);
		if (source_info)
			memset(source_info, 0, sizeof(*source_info));
	}

	mixed_data_info_release(mixed_data_info);

	return 0;
}

static int mixin_get_attribute(struct comp_dev *dev, uint32_t type, void *value)
{
	struct mixin_data *md = comp_get_drvdata(dev);

	switch (type) {
	case COMP_ATTR_BASE_CONFIG:
		*(struct ipc4_base_module_cfg *)value = md->base_cfg;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int mixout_get_attribute(struct comp_dev *dev, uint32_t type, void *value)
{
	struct mixout_data *md = comp_get_drvdata(dev);

	switch (type) {
	case COMP_ATTR_BASE_CONFIG:
		*(struct ipc4_base_module_cfg *)value = md->base_cfg;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int mixin_set_large_config(struct comp_dev *dev, uint32_t param_id, bool first_block,
				  bool last_block, uint32_t data_offset_or_size, char *data)
{
	struct ipc4_mixer_mode_config *cfg;
	struct mixin_data *mixin_data;
	int i;
	uint32_t sink_index;
	uint16_t gain;

	if (param_id != IPC4_MIXER_MODE) {
		comp_err(dev, "mixin_set_large_config() unsupported param_id: %u", param_id);
		return -EINVAL;
	}

	if (!(first_block && last_block)) {
		comp_err(dev, "mixin_set_large_config() data is expected to be sent as one chunk");
		return -EINVAL;
	}

	/* for a single chunk data, data_offset_or_size is size */
	if (data_offset_or_size < sizeof(struct ipc4_mixer_mode_config)) {
		comp_err(dev, "mixin_set_large_config() too small data size: %u",
			 data_offset_or_size);
		return -EINVAL;
	}

	if (data_offset_or_size > SOF_IPC_MSG_MAX_SIZE) {
		comp_err(dev, "mixin_set_large_config() too large data size: %u",
			 data_offset_or_size);
		return -EINVAL;
	}

	cfg = (struct ipc4_mixer_mode_config *)data;

	if (cfg->mixer_mode_config_count < 1 || cfg->mixer_mode_config_count > MIXIN_MAX_SINKS) {
		comp_err(dev, "mixin_set_large_config() invalid mixer_mode_config_count: %u",
			 cfg->mixer_mode_config_count);
		return -EINVAL;
	}

	if (sizeof(struct ipc4_mixer_mode_config) +
	    (cfg->mixer_mode_config_count - 1) * sizeof(struct ipc4_mixer_mode_sink_config) >
	    data_offset_or_size) {
		comp_err(dev, "mixin_set_large_config() unexpected data size: %u",
			 data_offset_or_size);
		return -EINVAL;
	}

	mixin_data = comp_get_drvdata(dev);

	for (i = 0; i < cfg->mixer_mode_config_count; i++) {
		sink_index = cfg->mixer_mode_sink_configs[i].output_queue_id;
		if (sink_index >= MIXIN_MAX_SINKS) {
			comp_err(dev, "mixin_set_large_config() invalid sink index: %u",
				 sink_index);
			return -EINVAL;
		}

		gain = cfg->mixer_mode_sink_configs[i].gain;
		if (gain > IPC4_MIXIN_UNITY_GAIN)
			gain = IPC4_MIXIN_UNITY_GAIN;
		mixin_data->sink_config[sink_index].gain = gain;

		comp_dbg(dev, "mixin_set_large_config() gain 0x%x will be applied for sink %u",
			 gain, sink_index);

		if (cfg->mixer_mode_sink_configs[i].mixer_mode ==
			IPC4_MIXER_CHANNEL_REMAPPING_MODE) {
			uint32_t channel_count =
				cfg->mixer_mode_sink_configs[i].output_channel_count;
			if (channel_count < 1 || channel_count > 8) {
				comp_err(dev, "Invalid output_channel_count %u for sink %u",
					 channel_count, sink_index);
				return -EINVAL;
			}

			mixin_data->sink_config[sink_index].output_channel_count = channel_count;
			mixin_data->sink_config[sink_index].output_channel_map =
				cfg->mixer_mode_sink_configs[i].output_channel_map;

			comp_dbg(dev, "output_channel_count: %u, chmap: 0x%x for sink: %u",
				 channel_count,
				 mixin_data->sink_config[sink_index].output_channel_map,
				 sink_index);
		}

		mixin_data->sink_config[sink_index].mixer_mode =
			cfg->mixer_mode_sink_configs[i].mixer_mode;
	}

	return 0;
}

static const struct comp_driver comp_mixin = {
	.uid	= SOF_RT_UUID(mixin_uuid),
	.tctx	= &mixin_tr,
	.ops	= {
		.create  = mixin_new,
		.free    = mixin_free,
		.params  = mixin_params,
		.prepare = mixin_prepare,
		.trigger = mixinout_trigger,
		.copy    = mixin_copy,
		.reset   = mixin_reset,
		.get_attribute = mixin_get_attribute,
		.set_large_config = mixin_set_large_config,
	},
};

static SHARED_DATA struct comp_driver_info comp_mixin_info = {
	.drv = &comp_mixin,
};

UT_STATIC void sys_comp_mixin_init(void)
{
	comp_register(platform_shared_get(&comp_mixin_info,
					  sizeof(comp_mixin_info)));
}

DECLARE_MODULE(sys_comp_mixin_init);

static const struct comp_driver comp_mixout = {
	.type	= SOF_COMP_MIXER,
	.uid	= SOF_RT_UUID(mixout_uuid),
	.tctx	= &mixout_tr,
	.ops	= {
		.create  = mixout_new,
		.free    = mixout_free,
		.params  = mixout_params,
		.prepare = mixout_prepare,
		.trigger = mixinout_trigger,
		.copy    = mixout_copy,
		.bind    = mixout_bind,
		.unbind  = mixout_unbind,
		.reset   = mixout_reset,
		.get_attribute = mixout_get_attribute,
	},
};

static SHARED_DATA struct comp_driver_info comp_mixout_info = {
	.drv = &comp_mixout,
};

UT_STATIC void sys_comp_mixer_init(void)
{
	comp_register(platform_shared_get(&comp_mixout_info,
					  sizeof(comp_mixout_info)));
}

DECLARE_MODULE(sys_comp_mixer_init);
