/* 
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 */

#ifndef __INCLUDE_STREAM__
#define __INCLUDE_STREAM__

#include <stdint.h>

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

/* stream channels */
#define STREAM_MAX_CHANNELS	8

/* channel to stream position mapping */
struct stream_channel {
	uint16_t channel;
	uint16_t position;
};

/* PCM stream params */
struct stream_pcm_params {
	uint32_t rate;
	uint16_t format_mask;
	uint8_t channels;
	uint8_t direction;
	struct stream_channel channel_map[STREAM_MAX_CHANNELS];	
};

/* compressed vorbis stream params if required */
struct stream_vorbis_params {
	/* TODO */	
};

/* stream parameters */
struct stream_params {
	uint16_t type;		/* STREAM_TYPE_ */
	union {
		struct stream_pcm_params pcm;
		struct stream_vorbis_params vorbis;
	};
};

#endif
