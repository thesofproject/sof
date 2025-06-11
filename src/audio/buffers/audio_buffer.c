// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2024 Intel Corporation. All rights reserved.
//
// Author: Marcin Szkudlinski <marcin.szkudlinski@intel.com>

#include <stdint.h>
#include <stddef.h>
#include <errno.h>
#include <rtos/panic.h>
#include <rtos/alloc.h>
#include <ipc/stream.h>
#include <sof/audio/audio_buffer.h>
#include <sof/audio/sink_api.h>
#include <sof/audio/source_api.h>
#include <sof/audio/sink_source_utils.h>

#if CONFIG_PIPELINE_2_0

int audio_buffer_attach_secondary_buffer(struct sof_audio_buffer *buffer, bool at_input,
					 struct sof_audio_buffer *secondary_buffer)
{
	if (buffer->secondary_buffer_sink || buffer->secondary_buffer_source)
		return -EINVAL;

	/* secondary buffer must share audio params with the primary buffer */
	secondary_buffer->audio_stream_params = buffer->audio_stream_params;
	/* for performance reasons pointers to params are also kept in sink/src structures */
	secondary_buffer->_sink_api.audio_stream_params = buffer->audio_stream_params;
	secondary_buffer->_source_api.audio_stream_params = buffer->audio_stream_params;

	if (at_input)
		buffer->secondary_buffer_sink = secondary_buffer;
	else
		buffer->secondary_buffer_source = secondary_buffer;

	return 0;
}

int audio_buffer_sync_secondary_buffer(struct sof_audio_buffer *buffer, size_t limit)
{
	int err;

	struct sof_source *data_src;
	struct sof_sink *data_dst;

	if (buffer->secondary_buffer_sink) {
		/*
		 * audio_buffer sink API is shadowed, that means there's a secondary_buffer
		 * at data input
		 * get data from secondary_buffer (use source API)
		 * copy to primary buffer (use sink API)
		 * note! can't use  audio_buffer_get_sink because it will provide a shadowed
		 * sink handler (to a secondary buffer).
		 */
		data_src = audio_buffer_get_source(buffer->secondary_buffer_sink);
		data_dst = &buffer->_sink_api;	/* primary buffer's sink API */
	} else if (buffer->secondary_buffer_source) {
		/*
		 * comp_buffer source API is shadowed, that means there's a secondary_buffer
		 * at data output
		 * get data from comp_buffer (use source API)
		 * copy to secondary_buffer (use sink API)
		 */
		data_src = &buffer->_source_api;
		data_dst = audio_buffer_get_sink(buffer->secondary_buffer_source);

	} else {
		return -EINVAL;
	}

	/*
	 * keep data_available and free_size in local variables to avoid check_time/use_time
	 * race in MIN macro
	 */
	size_t data_available = source_get_data_available(data_src);
	size_t free_size = sink_get_free_size(data_dst);
	size_t to_copy = MIN(MIN(data_available, free_size), limit);

	err = source_to_sink_copy(data_src, data_dst, true, to_copy);
	return err;
}

#endif /* CONFIG_PIPELINE_2_0 */

void audio_buffer_free(struct sof_audio_buffer *buffer)
{
	if (!buffer)
		return;

	CORE_CHECK_STRUCT(buffer);
#if CONFIG_PIPELINE_2_0
	audio_buffer_free(buffer->secondary_buffer_sink);
	audio_buffer_free(buffer->secondary_buffer_source);
#endif /* CONFIG_PIPELINE_2_0 */
	/* "virtual destructor": free the buffer internals and buffer memory */
	buffer->ops->free(buffer);
}

static
int audio_buffer_source_set_ipc_params_default(struct sof_audio_buffer *buffer,
					       struct sof_ipc_stream_params *params,
					       bool force_update)
{
	CORE_CHECK_STRUCT(buffer);

	if (audio_buffer_hw_params_configured(buffer) && !force_update)
		return 0;

	struct sof_audio_stream_params *audio_stream_params =
		audio_buffer_get_stream_params(buffer);

	audio_stream_params->frame_fmt = params->frame_fmt;
	audio_stream_params->rate = params->rate;
	audio_stream_params->channels = params->channels;
	audio_stream_params->buffer_fmt = params->buffer_fmt;

	audio_buffer_set_hw_params_configured(buffer);

	if (buffer->ops->on_audio_format_set)
		return buffer->ops->on_audio_format_set(buffer);
	return 0;
}

