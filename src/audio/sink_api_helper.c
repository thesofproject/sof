// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2023 Intel Corporation. All rights reserved.
 */

#include <sof/audio/sink_api.h>
#include <sof/audio/audio_stream.h>

/* This file contains private sink API functions intended for use only by the sof. */

void sink_init(struct sof_sink *sink, const struct sink_ops *ops,
	       struct sof_audio_stream_params *audio_stream_params)
{
	sink->ops = ops;
	sink->audio_stream_params = audio_stream_params;
}

size_t sink_get_num_of_processed_bytes(struct sof_sink *sink)
{
	return sink->num_of_bytes_processed;
}

void sink_reset_num_of_processed_bytes(struct sof_sink *sink)
{
	sink->num_of_bytes_processed = 0;
}

bool sink_get_overrun(struct sof_sink *sink)
{
	return sink->audio_stream_params->overrun_permitted;
}

void sink_set_min_free_space(struct sof_sink *sink, size_t min_free_space)
{
	sink->min_free_space = min_free_space;
}
