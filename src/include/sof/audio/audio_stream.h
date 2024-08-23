/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2020 - 2023 Intel Corporation. All rights reserved.
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
#include <sof/audio/sink_api.h>
#include <sof/audio/source_api.h>
#include <sof/compiler_attributes.h>
#include <rtos/panic.h>
#include <sof/math/numbers.h>
#include <rtos/alloc.h>
#include <rtos/cache.h>
#include <ipc/stream.h>
#include <ipc4/base-config.h>
#include <module/audio/audio_stream.h>

#include <stdbool.h>
#include <stdint.h>

/** \addtogroup audio_stream_api Audio Stream API
 *  @{
 */

/**
 * Audio stream is a circular buffer aware of audio format of the data
 * in the buffer so provides API for reading and writing not only bytes,
 * but also samples and frames.
 *
 * Audio stream does not perform any memory allocations. A client (a component
 * buffer or dma) must allocate the memory for the underlying data buffer and
 * provide it to the initialization routine.
 *
 * Once the client is done with reading/writing the data, it must commit the
 * consumption/production and update the buffer state by calling
 * audio_stream_consume()/audio_stream_produce() (just a single call following
 * series of reads/writes).
 *
 */
struct audio_stream {
	/* runtime data */
	uint32_t size;	/**< Runtime buffer size in bytes (period multiple) */
	uint32_t avail;	/**< Available bytes for reading */
	uint32_t free;	/**< Free bytes for writing */
	void *w_ptr;	/**< Buffer write pointer */
	void *r_ptr;	/**< Buffer read position */
	void *addr;	/**< Buffer base address */
	void *end_addr;	/**< Buffer end address */
	uint8_t byte_align_req;
	uint8_t frame_align_req;


	/* runtime stream params */
	struct sof_audio_stream_params runtime_stream_params;
};

void audio_stream_recalc_align(struct audio_stream *stream);

static inline void *audio_stream_get_rptr(const struct audio_stream *buf)
{
	return buf->r_ptr;
}

static inline void *audio_stream_get_wptr(const struct audio_stream *buf)
{
	return buf->w_ptr;
}

static inline void *audio_stream_get_end_addr(const struct audio_stream *buf)
{
	return buf->end_addr;
}

static inline void *audio_stream_get_addr(const struct audio_stream *buf)
{
	return buf->addr;
}

static inline uint32_t audio_stream_get_size(const struct audio_stream *buf)
{
	return buf->size;
}

static inline uint32_t audio_stream_get_avail(const struct audio_stream *buf)
{
	return buf->avail;
}

static inline uint32_t audio_stream_get_free(const struct audio_stream *buf)
{
	return buf->free;
}

static inline enum sof_ipc_frame audio_stream_get_frm_fmt(const struct audio_stream *buf)
{
	return buf->runtime_stream_params.frame_fmt;
}

static inline enum sof_ipc_frame audio_stream_get_valid_fmt(const struct audio_stream *buf)
{
	return buf->runtime_stream_params.valid_sample_fmt;
}

static inline uint32_t audio_stream_get_rate(const struct audio_stream *buf)
{
	return buf->runtime_stream_params.rate;
}

static inline uint32_t audio_stream_get_channels(const struct audio_stream *buf)
{
	return buf->runtime_stream_params.channels;
}

static inline bool audio_stream_get_underrun(const struct audio_stream *buf)
{
	return buf->runtime_stream_params.underrun_permitted;
}

static inline uint32_t audio_stream_get_buffer_fmt(const struct audio_stream *buf)
{
	return buf->runtime_stream_params.buffer_fmt;
}

static inline bool audio_stream_get_overrun(const struct audio_stream *buf)
{
	return buf->runtime_stream_params.overrun_permitted;
}

static inline void audio_stream_set_rptr(struct audio_stream *buf, void *val)
{
	buf->r_ptr = val;
}

static inline void audio_stream_set_wptr(struct audio_stream *buf, void *val)
{
	buf->w_ptr = val;
}

static inline void audio_stream_set_end_addr(struct audio_stream *buf, void *val)
{
	buf->end_addr = val;
}

static inline void audio_stream_set_addr(struct audio_stream *buf, void *val)
{
	buf->addr = val;
}

