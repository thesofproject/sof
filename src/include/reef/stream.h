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

/* maximum stream channels */
#define STREAM_MAX_CHANNELS	PLATFORM_MAX_CHANNELS

/* stream params type */
#define STREAM_PARAMS_TYPE_PCM		0
#define STREAM_PARAMS_TYPE_DMA		1
#define STREAM_PARAMS_TYPE_VORBIS	2

/* channel to stream position mapping */
struct stream_channel {
	uint16_t channel;
	uint16_t position;
};

/* PCM stream params */
struct stream_pcm_params {
	uint32_t rate;
	uint16_t format;
	uint8_t channels;
	uint8_t direction;
	uint16_t period_frames;
	uint16_t frame_size;
	struct stream_channel channel_map[STREAM_MAX_CHANNELS];	
};

/* DMA physical host ring buffer params */
struct stream_dma_ring_params {
	uint32_t ring_pt_address;
	uint32_t num_pages;
	uint32_t ring_size;
	uint32_t ring_offset;
	uint32_t ring_first_pfn;
};

/* DMA physical device params */
struct stream_dma_dev_params {
	uint32_t src;	/* TODO */
};

/* compressed vorbis stream params if required */
struct stream_vorbis_params {
	/* TODO */	
};

/* stream parameters */
struct stream_params {
	uint16_t type;		/* STREAM_PARAMS_TYPE_ */
	union {
		struct stream_pcm_params pcm;
		struct stream_dma_ring_params dma_ring;
		struct stream_dma_dev_params dma_dev;
		struct stream_vorbis_params vorbis;
	};
};

#endif
