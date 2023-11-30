/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright 2022 Intel Corporation. All rights reserved.
 */

/*
 * This file contains structures that are exact copies of an existing ABI used
 * by IOT middleware. They are Intel specific and will be used by one middleware.
 *
 * Some of the structures may contain programming implementations that makes them
 * unsuitable for generic use and general usage.
 *
 * This code is mostly copied "as-is" from existing C++ interface files hence the use of
 * different style in places. The intention is to keep the interface as close as possible to
 * original so it's easier to track changes with IPC host code.
 */

/**
 * \file include/ipc4/mixin_mixout.h
 * \brief IPC4 mixin/mixout definitions.
 */

#ifndef __SOF_IPC4_MIXIN_MIXOUT_H__
#define __SOF_IPC4_MIXIN_MIXOUT_H__

#include <stdint.h>
#include <rtos/bit.h>
#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/audio/format.h>
#include <sof/platform.h>
#include <stddef.h>

#define MIXIN_MIXOUT_GENERIC

#if defined(__XCC__)

#include <xtensa/config/core-isa.h>
#if XCHAL_HAVE_HIFI3 || XCHAL_HAVE_HIFI4
#undef MIXIN_MIXOUT_GENERIC
#define MIXIN_MIXOUT_HIFI3
#endif

#endif

enum ipc4_mixin_config_param {
	/* large_config_set param id for ipc4_mixer_mode_config */
	IPC4_MIXER_MODE = 1
};

/* Number of supported output pins (sinks) */
#define IPC4_MIXIN_MODULE_MAX_OUTPUT_QUEUES 3

/* Number of supported input pins that are mixed together */
#define IPC4_MIXOUT_MODULE_MAX_INPUT_QUEUES 8

enum ipc4_mixer_mode {
	/* Normal mode, just mixing */
	IPC4_MIXER_NORMAL_MODE = 0,

	/* Mixing with channel remapping */
	IPC4_MIXER_CHANNEL_REMAPPING_MODE = 1,
};

struct ipc4_mixer_mode_sink_config {
	/* Index of output queue (aka sink) this config is for,
	 * range from 0 to IPC4_MIXIN_MODULE_MAX_OUTPUT_QUEUES - 1
	 */
	uint32_t output_queue_id;

	/* Operational mode for given output queue index. enum ipc4_mixer_mode */
	uint32_t mixer_mode;

	/* These two below are used in channel remapping mode. */
	uint32_t output_channel_count;

	/* Output channel map for given output queue index. Each nibble (where nibble index is
	 * equivalent for output channel index) contains source channel index. Value 0xF in nibble
	 * means that output channel cannot be modified.
	 */
	uint32_t output_channel_map;

	/* Gain to be applied to input signal. Valid range: 0x0..0x400 (0.0 <= gain <= 1.0). Values
	 * greater than 0x400 are treated as 0x400 (unity gain). To apply gain, multiply sample
	 * by "gain" and divide by 1024.
	 */
	uint16_t gain;
	uint16_t reserved;
} __packed __aligned(4);

#define IPC4_MIXIN_GAIN_SHIFT 10
#define IPC4_MIXIN_UNITY_GAIN BIT(IPC4_MIXIN_GAIN_SHIFT)

/* Payload for large_config_set IPC4_MIXER_MODE param id */
struct ipc4_mixer_mode_config {
	/* Total number of given ipc4_mixer_mode_sink_config. Size of passed structure is
	 * determined by this number.
	 */
	uint32_t mixer_mode_config_count;

	/* Array of settings for sinks, size is mixer_mode_config_count. */
	struct ipc4_mixer_mode_sink_config mixer_mode_sink_configs[1];
} __packed __aligned(4);

/**
 * \brief normal mode mixin_mixout processing function interface
 */
typedef void (*normal_mix_func)(struct audio_stream *sink, int32_t start_frame,
				int32_t mixed_frames,
				const struct audio_stream *source,
				int32_t frame_count, uint16_t gain);

/**
 * \brief mixin_mixout mute processing function interface
 */
typedef void (*mute_func) (struct audio_stream *stream, int32_t channel_index,
			     int32_t start_frame, int32_t mixed_frames, int32_t frame_count);

/**
 * @brief mixin_mixout processing functions map.
 */
struct mix_func_map {
	uint16_t frame_fmt;				/* frame format */
	normal_mix_func normal_func;	/* normal mode mixin_mixout processing function */
	mute_func mute_func;			/* mute processing function */
};

extern const struct mix_func_map mix_func_map[];
extern const size_t mix_count;
/**
 * \brief Retrievies normal mode mixer processing function.
 * \param[in] fmt  stream PCM frame format
 */
static inline normal_mix_func normal_mix_get_processing_function(int fmt)
{
	int i;

	/* map the normal mode mixin_mixout function for source and sink buffers */
	for (i = 0; i < mix_count; i++) {
		if (fmt == mix_func_map[i].frame_fmt)
			return mix_func_map[i].normal_func;
	}

	return NULL;
}

/**
 * \brief Retrievies normal mode mixer processing function.
 * \param[in] fmt  stream PCM frame format
 */
static inline mute_func mute_mix_get_processing_function(int fmt)
{
	int i;

	/* map the mute function for source and sink buffers */
	for (i = 0; i < mix_count; i++) {
		if (fmt == mix_func_map[i].frame_fmt)
			return mix_func_map[i].mute_func;
	}

	return NULL;
}

#endif	/* __SOF_IPC4_MIXIN_MIXOUT_H__ */