static inline void audio_stream_set_size(struct audio_stream *buf, uint32_t val)
{
	buf->size = val;
}

static inline void audio_stream_set_avail(struct audio_stream *buf, uint32_t val)
{
	buf->avail = val;
}

static inline void audio_stream_set_free(struct audio_stream *buf, uint32_t val)
{
	buf->free = val;
}

static inline void audio_stream_set_frm_fmt(struct audio_stream *buf,
					    enum sof_ipc_frame val)
{
	buf->runtime_stream_params.frame_fmt = val;
	audio_stream_recalc_align(buf);
}

static inline void audio_stream_set_valid_fmt(struct audio_stream *buf,
					      enum sof_ipc_frame val)
{
	buf->runtime_stream_params.valid_sample_fmt = val;
}

static inline void audio_stream_set_rate(struct audio_stream *buf, uint32_t val)
{
	buf->runtime_stream_params.rate = val;
}

static inline void audio_stream_set_channels(struct audio_stream *buf, uint16_t val)
{
	buf->runtime_stream_params.channels = val;
	audio_stream_recalc_align(buf);
}

static inline void audio_stream_set_underrun(struct audio_stream *buf,
					     bool underrun_permitted)
{
	buf->runtime_stream_params.underrun_permitted = underrun_permitted;
}

static inline void audio_stream_set_overrun(struct audio_stream *buf,
					    bool overrun_permitted)
{
	buf->runtime_stream_params.overrun_permitted = overrun_permitted;
}

static inline void audio_stream_set_buffer_fmt(struct audio_stream *buf,
					       uint32_t buffer_fmt)
{
	buf->runtime_stream_params.buffer_fmt = buffer_fmt;
}

/**
 * Retrieves readable address of a sample at specified index (see versions of
 * this macro specialized for various sample types).
 * @param buffer Buffer.
 * @param idx Index of sample.
 * @param size Size of sample in bytes.
 * @return Pointer to the sample.
 *
 * Once the consumer finishes reading samples from the buffer, it should
 * "commit" the operation and update the buffer state by calling
 * audio_stream_consume().
 *
 * @note Components should call comp_update_buffer_consume().
 *
 * @see audio_stream_get_frag().
 * @see audio_stream_consume().
 * @see comp_update_buffer_consume().
 */
#define audio_stream_read_frag(buffer, idx, size) \
	audio_stream_get_frag(buffer, (buffer)->r_ptr, idx, size)

/**
 * Retrieves readable address of a signed 16-bit sample at specified index.
 * @param buffer Buffer.
 * @param idx Index of sample.
 * @return Pointer to the sample.
 *
 * @see audio_stream_get_frag().
 */
#define audio_stream_read_frag_s16(buffer, idx) \
	audio_stream_get_frag(buffer, (buffer)->r_ptr, idx, sizeof(int16_t))

/**
 * Retrieves readable address of a signed 32-bit sample at specified index.
 * @param buffer Buffer.
 * @param idx Index of sample.
 * @return Pointer to the sample.
 *
 * @see audio_stream_get_frag().
 */
#define audio_stream_read_frag_s32(buffer, idx) \
	audio_stream_get_frag(buffer, (buffer)->r_ptr, idx, sizeof(int32_t))

/**
 * Retrieves writeable address of a sample at specified index (see versions of
 * this macro specialized for various sample types).
 * @param buffer Buffer.
 * @param idx Index of sample.
 * @param size Size of sample in bytes.
 * @return Pointer to the space for sample.
 *
 * Once the producer finishes writing samples to the buffer, it should
 * "commit" the operation and update the buffer state by calling
 * audio_stream_produce().
 *
 * @note Components should call comp_update_buffer_produce().
 *
 * @see audio_stream_get_frag().
 * @see audio_stream_produce().
 * @see comp_update_buffer_produce().
 */
#define audio_stream_write_frag(buffer, idx, size) \
	audio_stream_get_frag(buffer, (buffer)->w_ptr, idx, size)

/**
 * Retrieves writeable address of a signed 16-bit sample at specified index.
 * @param buffer Buffer.
 * @param idx Index of sample.
 * @return Pointer to the space for sample.
 *
 * @see audio_stream_get_frag().
 */
