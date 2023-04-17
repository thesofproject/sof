// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2023 Intel Corporation. All rights reserved.
//

#include <sof/audio/audio_stream.h>
#include <sof/audio/buffer.h>

static size_t audio_stream_get_free_size(struct sof_sink __sparse_cache *sink)
{
	struct audio_stream __sparse_cache *audio_stream =
			attr_container_of(sink, struct audio_stream __sparse_cache,
					  sink_api, __sparse_cache);

	return audio_stream_get_free_bytes(audio_stream);
}

static int audio_stream_get_buffer(struct sof_sink __sparse_cache *sink, size_t req_size,
				   void **data_ptr, void **buffer_start, size_t *buffer_size)
{
	struct audio_stream __sparse_cache *audio_stream =
			attr_container_of(sink, struct audio_stream __sparse_cache,
					  sink_api, __sparse_cache);

	if (req_size > audio_stream_get_free_size(sink))
		return -ENODATA;

	/* get circular buffer parameters */
	*data_ptr = audio_stream->w_ptr;
	*buffer_start = audio_stream->addr;
	*buffer_size = audio_stream->size;
	return 0;
}

static int audio_stream_commit_buffer(struct sof_sink __sparse_cache *sink, size_t commit_size)
{
	struct audio_stream __sparse_cache *audio_stream =
			attr_container_of(sink, struct audio_stream __sparse_cache,
					  sink_api, __sparse_cache);

	if (commit_size)
		audio_stream_produce(audio_stream, commit_size);

	return 0;
}

static size_t audio_stream_get_data_available(struct sof_source __sparse_cache *source)
{
	struct audio_stream __sparse_cache *audio_stream =
			attr_container_of(source, struct audio_stream __sparse_cache,
					  source_api, __sparse_cache);

	return audio_stream_get_avail_bytes(audio_stream);
}

static int audio_stream_get_data(struct sof_source __sparse_cache *source, size_t req_size,
				 void  **data_ptr, void **buffer_start, size_t *buffer_size)
{
	struct audio_stream __sparse_cache *audio_stream =
			attr_container_of(source, struct audio_stream __sparse_cache,
					  source_api, __sparse_cache);

	if (req_size > audio_stream_get_data_available(source))
		return -ENODATA;

	/* get circular buffer parameters */
	*data_ptr = audio_stream->r_ptr;
	*buffer_start = audio_stream->addr;
	*buffer_size = audio_stream->size;
	return 0;
}

static int audio_stream_release_data(struct sof_source __sparse_cache *source, size_t free_size)
{
	struct audio_stream __sparse_cache *audio_stream =
			attr_container_of(source, struct audio_stream __sparse_cache,
					  source_api, __sparse_cache);

	if (free_size)
		audio_stream_consume(audio_stream, free_size);

	return 0;
}

static int audio_stream_set_ipc_params_source(struct sof_source __sparse_cache *source,
					      struct sof_ipc_stream_params *params,
					      bool force_update)
{
	struct audio_stream __sparse_cache *audio_stream =
			attr_container_of(source, struct audio_stream __sparse_cache,
					  source_api, __sparse_cache);
	struct comp_buffer __sparse_cache *buffer =
			attr_container_of(audio_stream, struct comp_buffer __sparse_cache,
					  stream, __sparse_cache);

	return buffer_set_params(buffer, params, force_update);
}

static int audio_stream_set_ipc_params_sink(struct sof_sink __sparse_cache *sink,
					    struct sof_ipc_stream_params *params,
					    bool force_update)
{
	struct audio_stream __sparse_cache *audio_stream =
			attr_container_of(sink, struct audio_stream __sparse_cache,
					  sink_api, __sparse_cache);
	struct comp_buffer __sparse_cache *buffer =
			attr_container_of(audio_stream, struct comp_buffer __sparse_cache,
					  stream, __sparse_cache);

	return buffer_set_params(buffer, params, force_update);
}

static int audio_stream_source_set_alignment_constants(struct sof_source __sparse_cache *source,
						       const uint32_t byte_align,
						       const uint32_t frame_align_req)
{
	struct audio_stream __sparse_cache *audio_stream =
			attr_container_of(source, struct audio_stream __sparse_cache,
					  source_api, __sparse_cache);

	audio_stream_init_alignment_constants(byte_align, frame_align_req, audio_stream);

	return 0;
}

static int audio_stream_sink_set_alignment_constants(struct sof_sink __sparse_cache *sink,
						     const uint32_t byte_align,
						     const uint32_t frame_align_req)
{
	struct audio_stream __sparse_cache *audio_stream =
			attr_container_of(sink, struct audio_stream __sparse_cache,
					  sink_api, __sparse_cache);

	audio_stream_init_alignment_constants(byte_align, frame_align_req, audio_stream);

	return 0;
}

static const struct source_ops audio_stream_source_ops = {
	.get_data_available = audio_stream_get_data_available,
	.get_data = audio_stream_get_data,
	.release_data = audio_stream_release_data,
	.audio_set_ipc_params = audio_stream_set_ipc_params_source,
	.set_alignment_constants = audio_stream_source_set_alignment_constants
};

static const struct sink_ops audio_stream_sink_ops = {
	.get_free_size = audio_stream_get_free_size,
	.get_buffer = audio_stream_get_buffer,
	.commit_buffer = audio_stream_commit_buffer,
	.audio_set_ipc_params = audio_stream_set_ipc_params_sink,
	.set_alignment_constants = audio_stream_sink_set_alignment_constants
};

void audio_stream_init(struct audio_stream __sparse_cache *audio_stream,
		       void *buff_addr, uint32_t size)
{
	audio_stream->size = size;
	audio_stream->addr = buff_addr;
	audio_stream->end_addr = (char *)audio_stream->addr + size;

	/* set the default alignment info.
	 * set byte_align as 1 means no alignment limit on byte.
	 * set frame_align as 1 means no alignment limit on frame.
	 */
	audio_stream_init_alignment_constants(1, 1, audio_stream);

	source_init(audio_stream_get_source(audio_stream), &audio_stream_source_ops,
		    (__sparse_force struct sof_audio_stream_params *)
		    &audio_stream->runtime_stream_params);
	sink_init(audio_stream_get_sink(audio_stream), &audio_stream_sink_ops,
		  (__sparse_force struct sof_audio_stream_params *)
		  &audio_stream->runtime_stream_params);
	audio_stream_reset(audio_stream);
}
