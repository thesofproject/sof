// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2023 Intel Corporation. All rights reserved.
//

#include <sof/audio/audio_stream.h>
#include <sof/audio/buffer.h>

static size_t audio_stream_get_free_size(struct sof_sink *sink)
{
	struct audio_stream *audio_stream = container_of(sink, struct audio_stream, sink_api);

	return audio_stream_get_free_bytes(audio_stream);
}

static int audio_stream_get_buffer(struct sof_sink *sink, size_t req_size,
				   void **data_ptr, void **buffer_start, size_t *buffer_size)
{
	struct audio_stream *audio_stream = container_of(sink, struct audio_stream, sink_api);

	if (req_size > audio_stream_get_free_size(sink))
		return -ENODATA;

	/* get circular buffer parameters */
	*data_ptr = audio_stream->w_ptr;
	*buffer_start = audio_stream->addr;
	*buffer_size = audio_stream->size;
	return 0;
}

static int audio_stream_commit_buffer(struct sof_sink *sink, size_t commit_size)
{
	struct audio_stream *audio_stream = container_of(sink, struct audio_stream, sink_api);
	struct comp_buffer *buffer = container_of(audio_stream, struct comp_buffer, stream);

	if (commit_size) {
		buffer_stream_writeback(buffer, commit_size);
		audio_stream_produce(audio_stream, commit_size);
	}

	return 0;
}

static size_t audio_stream_get_data_available(struct sof_source *source)
{
	struct audio_stream *audio_stream = container_of(source, struct audio_stream, source_api);

	return audio_stream_get_avail_bytes(audio_stream);
}

static int audio_stream_get_data(struct sof_source *source, size_t req_size,
				 void const **data_ptr, void const **buffer_start,
				 size_t *buffer_size)
{
	struct audio_stream *audio_stream = container_of(source, struct audio_stream, source_api);
	struct comp_buffer *buffer = container_of(audio_stream, struct comp_buffer, stream);

	if (req_size > audio_stream_get_data_available(source))
		return -ENODATA;

	buffer_stream_invalidate(buffer, req_size);

	/* get circular buffer parameters */
	*data_ptr = audio_stream->r_ptr;
	*buffer_start = audio_stream->addr;
	*buffer_size = audio_stream->size;
	return 0;
}

static int audio_stream_release_data(struct sof_source *source, size_t free_size)
{
	struct audio_stream *audio_stream = container_of(source, struct audio_stream, source_api);

	if (free_size)
		audio_stream_consume(audio_stream, free_size);

	return 0;
}

static int audio_stream_set_ipc_params_source(struct sof_source *source,
					      struct sof_ipc_stream_params *params,
					      bool force_update)
{
	struct audio_stream *audio_stream = container_of(source, struct audio_stream, source_api);
	struct comp_buffer *buffer = container_of(audio_stream, struct comp_buffer, stream);

	return buffer_set_params(buffer, params, force_update);
}

static int audio_stream_set_ipc_params_sink(struct sof_sink *sink,
					    struct sof_ipc_stream_params *params,
					    bool force_update)
{
	struct audio_stream *audio_stream = container_of(sink, struct audio_stream, sink_api);
	struct comp_buffer *buffer = container_of(audio_stream, struct comp_buffer, stream);

	return buffer_set_params(buffer, params, force_update);
}

static int audio_stream_source_set_alignment_constants(struct sof_source *source,
						       const uint32_t byte_align,
						       const uint32_t frame_align_req)
{
	struct audio_stream *audio_stream = container_of(source, struct audio_stream, source_api);

	audio_stream_init_alignment_constants(byte_align, frame_align_req, audio_stream);

	return 0;
}

static int audio_stream_sink_set_alignment_constants(struct sof_sink *sink,
						     const uint32_t byte_align,
						     const uint32_t frame_align_req)
{
	struct audio_stream *audio_stream = container_of(sink, struct audio_stream, sink_api);

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

void audio_stream_init(struct audio_stream *audio_stream, void *buff_addr, uint32_t size)
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
		    &audio_stream->runtime_stream_params);
	sink_init(audio_stream_get_sink(audio_stream), &audio_stream_sink_ops,
		  &audio_stream->runtime_stream_params);
	audio_stream_reset(audio_stream);
}
