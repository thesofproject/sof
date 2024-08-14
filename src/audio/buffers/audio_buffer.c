// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2024 Intel Corporation. All rights reserved.
//
// Author: Marcin Szkudlinski <marcin.szkudlinski@intel.com>

#include <stdint.h>
#include <stddef.h>
#include <errno.h>
#include <rtos/alloc.h>
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
	if (buffer->ops->free)
		buffer->ops->free(buffer);
	rfree(buffer);
}
