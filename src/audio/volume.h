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

/**
 * \file audio/volume.h
 * \brief Volume component header file
 * \authors Liam Girdwood <liam.r.girdwood@linux.intel.com>\n
 *          Keyon Jie <yang.jie@linux.intel.com>\n
 *          Tomasz Lauda <tomasz.lauda@linux.intel.com>
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

/** \brief Volume trace function. */
#define trace_volume(__e, ...)	trace_event(TRACE_CLASS_VOLUME, __e, ##__VA_ARGS__)

/** \brief Volume trace value function. */
#define tracev_volume(__e, ...)	tracev_event(TRACE_CLASS_VOLUME, __e, ##__VA_ARGS__)

/** \brief Volume trace error function. */
#define trace_volume_error(__e, ...)	trace_error(TRACE_CLASS_VOLUME, __e, ##__VA_ARGS__)

//** \brief Volume gain Qx.y integer x number of bits including sign bit. */
#define VOL_QXY_X 8

//** \brief Volume gain Qx.y fractional y number of bits. */
#define VOL_QXY_Y 16

/**
 * \brief Volume ramp update rate in microseconds.
 * Update volume gain value every 1 ms.
 */
#define VOL_RAMP_US 1000

/**
 * \brief Volume linear ramp length in milliseconds.
 * Use linear ramp length of 250 ms from mute to unity gain. The linear ramp
 * step in Q1.16 to use in vol_work function  is computed from the length.
 */
#define VOL_RAMP_LENGTH_MS 250
#define VOL_RAMP_STEP Q_CONVERT_FLOAT(1.0 / 1000 * \
				      VOL_RAMP_US / VOL_RAMP_LENGTH_MS, \
				      VOL_QXY_Y)

/**
 * \brief Volume maximum value.
 * TODO: This should be 1 << (VOL_QX_BITS + VOL_QY_BITS - 1) - 1 but
 * the current volume code cannot handle the full Q1.16 range correctly.
 */
#define VOL_MAX		((1 << (VOL_QXY_X + VOL_QXY_Y - 1)) - 1)

/** \brief Volume 0dB value. */
#define VOL_ZERO_DB	(1 << VOL_QXY_Y)

/** \brief Volume minimum value. */
#define VOL_MIN		0

/**
 * \brief Volume component private data.
 *
 * Gain amplitude value is between 0 (mute) ... 2^16 (0dB) ... 2^24 (~+48dB).
 */
struct comp_data {
	enum sof_ipc_frame source_format;	/**< source frame format */
	enum sof_ipc_frame sink_format;		/**< sink frame format */
	int32_t volume[SOF_IPC_MAX_CHANNELS];	/**< current volume */
	int32_t tvolume[SOF_IPC_MAX_CHANNELS];	/**< target volume */
	int32_t mvolume[SOF_IPC_MAX_CHANNELS];	/**< mute volume */
	/**< volume processing function */
	void (*scale_vol)(struct comp_dev *dev, struct comp_buffer *sink,
		struct comp_buffer *source, uint32_t frames);
	struct task volwork;	/**< volume scheduled work function */
	struct sof_ipc_ctrl_value_chan *hvol;	/**< host volume readback */
};

/** \brief Volume processing functions map. */
struct comp_func_map {
	uint16_t source;			/**< source frame format */
	uint16_t sink;				/**< sink frame format */
	/**< volume processing function */
	void (*func)(struct comp_dev *dev, struct comp_buffer *sink,
		struct comp_buffer *source, uint32_t frames);
};

/** \brief Map of formats with dedicated processing functions. */
extern const struct comp_func_map func_map[];

/** \brief Number of processing functions. */
extern const size_t func_count;

typedef void (*scale_vol)(struct comp_dev *, struct comp_buffer *,
			  struct comp_buffer *, uint32_t);

/**
 * \brief Retrievies volume processing function.
 * \param[in,out] dev Volume base component device.
 */
inline static scale_vol vol_get_processing_function(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int i;

	/* map the volume function for source and sink buffers */
	for (i = 0; i < func_count; i++) {
		if (cd->source_format != func_map[i].source)
			continue;
		if (cd->sink_format != func_map[i].sink)
			continue;

		return func_map[i].func;
	}

	return NULL;
}

#endif /* VOLUME_H */
