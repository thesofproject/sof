// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2023 Intel Corporation. All rights reserved.
//

#include <sof/audio/audio_stream.h>

static size_t audio_stream_get_free_size(struct sink __sparse_cache *sink)
{
	struct audio_stream __sparse_cache *audio_stream =
			attr_container_of(sink, struct audio_stream __sparse_cache,
					  sink_api, __sparse_cache);

	return audio_stream_get_free_bytes(audio_stream);
}

static int audio_stream_get_buffer(struct sink __sparse_cache *sink, size_t req_size,
				   void __sparse_cache **data_ptr,
				   void __sparse_cache **buffer_start, size_t *buffer_size)
{
	struct audio_stream __sparse_cache *audio_stream =
			attr_container_of(sink, struct audio_stream __sparse_cache,
					  sink_api, __sparse_cache);

	if (req_size > audio_stream_get_free_size(sink))
		return -ENODATA;

	/* get circular buffer parameters */
	*data_ptr = (__sparse_force void __sparse_cache *)audio_stream->w_ptr;
	*buffer_start = (__sparse_force void __sparse_cache *)audio_stream->addr;
	*buffer_size = audio_stream->size;
	return 0;
}

static int audio_stream_commit_buffer(struct sink __sparse_cache *sink, size_t commit_size)
{
	struct audio_stream __sparse_cache *audio_stream =
			attr_container_of(sink, struct audio_stream __sparse_cache,
					  sink_api, __sparse_cache);

	if (commit_size)
		audio_stream_produce(audio_stream, commit_size);

	return 0;
}

static size_t audio_stream_get_data_available(struct source __sparse_cache *source)
{
	struct audio_stream __sparse_cache *audio_stream =
			attr_container_of(source, struct audio_stream __sparse_cache,
					  source_api, __sparse_cache);

	return audio_stream_get_avail_bytes(audio_stream);
}

static int audio_stream_get_data(struct source __sparse_cache *source, size_t req_size,
				 void __sparse_cache **data_ptr, void __sparse_cache **buffer_start,
				 size_t *buffer_size)
{
	struct audio_stream __sparse_cache *audio_stream =
			attr_container_of(source, struct audio_stream __sparse_cache,
					  source_api, __sparse_cache);

	if (req_size > audio_stream_get_data_available(source))
		return -ENODATA;

	/* get circular buffer parameters */
	*data_ptr = (__sparse_force void __sparse_cache *)audio_stream->r_ptr;
	*buffer_start = (__sparse_force void __sparse_cache *)audio_stream->addr;
	*buffer_size = audio_stream->size;
	return 0;
}

static int audio_stream_put_data(struct source __sparse_cache *source, size_t free_size)
{
	struct audio_stream __sparse_cache *audio_stream =
			attr_container_of(source, struct audio_stream __sparse_cache,
					  source_api, __sparse_cache);

	if (free_size)
		audio_stream_consume(audio_stream, free_size);

	return 0;
}

static const struct source_ops audio_stream_source_ops = {
	.get_data_available = audio_stream_get_data_available,
	.get_data = audio_stream_get_data,
	.put_data = audio_stream_put_data
};

static const struct sink_ops audio_stream_sink_ops = {
	.get_free_size = audio_stream_get_free_size,
	.get_buffer = audio_stream_get_buffer,
	.commit_buffer = audio_stream_commit_buffer
};

inline void audio_stream_init(struct audio_stream __sparse_cache *buffer,
			      void *buff_addr, uint32_t size)
{
	buffer->size = size;
	buffer->addr = buff_addr;
	buffer->end_addr = (char *)buffer->addr + size;

	/* set the default alignment info.
	 * set byte_align as 1 means no alignment limit on byte.
	 * set frame_align as 1 means no alignment limit on frame.
	 */
	audio_stream_init_alignment_constants(1, 1, buffer);

	source_init(audio_stream_get_source(buffer), &audio_stream_source_ops,
		    &buffer->runtime_stream_params);
	sink_init(audio_stream_get_sink(buffer), &audio_stream_sink_ops,
		  &buffer->runtime_stream_params);
	audio_stream_reset(buffer);
}
