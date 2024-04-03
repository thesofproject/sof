// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2023 Intel Corporation. All rights reserved.
 */

#include <module/audio/sink_api.h>
#include <module/audio/audio_stream.h>
/*
 * When building native system-service modules only exported module headers can
 * be included, autoconf.h isn't included either. To identify such a build we
 * need any symbol that is guaranteed to be defined with any SOF build.
 * CONFIG_CORE_COUNT is such a symbol, it is always defined to an integer number
 */
#ifndef CONFIG_CORE_COUNT
#define EXPORT_SYMBOL(...)
#else
#include <rtos/symbol.h>
#endif

/* This file contains public sink API functions that were too large to mark is as inline. */

int sink_get_buffer(struct sof_sink *sink, size_t req_size,
		    void **data_ptr, void **buffer_start, size_t *buffer_size)
{
	int ret;

	if (sink->requested_write_frag_size)
		return -EBUSY;

	ret = sink->ops->get_buffer(sink, req_size, data_ptr,
						buffer_start, buffer_size);

	if (!ret)
		sink->requested_write_frag_size = req_size;
	return ret;
}
EXPORT_SYMBOL(sink_get_buffer);

int sink_commit_buffer(struct sof_sink *sink, size_t commit_size)
{
	int ret;

	/* check if there was a buffer obtained for writing by sink_get_buffer */
	if (!sink->requested_write_frag_size)
		return -ENODATA;

	/* limit size of data to be committed to previously obtained size */
	if (commit_size > sink->requested_write_frag_size)
		commit_size = sink->requested_write_frag_size;

	ret = sink->ops->commit_buffer(sink, commit_size);

	if (!ret)
		sink->requested_write_frag_size = 0;

	sink->num_of_bytes_processed += commit_size;
	return ret;
}
EXPORT_SYMBOL(sink_commit_buffer);

int sink_set_frm_fmt(struct sof_sink *sink, enum sof_ipc_frame frame_fmt)
{
	sink->audio_stream_params->frame_fmt = frame_fmt;

	/* notify the implementation */
	if (sink->ops->on_audio_format_set)
		return sink->ops->on_audio_format_set(sink);
	return 0;
}
EXPORT_SYMBOL(sink_set_frm_fmt);

size_t sink_get_frame_bytes(struct sof_sink *sink)
{
	return get_frame_bytes(sink_get_frm_fmt(sink), sink_get_channels(sink));
}
EXPORT_SYMBOL(sink_get_frame_bytes);

size_t sink_get_free_frames(struct sof_sink *sink)
{
	return sink_get_free_size(sink) / sink_get_frame_bytes(sink);
}
EXPORT_SYMBOL(sink_get_free_frames);

int sink_set_valid_fmt(struct sof_sink *sink, enum sof_ipc_frame valid_sample_fmt)
{
	sink->audio_stream_params->valid_sample_fmt = valid_sample_fmt;
	if (sink->ops->on_audio_format_set)
		return sink->ops->on_audio_format_set(sink);
	return 0;
}
EXPORT_SYMBOL(sink_set_valid_fmt);

int sink_set_rate(struct sof_sink *sink, unsigned int rate)
{
	sink->audio_stream_params->rate = rate;
	if (sink->ops->on_audio_format_set)
		return sink->ops->on_audio_format_set(sink);
	return 0;
}

int sink_set_channels(struct sof_sink *sink, unsigned int channels)
{
	sink->audio_stream_params->channels = channels;
	if (sink->ops->on_audio_format_set)
		return sink->ops->on_audio_format_set(sink);
	return 0;
}
EXPORT_SYMBOL(sink_set_channels);

int sink_set_buffer_fmt(struct sof_sink *sink, uint32_t buffer_fmt)
{
	sink->audio_stream_params->buffer_fmt = buffer_fmt;
	if (sink->ops->on_audio_format_set)
		return sink->ops->on_audio_format_set(sink);
	return 0;
}

int sink_set_overrun(struct sof_sink *sink, bool overrun_permitted)
{
	sink->audio_stream_params->overrun_permitted = overrun_permitted;
	if (sink->ops->on_audio_format_set)
		return sink->ops->on_audio_format_set(sink);
	return 0;
}

int sink_set_params(struct sof_sink *sink, struct sof_ipc_stream_params *params, bool force_update)
{
	if (sink->ops->audio_set_ipc_params)
		return sink->ops->audio_set_ipc_params(sink, params, force_update);
	return 0;
}

int sink_set_alignment_constants(struct sof_sink *sink, const uint32_t byte_align,
				 const uint32_t frame_align_req)
{
	if (sink->ops->set_alignment_constants)
		return sink->ops->set_alignment_constants(sink, byte_align, frame_align_req);
	return 0;
}
