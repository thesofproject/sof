/*
 * Copyright (c) 2018, Intel Corporation
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
 * Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

#ifndef VOLUME_H
#define VOLUME_H

#include <stdint.h>
#include <sof/audio/component.h>
#include <sof/audio/pipeline.h>
#include <sof/audio/format.h>

#define CONFIG_GENERIC

#if defined(__XCC__)
#include <xtensa/config/core-isa.h>

#if XCHAL_HAVE_HIFI3
#undef CONFIG_GENERIC
#endif

#endif

#define trace_volume(__e)	trace_event(TRACE_CLASS_VOLUME, __e)
#define tracev_volume(__e)	tracev_event(TRACE_CLASS_VOLUME, __e)
#define trace_volume_error(__e)	trace_error(TRACE_CLASS_VOLUME, __e)

/* this should ramp from 0dB to mute in 64ms.
 * i.e 2^16 -> 0 in 32 * 2048 steps each lasting 2ms
 */
#define VOL_RAMP_US	2000
#define VOL_RAMP_STEP	(1 << 11)
#define VOL_MAX		(1 << 16)
#define VOL_MIN		0

/* volume component private data */
struct comp_data {
	uint32_t source_period_bytes;
	uint32_t sink_period_bytes;
	enum sof_ipc_frame source_format;
	enum sof_ipc_frame sink_format;
	uint32_t volume[SOF_IPC_MAX_CHANNELS];	/* current volume */
	uint32_t tvolume[SOF_IPC_MAX_CHANNELS];	/* target volume */
	uint32_t mvolume[SOF_IPC_MAX_CHANNELS];	/* mute volume */
	void (*scale_vol)(struct comp_dev *dev, struct comp_buffer *sink,
			  struct comp_buffer *source);
	struct work volwork;

	/* host volume readback */
	struct sof_ipc_ctrl_value_chan *hvol;
};

struct comp_func_map {
	uint16_t source;	/* source format */
	uint16_t sink;		/* sink format */
	uint16_t channels;	/* channel number for the stream */
	void (*func)(struct comp_dev *dev, struct comp_buffer *sink,
		     struct comp_buffer *source);
};

/* map of source and sink buffer formats to volume function */
extern const struct comp_func_map func_map[];

typedef void (*scale_vol)(struct comp_dev *, struct comp_buffer *,
			  struct comp_buffer *);

scale_vol vol_get_processing_function(struct comp_dev *dev);

#endif /* VOLUME_H */
