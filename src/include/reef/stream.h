/*
 * Copyright (c) 2016, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the Intel Corporation nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 */

#ifndef __INCLUDE_STREAM__
#define __INCLUDE_STREAM__

#include <stdint.h>
#include <platform/platform.h>

/* stream type */
#define STREAM_TYPE_PCM		0
#define STREAM_TYPE_VORBIS	1
/* other compressed stream types here if supported */

/* supported format masks */
#define STREAM_FORMAT_S16_LE	1
#define STREAM_FORMAT_S24_3LE	2
#define STREAM_FORMAT_S24_4LE	4
#define STREAM_FORMAT_S32_LE	8

/* supported channel mappings */
#define STREAM_CHANNEL_MAP_MONO		0
#define STREAM_CHANNEL_MAP_LEFT		1
#define STREAM_CHANNEL_MAP_RIGHT	2

/* stream direction */
#define STREAM_DIRECTION_PLAYBACK	0
#define STREAM_DIRECTION_CAPTURE	1

/* maximum streams and channels */
#define STREAM_MAX_CHANNELS	PLATFORM_MAX_CHANNELS
#define STREAM_MAX_STREAMS	PLATFORM_MAX_STREAMS

/* stream params type */
#define STREAM_PARAMS_TYPE_PCM		0
#define STREAM_PARAMS_TYPE_DMA		1
#define STREAM_PARAMS_TYPE_VORBIS	2

/* channel to stream position mapping */
struct stream_channel {
	uint32_t channel;
	uint32_t position;
};

/* PCM stream params */
struct stream_pcm_params {
	uint32_t rate;
	uint32_t format;
	struct stream_channel channel_map[STREAM_MAX_CHANNELS];
};

/* compressed vorbis stream params if required */
struct stream_vorbis_params {
	/* TODO */
};

/* stream parameters */
struct stream_params {
	uint32_t type;		/* STREAM_PARAMS_TYPE_ */
	uint32_t direction;
	uint32_t channels;
	uint32_t period_frames;
	uint32_t frame_size;
	union {
		struct stream_pcm_params pcm;
		struct stream_vorbis_params vorbis;
	};
};

#endif
