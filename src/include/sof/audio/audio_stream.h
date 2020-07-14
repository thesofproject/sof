/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2020 Intel Corporation. All rights reserved.
 *
 * Author: Karol Trzcinski <karolx.trzcinski@linux.intel.com>
 */

/**
  * \file include/sof/audio/audio_stream.h
  * \brief Audio Stream API definition
  * \author Karol Trzcinski <karolx.trzcinski@linux.intel.com>
  */

#ifndef __SOF_AUDIO_AUDIO_STREAM_H__
#define __SOF_AUDIO_AUDIO_STREAM_H__

#include <sof/audio/format.h>
#include <sof/debug/panic.h>
#include <sof/math/numbers.h>
#include <sof/lib/alloc.h>
#include <sof/lib/cache.h>
#include <ipc/stream.h>

#include <stdbool.h>
#include <stdint.h>

/** \addtogroup audio_stream_api Audio Stream API
 *  @{
 */

/**
 * Audio stream is a circular buffer aware of audio format of the data
 * in the stream so provides API for reading and writing not only bytes,
 * but also samples and frames.
 *
 * Audio stream does not perform any memory allocations. A client (a component
 * stream or dma) must allocate the memory for the underlying data stream and
 * provide it to the initialization routine.
 *
 * Once the client is done with reading/writing the data, it must commit the
 * consumption/production and update the stream state by calling
 * audio_stream_consume()/audio_stream_produce() (just a single call following
 * series of reads/writes).
 */
struct audio_stream {
	/* runtime data */
	uint32_t size;	/**< Runtime stream size in bytes (period multiple) */
	uint32_t avail;	/**< Available bytes for reading */
	uint32_t free;	/**< Free bytes for writing */
	void *w_ptr;	/**< Stream write pointer */
	void *r_ptr;	/**< Stream read position */
	void *addr;	/**< Stream base address */
	void *end_addr;	/**< Stream end address */

	/* runtime stream params */
	enum sof_ipc_frame frame_fmt;	/**< Sample data format */
	uint32_t rate;		/**< Number of data frames per second [Hz] */
	uint16_t channels;	/**< Number of samples in each frame */

	bool overrun_permitted; /**< indicates whether overrun is permitted */
	bool underrun_permitted; /**< indicates whether underrun is permitted */
};

/**
 * Retrieves readable address of a sample at specified index (see versions of
 * this macro specialized for various sample types).
 * @param stream Stream.
 * @param idx Index of sample.
 * @param size Size of sample in bytes.
 * @return Pointer to the sample.
 *
 * Once the consumer finishes reading samples from the stream, it should
 * "commit" the operation and update the stream state by calling
 * audio_stream_consume().
 *
 * @note Components should call comp_update_stream_consume().
 *
 * @see audio_stream_get_frag().
 * @see audio_stream_consume().
 * @see comp_update_stream_consume().
 */
#define audio_stream_read_frag(stream, idx, size) \
	audio_stream_get_frag(stream, (stream)->r_ptr, idx, size)

/**
 * Retrieves readable address of a signed 16-bit sample at specified index.
 * @param stream Stream.
 * @param idx Index of sample.
 * @return Pointer to the sample.
 *
 * @see audio_stream_get_frag().
 */
#define audio_stream_read_frag_s16(stream, idx) \
	audio_stream_get_frag(stream, (stream)->r_ptr, idx, sizeof(int16_t))

/**
 * Retrieves readable address of a signed 32-bit sample at specified index.
 * @param stream Stream.
 * @param idx Index of sample.
 * @return Pointer to the sample.
 *
 * @see audio_stream_get_frag().
 */
#define audio_stream_read_frag_s32(stream, idx) \
	audio_stream_get_frag(stream, (stream)->r_ptr, idx, sizeof(int32_t))

/**
 * Retrieves writeable address of a sample at specified index (see versions of
 * this macro specialized for various sample types).
 * @param stream Stream.
 * @param idx Index of sample.
 * @param size Size of sample in bytes.
 * @return Pointer to the space for sample.
 *
 * Once the producer finishes writing samples to the stream, it should
 * "commit" the operation and update the stream state by calling
 * audio_stream_produce().
 *
 * @note Components should call comp_update_stream_produce().
 *
 * @see audio_stream_get_frag().
 * @see audio_stream_produce().
 * @see comp_update_stream_produce().
 */
#define audio_stream_write_frag(stream, idx, size) \
	audio_stream_get_frag(stream, (stream)->w_ptr, idx, size)

