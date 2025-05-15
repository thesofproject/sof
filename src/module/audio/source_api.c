// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2023 Intel Corporation. All rights reserved.
 */

#include <module/audio/source_api.h>
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

/* This file contains public source API functions that were too large to mark is as inline. */

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
EXPORT_SYMBOL(source_get_data);

int source_get_data_s16(struct sof_source *source, size_t req_size, int16_t const **data_ptr,
			int16_t const **buffer_start, int *buffer_samples)
{
	size_t buffer_size;
	int ret;

	ret = source_get_data(source, req_size, (void const **)data_ptr,
			      (void const **)buffer_start, &buffer_size);
	if (ret)
		return ret;

	*buffer_samples = buffer_size >> 1;
	return 0;
}
EXPORT_SYMBOL(source_get_data_s16);

int source_get_data_s32(struct sof_source *source, size_t req_size, int32_t const **data_ptr,
			int32_t const **buffer_start, int *buffer_samples)
{
	size_t buffer_size;
	int ret;

	ret = source_get_data(source, req_size, (void const **)data_ptr,
			      (void const **)buffer_start, &buffer_size);
	if (ret)
		return ret;

	*buffer_samples = buffer_size >> 2;
	return 0;
}
EXPORT_SYMBOL(source_get_data_s32);

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
EXPORT_SYMBOL(source_release_data);

size_t source_get_frame_bytes(struct sof_source *source)
{
	return get_frame_bytes(source_get_frm_fmt(source),
				source_get_channels(source));
}
EXPORT_SYMBOL(source_get_frame_bytes);

size_t source_get_data_frames_available(struct sof_source *source)
{
	uint32_t frame_bytes = source_get_frame_bytes(source);

	if (frame_bytes > 0)
		return source_get_data_available(source) / frame_bytes;
	else
		return 0;
}
EXPORT_SYMBOL(source_get_data_frames_available);
