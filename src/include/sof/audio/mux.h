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

struct mux_copy_elem {
	uint32_t stream_id;
	uint32_t in_ch;
	uint32_t out_ch;

	void *dest;
	void *src;

	uint32_t dest_inc;
	uint32_t src_inc;
};

struct mux_look_up {
	uint32_t num_elems;
	struct mux_copy_elem copy_elem[PLATFORM_MAX_CHANNELS];
};

struct mux_stream_data {
	uint32_t pipeline_id;
	uint8_t num_channels_deprecated;	/* deprecated in ABI 3.15 */
	uint8_t mask[PLATFORM_MAX_CHANNELS];

	uint8_t reserved[(20 - PLATFORM_MAX_CHANNELS - 1) % 4]; // padding to ensure proper alignment of following instances
};

typedef void(*demux_func)(struct comp_dev *dev, struct audio_stream *sink,
			  const struct audio_stream *source, uint32_t frames,
			  struct mux_look_up *look_up);
typedef void(*mux_func)(struct comp_dev *dev, struct audio_stream *sink,
			const struct audio_stream **sources, uint32_t frames,
			struct mux_look_up *look_up);

/**
 * \brief Mux/Demux component config structure.
 *
 * The multiplexer/demultiplexer component copies its input audio channels
 * into output audio channels according to a specific routing matrix.
 * Multiplexer has multiple input audio streams and a single audio output
 * stream. Demultiplexer has a single input stream and multiple output streams.
 *
 * Struct sof_mux_config includes array of mux_stream_data struct elements -
 * streams[]. Each element of streams[] array refers to streams on "many" side
 * of mux/demux component i.e. input streams for mux and output streams for
 * demux.
 *
 * Struct mux_stream_data consists mask[] array.
 * In the mux case, one mask[] element per input channel - each mask shows, to
 * which output channel data should be copied.
 * In the demux case, one mask[] element per output channel - each mask shows,
 * from which input channel data should be taken.
 *
 * Mux example:
 * Assuming that below mask array refers to x input stream:
 * mask[] = {	0b00000001,
 *		0b00000100 }
 * it means that:
 * - first input channel of stream x (mask[0]) will be copied to first output
 *   channel (0b00000001 & BIT(0));
 * - second input channel of stream x (mask[1]) will be copied to third output
 *   channel (0b00000100 & BIT(2)).
 *
 * Demux example:
 * Assuming that below mask array refers to x output stream:
 * mask[] = {	0b00000001,
 *		0b00000100 }
 * it means that:
 * - first input channel (0b00000001 & BIT(0)) will be copied to first output
 *   (mask[0]) channel of stream x;
 * - third input channel (0b00000100 & BIT(2)) will be copied to second output
 *   (mask[1]) channel of stream x.
 */
struct sof_mux_config {
	uint16_t frame_format_deprecated;	/* deprecated in ABI 3.15 */
	uint16_t num_channels_deprecated;	/* deprecated in ABI 3.15 */
	uint16_t num_streams;

	uint16_t reserved; // padding to ensure proper alignment

	struct mux_stream_data streams[];
};

struct comp_data {
	union {
		mux_func mux;
		demux_func demux;
	};

	struct mux_look_up lookup[MUX_MAX_STREAMS];
	struct mux_look_up active_lookup;
	struct sof_mux_config config;
};

struct comp_func_map {
	uint16_t frame_format;
	mux_func mux_proc_func;
	demux_func demux_proc_func;
};

extern const struct comp_func_map mux_func_map[];

void mux_prepare_look_up_table(struct comp_dev *dev);
void demux_prepare_look_up_table(struct comp_dev *dev);

mux_func mux_get_processing_function(struct comp_dev *dev);
demux_func demux_get_processing_function(struct comp_dev *dev);

#ifdef UNIT_TEST
void sys_comp_mux_init(void);

#if CONFIG_FORMAT_S16LE
int32_t calc_sample_s16le(const struct audio_stream *source,
			  uint32_t offset, uint8_t mask);
#endif /* CONFIG_FORMAT_S16LE */
#if CONFIG_FORMAT_S24LE
int32_t calc_sample_s24le(const struct audio_stream *source,
			  uint32_t offset, uint8_t mask);
#endif /* CONFIG_FORMAT_S24LE */
#if CONFIG_FORMAT_S32LE
int64_t calc_sample_s32le(const struct audio_stream *source,
			  uint32_t offset, uint8_t mask);
#endif /* CONFIG_FORMAT_S32LE */
#endif /* UNIT_TEST */

#endif /* CONFIG_COMP_MUX */

#endif /* __SOF_AUDIO_MUX_H__ */
