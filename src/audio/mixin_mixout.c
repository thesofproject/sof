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
#include <sof/lib/alloc.h>
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
 * ensuring all connected mixers have mixed their data into mixout sink buffer.
 * So for each connected mixin, mixout keeps knowledge of data already consumed
 * by mixin but not yet produced in mixout.
 */
struct mixout_source_info {
	struct comp_dev *mixin;
	uint32_t consumed_yet_not_produced_bytes;
};

/*
 * Data used by both mixin and mixout: number of currently mixed bytes in
 * mixout sink buffer and each mixin consumed data amount (and so mixout should
 * produce the appropreate amount of data).
 * Can be accessed from different cores.
 */
struct mixed_data_info {
	struct coherent c;
	uint32_t mixed_bytes;
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

/* mixin component private data */
struct mixin_data {
	/* Must be the 1st field, function ipc4_comp_get_base_module_cfg casts components
	 * private data as ipc4_base_module_cfg!
	 */
	struct ipc4_base_module_cfg base_cfg;
	void (*mix_func)(struct audio_stream __sparse_cache *sink, uint32_t start,
			 uint32_t mixed_bytes, const struct audio_stream __sparse_cache *source,
			 uint32_t size, uint16_t gain);

	/* Gain as described in struct ipc4_mixer_mode_sink_config */
	uint16_t gain[MIXIN_MAX_SINKS];
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

	for (i = 0; i < MIXIN_MAX_SINKS; i++)
		md->gain[i] = IPC4_MIXIN_UNITY_GAIN;

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

	md->mixed_data_info = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM,
				      sizeof(struct mixed_data_info));
	if (!md->mixed_data_info) {
		rfree(md);
		rfree(dev);
		return NULL;
	}

	coherent_init(md->mixed_data_info, c);
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

