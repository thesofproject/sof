/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2020 Intel Corporation. All rights reserved.
 *
 * Author: Karol Trzcinski <karolx.trzcinski@linux.intel.com>
 */

/**
 * audio_stream is kind of circular buffer with information about data format
 * and buffer size. Audio processing functions should work on this component.
 * This component is not responsible for memory menagement for himself,
 * it is a role of highly coupled comp_buffer or dma component as usual.
 */

#ifndef __SOF_AUDIO_AUDIO_STREAM_H__
#define __SOF_AUDIO_AUDIO_STREAM_H__

#include <sof/audio/format.h>
#include <sof/debug/panic.h>
#include <sof/math/numbers.h>
#include <sof/lib/alloc.h>
#include <sof/lib/cache.h>
#include <ipc/stream.h>
#include <config.h>
#include <stdint.h>

/* audio circular stream */
struct audio_stream {
	/* runtime data */
	uint32_t size;	/* runtime buffer size in bytes (period multiple) */
	uint32_t avail;		/* available bytes for reading */
	uint32_t free;		/* free bytes for writing */
	void *w_ptr;		/* buffer write pointer */
	void *r_ptr;		/* buffer read position */
	void *addr;		/* buffer base address */
	void *end_addr;		/* buffer end address */

	/* runtime stream params */
	enum sof_ipc_frame frame_fmt;	/**< sample data format */
	uint32_t rate;		/**< number of data frames per second [Hz] */
	uint16_t channels;	/**< number of samples in each frame */
};

#define audio_stream_read_frag(buffer, idx, size) \
	audio_stream_get_frag(buffer, buffer->r_ptr, idx, size)

#define audio_stream_read_frag_s16(buffer, idx) \
	audio_stream_get_frag(buffer, buffer->r_ptr, idx, sizeof(int16_t))

#define audio_stream_read_frag_s32(buffer, idx) \
	audio_stream_get_frag(buffer, buffer->r_ptr, idx, sizeof(int32_t))

#define audio_stream_write_frag(buffer, idx, size) \
	audio_stream_get_frag(buffer, buffer->w_ptr, idx, size)

#define audio_stream_write_frag_s16(buffer, idx) \
	audio_stream_get_frag(buffer, buffer->w_ptr, idx, sizeof(int16_t))

#define audio_stream_write_frag_s32(buffer, idx) \
	audio_stream_get_frag(buffer, buffer->w_ptr, idx, sizeof(int32_t))

#define audio_stream_get_frag(buffer, ptr, idx, sample_size) \
	audio_stream_wrap(buffer, (char *)(ptr) + ((idx) * (sample_size)))

static inline int audio_stream_set_params(struct audio_stream *buffer,
					  struct sof_ipc_stream_params *params)
{
	if (!params)
		return -EINVAL;

	buffer->frame_fmt = params->frame_fmt;
	buffer->rate = params->rate;
	buffer->channels = params->channels;

	return 0;
}

static inline void *audio_stream_wrap(const struct audio_stream *buffer,
				      void *ptr)
{
	if (ptr >= buffer->end_addr)
		ptr = (char *)buffer->addr +
			((char *)ptr - (char *)buffer->end_addr);

	return ptr;
}

/* get the max number of bytes that can be copied between sink and source */
static inline int audio_stream_can_copy_bytes(const struct audio_stream *source,
					      const struct audio_stream *sink,
					      uint32_t bytes)
{
	/* check for underrun */
	if (source->avail < bytes)
		return -1;

	/* check for overrun */
	if (sink->free < bytes)
		return 1;

	/* we are good to copy */
	return 0;
}

static inline uint32_t
audio_stream_get_copy_bytes(const struct audio_stream *source,
			    const struct audio_stream *sink)
{
	if (source->avail > sink->free)
		return sink->free;
	else
		return source->avail;
}

/**
 * Calculates period size in bytes based on component stream's parameters.
 * @param buf Component buffer.
 * @return Period size in bytes.
 */
static inline uint32_t audio_stream_frame_bytes(const struct audio_stream *buf)
{
	return get_frame_bytes(buf->frame_fmt, buf->channels);
}

/**
 * Calculates sample size in bytes based on component stream's parameters.
 * @param buf Component buffer.
 * @return Size of sample in bytes.
 */
static inline uint32_t audio_stream_sample_bytes(const struct audio_stream *buf)
{
	return get_sample_bytes(buf->frame_fmt);
}

/**
 * Calculates period size in bytes based on component stream's parameters.
 * @param buf Component buffer.
 * @param frames Number of processing frames.
 * @return Period size in bytes.
 */
static inline uint32_t audio_stream_period_bytes(const struct audio_stream *buf,
						 uint32_t frames)
{
	return frames * audio_stream_frame_bytes(buf);
}

static inline uint32_t
audio_stream_avail_frames(const struct audio_stream *source,
			  const struct audio_stream *sink)
{
	uint32_t src_frames = source->avail / audio_stream_frame_bytes(source);
	uint32_t sink_frames = sink->free / audio_stream_frame_bytes(sink);

	return MIN(src_frames, sink_frames);
}

