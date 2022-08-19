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
#include <sof/audio/module_adapter/module/generic.h>
#include <rtos/bit.h>
#include <sof/common.h>
#include <sof/trace/trace.h>
#include <ipc/stream.h>
#include <user/trace.h>
#include <stddef.h>
#include <stdint.h>
#if CONFIG_IPC_MAJOR_4
#include <ipc4/peak_volume.h>
#include <ipc4/fw_reg.h>
#endif

struct comp_buffer;
struct sof_ipc_ctrl_value_chan;

#define CONFIG_GENERIC

#if defined(__XCC__)
#include <xtensa/config/core-isa.h>

#if XCHAL_HAVE_HIFI3
#undef CONFIG_GENERIC
#endif

#endif

/**
 * \brief In IPC3 volume is in Q8.16 format, in IPC4 in Q1.31, but is converted
 * by firmware to Q1.23 format.
 */
#if CONFIG_IPC_MAJOR_3

//** \brief Volume gain Qx.y */
#define COMP_VOLUME_Q8_16 1

//** \brief Volume gain Qx.y integer x number of bits including sign bit. */
#define VOL_QXY_X 8

//** \brief Volume gain Qx.y fractional y number of bits. */
#define VOL_QXY_Y 16

#else

//** \brief Volume gain Qx.y */
#define COMP_VOLUME_Q1_23 1

//** \brief Volume gain Qx.y integer x number of bits including sign bit. */
#define VOL_QXY_X 1

//** \brief Volume gain Qx.y fractional y number of bits. */
#define VOL_QXY_Y 23

#endif

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

/** \brief Macros to convert without division bytes count to samples count */
#define VOL_BYTES_TO_S16_SAMPLES(b)	((b) >> 1)
#define VOL_BYTES_TO_S32_SAMPLES(b)	((b) >> 2)

#define VOL_S16_SAMPLES_TO_BYTES(s)	((s) << 1)
#define VOL_S32_SAMPLES_TO_BYTES(s)	((s) << 2)

/**
 * \brief volume processing function interface
 */
typedef void (*vol_scale_func)(struct processing_module *mod, struct input_stream_buffer *source,
			       struct output_stream_buffer *sink, uint32_t frames);

/**
 * \brief volume interface for function getting nearest zero crossing frame
 */
typedef uint32_t (*vol_zc_func)(const struct audio_stream __sparse_cache *source,
				uint32_t frames, int64_t *prev_sum);

/**
 * \brief Function for volume ramp shape function
 */

typedef int32_t (*vol_ramp_func)(struct processing_module *mod, int32_t ramp_time, int channel);

struct vol_data {
#if CONFIG_IPC_MAJOR_4
	struct ipc4_base_module_cfg base;	/**< module config */
	uint32_t mailbox_offset;		/**< store peak volume in mailbox */

	/**< these values will be stored to mailbox for host (IPC4) */
	struct ipc4_peak_volume_regs peak_regs;
#endif
	int32_t volume[SOF_IPC_MAX_CHANNELS];	/**< current volume */
	int32_t tvolume[SOF_IPC_MAX_CHANNELS];	/**< target volume */
	int32_t mvolume[SOF_IPC_MAX_CHANNELS];	/**< mute volume */
	int32_t rvolume[SOF_IPC_MAX_CHANNELS];	/**< ramp start volume */
	int32_t ramp_coef[SOF_IPC_MAX_CHANNELS]; /**< parameter for slope */
	/**< store current volume 4 times for scale_vol function */
	int32_t *vol;
	uint32_t initial_ramp;			/**< ramp space in ms */
	uint32_t ramp_type;			/**< SOF_VOLUME_ */
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
extern const struct comp_func_map volume_func_map[];

/** \brief Number of processing functions. */
extern const size_t volume_func_count;

/** \brief Volume zero crossing functions map. */
struct comp_zc_func_map {
	uint16_t frame_fmt;	/**< frame format */
	vol_zc_func func;	/**< volume zc function */
};

#if CONFIG_IPC_MAJOR_3
/**
 * \brief Retrievies volume processing function.
 * \param[in,out] dev Volume base component device.
 * \param[in] sinkb Sink buffer to match against
 */
static inline vol_scale_func vol_get_processing_function(struct comp_dev *dev,
							 struct comp_buffer __sparse_cache *sinkb)
{
	int i;

	/* map the volume function for source and sink buffers */
	for (i = 0; i < volume_func_count; i++) {
		if (sinkb->stream.frame_fmt != volume_func_map[i].frame_fmt)
			continue;

		return volume_func_map[i].func;
	}

	return NULL;
}
#else
/**
 * \brief Retrievies volume processing function.
 * \param[in,out] dev Volume base component device.
 * \param[in] sinkb Sink buffer to match against
 */
static inline vol_scale_func vol_get_processing_function(struct comp_dev *dev,
							 struct comp_buffer __sparse_cache *sinkb)
{
	struct processing_module *mod = comp_get_drvdata(dev);
	struct vol_data *cd = module_get_private_data(mod);

	switch (cd->base.audio_fmt.depth) {
	case IPC4_DEPTH_16BIT:
		return volume_func_map[0].func;
	case IPC4_DEPTH_32BIT:
		return volume_func_map[2].func;
	default:
		comp_err(dev, "vol_get_processing_function(): unsupported depth %d",
			 cd->base.audio_fmt.depth);
		return NULL;
	}
}
#endif

static inline void peak_vol_update(struct vol_data *cd)
{
#if CONFIG_COMP_PEAK_VOL
	/* update peakvol in mailbox */
	mailbox_sw_regs_write(cd->mailbox_offset, &cd->peak_regs, sizeof(cd->peak_regs));
#endif
}

#ifdef UNIT_TEST
#if CONFIG_COMP_LEGACY_INTERFACE
void sys_comp_volume_init(void);
#else
void sys_comp_module_volume_interface_init(void);
#endif
#endif

#endif /* __SOF_AUDIO_VOLUME_H__ */