	coherent_free(mixout_data->mixed_data_info, c);
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

static void audio_stream_bytes_copy(struct audio_stream __sparse_cache *dst_stream, void *dst,
				    const struct audio_stream __sparse_cache *src_stream,
				    void *src, uint32_t size)
{
	int n;
	uint8_t *pdst = (uint8_t *)dst;
	uint8_t *psrc = (uint8_t *)src;

	while (size) {
		n = MIN(audio_stream_bytes_without_wrap(dst_stream, pdst), size);
		n = MIN(audio_stream_bytes_without_wrap(src_stream, psrc), n);
		memcpy_s(pdst, n, psrc, n);
		size -= n;
		pdst = audio_stream_wrap(dst_stream, pdst + n);
		psrc = audio_stream_wrap(src_stream, psrc + n);
	}
}

#if CONFIG_FORMAT_S16LE
static void mix_s16(struct audio_stream __sparse_cache *sink, uint32_t start, uint32_t mixed_bytes,
		    const struct audio_stream __sparse_cache *source, uint32_t size, uint16_t gain)
{
	int16_t *dest, *src;
	uint32_t bytes_to_mix, bytes_to_copy;
	int i, n;

	dest = audio_stream_wrap(sink, (uint8_t *)sink->w_ptr + start);
	src = source->r_ptr;

	assert(mixed_bytes >= start);
	bytes_to_mix = MIN(mixed_bytes - start, size);
	bytes_to_copy = size - bytes_to_mix;

	while (bytes_to_mix != 0) {
		n = MIN(audio_stream_bytes_without_wrap(sink, dest), bytes_to_mix);
		n = MIN(audio_stream_bytes_without_wrap(source, src), n);
		bytes_to_mix -= n;
		n >>= 1;	/* divide by 2 */
		if (gain == IPC4_MIXIN_UNITY_GAIN) {
			for (i = 0; i < n; i++) {
				*dest = sat_int16((int32_t)*dest + (int32_t)*src);
				src++;
				dest++;
			}
		} else {
			for (i = 0; i < n; i++) {
				*dest = sat_int16((int32_t)*dest +
						  q_mults_16x16(*src, gain,
								IPC4_MIXIN_GAIN_SHIFT));
				src++;
				dest++;
			}
		}

		dest = audio_stream_wrap(sink, dest);
		src = audio_stream_wrap(source, src);
	}

	if (gain == IPC4_MIXIN_UNITY_GAIN) {
		audio_stream_bytes_copy(sink, dest, source, src, bytes_to_copy);
	} else {
		while (bytes_to_copy != 0) {
			n = MIN(audio_stream_bytes_without_wrap(sink, dest), bytes_to_copy);
			n = MIN(audio_stream_bytes_without_wrap(source, src), n);
			bytes_to_copy -= n;
			n >>= 1;	/* divide by 2 */
			for (i = 0; i < n; i++) {
				*dest = (int16_t)q_mults_16x16(*src, gain, IPC4_MIXIN_GAIN_SHIFT);
				src++;
				dest++;
			}

			dest = audio_stream_wrap(sink, dest);
			src = audio_stream_wrap(source, src);
		}
	}
}
#endif	/* CONFIG_FORMAT_S16LE */

#if CONFIG_FORMAT_S24LE
static void mix_s24(struct audio_stream __sparse_cache *sink, uint32_t start, uint32_t mixed_bytes,
		    const struct audio_stream __sparse_cache *source, uint32_t size, uint16_t gain)
{
	int32_t *dest, *src;
	uint32_t bytes_to_mix, bytes_to_copy;
	int i, n;

	dest = audio_stream_wrap(sink, (uint8_t *)sink->w_ptr + start);
	src = source->r_ptr;

	assert(mixed_bytes >= start);
	bytes_to_mix = MIN(mixed_bytes - start, size);
	bytes_to_copy = size - bytes_to_mix;

	while (bytes_to_mix != 0) {
		n = MIN(audio_stream_bytes_without_wrap(sink, dest), bytes_to_mix);
		n = MIN(audio_stream_bytes_without_wrap(source, src), n);
		bytes_to_mix -= n;
		n >>= 2;	/* divide by 4 */
		if (gain == IPC4_MIXIN_UNITY_GAIN) {
			for (i = 0; i < n; i++) {
				*dest = sat_int24(sign_extend_s24(*dest) + sign_extend_s24(*src));
				src++;
				dest++;
			}
		} else {
			for (i = 0; i < n; i++) {
				*dest = sat_int24(sign_extend_s24(*dest) +
				  (int32_t)q_mults_32x32(sign_extend_s24(*src),
							 gain, IPC4_MIXIN_GAIN_SHIFT));
				src++;
				dest++;
			}
		}

		dest = audio_stream_wrap(sink, dest);
		src = audio_stream_wrap(source, src);
	}

	if (gain == IPC4_MIXIN_UNITY_GAIN) {
		audio_stream_bytes_copy(sink, dest, source, src, bytes_to_copy);
	} else {
		while (bytes_to_copy != 0) {
			n = MIN(audio_stream_bytes_without_wrap(sink, dest), bytes_to_copy);
			n = MIN(audio_stream_bytes_without_wrap(source, src), n);
			bytes_to_copy -= n;
			n >>= 2;	/* divide by 4 */
			for (i = 0; i < n; i++) {
				*dest = (int32_t)q_mults_32x32(sign_extend_s24(*src),
							       gain, IPC4_MIXIN_GAIN_SHIFT);
				src++;
				dest++;
			}

			dest = audio_stream_wrap(sink, dest);
			src = audio_stream_wrap(source, src);
		}
	}
}
#endif	/* CONFIG_FORMAT_S24LE */

#if CONFIG_FORMAT_S32LE
static void mix_s32(struct audio_stream __sparse_cache *sink, uint32_t start, uint32_t mixed_bytes,
		    const struct audio_stream __sparse_cache *source, uint32_t size, uint16_t gain)
{
	int32_t *dest, *src;
	uint32_t bytes_to_mix, bytes_to_copy;
	int i, n;

	dest = audio_stream_wrap(sink, (uint8_t *)sink->w_ptr + start);
	src = source->r_ptr;

	assert(mixed_bytes >= start);
	bytes_to_mix = MIN(mixed_bytes - start, size);
	bytes_to_copy = size - bytes_to_mix;

	while (bytes_to_mix != 0) {
		n = MIN(audio_stream_bytes_without_wrap(sink, dest), bytes_to_mix);
		n = MIN(audio_stream_bytes_without_wrap(source, src), n);
		bytes_to_mix -= n;
		n >>= 2;	/* divide by 4 */
		if (gain == IPC4_MIXIN_UNITY_GAIN) {
			for (i = 0; i < n; i++) {
				*dest = sat_int32((int64_t)*dest + (int64_t)*src);
				src++;
				dest++;
			}
		} else {
			for (i = 0; i < n; i++) {
				*dest = sat_int32((int64_t)*dest +
				  q_mults_32x32(*src, gain, IPC4_MIXIN_GAIN_SHIFT));
				src++;
				dest++;
			}
		}

		dest = audio_stream_wrap(sink, dest);
		src = audio_stream_wrap(source, src);
	}

	if (gain == IPC4_MIXIN_UNITY_GAIN) {
		audio_stream_bytes_copy(sink, dest, source, src, bytes_to_copy);
	} else {
		while (bytes_to_copy != 0) {
			n = MIN(audio_stream_bytes_without_wrap(sink, dest), bytes_to_copy);
			n = MIN(audio_stream_bytes_without_wrap(source, src), n);
			bytes_to_copy -= n;
			n >>= 2;	/* divide by 4 */
			for (i = 0; i < n; i++) {
				*dest = (int32_t)q_mults_32x32(*src, gain, IPC4_MIXIN_GAIN_SHIFT);
				src++;
				dest++;
			}

			dest = audio_stream_wrap(sink, dest);
			src = audio_stream_wrap(source, src);
		}
	}
}
#endif	/* CONFIG_FORMAT_S32LE */

/* mix silence into stream, i.e. set not yet mixed data in stream to zero */
static void silence(struct audio_stream __sparse_cache *stream, uint32_t start,
		    uint32_t mixed_bytes, uint32_t size)
{
	uint32_t skip_mixed;
	uint8_t *ptr;
	int n;

	assert(mixed_bytes >= start);
	skip_mixed = mixed_bytes - start;

	if (size <= skip_mixed)
		return;

	size -= skip_mixed;
	ptr = audio_stream_wrap(stream, (uint8_t *)stream->w_ptr + mixed_bytes);

	while (size) {
		n = MIN(audio_stream_bytes_without_wrap(stream, ptr), size);
		memset(ptr, 0, n);
		size -= n;
		ptr = audio_stream_wrap(stream, ptr + n);
	}
}

/* Most of the mixing is done here on mixin side. mixin mixes its source data
 * into each connected mixout sink buffer. Basically, if mixout sink buffer has
 * no data, mixin copies its source data into mixout sink buffer. If moxout sink
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
	uint32_t source_avail_bytes, sinks_free_bytes;
	struct list_item *blist;
	uint32_t bytes_to_consume_from_source_buf;
	uint32_t bytes_to_copy;

	comp_dbg(dev, "mixin_copy()");

	mixin_data = comp_get_drvdata(dev);

	source = list_first_item(&dev->bsource_list, struct comp_buffer, sink_list);
	source_c = buffer_acquire(source);

	source_avail_bytes = audio_stream_get_avail_bytes(&source_c->stream);
	sinks_free_bytes = INT32_MAX;

	/* first, let's find out how many bytes can be now processed --
	 * it is a nimimal value among bytes available in source buffer
	 * and bytes free in each connected mixout sink buffer.
	 */
	list_for_item(blist, &dev->bsink_list) {
		struct comp_buffer *unused_in_between_buf;
		struct comp_dev *mixout;
		struct comp_buffer *sink;
		struct mixout_data *mixout_data;
		struct mixed_data_info __sparse_cache *mixed_data_info;
		struct mixout_source_info *src_info;
		struct comp_buffer __sparse_cache *sink_c;
		uint32_t stream_free_bytes;

		/* unused buffer between mixin and mixout */
		unused_in_between_buf = buffer_from_list(blist, struct comp_buffer,
							 PPL_DIR_DOWNSTREAM);
		mixout = buffer_get_comp(unused_in_between_buf, PPL_DIR_DOWNSTREAM);
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

		stream_free_bytes = audio_stream_get_free_bytes(&sink_c->stream);

		/* mixout sink buffer may still have not yet produced data -- data
		 * consumed and written there by mixin on previous mixin_copy() run.
		 * We do NOT want to overwrite that data.
		 */
		assert(stream_free_bytes >= src_info->consumed_yet_not_produced_bytes);
		sinks_free_bytes = MIN(sinks_free_bytes,
				       stream_free_bytes -
				       src_info->consumed_yet_not_produced_bytes);

		buffer_release(sink_c);
		mixed_data_info_release(mixed_data_info);
	}

