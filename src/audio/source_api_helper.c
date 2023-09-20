// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2023 Intel Corporation. All rights reserved.
//

#include <sof/audio/source_api.h>
#include <sof/audio/source_api_implementation.h>
#include <sof/audio/audio_stream.h>

void source_init(struct sof_source *source, const struct source_ops *ops,
		 struct sof_audio_stream_params *audio_stream_params)
{
	source->ops = ops;
	source->requested_read_frag_size = 0;
	source->audio_stream_params = audio_stream_params;
}

size_t source_get_data_available(struct sof_source *source)
{
	return source->ops->get_data_available(source);
}

int source_get_data(struct sof_source *source, size_t req_size,
		    void const **data_ptr, void const **buffer_start, size_t *buffer_size)
{
	int ret;

	if (source->requested_read_frag_size)
		return -EBUSY;

	ret = source->ops->get_data(source, req_size, data_ptr, buffer_start, buffer_size);

	if (!ret)
		source->requested_read_frag_size = req_size;
	return ret;
}

int source_release_data(struct sof_source *source, size_t free_size)
{
	int ret;

	/* Check if anything was obtained before for reading by source_get_data */
	if (!source->requested_read_frag_size)
		return -ENODATA;

	/* limit size of data to be freed to previously obtained size */
	if (free_size > source->requested_read_frag_size)
		free_size = source->requested_read_frag_size;

	ret = source->ops->release_data(source, free_size);

	if (!ret)
		source->requested_read_frag_size = 0;

	source->num_of_bytes_processed += free_size;
	return ret;
}

size_t source_get_num_of_processed_bytes(struct sof_source *source)
{
	return source->num_of_bytes_processed;
}

void source_reset_num_of_processed_bytes(struct sof_source *source)
{
	source->num_of_bytes_processed = 0;
}

enum sof_ipc_frame source_get_frm_fmt(struct sof_source *source)
{
	return source->audio_stream_params->frame_fmt;
}

enum sof_ipc_frame source_get_valid_fmt(struct sof_source *source)
{
	return source->audio_stream_params->valid_sample_fmt;
}

unsigned int source_get_rate(struct sof_source *source)
{
	return source->audio_stream_params->rate;
}

unsigned int source_get_channels(struct sof_source *source)
{
	return source->audio_stream_params->channels;
}

uint32_t source_get_buffer_fmt(struct sof_source *source)
{
	return source->audio_stream_params->buffer_fmt;
}

bool source_get_underrun(struct sof_source *source)
{
	return source->audio_stream_params->underrun_permitted;
}

int source_set_valid_fmt(struct sof_source *source,
			 enum sof_ipc_frame valid_sample_fmt)
{
	source->audio_stream_params->valid_sample_fmt = valid_sample_fmt;
	if (source->ops->on_audio_format_set)
		return source->ops->on_audio_format_set(source);
	return 0;
}

int source_set_rate(struct sof_source *source, unsigned int rate)
{
	source->audio_stream_params->rate = rate;
	if (source->ops->on_audio_format_set)
		return source->ops->on_audio_format_set(source);
	return 0;
}

int source_set_channels(struct sof_source *source, unsigned int channels)
{
	source->audio_stream_params->channels = channels;
	if (source->ops->on_audio_format_set)
		return source->ops->on_audio_format_set(source);
	return 0;
}

int source_set_buffer_fmt(struct sof_source *source, uint32_t buffer_fmt)
{
	source->audio_stream_params->buffer_fmt = buffer_fmt;
	if (source->ops->on_audio_format_set)
		return source->ops->on_audio_format_set(source);
	return 0;
}

int source_set_underrun(struct sof_source *source, bool underrun_permitted)
{
	source->audio_stream_params->underrun_permitted = underrun_permitted;
	if (source->ops->on_audio_format_set)
		return source->ops->on_audio_format_set(source);
	return 0;
}

size_t source_get_frame_bytes(struct sof_source *source)
{
	return get_frame_bytes(source_get_frm_fmt(source),
				source_get_channels(source));
}

size_t source_get_data_frames_available(struct sof_source *source)
{
	return source_get_data_available(source) /
			source_get_frame_bytes(source);
}

int source_set_params(struct sof_source *source,
		      struct sof_ipc_stream_params *params, bool force_update)
{
	if (source->ops->audio_set_ipc_params)
		return source->ops->audio_set_ipc_params(source, params, force_update);
	return 0;
}

int source_set_alignment_constants(struct sof_source *source,
				   const uint32_t byte_align,
				   const uint32_t frame_align_req)
{
	if (source->ops->set_alignment_constants)
		return source->ops->set_alignment_constants(source, byte_align, frame_align_req);
	return 0;
}

void source_set_min_available(struct sof_source *source, size_t min_available)
{
	source->min_available = min_available;
}

size_t source_get_min_available(struct sof_source *source)
{
	return source->min_available;
}
