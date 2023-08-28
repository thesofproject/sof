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
#if CONFIG_IPC_MAJOR_4
#include <ipc4/base-config.h>
#endif
struct comp_buffer;
struct comp_dev;

/** \brief Supported streams count. */
#if CONFIG_IPC_MAJOR_3
#define MUX_MAX_STREAMS 4
#else
#define MUX_MAX_STREAMS 2
#endif
#define BASE_CFG_QUEUED_ID 0
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

	uint8_t reserved1[8 - PLATFORM_MAX_CHANNELS]; // padding for extra channels
	uint8_t reserved2[3]; // padding to ensure proper alignment of following instances
} __attribute__((packed, aligned(4)));

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
} __attribute__((packed, aligned(4)));

#if CONFIG_IPC_MAJOR_4
/**
 * \brief MUX module configuration in IPC4.
 *
 * This module output map is statically defined by the adapter (shim) as:
 *  - Input pin 0 channel "x" ("x" = 0.."M", "M" <=3) to output channel "x",
 *    where "M" is number of channels on input pin 0,
 *  - Input pin 1 (reference) channel "y" (y = 0..1) to output channel "M"+1+"y".
 *
 * If input pin 0 is not connected, module will not produce any output.
 * If input pin 1 (know also as reference pin) is not connected then module will
 * in output (for time slot meant for pin 1) generate zeros.
 *
 * Setting masks for streams is done according to the order of pins and channels.
 * First the first input stream, then the reference.
 * For example, for base config 2-channel and reference 2-channel, masks
 * should look like: mask[] = { 0b00000001, 0b00000010 } for the first
 * stream and mask[] = { 0b00000100, 0b00001000 } for the second
 * (reference)
 *           +---+           +---+
 *           | 0 |---------> | 0 |
 * INPUT     +---+           +---+
 * STREAM 0  | 1 |---------> | 1 |
 *           +---+           +---+  OUTPUT
 *                    +----> | 2 |  STREAM
 *           +---+    |      +---+
 *           | 0 |----+  +-> | 3 |
 * INPUT     +---+       |   +---+
 * STREAM 1  | 1 |-------+
 *           +---+
 */
struct mux_data {
	struct ipc4_base_module_cfg base_cfg;
	 //! Reference pin format.
	struct ipc4_audio_format reference_format;
	 //! Output pin format.
	struct ipc4_audio_format output_format;
};
#endif

struct comp_data {
#if CONFIG_IPC_MAJOR_4
	struct mux_data md;
#endif
	union {
		mux_func mux;
		demux_func demux;
	};

	struct mux_look_up lookup[MUX_MAX_STREAMS];
	struct mux_look_up active_lookup;
	struct comp_data_blob_handler *model_handler;
	struct sof_mux_config config; /* Keep last due to flexible array member in end */
};

struct comp_func_map {
	uint16_t frame_format;
	mux_func mux_proc_func;
	demux_func demux_proc_func;
};

extern const struct comp_func_map mux_func_map[];

void mux_prepare_look_up_table(struct processing_module *mod);
void demux_prepare_look_up_table(struct processing_module *mod);

mux_func mux_get_processing_function(struct processing_module *mod);
demux_func demux_get_processing_function(struct processing_module *mod);

#ifdef UNIT_TEST

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

void sys_comp_module_mux_interface_init(void);
void sys_comp_module_demux_interface_init(void);

#endif /* UNIT_TEST */

#endif /* CONFIG_COMP_MUX */

#endif /* __SOF_AUDIO_MUX_H__ */