#define audio_stream_write_frag_s16(buffer, idx) \
	audio_stream_get_frag(buffer, (buffer)->w_ptr, idx, sizeof(int16_t))

/**
 * Retrieves writeable address of a signed 32-bit sample at specified index.
 * @param buffer Buffer.
 * @param idx Index of sample.
 * @return Pointer to the space for sample.
 *
 * @see audio_stream_get_frag().
 */
#define audio_stream_write_frag_s32(buffer, idx) \
	audio_stream_get_frag(buffer, (buffer)->w_ptr, idx, sizeof(int32_t))

/**
 * Retrieves address of sample (space for sample) at specified index within
 * the buffer. Index is interpreted as an offset relative to the specified
 * pointer, rollover is ensured.
 * @param buffer Circular buffer.
 * @param ptr Pointer to start from, it may be either read or write pointer.
 * @param idx Index of the sample.
 * @param sample_size Size of the sample in bytes.
 * @return Pointer to the sample.
 */
#define audio_stream_get_frag(buffer, ptr, idx, sample_size) \
	audio_stream_wrap(buffer, (char *)(ptr) + ((idx) * (sample_size)))

/**
 * Calculates period size in bytes based on component stream's parameters.
 * @param buf Component buffer.
 * @return Period size in bytes.
 */
static inline uint32_t audio_stream_frame_bytes(const struct audio_stream *buf)
{
	return get_frame_bytes(buf->runtime_stream_params.frame_fmt,
			       buf->runtime_stream_params.channels);
}

/**
 * Calculates sample size in bytes based on component stream's parameters.
 * @param buf Component buffer.
 * @return Size of sample in bytes.
 */
static inline uint32_t audio_stream_sample_bytes(const struct audio_stream *buf)
{
	return get_sample_bytes(buf->runtime_stream_params.frame_fmt);
}

/**
 * @brief Set processing alignment requirements
 *
 * Sets the sample byte alignment and aligned frame count required for
 * processing done on this stream.  This function may be called at any
 * time, the internal constants are recalculated if the frame/sample
 * size changes.  @see audio_stream_avail_frames_aligned().
 *
 * @param byte_align Processing byte alignment requirement.
 * @param frame_align_req Processing frames alignment requirement.
 * @param stream Sink or source stream structure which to be set.
 */
void audio_stream_set_align(const uint32_t byte_align,
			    const uint32_t frame_align_req,
			    struct audio_stream *stream);

/**
 * Applies parameters to the buffer.
 * @param buffer Audio stream buffer.
 * @param params Parameters (frame format, rate, number of channels).
 * @return 0 if succeeded, error code otherwise.
 */
static inline int audio_stream_set_params(struct audio_stream *buffer,
					  struct sof_ipc_stream_params *params)
{
	if (!params)
		return -EINVAL;

	buffer->runtime_stream_params.frame_fmt = (enum sof_ipc_frame)params->frame_fmt;
	buffer->runtime_stream_params.rate = params->rate;
	buffer->runtime_stream_params.channels = params->channels;

	audio_stream_recalc_align(buffer);

	return 0;
}

/**
 * Calculates period size in bytes based on component stream's parameters.
 * @param buf Component buffer.
 * @param frames Number of processing frames.
 * @return Period size in bytes.
 */
static inline uint32_t audio_stream_period_bytes(const struct audio_stream *buf, uint32_t frames)
{
	return frames * audio_stream_frame_bytes(buf);
}

/**
 * Verifies the pointer and performs rollover when reached the end of
 * the buffer.
 * @param buffer Buffer accessed by the pointer.
 * @param ptr Pointer
 * @return Pointer, adjusted if necessary.
 */
static inline void *audio_stream_wrap(const struct audio_stream *buffer, void *ptr)
{
	if (ptr >= buffer->end_addr)
		ptr = (char *)buffer->addr +
			((char *)ptr - (char *)buffer->end_addr);

	assert((intptr_t)ptr <= (intptr_t)buffer->end_addr);

	return ptr;
}

/**
 * Verifies the pointer and performs rollover when reached the end of
 * the circular buffer.
 * @param ptr Pointer
 * @param buf_addr Start address of the circular buffer.
 * @param buf_end End address of the circular buffer.
 * @return Pointer, adjusted if necessary.
 */
