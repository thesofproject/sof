/*
 * Copyright (c) 2019, Intel Corporation
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
 * Author: Artur Kloniecki <arturx.kloniecki@linux.intel.com>
 */

/**
 * \file audio/mux.h
 * \brief Multiplexer component header file
 * \authors Artur Kloniecki <arturx.kloniecki@linux.intel.com>
 */

#ifndef MUX_H
#define MUX_H

#include <config.h>

#if CONFIG_COMP_MUX

#include <stdint.h>
#include <sof/audio/component.h>
#include <sof/audio/pipeline.h>
#include <sof/audio/format.h>

 /* tracing */
#define trace_mux(__e, ...) trace_event(TRACE_CLASS_MUX, __e, ##__VA_ARGS__)
#define trace_mux_error(__e, ...) \
	trace_error(TRACE_CLASS_MUX, __e, ##__VA_ARGS__)
#define tracev_mux(__e, ...) \
	tracev_event(TRACE_CLASS_MUX, __e, ##__VA_ARGS__)

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
			struct comp_buffer *source, uint32_t frames,
			struct mux_stream_data *data);
typedef void(*mux_func)(struct comp_dev *dev, struct comp_buffer *sink,
			  struct comp_buffer **sources, uint32_t frames,
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

#endif /* CONFIG_COMP_MUX */

#endif /* MUX_H */
