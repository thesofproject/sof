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

 /* tracing */
#define trace_mux(__e, ...) trace_event(TRACE_CLASS_MUX, __e, ##__VA_ARGS__)
#define trace_mux_with_ids(comp_ptr, __e, ...)			\
	trace_event_comp(TRACE_CLASS_MUX, comp_ptr,		\
			 __e, ##__VA_ARGS__)

#define tracev_mux(__e, ...) \
	tracev_event(TRACE_CLASS_MUX, __e, ##__VA_ARGS__)
#define tracev_mux_with_ids(comp_ptr, __e, ...)			\
	tracev_event_comp(TRACE_CLASS_MUX, comp_ptr,		\
			  __e, ##__VA_ARGS__)

#define trace_mux_error(__e, ...) \
	trace_error(TRACE_CLASS_MUX, __e, ##__VA_ARGS__)
#define trace_mux_error_with_ids(comp_ptr, __e, ...)		\
	trace_error_comp(TRACE_CLASS_MUX, comp_ptr,		\
			 __e, ##__VA_ARGS__)

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

typedef void(*demux_func)(struct comp_dev *dev, struct comp_buffer *sink,
			  const struct comp_buffer *source, uint32_t frames,
			  struct mux_stream_data *data);
typedef void(*mux_func)(struct comp_dev *dev, struct comp_buffer *sink,
			const struct comp_buffer **sources, uint32_t frames,
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
int32_t calc_sample_s16le(const struct comp_buffer *source,
			  uint8_t num_ch, uint32_t offset,
			  uint8_t mask);
#endif /* CONFIG_FORMAT_S16LE */
#if CONFIG_FORMAT_S24LE
int32_t calc_sample_s24le(const struct comp_buffer *source,
			  uint8_t num_ch, uint32_t offset,
			  uint8_t mask);
#endif /* CONFIG_FORMAT_S24LE */
#if CONFIG_FORMAT_S32LE
int64_t calc_sample_s32le(const struct comp_buffer *source,
			  uint8_t num_ch, uint32_t offset,
			  uint8_t mask);
#endif /* CONFIG_FORMAT_S32LE */
#endif /* UNIT_TEST */

#endif /* CONFIG_COMP_MUX */

#endif /* __SOF_AUDIO_MUX_H__ */