	bytes_to_consume_from_source_buf = 0;
	if (source_avail_bytes > 0) {
		bytes_to_copy = MIN(source_avail_bytes, sinks_free_bytes);
		bytes_to_consume_from_source_buf = bytes_to_copy;
		buffer_stream_invalidate(source_c, bytes_to_copy);
	} else {
		/* if source does not produce any data -- do NOT stop mixing but generate
		 * silence as that source output.
		 *
		 * here bytes_to_copy is silence size.
		 */
		bytes_to_copy = MIN(audio_stream_period_bytes(&source_c->stream, dev->frames),
				    sinks_free_bytes);
	}

	/* iterate over all connected mixouts and mix source data into each mixout sink buffer */
	list_for_item(blist, &dev->bsink_list) {
		struct comp_buffer *unused_in_between_buf;
		struct comp_dev *mixout;
		struct comp_buffer *sink;
		struct mixout_data *mixout_data;
		struct mixed_data_info __sparse_cache *mixed_data_info;
		struct mixout_source_info *src_info;
		uint32_t start;
		struct comp_buffer __sparse_cache *sink_c;

		unused_in_between_buf = buffer_from_list(blist, struct comp_buffer,
							 PPL_DIR_DOWNSTREAM);
		mixout = buffer_get_comp(unused_in_between_buf, PPL_DIR_DOWNSTREAM);
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
		 * Normally start would be 0 unless mixout pipeline has serious
		 * performance problems with processing data on time in mixout.
		 */
		start = src_info->consumed_yet_not_produced_bytes;
		assert(sinks_free_bytes >= start);

		sink_c = buffer_acquire(sink);

		/* in case mixout and mixin source are in different states -- generate
		 * silence instead of that source data
		 */
		if (source_avail_bytes == 0 ||
		    comp_get_state(dev, source_c->source) != comp_get_state(dev, mixout)) {
			/* generate silence */
			silence(&sink_c->stream, start, mixed_data_info->mixed_bytes,
				bytes_to_copy);
		} else {
			uint16_t sink_index = IPC4_SRC_QUEUE_ID(unused_in_between_buf->id);

			if (sink_index >= MIXIN_MAX_SINKS) {
				comp_err(dev, "Sink index out of range: %u, max sinks count: %u",
					 (uint32_t)sink_index, MIXIN_MAX_SINKS);
				buffer_release(sink_c);
				mixed_data_info_release(mixed_data_info);
				buffer_release(source_c);
				return -EINVAL;
			}

			/* basically, if sink buffer has no data -- copy source data there, if
			 * sink buffer has some data (written by another mixin) mix that data
			 * with source data.
			 */
			mixin_data->mix_func(&sink_c->stream, start, mixed_data_info->mixed_bytes,
					     &source_c->stream, bytes_to_copy,
					     mixin_data->gain[sink_index]);
		}

		/* it would be better to writeback (sink_c + start, bytes_to_copy)
		 * memory region, but seems there is no appropreate API. Anyway, start
		 * would be 0 most of the time.
		 */
		buffer_stream_writeback(sink_c, bytes_to_copy + start);
		buffer_release(sink_c);

		src_info->consumed_yet_not_produced_bytes += bytes_to_copy;

		if (bytes_to_copy + start > mixed_data_info->mixed_bytes)
			mixed_data_info->mixed_bytes = bytes_to_copy + start;

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
	uint32_t bytes_to_produce = INT32_MAX;

	comp_dbg(dev, "mixout_copy()");

	mixout_data = comp_get_drvdata(dev);
	mixed_data_info = mixed_data_info_acquire(mixout_data->mixed_data_info);

	/* iterate over all connected mixins to find minimal value of bytes they consumed
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

		bytes_to_produce = MIN(bytes_to_produce, src_info->consumed_yet_not_produced_bytes);
	}

	if (bytes_to_produce > 0 && bytes_to_produce < INT32_MAX) {
		int i;
		struct comp_buffer *sink;
		struct comp_buffer __sparse_cache *sink_c;

		for (i = 0; i < MIXOUT_MAX_SOURCES; i++) {
			if (mixed_data_info->source_info[i].mixin)
				mixed_data_info->source_info[i].consumed_yet_not_produced_bytes -=
						bytes_to_produce;
		}

		assert(mixed_data_info->mixed_bytes >= bytes_to_produce);
		mixed_data_info->mixed_bytes -= bytes_to_produce;

		sink = list_first_item(&dev->bsink_list, struct comp_buffer, source_list);
		sink_c = buffer_acquire(sink);
		/* writeback is already done in mixin while mixing */
		comp_update_buffer_produce(sink_c, bytes_to_produce);
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
	mixin_data->mix_func = NULL;

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
	fmt = sink_c->stream.frame_fmt;
	buffer_release(sink_c);

	/* currently inactive so setup mixer */
	switch (fmt) {
#if CONFIG_FORMAT_S16LE
	case SOF_IPC_FRAME_S16_LE:
		md->mix_func = mix_s16;
		break;
#endif /* CONFIG_FORMAT_S16LE */
#if CONFIG_FORMAT_S24LE
	case SOF_IPC_FRAME_S24_4LE:
		md->mix_func = mix_s24;
		break;
#endif /* CONFIG_FORMAT_S24LE */
#if CONFIG_FORMAT_S32LE
	case SOF_IPC_FRAME_S32_LE:
		md->mix_func = mix_s32;
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
			coherent_shared(md->mixed_data_info, c);
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

/* params are derived from base config for ipc4 path */
static int mixinout_common_params(struct comp_dev *dev,
				  struct sof_ipc_stream_params *params,
				  const struct ipc4_base_module_cfg *base_cfg)
{
	struct list_item *blist;
	int ret;

	memset(params, 0, sizeof(*params));
	params->channels = base_cfg->audio_fmt.channels_count;
	params->rate = base_cfg->audio_fmt.sampling_frequency;
	params->sample_container_bytes = base_cfg->audio_fmt.depth;
	params->sample_valid_bytes = base_cfg->audio_fmt.valid_bit_depth;
	params->frame_fmt = dev->ipc_config.frame_fmt;
	params->buffer_fmt = base_cfg->audio_fmt.interleaving_style;
	params->buffer.size = base_cfg->ibs;

	/* update each sink format based on base_cfg initialized by
	 * host driver. There is no hw_param ipc message for ipc4, instead
	 * all module params are built into module initialization data by
	 * host driver based on runtime hw_params and topology setting.
	 *
	 * This might be not necessary for mixin as buffers between mixin and
	 * mixout are not used (mixin writes data directly to mixout sink).
	 * But let's keep buffer setup just in case.
	 */

	list_for_item(blist, &dev->bsink_list) {
		struct comp_buffer *sink;
		struct comp_buffer __sparse_cache *sink_c;
		int i;

		sink = buffer_from_list(blist, struct comp_buffer, PPL_DIR_DOWNSTREAM);
		sink_c = buffer_acquire(sink);

		sink_c->stream.channels = base_cfg->audio_fmt.channels_count;
		sink_c->stream.rate = base_cfg->audio_fmt.sampling_frequency;
		audio_stream_fmt_conversion(base_cfg->audio_fmt.depth,
					    base_cfg->audio_fmt.valid_bit_depth,
					    &sink_c->stream.frame_fmt,
					    &sink_c->stream.valid_sample_fmt,
					    base_cfg->audio_fmt.s_type);

		sink_c->buffer_fmt = base_cfg->audio_fmt.interleaving_style;

		/* 8 ch stream is supported by ch_map and each channel
		 * is mapped by 4 bits. The first channel will be mapped
		 * by the bits of 0 ~ 3 and the 2th channel will be mapped
		 * by bits 4 ~ 7. The N channel will be mapped by bits
		 * N*4 ~ N*4 + 3
		 */
		for (i = 0; i < SOF_IPC_MAX_CHANNELS; i++)
			sink_c->chmap[i] = (base_cfg->audio_fmt.ch_map >> i * 4) & 0xf;

		buffer_release(sink_c);
	}

	ret = comp_verify_params(dev, BUFF_PARAMS_CHANNELS, params);
	if (ret < 0) {
		comp_err(dev, "mixinout_common_params(): comp_verify_params() failed!");
		return -EINVAL;
	}

	return 0;
}

static int mixin_params(struct comp_dev *dev,
			struct sof_ipc_stream_params *params)
{
	struct mixin_data *md;

	comp_dbg(dev, "mixin_params()");

	md = comp_get_drvdata(dev);
	return mixinout_common_params(dev, params, &md->base_cfg);
}

static int mixout_params(struct comp_dev *dev,
			 struct sof_ipc_stream_params *params)
{
	struct mixout_data *md;
	int ret;
	struct comp_buffer *sinkb;
	struct comp_buffer __sparse_cache *sink_c;
	uint32_t sink_period_bytes, sink_stream_size;

	comp_dbg(dev, "mixout_params()");

	md = comp_get_drvdata(dev);

	ret = mixinout_common_params(dev, params, &md->base_cfg);
	if (ret < 0)
		return ret;

	sinkb = list_first_item(&dev->bsink_list, struct comp_buffer,
				source_list);
	sink_c = buffer_acquire(sinkb);

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
		source_info->consumed_yet_not_produced_bytes = 0;
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
		mixed_data_info->mixed_bytes = 0;
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

	dcache_invalidate_region((__sparse_force void __sparse_cache *)data, data_offset_or_size);

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
		mixin_data->gain[sink_index] = gain;

		comp_dbg(dev, "mixin_set_large_config() gain 0x%x will be applied for sink %u",
			 gain, sink_index);
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
