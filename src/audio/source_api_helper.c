// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2023 Intel Corporation. All rights reserved.
//

#include <sof/audio/source_api.h>
#include <sof/audio/source_api_implementation.h>

void source_init(struct source __sparse_cache *source, const struct source_ops *ops,
		 struct sof_audio_stream_params *audio_stream_params)
{
	source->ops = ops;
	source->locked_read_frag_size = 0;
	source->audio_stream_params = audio_stream_params;
}

size_t source_get_data_available(struct source __sparse_cache *source)
{
	return source->ops->get_data_available(source);
}

int source_get_data(struct source __sparse_cache *source, size_t req_size,
		    void __sparse_cache **data_ptr, void __sparse_cache **buffer_start,
		    size_t *buffer_size)
{
	int ret;

	if (source->locked_read_frag_size)
		return -EBUSY;

	ret = source->ops->get_data(source, req_size, data_ptr, buffer_start, buffer_size);

	if (!ret)
		source->locked_read_frag_size = req_size;
	return ret;
}

int source_put_data(struct source __sparse_cache *source, size_t free_size)
{
	int ret;

	/* Check if anything was obtained before for reading by source_get_data */
	if (!source->locked_read_frag_size)
		return -ENODATA;

	/* limit size of data to be freed to previously obtained size */
	if (free_size > source->locked_read_frag_size)
		free_size = source->locked_read_frag_size;

	ret = source->ops->put_data(source, free_size);

	if (!ret)
		source->locked_read_frag_size = 0;

	source->num_of_bytes_processed += free_size;
	return ret;
}

size_t source_get_num_of_processed_bytes(struct source __sparse_cache *source)
{
	return source->num_of_bytes_processed;
}

void source_reset_num_of_processed_bytes(struct source __sparse_cache *source)
{
	source->num_of_bytes_processed = 0;
}

enum sof_ipc_frame source_get_frm_fmt(struct source __sparse_cache *source)
{
	return source->audio_stream_params->frame_fmt;
}

enum sof_ipc_frame source_get_valid_fmt(struct source __sparse_cache *source)
{
	return source->audio_stream_params->valid_sample_fmt;
}

unsigned int source_get_rate(struct source __sparse_cache *source)
{
	return source->audio_stream_params->rate;
}

unsigned int source_get_channels(struct source __sparse_cache *source)
{
	return source->audio_stream_params->channels;
}

bool source_get_underrun(struct source __sparse_cache *source)
{
	return source->audio_stream_params->underrun_permitted;
}

void source_set_valid_fmt(struct source __sparse_cache *source,
			  enum sof_ipc_frame valid_sample_fmt)
{
	source->audio_stream_params->valid_sample_fmt = valid_sample_fmt;
	if (source->ops->on_audio_format_set)
		source->ops->on_audio_format_set(source);
}

void source_set_rate(struct source __sparse_cache *source, unsigned int rate)
{
	source->audio_stream_params->rate = rate;
	if (source->ops->on_audio_format_set)
		source->ops->on_audio_format_set(source);
}

void source_set_channels(struct source __sparse_cache *source, unsigned int channels)
{
	source->audio_stream_params->channels = channels;
	if (source->ops->on_audio_format_set)
		source->ops->on_audio_format_set(source);
}

void source_set_underrun(struct source __sparse_cache *source, bool underrun_permitted)
{
	source->audio_stream_params->underrun_permitted = underrun_permitted;
	if (source->ops->on_audio_format_set)
		source->ops->on_audio_format_set(source);
}

size_t source_get_frame_bytes(struct source __sparse_cache *source)
{
	return get_frame_bytes(source_get_frm_fmt(source),
				source_get_channels(source));
}

size_t source_get_data_frames_available(struct source __sparse_cache *source)
{
	return source_get_data_available(source) /
			source_get_frame_bytes(source);
}