/* called only by a comp_buffer procedures */
static inline void audio_stream_produce(struct audio_stream *buffer,
					uint32_t bytes)
{
	buffer->w_ptr = audio_stream_wrap(buffer,
					  (char *)buffer->w_ptr + bytes);

	/* "overwrite" old data in circular wrap case */
	if (bytes > buffer->free)
		buffer->r_ptr = buffer->w_ptr;

	/* calculate available bytes */
	if (buffer->r_ptr < buffer->w_ptr)
		buffer->avail = (char *)buffer->w_ptr - (char *)buffer->r_ptr;
	else if (buffer->r_ptr == buffer->w_ptr)
		buffer->avail = buffer->size; /* full */
	else
		buffer->avail = buffer->size -
			((char *)buffer->r_ptr - (char *)buffer->w_ptr);

	/* calculate free bytes */
	buffer->free = buffer->size - buffer->avail;
}

/* called only by a comp_buffer procedures */
static inline void audio_stream_consume(struct audio_stream *buffer,
					uint32_t bytes)
{
	buffer->r_ptr = audio_stream_wrap(buffer,
					  (char *)buffer->r_ptr + bytes);

	/* calculate available bytes */
	if (buffer->r_ptr < buffer->w_ptr)
		buffer->avail = (char *)buffer->w_ptr - (char *)buffer->r_ptr;
	else if (buffer->r_ptr == buffer->w_ptr)
		buffer->avail = 0; /* empty */
	else
		buffer->avail = buffer->size -
			((char *)buffer->r_ptr - (char *)buffer->w_ptr);

	/* calculate free bytes */
	buffer->free = buffer->size - buffer->avail;
}

static inline void audio_stream_reset(struct audio_stream *buffer)
{
	/* reset read and write pointer to buffer bas */
	buffer->w_ptr = buffer->addr;
	buffer->r_ptr = buffer->addr;

	/* free space is buffer size */
	buffer->free = buffer->size;

	/* there are no avail samples at reset */
	buffer->avail = 0;
}

static inline void audio_stream_init(struct audio_stream *buffer,
				     void *buff_addr, uint32_t size)
{
	buffer->size = size;
	buffer->addr = buff_addr;
	buffer->end_addr = (char *)buffer->addr + size;
	audio_stream_reset(buffer);
}

static inline void audio_stream_invalidate(struct audio_stream *buffer,
					   uint32_t bytes)
{
	uint32_t head_size = bytes;
	uint32_t tail_size = 0;

	/* check for potential wrap */
	if ((char *)buffer->r_ptr + bytes > (char *)buffer->end_addr) {
		head_size = (char *)buffer->end_addr - (char *)buffer->r_ptr;
		tail_size = bytes - head_size;
	}

	dcache_invalidate_region(buffer->r_ptr, head_size);
	if (tail_size)
		dcache_invalidate_region(buffer->addr, tail_size);
}

static inline void audio_stream_writeback(struct audio_stream *buffer,
					  uint32_t bytes)
{
	uint32_t head_size = bytes;
	uint32_t tail_size = 0;

	/* check for potential wrap */
	if ((char *)buffer->w_ptr + bytes > (char *)buffer->end_addr) {
		head_size = (char *)buffer->end_addr - (char *)buffer->w_ptr;
		tail_size = bytes - head_size;
	}

	dcache_writeback_region(buffer->w_ptr, head_size);
	if (tail_size)
		dcache_writeback_region(buffer->addr, tail_size);
}

static inline void audio_stream_copy(const struct audio_stream *source,
				     uint32_t ioffset_bytes,
				     struct audio_stream *sink,
				     uint32_t ooffset_bytes, uint32_t bytes)
{
	void *src = audio_stream_wrap(source,
				      (char *)source->r_ptr + ioffset_bytes);
	void *snk = audio_stream_wrap(sink,
				      (char *)sink->w_ptr + ooffset_bytes);
	uint32_t bytes_src;
	uint32_t bytes_snk;
	uint32_t bytes_copied;
	int ret;

	while (bytes) {
		bytes_src = (char *)source->end_addr - (char *)src;
		bytes_snk = (char *)sink->end_addr - (char *)snk;
		bytes_copied = MIN(bytes, MIN(bytes_src, bytes_snk));

		ret = memcpy_s(snk, bytes_snk, src, bytes_copied);
		assert(!ret);

		bytes -= bytes_copied;
		src = (char *)src + bytes_copied;
		snk = (char *)snk + bytes_copied;

		src = audio_stream_wrap(source, src);
		snk = audio_stream_wrap(sink, snk);
	}
}

#if CONFIG_FORMAT_S16LE

static inline void audio_stream_copy_s16(const struct audio_stream *source,
					 uint32_t ioffset,
					 struct audio_stream *sink,
					 uint32_t ooffset, uint32_t samples)
{
	const int ssize = sizeof(int16_t);

	audio_stream_copy(source, ioffset * ssize, sink, ooffset * ssize,
			  samples * ssize);
}

#endif /* CONFIG_FORMAT_S16LE */

#if CONFIG_FORMAT_S24LE || CONFIG_FORMAT_S32LE || CONFIG_FORMAT_FLOAT

static inline void audio_stream_copy_s32(const struct audio_stream *source,
					 uint32_t ioffset,
					 struct audio_stream *sink,
					 uint32_t ooffset, uint32_t samples)
{
	const int ssize = sizeof(int32_t);

	audio_stream_copy(source, ioffset * ssize, sink, ooffset * ssize,
			  samples * ssize);
}

#endif /* CONFIG_FORMAT_S24LE || CONFIG_FORMAT_S32LE || CONFIG_FORMAT_FLOAT */

#endif /* __SOF_AUDIO_AUDIO_STREAM_H__ */