/**
 * Retrieves writeable address of a signed 16-bit sample at specified index.
 * @param stream Stream.
 * @param idx Index of sample.
 * @return Pointer to the space for sample.
 *
 * @see audio_stream_get_frag().
 */
#define audio_stream_write_frag_s16(stream, idx) \
	audio_stream_get_frag(stream, (stream)->w_ptr, idx, sizeof(int16_t))

/**
 * Retrieves writeable address of a signed 32-bit sample at specified index.
 * @param stream Stream.
 * @param idx Index of sample.
 * @return Pointer to the space for sample.
 *
 * @see audio_stream_get_frag().
 */
#define audio_stream_write_frag_s32(stream, idx) \
	audio_stream_get_frag(stream, (stream)->w_ptr, idx, sizeof(int32_t))

/**
 * Retrieves address of sample (space for sample) at specified index within
 * the stream. Index is interpreted as an offset relative to the specified
 * pointer, rollover is ensured.
 * @param stream Circular stream.
 * @param ptr Pointer to start from, it may be either read or write pointer.
 * @param idx Index of the sample.
 * @param sample_size Size of the sample in bytes.
 * @return Pointer to the sample.
 */
#define audio_stream_get_frag(stream, ptr, idx, sample_size) \
	audio_stream_wrap(stream, (char *)(ptr) + ((idx) * (sample_size)))

/**
 * Applies parameters to the stream.
 * @param stream Stream.
 * @param params Parameters (frame format, rate, number of channels).
 * @return 0 if succeeded, error code otherwise.
 */
static inline int audio_stream_set_params(struct audio_stream *stream,
					  struct sof_ipc_stream_params *params)
{
	if (!params)
		return -EINVAL;

	stream->frame_fmt = params->frame_fmt;
	stream->rate = params->rate;
	stream->channels = params->channels;

	return 0;
}

/**
 * Calculates period size in bytes based on component stream's parameters.
 * @param buf Component stream.
 * @return Period size in bytes.
 */
static inline uint32_t audio_stream_frame_bytes(const struct audio_stream *buf)
{
	return get_frame_bytes(buf->frame_fmt, buf->channels);
}

/**
 * Calculates sample size in bytes based on component stream's parameters.
 * @param buf Component stream.
 * @return Size of sample in bytes.
 */
static inline uint32_t audio_stream_sample_bytes(const struct audio_stream *buf)
{
	return get_sample_bytes(buf->frame_fmt);
}

/**
 * Calculates period size in bytes based on component stream's parameters.
 * @param buf Component stream.
 * @param frames Number of processing frames.
 * @return Period size in bytes.
 */
static inline uint32_t audio_stream_period_bytes(const struct audio_stream *buf,
						 uint32_t frames)
{
	return frames * audio_stream_frame_bytes(buf);
}

/**
 * Verifies the pointer and performs rollover when reached the end of
 * the stream.
 * @param stream Stream accessed by the pointer.
 * @param ptr Pointer
 * @return Pointer, adjusted if necessary.
 */
static inline void *audio_stream_wrap(const struct audio_stream *stream,
				      void *ptr)
{
	if (ptr >= stream->end_addr)
		ptr = (char *)stream->addr +
			((char *)ptr - (char *)stream->end_addr);

	return ptr;
}

/**
 * Calculates available data in bytes, handling underrun_permitted behaviour
 * @param stream Stream pointer
 * @return amount of data available for processing in bytes
 */
static inline uint32_t
audio_stream_get_avail_bytes(const struct audio_stream *stream)
{
	/*
	 * In case of underrun-permitted stream, report stream full instead of
	 * empty. This way, any data present in such stream is processed at
	 * regular pace, but stream will never be seen as completely empty by
	 * clients, and in turn will not cause underrun/XRUN.
	 */
	if (stream->underrun_permitted)
		return stream->avail != 0 ? stream->avail : stream->size;

	return stream->avail;
}

/**
 * Calculates available data in samples, handling underrun_permitted behaviour
 * @param stream Stream pointer
 * @return amount of data available for processing in samples
 */
static inline uint32_t
audio_stream_get_avail_samples(const struct audio_stream *stream)
{
	return audio_stream_get_avail_bytes(stream) /
		audio_stream_sample_bytes(stream);
}

/**
 * Calculates available data in frames, handling underrun_permitted behaviour
 * @param stream Stream pointer
 * @return amount of data available for processing in frames
 */
static inline uint32_t
audio_stream_get_avail_frames(const struct audio_stream *stream)
{
	return audio_stream_get_avail_bytes(stream) /
		audio_stream_frame_bytes(stream);
}