static
int audio_buffer_sink_set_ipc_params(struct sof_sink *sink, struct sof_ipc_stream_params *params,
				     bool force_update)
{
	struct sof_audio_buffer *buffer = sof_audio_buffer_from_sink(sink);

	if (buffer->ops->audio_set_ipc_params)
		return buffer->ops->audio_set_ipc_params(buffer, params, force_update);
	return audio_buffer_source_set_ipc_params_default(buffer, params, force_update);
}

static
int audio_buffer_sink_on_audio_format_set(struct sof_sink *sink)
{
	struct sof_audio_buffer *buffer = sof_audio_buffer_from_sink(sink);

	if (buffer->ops->on_audio_format_set)
		return buffer->ops->on_audio_format_set(buffer);
	return 0;
}

static
int audio_buffer_sink_set_alignment_constants(struct sof_sink *sink,
					      const uint32_t byte_align,
					      const uint32_t frame_align_req)
{
	struct sof_audio_buffer *buffer = sof_audio_buffer_from_sink(sink);

	if (buffer->ops->set_alignment_constants)
		return buffer->ops->set_alignment_constants(buffer, byte_align, frame_align_req);
	return 0;
}

static
int audio_buffer_source_set_ipc_params(struct sof_source *source,
				       struct sof_ipc_stream_params *params, bool force_update)
{
	struct sof_audio_buffer *buffer = sof_audio_buffer_from_source(source);

	if (buffer->ops->audio_set_ipc_params)
		return buffer->ops->audio_set_ipc_params(buffer, params, force_update);
	return audio_buffer_source_set_ipc_params_default(buffer, params, force_update);
}

static
int audio_buffer_source_on_audio_format_set(struct sof_source *source)
{
	struct sof_audio_buffer *buffer = sof_audio_buffer_from_source(source);

	if (buffer->ops->on_audio_format_set)
		return buffer->ops->on_audio_format_set(buffer);
	return 0;
}

static
int audio_buffer_source_set_alignment_constants(struct sof_source *source,
						const uint32_t byte_align,
						const uint32_t frame_align_req)
{
	struct sof_audio_buffer *buffer = sof_audio_buffer_from_source(source);

	if (buffer->ops->set_alignment_constants)
		return buffer->ops->set_alignment_constants(buffer, byte_align, frame_align_req);
	return 0;
}

void audio_buffer_init(struct sof_audio_buffer *buffer, uint32_t buffer_type, bool is_shared,
		       struct source_ops *source_ops, struct sink_ops *sink_ops,
		       const struct audio_buffer_ops *audio_buffer_ops,
		       struct sof_audio_stream_params *audio_stream_params)
{
	CORE_CHECK_STRUCT_INIT(&buffer, is_shared);
	buffer->buffer_type = buffer_type;
	buffer->ops = audio_buffer_ops;
	assert(audio_buffer_ops->free);
	buffer->audio_stream_params = audio_stream_params;
	buffer->is_shared = is_shared;

	/* set default implementations of sink/source methods, if there's no
	 * specific implementation provided
	 */
	if (!sink_ops->audio_set_ipc_params)
		sink_ops->audio_set_ipc_params = audio_buffer_sink_set_ipc_params;
	if (!sink_ops->on_audio_format_set  && buffer->ops->on_audio_format_set)
		sink_ops->on_audio_format_set = audio_buffer_sink_on_audio_format_set;
	if (!sink_ops->set_alignment_constants && buffer->ops->set_alignment_constants)
		sink_ops->set_alignment_constants = audio_buffer_sink_set_alignment_constants;

	if (!source_ops->audio_set_ipc_params)
		source_ops->audio_set_ipc_params = audio_buffer_source_set_ipc_params;
	if (!source_ops->on_audio_format_set  && buffer->ops->on_audio_format_set)
		source_ops->on_audio_format_set = audio_buffer_source_on_audio_format_set;
	if (!source_ops->set_alignment_constants && buffer->ops->set_alignment_constants)
		source_ops->set_alignment_constants = audio_buffer_source_set_alignment_constants;

	source_init(audio_buffer_get_source(buffer), source_ops,
		    audio_buffer_get_stream_params(buffer));
	sink_init(audio_buffer_get_sink(buffer), sink_ops,
		  audio_buffer_get_stream_params(buffer));
}
