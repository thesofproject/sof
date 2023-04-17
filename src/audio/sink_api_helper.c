// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2023 Intel Corporation. All rights reserved.
//

#include <sof/audio/sink_api.h>
#include <sof/audio/sink_api_implementation.h>

void sink_init(struct sink __sparse_cache *sink, const struct sink_ops *ops,
	       struct sof_audio_stream_params *audio_stream_params)
{
	sink->ops = ops;
	sink->audio_stream_params = audio_stream_params;
}

size_t sink_get_free_size(struct sink __sparse_cache *sink)
{
	return sink->ops->get_free_size(sink);
}

int sink_get_buffer(struct sink __sparse_cache *sink, size_t req_size,
		    void __sparse_cache **data_ptr, void __sparse_cache **buffer_start,
		    size_t *buffer_size)
{
	int ret;

	if (sink->locked_write_frag_size)
		return -EBUSY;

	ret = sink->ops->get_buffer(sink, req_size, data_ptr,
						buffer_start, buffer_size);

	if (!ret)
		sink->locked_write_frag_size = req_size;
	return ret;
}

int sink_commit_buffer(struct sink __sparse_cache *sink, size_t commit_size)
{
	int ret;

	/* check if there was a buffer obtained for writing by sink_get_buffer */
	if (!sink->locked_write_frag_size)
		return -ENODATA;

	/* limit size of data to be committed to previously obtained size */
	if (commit_size > sink->locked_write_frag_size)
		commit_size = sink->locked_write_frag_size;

	ret = sink->ops->commit_buffer(sink, commit_size);

	if (!ret)
		sink->locked_write_frag_size = 0;

	sink->num_of_bytes_processed += commit_size;
	return ret;
}

size_t sink_get_num_of_processed_bytes(struct sink __sparse_cache *sink)
{
	return sink->num_of_bytes_processed;
}

void sink_reset_num_of_processed_bytes(struct sink __sparse_cache *sink)
{
	sink->num_of_bytes_processed = 0;
}

enum sof_ipc_frame sink_get_frm_fmt(struct sink __sparse_cache *sink)
{
	return sink->audio_stream_params->frame_fmt;
}

enum sof_ipc_frame sink_get_valid_fmt(struct sink __sparse_cache *sink)
{
	return sink->audio_stream_params->valid_sample_fmt;
}

uint32_t sink_get_rate(struct sink __sparse_cache *sink)
{
	return sink->audio_stream_params->rate;
}

uint32_t sink_get_channels(struct sink __sparse_cache *sink)
{
	return sink->audio_stream_params->channels;
}

bool sink_get_overrun(struct sink __sparse_cache *sink)
{
	return sink->audio_stream_params->overrun_permitted;
}

void sink_set_frm_fmt(struct sink __sparse_cache *sink, enum sof_ipc_frame frame_fmt)
{
	sink->audio_stream_params->frame_fmt = frame_fmt;

	/* notify the implementation */
	if (sink->ops->on_audio_format_set)
		sink->ops->on_audio_format_set(sink);
}

void sink_set_valid_fmt(struct sink __sparse_cache *sink,
			enum sof_ipc_frame valid_sample_fmt)
{
	sink->audio_stream_params->valid_sample_fmt = valid_sample_fmt;
	if (sink->ops->on_audio_format_set)
		sink->ops->on_audio_format_set(sink);
}

void sink_set_rate(struct sink __sparse_cache *sink, unsigned int rate)
{
	sink->audio_stream_params->rate = rate;
	if (sink->ops->on_audio_format_set)
		sink->ops->on_audio_format_set(sink);
}

void sink_set_channels(struct sink __sparse_cache *sink, unsigned int channels)
{
	sink->audio_stream_params->channels = channels;
	if (sink->ops->on_audio_format_set)
		sink->ops->on_audio_format_set(sink);
}

void sink_set_overrun(struct sink __sparse_cache *sink, bool overrun_permitted)
{
	sink->audio_stream_params->overrun_permitted = overrun_permitted;
	if (sink->ops->on_audio_format_set)
		sink->ops->on_audio_format_set(sink);
}

size_t sink_get_frame_bytes(struct sink __sparse_cache *sink)
{
	return get_frame_bytes(sink_get_frm_fmt(sink),
				sink_get_channels(sink));
}

size_t sink_get_free_frames(struct sink __sparse_cache *sink)
{
	return sink_get_free_size(sink) /
			sink_get_frame_bytes(sink);
}
