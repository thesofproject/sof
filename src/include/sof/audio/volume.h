/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2018 Intel Corporation. All rights reserved.
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

#ifndef __SOF_AUDIO_VOLUME_H__
#define __SOF_AUDIO_VOLUME_H__

#include <sof/audio/component.h>
#include <sof/bit.h>
#include <sof/schedule/schedule.h>
#include <sof/trace.h>
#include <ipc/stream.h>
#include <stddef.h>
#include <stdint.h>

struct comp_buffer;
struct sof_ipc_ctrl_value_chan;

#define CONFIG_GENERIC

#if defined(__XCC__)
#include <xtensa/config/core-isa.h>

#if XCHAL_HAVE_HIFI3
#undef CONFIG_GENERIC
#endif

#endif

/** \brief Volume trace function. */
#define trace_volume(__e, ...) \
	trace_event(TRACE_CLASS_VOLUME, __e, ##__VA_ARGS__)

/** \brief Volume trace value function. */
#define tracev_volume(__e, ...) \
	tracev_event(TRACE_CLASS_VOLUME, __e, ##__VA_ARGS__)

/** \brief Volume trace error function. */
#define trace_volume_error(__e, ...) \
	trace_error(TRACE_CLASS_VOLUME, __e, ##__VA_ARGS__)

//** \brief Volume gain Qx.y integer x number of bits including sign bit. */
#define VOL_QXY_X 8

//** \brief Volume gain Qx.y fractional y number of bits. */
#define VOL_QXY_Y 16

/**
 * \brief Volume ramp update rate in microseconds.
 * Update volume gain value every 1 ms.
 */
#define VOL_RAMP_UPDATE_US 1000

/**
 * \brief Macro for volume linear gain ramp step computation
 * Volume gain ramp step as Q1.16 is computed with equation
 * step = VOL_RAMP_STEP_CONST/ SOF_TKN_VOLUME_RAMP_STEP_MS. This
 * macro defines as Q1.16 value the constant term
 * (1000 / VOL_RAMP_UPDATE) for step calculation.
 */
#define VOL_RAMP_STEP_CONST \
	Q_CONVERT_FLOAT(1000.0 / VOL_RAMP_UPDATE_US, VOL_QXY_Y)

/**
 * \brief Volume maximum value.
 * TODO: This should be 1 << (VOL_QX_BITS + VOL_QY_BITS - 1) - 1 but
 * the current volume code cannot handle the full Q1.16 range correctly.
 */
#define VOL_MAX		((1 << (VOL_QXY_X + VOL_QXY_Y - 1)) - 1)

/** \brief Volume 0dB value. */
#define VOL_ZERO_DB	BIT(VOL_QXY_Y)

/** \brief Volume minimum value. */
#define VOL_MIN		0

/**
 * \brief Volume component private data.
 *
 * Gain amplitude value is between 0 (mute) ... 2^16 (0dB) ... 2^24 (~+48dB).
 */
struct comp_data {
	struct task volwork;		/**< volume scheduled work function */
	struct sof_ipc_ctrl_value_chan *hvol;	/**< host volume readback */
	int32_t volume[SOF_IPC_MAX_CHANNELS];	/**< current volume */
	int32_t tvolume[SOF_IPC_MAX_CHANNELS];	/**< target volume */
	int32_t mvolume[SOF_IPC_MAX_CHANNELS];	/**< mute volume */
	int32_t ramp_increment[SOF_IPC_MAX_CHANNELS]; /**< for linear ramp */
	int32_t vol_min;			/**< minimum volume */
	int32_t vol_max;			/**< maximum volume */
	int32_t	vol_ramp_range;			/**< max ramp transition */
	enum sof_ipc_frame source_format;	/**< source frame format */
	enum sof_ipc_frame sink_format;		/**< sink frame format */
	/**< volume processing function */
	void (*scale_vol)(struct comp_dev *dev, struct comp_buffer *sink,
			  struct comp_buffer *source, uint32_t frames);
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
static inline scale_vol vol_get_processing_function(struct comp_dev *dev)
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

#endif /* __SOF_AUDIO_VOLUME_H__ */
