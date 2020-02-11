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
#include <sof/schedule/task.h>
#include <sof/trace/trace.h>
#include <ipc/stream.h>
#include <user/trace.h>
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
 * step = VOL_RAMP_STEP_CONST / SOF_TKN_VOLUME_RAMP_STEP_MS. This
 * macro defines as Q1.16 value the constant term
 * VOL_RAMP_UPDATE / 1000.0 for step calculation. The value 1000
 * is used to to convert microseconds to milliseconds.
 */
#define VOL_RAMP_STEP_CONST \
	Q_CONVERT_FLOAT(VOL_RAMP_UPDATE_US / 1000.0, VOL_QXY_Y)

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
 * \brief volume processing function interface
 */
typedef void (*vol_scale_func)(struct comp_dev *dev, struct audio_stream *sink,
			       const struct audio_stream *source,
			       uint32_t frames);
/**
 * \brief Volume component private data.
 *
 * Gain amplitude value is between 0 (mute) ... 2^16 (0dB) ... 2^24 (~+48dB).
 */
struct comp_data {
	struct task *volwork;		/**< volume scheduled work function */
	struct sof_ipc_ctrl_value_chan *hvol;	/**< host volume readback */
	int32_t volume[SOF_IPC_MAX_CHANNELS];	/**< current volume */
	int32_t tvolume[SOF_IPC_MAX_CHANNELS];	/**< target volume */
	int32_t mvolume[SOF_IPC_MAX_CHANNELS];	/**< mute volume */
	int32_t ramp_increment[SOF_IPC_MAX_CHANNELS]; /**< for linear ramp */
	int32_t vol_min;			/**< minimum volume */
	int32_t vol_max;			/**< maximum volume */
	int32_t	vol_ramp_range;			/**< max ramp transition */
	unsigned int channels;			/**< current channels count */
	bool muted[SOF_IPC_MAX_CHANNELS];	/**< set if channel is muted */
	bool vol_ramp_active;			/**< set if volume is ramped */
	bool ramp_started;			/**< control ramp launch */
	vol_scale_func scale_vol;	/**< volume processing function */
};

/** \brief Volume processing functions map. */
struct comp_func_map {
	uint16_t frame_fmt;	/**< frame format */
	vol_scale_func func;	/**< volume processing function */
};

/** \brief Map of formats with dedicated processing functions. */
extern const struct comp_func_map func_map[];

/** \brief Number of processing functions. */
extern const size_t func_count;

/**
 * \brief Retrievies volume processing function.
 * \param[in,out] dev Volume base component device.
 */
static inline vol_scale_func vol_get_processing_function(struct comp_dev *dev)
{
	struct comp_buffer *sinkb;
	int i;

	sinkb = list_first_item(&dev->bsink_list, struct comp_buffer,
				source_list);

	/* map the volume function for source and sink buffers */
	for (i = 0; i < func_count; i++) {
		if (sinkb->stream.frame_fmt != func_map[i].frame_fmt)
			continue;

		return func_map[i].func;
	}

	return NULL;
}

#endif /* __SOF_AUDIO_VOLUME_H__ */
