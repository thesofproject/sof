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

#include <../include/format.h>
#include <../include/math/numbers.h>
#include <../include/ipc/stream.h>
#include <../include/ipc4/base-config.h>

#include <stdbool.h>
#include <stdint.h>
#include <../include/errno.h>
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

	/* runtime stream params */
	enum sof_ipc_frame frame_fmt;	/**< Sample data format */
	enum sof_ipc_frame valid_sample_fmt;

	uint32_t rate;		/**< Number of data frames per second [Hz] */
	uint16_t channels;	/**< Number of samples in each frame */

	/** alignment limit of stream copy, this value indicates how many
	 * integer frames which can meet both byte align and frame align
	 * requirement. it should be set in component prepare or param functions
	 */
	uint16_t frame_align;

	/**
	 * alignment limit of stream copy, alignment is the frame_align_shift-th
	 * power of 2 bytes. it should be set in component prepare or param functions
	 */
	uint16_t frame_align_shift;

	bool overrun_permitted; /**< indicates whether overrun is permitted */
	bool underrun_permitted; /**< indicates whether underrun is permitted */
};

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
 * Applies parameters to the buffer.
 * @param buffer Buffer.
 * @param params Parameters (frame format, rate, number of channels).
 * @return 0 if succeeded, error code otherwise.
 */
static inline int audio_stream_set_params(struct audio_stream  *buffer,
					  struct sof_ipc_stream_params *params)
{
	if (!params)
		return -EINVAL;

	buffer->frame_fmt = params->frame_fmt;
	buffer->rate = params->rate;
	buffer->channels = params->channels;

	return 0;
}

/**
 * Calculates period size in bytes based on component stream's parameters.
 * @param buf Component buffer.
 * @return Period size in bytes.
 */
static inline uint32_t audio_stream_frame_bytes(const struct audio_stream  *buf)
{
	return get_frame_bytes(buf->frame_fmt, buf->channels);
}

/**
 * Calculates sample size in bytes based on component stream's parameters.
 * @param buf Component buffer.
 * @return Size of sample in bytes.
 */
static inline uint32_t audio_stream_sample_bytes(const struct audio_stream  *buf)
{
	return get_sample_bytes(buf->frame_fmt);
}

/**
 * Return the frames that meet the align requirement of both byte_align and
 * frame_align_req.
 * @param byte_align Processing byte alignment requirement.
 * @param frame_align_req Processing frames alignment requirement.
 * @param frame_size Size of the frame in bytes.
 * @return frame number.
 */
static inline uint32_t audio_stream_frame_align_get(const uint32_t byte_align,
						    const uint32_t frame_align_req,
						    uint32_t frame_size)
{
	/* Figure out how many frames are needed to meet the byte_align alignment requirements */
	uint32_t frame_num = byte_align / gcd(byte_align, frame_size);

	/** return the lcm of frame_num and frame_align_req*/
	return frame_align_req * frame_num / gcd(frame_num, frame_align_req);
}

/**
 * Set frame_align_shift and frame_align of stream according to byte_align and
 * frame_align_req alignment requirement. Once the channel number,frame size
 * are determined，the frame_align and frame_align_shift are determined too.
 * these two feature will be used in audio_stream_get_avail_frames_aligned
 * to calculate the available frames. it should be called in component prepare
 * or param functions only once before stream copy. if someone forgets to call
 * this first, there would be unexampled error such as nothing is copied at all.
 * @param byte_align Processing byte alignment requirement.
 * @param frame_align_req Processing frames alignment requirement.
 * @param stream Sink or source stream structure which to be set.
 */
static inline void audio_stream_init_alignment_constants(const uint32_t byte_align,
							 const uint32_t frame_align_req,
							 struct audio_stream  *stream)
{
	uint32_t process_size;
	uint32_t frame_size = audio_stream_frame_bytes(stream);

	stream->frame_align = audio_stream_frame_align_get(byte_align, frame_align_req, frame_size);
	process_size = stream->frame_align * frame_size;
	stream->frame_align_shift = (is_power_of_2(process_size) ? 31 : 32) - clz(process_size);
}

/**
 * Calculates period size in bytes based on component stream's parameters.
 * @param buf Component buffer.
 * @param frames Number of processing frames.
 * @return Period size in bytes.
 */
static inline uint32_t audio_stream_period_bytes(const struct audio_stream  *buf,
						 uint32_t frames)
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
static inline void *audio_stream_wrap(const struct audio_stream  *buffer,
				      void *ptr)
{
	if (ptr >= buffer->end_addr)
		ptr = (char *)buffer->addr +
			((char *)ptr - (char *)buffer->end_addr);

	assert((intptr_t)ptr <= (intptr_t)buffer->end_addr);

	return ptr;
}

