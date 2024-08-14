// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2023 Intel Corporation. All rights reserved.
 */

#include <sof/audio/sink_api.h>
#include <sof/audio/source_api.h>
#include <sof/audio/audio_stream.h>

/* This file contains private source API functions intended for use only by the sof. */

void source_init(struct sof_source *source, const struct source_ops *ops,
		 struct sof_audio_stream_params *audio_stream_params)
{
	source->ops = ops;
	source->requested_read_frag_size = 0;
	source->audio_stream_params = audio_stream_params;
}

size_t source_get_num_of_processed_bytes(struct sof_source *source)
{
	return source->num_of_bytes_processed;
}

void source_reset_num_of_processed_bytes(struct sof_source *source)
{
	source->num_of_bytes_processed = 0;
}

bool source_get_underrun(struct sof_source *source)
{
	return source->audio_stream_params->underrun_permitted;
}

int source_set_frm_fmt(struct sof_source *source, enum sof_ipc_frame frm_fmt)
{
	source->audio_stream_params->frame_fmt = frm_fmt;
	if (source->ops->on_audio_format_set)
		return source->ops->on_audio_format_set(source);
	return 0;
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