/**
 * Calculates free space in bytes, handling overrun_permitted behaviour
 * @param stream Stream pointer
 * @return amount of space free in bytes
 */
static inline uint32_t
audio_stream_get_free_bytes(const struct audio_stream *stream)
{
	/*
	 * In case of overrun-permitted stream, report stream empty instead of
	 * full. This way, if there's any actual free space for data it is
	 * processed at regular pace, but stream will never be seen as
	 * completely full by clients, and in turn will not cause overrun/XRUN.
	 */
	if (stream->overrun_permitted)
		return stream->free != 0 ? stream->free : stream->size;

	return stream->free;
}

/**
 * Calculates free space in samples, handling overrun_permitted behaviour
 * @param stream Stream pointer
 * @return amount of space free in samples
 */
static inline uint32_t
audio_stream_get_free_samples(const struct audio_stream *stream)
{
	return audio_stream_get_free_bytes(stream) /
		audio_stream_sample_bytes(stream);
}

/**
 * Calculates free space in frames, handling overrun_permitted behaviour
 * @param stream Stream pointer
 * @return amount of space free in frames
 */
static inline uint32_t
audio_stream_get_free_frames(const struct audio_stream *stream)
{
	return audio_stream_get_free_bytes(stream) /
		audio_stream_frame_bytes(stream);
}

/**
 *  Verifies whether specified number of bytes can be copied from source stream
 *  to sink stream.
 *  @param source Source stream.
 *  @param sink Sink stream.
 *  @param bytes Number of bytes to copy.
 *  @return 0 if there is enough data in source and enough free space in sink.
 *  @return 1 if there is not enough free space in sink.
 *  @return -1 if there is not enough data in source.
 */
static inline int audio_stream_can_copy_bytes(const struct audio_stream *source,
					      const struct audio_stream *sink,
					      uint32_t bytes)
{
	/* check for underrun */
	if (audio_stream_get_avail_bytes(source) < bytes)
		return -1;

	/* check for overrun */
	if (audio_stream_get_free_bytes(sink) < bytes)
		return 1;

	/* we are good to copy */
	return 0;
}

/**
 * Computes maximum number of bytes that can be copied from source stream to
 * sink stream, verifying number of bytes available in source vs. free space
 * available in sink.
 * @param source Source stream.
 * @param sink Sink stream.
 * @return Number of bytes.
 */
static inline uint32_t
audio_stream_get_copy_bytes(const struct audio_stream *source,
			    const struct audio_stream *sink)
{
	uint32_t avail = audio_stream_get_avail_bytes(source);
	uint32_t free = audio_stream_get_free_bytes(sink);

	if (avail > free)
		return free;
	else
		return avail;
}

/**
 * Computes maximum number of frames that can be copied from source stream
 * to sink stream, verifying number of available source frames vs. free
 * space available in sink.
 * @param source Source stream.
 * @param sink Sink stream.
 * @return Number of frames.
 */
static inline uint32_t
audio_stream_avail_frames(const struct audio_stream *source,
			  const struct audio_stream *sink)
{
	uint32_t src_frames = audio_stream_get_avail_frames(source);
	uint32_t sink_frames = audio_stream_get_free_frames(sink);

	return MIN(src_frames, sink_frames);
}

/**
 * Updates the stream state after writing to the stream.
 * @param stream Stream to update.
 * @param bytes Number of written bytes.
 */
static inline void audio_stream_produce(struct audio_stream *stream,
					uint32_t bytes)
{
	stream->w_ptr = audio_stream_wrap(stream,
					  (char *)stream->w_ptr + bytes);

	/* "overwrite" old data in circular wrap case */
	if (bytes > stream->free)
		stream->r_ptr = stream->w_ptr;

	/* calculate available bytes */
	if (stream->r_ptr < stream->w_ptr)
		stream->avail = (char *)stream->w_ptr - (char *)stream->r_ptr;
	else if (stream->r_ptr == stream->w_ptr)
		stream->avail = stream->size; /* full */
	else
		stream->avail = stream->size -
			((char *)stream->r_ptr - (char *)stream->w_ptr);

	/* calculate free bytes */
	stream->free = stream->size - stream->avail;
}

/**
 * Updates the stream state after reading from the stream.
 * @param stream Stream to update.
 * @param bytes Number of read bytes.
 */