static inline void *cir_buf_wrap(void *ptr, void *buf_addr, void *buf_end)
{
	if (ptr >= buf_end)
		ptr = (char *)buf_addr +
			((char *)ptr - (char *)buf_end);

	assert((intptr_t)ptr <= (intptr_t)buf_end);

	return ptr;
}

/**
 * Verifies the pointer and performs rollover when reached the end of
 * the buffer.
 * @param buffer Buffer accessed by the pointer.
 * @param ptr Pointer
 * @return Pointer, adjusted if necessary.
 */
static inline void *audio_stream_rewind_wrap(const struct audio_stream *buffer, void *ptr)
{
	if (ptr < buffer->addr)
		ptr = (char *)buffer->end_addr - ((char *)buffer->addr - (char *)ptr);

	assert((intptr_t)ptr >= (intptr_t)buffer->addr);

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
	 * In case of underrun-permitted stream, report buffer full instead of
	 * empty. This way, any data present in such stream is processed at
	 * regular pace, but buffer will never be seen as completely empty by
	 * clients, and in turn will not cause underrun/XRUN.
	 */
	if (stream->runtime_stream_params.underrun_permitted)
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
	 * In case of overrun-permitted stream, report buffer empty instead of
	 * full. This way, if there's any actual free space for data it is
	 * processed at regular pace, but buffer will never be seen as
	 * completely full by clients, and in turn will not cause overrun/XRUN.
	 */
	if (stream->runtime_stream_params.overrun_permitted)
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
 *  Verifies whether specified number of bytes can be copied from source buffer
 *  to sink buffer.
 *  @param source Source buffer.
 *  @param sink Sink buffer.
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
 * Computes maximum number of bytes that can be copied from source buffer to
 * sink buffer, verifying number of bytes available in source vs. free space
 * available in sink.
 * @param source Source buffer.
 * @param sink Sink buffer.
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
 * Computes maximum number of frames that can be copied from source buffer
 * to sink buffer, verifying number of available source frames vs. free
 * space available in sink.
 * @param source Source buffer.
 * @param sink Sink buffer.
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
 * Computes maximum number of frames aligned that can be copied from
 * source buffer to sink buffer, verifying number of available source
 * frames vs. free space available in sink.
 * @param source Buffer of source.
 * @param sink Buffer of sink.
 * @return Number of frames.
 */
static inline uint32_t
audio_stream_avail_frames_aligned(const struct audio_stream *source,
				  const struct audio_stream *sink)
{
	uint32_t src_frames = (audio_stream_get_avail_bytes(source) >>
			source->runtime_stream_params.align_shift_idx) *
					source->runtime_stream_params.align_frame_cnt;
	uint32_t sink_frames = (audio_stream_get_free_bytes(sink) >>
			sink->runtime_stream_params.align_shift_idx) *
					sink->runtime_stream_params.align_frame_cnt;

	return MIN(src_frames, sink_frames);
}

/**
 * Updates the buffer state after writing to the buffer.
 * @param buffer Buffer to update.
 * @param bytes Number of written bytes.
 */
static inline void audio_stream_produce(struct audio_stream *buffer, uint32_t bytes)
{
	buffer->w_ptr = audio_stream_wrap(buffer,
					  (char *)buffer->w_ptr + bytes);

	/* "overwrite" old data in circular wrap case */
	if (bytes > audio_stream_get_free_bytes(buffer))
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

/**
 * Updates the buffer state after reading from the buffer.
 * @param buffer Buffer to update.
 * @param bytes Number of read bytes.
 */
static inline void audio_stream_consume(struct audio_stream *buffer, uint32_t bytes)
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

/**
 * Resets the buffer.
 * @param buffer Buffer to reset.
 */
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

/**
 * Initializes the buffer with specified memory block and size.
 * @param audio_stream the audio_stream a to initialize.
 * @param buff_addr Address of the memory block to assign.
 * @param size Size of the memory block in bytes.
 */
void audio_stream_init(struct audio_stream *audio_stream, void *buff_addr, uint32_t size);

/**
 * Invalidates (in DSP d-cache) the buffer in range [r_ptr, r_ptr+bytes],
 * with rollover if necessary.
 * @param buffer Buffer.
 * @param bytes Size of the fragment to invalidate.
 */
static inline void audio_stream_invalidate(struct audio_stream *buffer, uint32_t bytes)
{
	uint32_t head_size = bytes;
	uint32_t tail_size = 0;

	/* check for potential wrap */
	if ((char *)buffer->r_ptr + bytes > (char *)buffer->end_addr) {
		head_size = (char *)buffer->end_addr - (char *)buffer->r_ptr;
		tail_size = bytes - head_size;
	}

	dcache_invalidate_region((__sparse_force void __sparse_cache *)buffer->r_ptr, head_size);
	if (tail_size)
		dcache_invalidate_region((__sparse_force void __sparse_cache *)buffer->addr,
					 tail_size);
}

/**
 * Writes back (from DSP d-cache) the buffer in range [w_ptr, w_ptr+bytes],
 * with rollover if necessary.
 * @param buffer Buffer.
 * @param bytes Size of the fragment to write back.
 */
static inline void audio_stream_writeback(struct audio_stream *buffer, uint32_t bytes)
{
	uint32_t head_size = bytes;
	uint32_t tail_size = 0;

	/* check for potential wrap */
	if ((char *)buffer->w_ptr + bytes > (char *)buffer->end_addr) {
		head_size = (char *)buffer->end_addr - (char *)buffer->w_ptr;
		tail_size = bytes - head_size;
	}

	dcache_writeback_region((__sparse_force void __sparse_cache *)buffer->w_ptr, head_size);
	if (tail_size)
		dcache_writeback_region((__sparse_force void __sparse_cache *)buffer->addr,
					tail_size);
}

/**
 * @brief Calculates numbers of bytes to buffer wrap.
 * @param source Stream to get information from.
 * @param ptr Read or write pointer from source
 * @return Number of data samples to buffer wrap.
 */
static inline int
audio_stream_bytes_without_wrap(const struct audio_stream *source, const void *ptr)
{
	assert((intptr_t)source->end_addr >= (intptr_t)ptr);
	return (intptr_t)source->end_addr - (intptr_t)ptr;
}

/**
 * @brief Calculates numbers of bytes to buffer wrap when reading stream
 *	  backwards from current sample pointed by ptr towards begin.
 * @param source Stream to get information from.
 * @param ptr Read or write pointer from source
 * @return Number of bytes to buffer wrap. For number of samples calculate
 *	   need to add size of sample to returned bytes count.
 */
static inline int
audio_stream_rewind_bytes_without_wrap(const struct audio_stream *source, const void *ptr)
{
	assert((intptr_t)ptr >= (intptr_t)source->addr);
	int to_begin = (intptr_t)ptr - (intptr_t)source->addr;
	return to_begin;
}

/**
 * @brief Calculate position of write pointer to the position before the data was copied
 * to source buffer.
 * @param source Stream to get information from.
 * @param bytes  Number of bytes copied to source buffer.
 * @return Previous position of the write pointer.
 */
static inline uint32_t
*audio_stream_rewind_wptr_by_bytes(const struct audio_stream *source, const uint32_t bytes)
{
	void *wptr = audio_stream_get_wptr(source);
	int to_begin = audio_stream_rewind_bytes_without_wrap(source, wptr);

	assert((intptr_t)wptr >= (intptr_t)source->addr);
	assert((intptr_t)source->end_addr > (intptr_t)wptr);

	if (to_begin > bytes)
		return (uint32_t *)((intptr_t)wptr - bytes);
	else
		return (uint32_t *)((intptr_t)source->end_addr - (bytes - to_begin));
}

/**
 * @brief Calculates numbers of s16 samples to buffer wrap when buffer
 * is read forward towards end address.
 * @param source Stream to get information from.
 * @param ptr Read or write pointer from source
 * @return Number of data s16 samples until circular wrap need at end
 */
static inline int
audio_stream_samples_without_wrap_s16(const struct audio_stream *source, const void *ptr)
{
	int to_end = (int16_t *)source->end_addr - (int16_t *)ptr;

	assert((intptr_t)source->end_addr >= (intptr_t)ptr);
	return to_end;
}

/**
 * @brief Calculates numbers of s24 samples to buffer wrap when buffer
 * is read forward towards end address.
 * @param source Stream to get information from.
 * @param ptr Read or write pointer from source
 * @return Number of data s24 samples until circular wrap need at end
 */
static inline int
audio_stream_samples_without_wrap_s24(const struct audio_stream *source, const void *ptr)
{
	int to_end = (int32_t *)source->end_addr - (int32_t *)ptr;

	assert((intptr_t)source->end_addr >= (intptr_t)ptr);
	return to_end;
}

/**
 * @brief Calculates numbers of s32 samples to buffer wrap when buffer
 * is read forward towards end address.
 * @param source Stream to get information from.
 * @param ptr Read or write pointer from source
 * @return Number of data s32 samples until circular wrap need at end
 */
static inline int
audio_stream_samples_without_wrap_s32(const struct audio_stream *source, const void *ptr)
{
	int to_end = (int32_t *)source->end_addr - (int32_t *)ptr;

	assert((intptr_t)source->end_addr >= (intptr_t)ptr);
	return to_end;
}

/**
 * @brief Calculates numbers of bytes to buffer wrap when reading stream
 *	  backwards from current sample pointed by ptr towards begin.
 * @param ptr Read or write pointer og circular buffer.
 * @param buf_end End address of circular buffer.
 * @return Number of bytes to buffer wrap. For number of samples calculate
 *	   need to add size of sample to returned bytes count.
 */
static inline int cir_buf_bytes_without_wrap(void *ptr, void *buf_end)
{
	assert((intptr_t)buf_end >= (intptr_t)ptr);
	return (intptr_t)buf_end - (intptr_t)ptr;
}

/**
 * @brief Calculates numbers of s32 samples to buffer wrap when reading stream
 *	  backwards from current sample pointed by ptr towards begin.
 * @param ptr Read or write pointer og circular buffer.
 * @param buf_end End address of circular buffer.
 * @return Number of bytes to buffer wrap. For number of samples calculate
 *	   need to add size of sample to returned bytes count.
 */
static inline int cir_buf_samples_without_wrap_s32(void *ptr, void *buf_end)
{
	int to_end = (int32_t *)buf_end - (int32_t *)ptr;

	assert((intptr_t)buf_end >= (intptr_t)ptr);
	return to_end;
}

/**
 * @brief Calculates numbers of frames to buffer wrap and return
 *	  minimum of calculated value.
 * @param source Stream to get information from.
 * @param ptr Read or write pointer from source
 * @return Number of data frames to buffer wrap.
 */
static inline uint32_t
audio_stream_frames_without_wrap(const struct audio_stream *source, const void *ptr)
{
	uint32_t bytes = audio_stream_bytes_without_wrap(source, ptr);
	uint32_t frame_bytes = audio_stream_frame_bytes(source);

	return bytes / frame_bytes;
}

/**
 * Copies data from source buffer to sink buffer.
 * @param source Source buffer.
 * @param ioffset Offset (in samples) in source buffer to start reading from.
 * @param sink Sink buffer.
 * @param ooffset Offset (in samples) in sink buffer to start writing to.
 * @param samples Number of samples to copy.
 * @return number of processed samples.
 */
int audio_stream_copy(const struct audio_stream *source, uint32_t ioffset,
		      struct audio_stream *sink, uint32_t ooffset, uint32_t samples);

/**
 * Copies data from one circular buffer to another circular buffer.
 * @param src Source pointer of source circular buffer.
 * @param src_addr Start address of source circular buffer.
 * @param src_end End address of source circular buffer.
 * @param dst Sink pointer of source circular buffer.
 * @param dst_addr Start address of sink circular buffer
 * @param dst_end End address of sink circular buffer.
 * @param byte_size Number of bytes to copy.
 */
void cir_buf_copy(void *src, void *src_addr, void *src_end, void *dst,
		  void *dst_addr, void *dst_end, size_t byte_size);

/**
 * Copies data from linear source buffer to circular sink buffer.
 * @param linear_source Source buffer.
 * @param ioffset Offset (in samples) in source buffer to start reading from.
 * @param sink Sink buffer.
 * @param ooffset Offset (in samples) in sink buffer to start writing to.
 * @param samples Number of samples to copy.
 */
void audio_stream_copy_from_linear(const void *linear_source, int ioffset,
				   struct audio_stream *sink, int ooffset,
				   unsigned int samples);

/**
 * Copies data from circular source buffer to linear sink buffer.
 * @param source Source buffer.
 * @param ioffset Offset (in samples) in source buffer to start reading from.
 * @param linear_sink Sink buffer.
 * @param ooffset Offset (in samples) in sink buffer to start writing to.
 * @param samples Number of samples to copy.
 */
void audio_stream_copy_to_linear(const struct audio_stream *source, int ioffset,
				 void *linear_sink, int ooffset, unsigned int samples);

/**
 * Writes zeros in range [w_ptr, w_ptr+bytes], with rollover if necessary.
 * @param buffer Buffer.
 * @param bytes Size of the fragment to write zero.
 * @return 0 if there is enough free space in buffer.
 * @return 1 if there is not enough free space in buffer.
 */
static inline int audio_stream_set_zero(struct audio_stream *buffer, uint32_t bytes)
{
	uint32_t head_size = bytes;
	uint32_t tail_size = 0;

	/* check for overrun */
	if (audio_stream_get_free_bytes(buffer) < bytes)
		return 1;

	/* check for potential wrap */
	if ((char *)buffer->w_ptr + bytes > (char *)buffer->end_addr) {
		head_size = (char *)buffer->end_addr - (char *)buffer->w_ptr;
		tail_size = bytes - head_size;
	}

	memset(buffer->w_ptr, 0, head_size);
	if (tail_size)
		memset(buffer->addr, 0, tail_size);

	return 0;
}

/**
 * Writes zeros to circular buffer in range [ptr, ptr+bytes] with rollover if necessary.
 * @param ptr Pointer inside circular biffer to start writing from.
 * @param buf_addr Start of the circular buffer.
 * @param buf_end End of the circular buffer.
 * @param bytes Size of the fragment to write zeros.
 */
static inline void cir_buf_set_zero(void *ptr, void *buf_addr, void *buf_end, uint32_t bytes)
{
	uint32_t head_size = bytes;
	uint32_t tail_size = 0;

	/* check for potential wrap */
	if ((char *)ptr + bytes > (char *)buf_end) {
		head_size = (char *)buf_end - (char *)ptr;
		tail_size = bytes - head_size;
	}

	memset(ptr, 0, head_size);
	if (tail_size)
		memset(buf_addr, 0, tail_size);
}

static inline void audio_stream_fmt_conversion(enum ipc4_bit_depth depth,
					       enum ipc4_bit_depth valid,
					       enum sof_ipc_frame *frame_fmt,
					       enum sof_ipc_frame *valid_fmt,
					       enum ipc4_sample_type type)
{
	/* IPC4_DEPTH_16BIT (16) <---> SOF_IPC_FRAME_S16_LE (0)
	 * IPC4_DEPTH_24BIT (24) <---> SOF_IPC_FRAME_S24_4LE (1)
	 * IPC4_DEPTH_32BIT (32) <---> SOF_IPC_FRAME_S32_LE (2)
	 */
	*frame_fmt = (enum sof_ipc_frame)((depth >> 3) - 2);
	*valid_fmt = (enum sof_ipc_frame)((valid >> 3) - 2);

#ifdef CONFIG_FORMAT_U8
	if (depth == 8)
		*frame_fmt = SOF_IPC_FRAME_U8;

	if (valid == 8)
		*valid_fmt = SOF_IPC_FRAME_U8;
#endif /* CONFIG_FORMAT_U8 */

	if (valid == 24) {
#ifdef CONFIG_FORMAT_S24_3LE
		if (depth == 24) {
			*frame_fmt = SOF_IPC_FRAME_S24_3LE;
			*valid_fmt = SOF_IPC_FRAME_S24_3LE;
		}
#endif
	}

	if (type == IPC4_TYPE_FLOAT && depth == 32) {
		*frame_fmt = SOF_IPC_FRAME_FLOAT;
		*valid_fmt = SOF_IPC_FRAME_FLOAT;
	}
}
/** @}*/

#endif /* __SOF_AUDIO_AUDIO_STREAM_H__ */
