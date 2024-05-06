// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2023 Intel Corporation. All rights reserved.
//

#include <sof/audio/audio_stream.h>
#include <sof/audio/buffer.h>
#include <sof/audio/dp_queue.h>

static size_t audio_stream_get_free_size(struct sof_sink *sink)
{
	struct audio_stream *audio_stream = container_of(sink, struct audio_stream, _sink_api);

	return audio_stream_get_free_bytes(audio_stream);
}

static int audio_stream_get_buffer(struct sof_sink *sink, size_t req_size,
				   void **data_ptr, void **buffer_start, size_t *buffer_size)
{
	struct audio_stream *audio_stream = container_of(sink, struct audio_stream, _sink_api);

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
	struct audio_stream *audio_stream = container_of(sink, struct audio_stream, _sink_api);
	struct comp_buffer *buffer = container_of(audio_stream, struct comp_buffer, stream);

	if (commit_size) {
		buffer_stream_writeback(buffer, commit_size);
		audio_stream_produce(audio_stream, commit_size);
	}

	return 0;
}

static size_t audio_stream_get_data_available(struct sof_source *source)
{
	struct audio_stream *audio_stream = container_of(source, struct audio_stream, _source_api);

	return audio_stream_get_avail_bytes(audio_stream);
}

static int audio_stream_get_data(struct sof_source *source, size_t req_size,
				 void const **data_ptr, void const **buffer_start,
				 size_t *buffer_size)
{
	struct audio_stream *audio_stream = container_of(source, struct audio_stream, _source_api);
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

static uint32_t audio_stream_frame_align_get(const uint32_t byte_align,
					     const uint32_t frame_align_req,
					     uint32_t frame_size)
{
	/* Figure out how many frames are needed to meet the byte_align alignment requirements */
	uint32_t frame_num = byte_align / gcd(byte_align, frame_size);

	/** return the lcm of frame_num and frame_align_req*/
	return frame_align_req * frame_num / gcd(frame_num, frame_align_req);
}


void audio_stream_recalc_align(struct audio_stream *stream)
{
	const uint32_t byte_align = stream->byte_align_req;
	const uint32_t frame_align_req = stream->frame_align_req;
	uint32_t process_size;
	uint32_t frame_size = audio_stream_frame_bytes(stream);

	stream->runtime_stream_params.align_frame_cnt =
			audio_stream_frame_align_get(byte_align, frame_align_req, frame_size);
	process_size = stream->runtime_stream_params.align_frame_cnt * frame_size;
	stream->runtime_stream_params.align_shift_idx	=
			(is_power_of_2(process_size) ? 31 : 32) - clz(process_size);
}

void audio_stream_set_align(const uint32_t byte_align,
					   const uint32_t frame_align_req,
					   struct audio_stream *stream)
{
	stream->byte_align_req = byte_align;
	stream->frame_align_req = frame_align_req;
	audio_stream_recalc_align(stream);
}


static int audio_stream_release_data(struct sof_source *source, size_t free_size)
{
	struct audio_stream *audio_stream = container_of(source, struct audio_stream, _source_api);

	if (free_size)
		audio_stream_consume(audio_stream, free_size);

	return 0;
}

static int audio_stream_set_ipc_params_source(struct sof_source *source,
					      struct sof_ipc_stream_params *params,
					      bool force_update)
{
	struct audio_stream *audio_stream = container_of(source, struct audio_stream, _source_api);
	struct comp_buffer *buffer = container_of(audio_stream, struct comp_buffer, stream);

	return buffer_set_params(buffer, params, force_update);
}

static int audio_stream_set_ipc_params_sink(struct sof_sink *sink,
					    struct sof_ipc_stream_params *params,
					    bool force_update)
{
	struct audio_stream *audio_stream = container_of(sink, struct audio_stream, _sink_api);
	struct comp_buffer *buffer = container_of(audio_stream, struct comp_buffer, stream);

	return buffer_set_params(buffer, params, force_update);
}

static int audio_stream_source_set_alignment_constants(struct sof_source *source,
						       const uint32_t byte_align,
						       const uint32_t frame_align_req)
{
	struct audio_stream *audio_stream = container_of(source, struct audio_stream, _source_api);

	audio_stream_set_align(byte_align, frame_align_req, audio_stream);

	return 0;
}

static int audio_stream_sink_set_alignment_constants(struct sof_sink *sink,
						     const uint32_t byte_align,
						     const uint32_t frame_align_req)
{
	struct audio_stream *audio_stream = container_of(sink, struct audio_stream, _sink_api);

	audio_stream_set_align(byte_align, frame_align_req, audio_stream);

	return 0;
}

static int source_format_set(struct sof_source *source)
{
	struct audio_stream *s = container_of(source, struct audio_stream, _source_api);

	audio_stream_recalc_align(s);
	return 0;
}

static int sink_format_set(struct sof_sink *sink)
{
	struct audio_stream *s = container_of(sink, struct audio_stream, _sink_api);

	audio_stream_recalc_align(s);
	return 0;
}

static const struct source_ops audio_stream_source_ops = {
	.get_data_available = audio_stream_get_data_available,
	.get_data = audio_stream_get_data,
	.release_data = audio_stream_release_data,
	.audio_set_ipc_params = audio_stream_set_ipc_params_source,
	.on_audio_format_set = source_format_set,
	.set_alignment_constants = audio_stream_source_set_alignment_constants
};

static const struct sink_ops audio_stream_sink_ops = {
	.get_free_size = audio_stream_get_free_size,
	.get_buffer = audio_stream_get_buffer,
	.commit_buffer = audio_stream_commit_buffer,
	.audio_set_ipc_params = audio_stream_set_ipc_params_sink,
	.on_audio_format_set = sink_format_set,
	.set_alignment_constants = audio_stream_sink_set_alignment_constants
};

void audio_stream_init(struct audio_stream *audio_stream, void *buff_addr, uint32_t size)
{
	audio_stream->size = size;
	audio_stream->addr = buff_addr;
	audio_stream->end_addr = (char *)audio_stream->addr + size;

	audio_stream_set_align(1, 1, audio_stream);
	source_init(audio_stream_get_source(audio_stream), &audio_stream_source_ops,
		    &audio_stream->runtime_stream_params);
	sink_init(audio_stream_get_sink(audio_stream), &audio_stream_sink_ops,
		  &audio_stream->runtime_stream_params);
	audio_stream_reset(audio_stream);
}

/* get a handler to source API */
#if CONFIG_ZEPHYR_DP_SCHEDULER
struct sof_source *audio_stream_get_source(struct audio_stream *audio_stream)
{
	return audio_stream->dp_queue_source ?
			dp_queue_get_source(audio_stream->dp_queue_source) :
			&audio_stream->_source_api;
}

struct sof_sink *audio_stream_get_sink(struct audio_stream *audio_stream)
{
	return audio_stream->dp_queue_sink ?
			dp_queue_get_sink(audio_stream->dp_queue_sink) :
			&audio_stream->_sink_api;
}

#else /* CONFIG_ZEPHYR_DP_SCHEDULER */

struct sof_source *audio_stream_get_source(struct audio_stream *audio_stream)
{
	return &audio_stream->_source_api;
}

struct sof_sink *audio_stream_get_sink(struct audio_stream *audio_stream)
{
	return &audio_stream->_sink_api;
}
#endif /* CONFIG_ZEPHYR_DP_SCHEDULER */
