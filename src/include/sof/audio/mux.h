/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 *
 * Author: Artur Kloniecki <arturx.kloniecki@linux.intel.com>
 */

/**
 * \file include/sof/audio/mux.h
 * \brief Multiplexer component header file
 * \authors Artur Kloniecki <arturx.kloniecki@linux.intel.com>
 */

#ifndef __SOF_AUDIO_MUX_H__
#define __SOF_AUDIO_MUX_H__

#include <config.h>

#if CONFIG_COMP_MUX

#include <sof/common.h>
#include <sof/platform.h>
#include <sof/trace/trace.h>
#include <sof/ut.h>
#include <user/trace.h>
#include <stdint.h>

struct comp_buffer;
struct comp_dev;

/** \brief Supported streams count. */
#define MUX_MAX_STREAMS 4

/** guard against invalid amount of streams defined */
STATIC_ASSERT(MUX_MAX_STREAMS < PLATFORM_MAX_STREAMS,
	      unsupported_amount_of_streams_for_mux);

struct mux_stream_data {
	uint32_t pipeline_id;
	uint8_t num_channels;
	uint8_t mask[PLATFORM_MAX_CHANNELS];

	uint8_t reserved[(20 - PLATFORM_MAX_CHANNELS - 1) % 4]; // padding to ensure proper alignment of following instances
};

typedef void(*demux_func)(const struct comp_dev *dev, struct audio_stream *sink,
			  const struct audio_stream *source, uint32_t frames,
			  struct mux_stream_data *data);
typedef void(*mux_func)(const struct comp_dev *dev, struct audio_stream *sink,
			const struct audio_stream **sources, uint32_t frames,
			struct mux_stream_data *data);

struct sof_mux_config {
	uint16_t frame_format;
	uint16_t num_channels;
	uint16_t num_streams;

	uint16_t reserved; // padding to ensure proper alignment

	struct mux_stream_data streams[];
};

struct comp_data {
	union {
		mux_func mux;
		demux_func demux;
	};

	struct sof_mux_config config;
};

struct comp_func_map {
	uint16_t frame_format;
	mux_func mux_proc_func;
	demux_func demux_proc_func;
};

extern const struct comp_func_map mux_func_map[];

mux_func mux_get_processing_function(struct comp_dev *dev);
demux_func demux_get_processing_function(struct comp_dev *dev);

#ifdef UNIT_TEST
void sys_comp_mux_init(void);

#if CONFIG_FORMAT_S16LE
int32_t calc_sample_s16le(const struct audio_stream *source,
			  uint8_t num_ch, uint32_t offset,
			  uint8_t mask);
#endif /* CONFIG_FORMAT_S16LE */
#if CONFIG_FORMAT_S24LE
int32_t calc_sample_s24le(const struct audio_stream *source,
			  uint8_t num_ch, uint32_t offset,
			  uint8_t mask);
#endif /* CONFIG_FORMAT_S24LE */
#if CONFIG_FORMAT_S32LE
int64_t calc_sample_s32le(const struct audio_stream *source,
			  uint8_t num_ch, uint32_t offset,
			  uint8_t mask);
#endif /* CONFIG_FORMAT_S32LE */
#endif /* UNIT_TEST */

#endif /* CONFIG_COMP_MUX */

#endif /* __SOF_AUDIO_MUX_H__ */