static inline void audio_stream_consume(struct audio_stream *stream,
					uint32_t bytes)
{
	stream->r_ptr = audio_stream_wrap(stream,
					  (char *)stream->r_ptr + bytes);

	/* calculate available bytes */
	if (stream->r_ptr < stream->w_ptr)
		stream->avail = (char *)stream->w_ptr - (char *)stream->r_ptr;
	else if (stream->r_ptr == stream->w_ptr)
		stream->avail = 0; /* empty */
	else
		stream->avail = stream->size -
			((char *)stream->r_ptr - (char *)stream->w_ptr);

	/* calculate free bytes */
	stream->free = stream->size - stream->avail;
}

/**
 * Resets the stream.
 * @param stream Stream to reset.
 */
static inline void audio_stream_reset(struct audio_stream *stream)
{
	/* reset read and write pointer to stream bas */
	stream->w_ptr = stream->addr;
	stream->r_ptr = stream->addr;

	/* free space is stream size */
	stream->free = stream->size;

	/* there are no avail samples at reset */
	stream->avail = 0;
}

/**
 * Initializes the stream with specified memory block and size.
 * @param stream Stream to initialize.
 * @param buff_addr Address of the memory block to assign.
 * @param size Size of the memory block in bytes.
 */
static inline void audio_stream_init(struct audio_stream *stream,
				     void *buff_addr, uint32_t size)
{
	stream->size = size;
	stream->addr = buff_addr;
	stream->end_addr = (char *)stream->addr + size;
	audio_stream_reset(stream);
}

/**
 * Invalidates (in DSP d-cache) the stream in range [r_ptr, r_ptr+bytes],
 * with rollover if necessary.
 * @param stream Stream.
 * @param bytes Size of the fragment to invalidate.
 */
static inline void audio_stream_invalidate(struct audio_stream *stream,
					   uint32_t bytes)
{
	uint32_t head_size = bytes;
	uint32_t tail_size = 0;

	/* check for potential wrap */
	if ((char *)stream->r_ptr + bytes > (char *)stream->end_addr) {
		head_size = (char *)stream->end_addr - (char *)stream->r_ptr;
		tail_size = bytes - head_size;
	}

	dcache_invalidate_region(stream->r_ptr, head_size);
	if (tail_size)
		dcache_invalidate_region(stream->addr, tail_size);
}

/**
 * Writes back (from DSP d-cache) the stream in range [w_ptr, w_ptr+bytes],
 * with rollover if necessary.
 * @param stream Stream.
 * @param bytes Size of the fragment to write back.
 */
static inline void audio_stream_writeback(struct audio_stream *stream,
					  uint32_t bytes)
{
	uint32_t head_size = bytes;
	uint32_t tail_size = 0;

	/* check for potential wrap */
	if ((char *)stream->w_ptr + bytes > (char *)stream->end_addr) {
		head_size = (char *)stream->end_addr - (char *)stream->w_ptr;
		tail_size = bytes - head_size;
	}

	dcache_writeback_region(stream->w_ptr, head_size);
	if (tail_size)
		dcache_writeback_region(stream->addr, tail_size);
}

/**
 * @brief Calculates numbers of bytes to stream wrap and return
 *	  minimum of calculated value and given bytes number.
 * @param source Stream to get information from.
 * @param ptr Read or write pointer from source
 * @return Number of data samples to stream wrap or given samples number.
 */
static inline int
audio_stream_bytes_without_wrap(const struct audio_stream *source,
				const void *ptr)
{
	int to_end = (intptr_t)source->end_addr - (intptr_t)ptr;
	return to_end;
}

/**
 * Copies data from source stream to sink stream.
 * @param source Source stream.
 * @param ioffset_bytes Offset (in bytes) in source stream to start reading
 *	from.
 * @param sink Sink stream.
 * @param ooffset_bytes Offset (in bytes) in sink stream to start writing to.
 * @param bytes Number of bytes to copy.
 */
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
		bytes_src = audio_stream_bytes_without_wrap(source, src);
		bytes_snk = audio_stream_bytes_without_wrap(sink, snk);
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

/**
 * Copies signed 16-bit samples from source stream to sink stream.
 * @param source Source stream.
 * @param ioffset Offset (in samples) in source stream to start reading from.
 * @param sink Sink stream.
 * @param ooffset Offset (in samples) in sink stream to start writing to.
 * @param samples Number of samples to copy.
 */
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

/**
 * Copies signed 32-bit samples from source stream to sink stream.
 * @param source Source stream.
 * @param ioffset Offset (in samples) in source stream to start reading from.
 * @param sink Sink stream.
 * @param ooffset Offset (in samples) in sink stream to start writing to.
 * @param samples Number of samples to copy.
 */
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

/** @}*/

#endif /* __SOF_AUDIO_AUDIO_STREAM_H__ */
