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

/* supported formats */
#define STREAM_FORMAT_S16_LE	1
#define STREAM_FORMAT_S24_3LE	2
#define STREAM_FORMAT_S24_4LE	3
#define STREAM_FORMAT_S32_LE	4

/* supported channel mappings */
#define STREAM_CHANNEL_MAP_MONO		0
#define STREAM_CHANNEL_MAP_LEFT		1
#define STREAM_CHANNEL_MAP_RIGHT	2

/* stream direction */
#define STREAM_DIRECTION_PLAYBACK	0
#define STREAM_DIRECTION_CAPTURE	1

struct channel {
	uint16_t channel;
	uint16_t position;
};

struct stream {
	uint32_t rate;
	uint16_t format;
	uint8_t channels;
	uint8_t direction;
	struct channel map[0];	
};

#endif