/**
 * Verifies the pointer and performs rollover when reached the end of
 * the buffer.
 * @param buffer Buffer accessed by the pointer.
 * @param ptr Pointer
 * @return Pointer, adjusted if necessary.
 */
static inline void *audio_stream_rewind_wrap(const struct audio_stream  *buffer,
					     void *ptr)
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
audio_stream_get_avail_bytes(const struct audio_stream  *stream)
{
	/*
	 * In case of underrun-permitted stream, report buffer full instead of
	 * empty. This way, any data present in such stream is processed at
	 * regular pace, but buffer will never be seen as completely empty by
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
audio_stream_get_avail_samples(const struct audio_stream  *stream)
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
audio_stream_get_avail_frames(const struct audio_stream  *stream)
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
audio_stream_get_free_bytes(const struct audio_stream  *stream)
{
	/*
	 * In case of overrun-permitted stream, report buffer empty instead of
	 * full. This way, if there's any actual free space for data it is
	 * processed at regular pace, but buffer will never be seen as
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
audio_stream_get_free_samples(const struct audio_stream  *stream)
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
audio_stream_get_free_frames(const struct audio_stream  *stream)
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
static inline int audio_stream_can_copy_bytes(const struct audio_stream  *source,
					      const struct audio_stream  *sink,
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
audio_stream_get_copy_bytes(const struct audio_stream  *source,
			    const struct audio_stream  *sink)
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
audio_stream_avail_frames(const struct audio_stream  *source,
			  const struct audio_stream  *sink)
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
audio_stream_avail_frames_aligned(const struct audio_stream  *source,
				  const struct audio_stream  *sink)
{
	uint32_t src_frames = (audio_stream_get_avail_bytes(source) >> source->frame_align_shift)
		 * source->frame_align;
	uint32_t sink_frames = (audio_stream_get_free_bytes(sink) >> sink->frame_align_shift)
		 * sink->frame_align;

	return MIN(src_frames, sink_frames);
}

/**
 * Updates the buffer state after writing to the buffer.
 * @param buffer Buffer to update.
 * @param bytes Number of written bytes.
 */
static inline void audio_stream_produce(struct audio_stream  *buffer,
					uint32_t bytes)
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
static inline void audio_stream_consume(struct audio_stream  *buffer,
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

/**
 * Resets the buffer.
 * @param buffer Buffer to reset.
 */
static inline void audio_stream_reset(struct audio_stream  *buffer)
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
 * @param buffer Buffer to initialize.
 * @param buff_addr Address of the memory block to assign.
 * @param size Size of the memory block in bytes.
 */
static inline void audio_stream_init(struct audio_stream  *buffer,
				     void *buff_addr, uint32_t size)
{
	buffer->size = size;
	buffer->addr = buff_addr;
	buffer->end_addr = (char *)buffer->addr + size;
	audio_stream_reset(buffer);
}

/**
 * Invalidates (in DSP d-cache) the buffer in range [r_ptr, r_ptr+bytes],
 * with rollover if necessary.
 * @param buffer Buffer.
 * @param bytes Size of the fragment to invalidate.
 */
static inline void audio_stream_invalidate(struct audio_stream  *buffer,
					   uint32_t bytes)
{
	uint32_t head_size = bytes;
	uint32_t tail_size = 0;

	/* check for potential wrap */
	if ((char *)buffer->r_ptr + bytes > (char *)buffer->end_addr) {
		head_size = (char *)buffer->end_addr - (char *)buffer->r_ptr;
		tail_size = bytes - head_size;
	}

	dcache_invalidate_region((__attribute__((force)) void  *)buffer->r_ptr, head_size);
	if (tail_size)
		dcache_invalidate_region((__attribute__((force)) void  *)buffer->addr,
					 tail_size);
}

/**
 * Writes back (from DSP d-cache) the buffer in range [w_ptr, w_ptr+bytes],
 * with rollover if necessary.
 * @param buffer Buffer.
 * @param bytes Size of the fragment to write back.
 */
static inline void audio_stream_writeback(struct audio_stream  *buffer,
					  uint32_t bytes)
{
	uint32_t head_size = bytes;
	uint32_t tail_size = 0;

	/* check for potential wrap */
	if ((char *)buffer->w_ptr + bytes > (char *)buffer->end_addr) {
		head_size = (char *)buffer->end_addr - (char *)buffer->w_ptr;
		tail_size = bytes - head_size;
	}

	dcache_writeback_region((__attribute__((force)) void  *)buffer->w_ptr, head_size);
	if (tail_size)
		dcache_writeback_region((__attribute__((force)) void  *)buffer->addr,
					tail_size);
}

/**
 * @brief Calculates numbers of bytes to buffer wrap.
 * @param source Stream to get information from.
 * @param ptr Read or write pointer from source
 * @return Number of data samples to buffer wrap.
 */
static inline int
audio_stream_bytes_without_wrap(const struct audio_stream  *source,
				const void *ptr)
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
audio_stream_rewind_bytes_without_wrap(const struct audio_stream  *source,
				       const void *ptr)
{
	assert((intptr_t)ptr >= (intptr_t)source->addr);
	int to_begin = (intptr_t)ptr - (intptr_t)source->addr;
	return to_begin;
}

/**
 * @brief Calculates numbers of s16 samples to buffer wrap when buffer
 * is read forward towards end address.
 * @param source Stream to get information from.
 * @param ptr Read or write pointer from source
 * @return Number of data s16 samples until circular wrap need at end
 */
static inline int
audio_stream_samples_without_wrap_s16(const struct audio_stream  *source,
				      const void *ptr)
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
audio_stream_samples_without_wrap_s24(const struct audio_stream  *source,
				      const void *ptr)
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
audio_stream_samples_without_wrap_s32(const struct audio_stream  *source,
				      const void *ptr)
{
	int to_end = (int32_t *)source->end_addr - (int32_t *)ptr;

	assert((intptr_t)source->end_addr >= (intptr_t)ptr);
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
audio_stream_frames_without_wrap(const struct audio_stream  *source,
				 const void *ptr)
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
int audio_stream_copy(const struct audio_stream  *source, uint32_t ioffset,
		      struct audio_stream  *sink, uint32_t ooffset, uint32_t samples);

/**
 * Copies data from linear source buffer to circular sink buffer.
 * @param linear_source Source buffer.
 * @param ioffset Offset (in samples) in source buffer to start reading from.
 * @param sink Sink buffer.
 * @param ooffset Offset (in samples) in sink buffer to start writing to.
 * @param samples Number of samples to copy.
 */
void audio_stream_copy_from_linear(const void *linear_source, int ioffset,
				   struct audio_stream  *sink, int ooffset,
				   unsigned int samples);

/**
 * Copies data from circular source buffer to linear sink buffer.
 * @param source Source buffer.
 * @param ioffset Offset (in samples) in source buffer to start reading from.
 * @param linear_sink Sink buffer.
 * @param ooffset Offset (in samples) in sink buffer to start writing to.
 * @param samples Number of samples to copy.
 */
void audio_stream_copy_to_linear(const struct audio_stream  *source, int ioffset,
				 void *linear_sink, int ooffset, unsigned int samples);

/**
 * Writes zeros in range [w_ptr, w_ptr+bytes], with rollover if necessary.
 * @param buffer Buffer.
 * @param bytes Size of the fragment to write zero.
 * @return 0 if there is enough free space in buffer.
 * @return 1 if there is not enough free space in buffer.
 */
static inline int audio_stream_set_zero(struct audio_stream  *buffer,
					uint32_t bytes)
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

static inline void audio_stream_fmt_conversion(enum ipc4_bit_depth depth,
					       enum ipc4_bit_depth valid,
					       enum sof_ipc_frame  *frame_fmt,
					       enum sof_ipc_frame  *valid_fmt,
					       enum ipc4_sample_type type)
{
	/* IPC4_DEPTH_16BIT (16) <---> SOF_IPC_FRAME_S16_LE (0)
	 * IPC4_DEPTH_24BIT (24) <---> SOF_IPC_FRAME_S24_4LE (1)
	 * IPC4_DEPTH_32BIT (32) <---> SOF_IPC_FRAME_S32_LE (2)
	 */
	*frame_fmt = (depth >> 3) - 2;
	*valid_fmt = (valid >> 3) - 2;

	/* really 24_3LE */
	if (valid == 24 && depth == 24) {
		*frame_fmt = SOF_IPC_FRAME_S24_3LE;
		*valid_fmt = SOF_IPC_FRAME_S24_3LE;
	}

	if (type == IPC4_TYPE_FLOAT && depth == 32) {
		*frame_fmt = SOF_IPC_FRAME_FLOAT;
		*valid_fmt = SOF_IPC_FRAME_FLOAT;
	}
}
/** @}*/

#endif /* __SOF_AUDIO_AUDIO_STREAM_H__ */
