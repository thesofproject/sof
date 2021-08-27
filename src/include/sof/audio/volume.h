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
#include <sof/audio/ipc-config.h>
#include <sof/bit.h>
#include <sof/common.h>
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
 * Update volume gain value every 125 to 1000 us. The faster gain ramps need
 * higher update rate to avoid annoying zipper noise sound. The below
 * values were tested subjectively for constraint of 125 microseconds
 * multiple gain update rate.
 */
#define VOL_RAMP_UPDATE_SLOWEST_US	1000
#define VOL_RAMP_UPDATE_SLOW_US		500
#define VOL_RAMP_UPDATE_FAST_US		250
#define VOL_RAMP_UPDATE_FASTEST_US	125

#define VOL_RAMP_UPDATE_THRESHOLD_SLOW_MS	128
#define VOL_RAMP_UPDATE_THRESHOLD_FAST_MS	64
#define VOL_RAMP_UPDATE_THRESHOLD_FASTEST_MS	32

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
 * \brief volume interface for function getting nearest zero crossing frame
 */
typedef uint32_t (*vol_zc_func)(const struct audio_stream *source,
				uint32_t frames, int64_t *prev_sum);

/**
 * \brief Function for volume ramp shape function
 */

typedef int32_t (*vol_ramp_func)(struct comp_dev *dev, int32_t ramp_time, int channel);

/**
 * \brief Volume component private data.
 *
 * Gain amplitude value is between 0 (mute) ... 2^16 (0dB) ... 2^24 (~+48dB).
 */
struct vol_data {
	struct sof_ipc_ctrl_value_chan *hvol;	/**< host volume readback */
	int32_t volume[SOF_IPC_MAX_CHANNELS];	/**< current volume */
	int32_t tvolume[SOF_IPC_MAX_CHANNELS];	/**< target volume */
	int32_t mvolume[SOF_IPC_MAX_CHANNELS];	/**< mute volume */
	int32_t rvolume[SOF_IPC_MAX_CHANNELS];	/**< ramp start volume */
	int32_t ramp_coef[SOF_IPC_MAX_CHANNELS]; /**< parameter for slope */
	struct ipc_config_volume ipc_config;
	int32_t vol_min;			/**< minimum volume */
	int32_t vol_max;			/**< maximum volume */
	int32_t	vol_ramp_range;			/**< max ramp transition */
	/**< max number of frames to process per ramp transition */
	uint32_t vol_ramp_frames;
	uint32_t vol_ramp_elapsed_frames;	/**< frames since transition */
	uint32_t sample_rate;			/**< stream sample rate in Hz */
	unsigned int channels;			/**< current channels count */
	bool muted[SOF_IPC_MAX_CHANNELS];	/**< set if channel is muted */
	bool vol_ramp_active;			/**< set if volume is ramped */
	bool ramp_finished;			/**< control ramp launch */
	vol_scale_func scale_vol;		/**< volume processing function */
	vol_zc_func zc_get;			/**< function getting nearest zero crossing frame */
	vol_ramp_func ramp_func;		/**< function for ramp shape */
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

/** \brief Volume zero crossing functions map. */
struct comp_zc_func_map {
	uint16_t frame_fmt;	/**< frame format */
	vol_zc_func func;	/**< volume zc function */
};

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

#ifdef UNIT_TEST
void sys_comp_volume_init(void);
#endif

#endif /* __SOF_AUDIO_VOLUME_H__ */
