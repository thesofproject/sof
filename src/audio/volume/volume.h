/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2018 Intel Corporation. All rights reserved.
 *
 * Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

/**
 * \file volume/volume.h
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
#include "peak_volume.h"
#include <ipc4/fw_reg.h>
#endif

struct comp_buffer;
struct sof_ipc_ctrl_value_chan;

#if defined(__XCC__)
# include <xtensa/config/core-isa.h>
# if XCHAL_HAVE_HIFI4
#  define VOLUME_HIFI4
# elif XCHAL_HAVE_HIFI3
#  define VOLUME_HIFI3
# else
#  define VOLUME_GENERIC
# endif
#else
# define VOLUME_GENERIC
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

/** \brief Volume gain Qx.y integer x number of bits including sign bit.
 * With Q8.23 format the gain range is -138.47 to +42.14 dB.
 */
#define VOL_QXY_X 8

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
 * \brief left shift 8 bits to put the valid 24 bits into
 * higher part of 32 bits container.
 */
#define PEAK_24S_32C_ADJUST 8

/**
 * \brief left shift 16 bits to put the valid 16 bits into
 * higher part of 32 bits container.
 */
#define PEAK_16S_32C_ADJUST 16

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
			struct output_stream_buffer *sink, uint32_t frames, uint32_t attenuation);

/**
 * \brief volume interface for function getting nearest zero crossing frame
 */
typedef uint32_t (*vol_zc_func)(const struct audio_stream *source,
				uint32_t frames, int64_t *prev_sum);

/**
 * \brief Function for volume ramp shape function
 */

struct vol_data {
#if CONFIG_IPC_MAJOR_4
	uint32_t mailbox_offset;		/**< store peak volume in mailbox */

	/**< these values will be stored to mailbox for host (IPC4) */
	struct ipc4_peak_volume_regs peak_regs;
	/**< store temp peak volume 4 times for scale_vol function */
	int32_t *peak_vol;
	uint32_t peak_cnt;		/**< accumulated period of volume processing*/
	uint32_t peak_report_cnt;		/**< the period number to update peak meter*/
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
	int32_t sample_rate_inv;		/**< 1000x inverse of sample rate as Q1.31 */
	unsigned int channels;			/**< current channels count */
	bool muted[SOF_IPC_MAX_CHANNELS];	/**< set if channel is muted */
	bool ramp_finished;			/**< control ramp launch */
	vol_scale_func scale_vol;		/**< volume processing function */
	vol_zc_func zc_get;			/**< function getting nearest zero crossing frame */
	bool copy_gain;				/**< control copy gain or not */
	uint32_t attenuation;			/**< peakmeter adjustment in range [0 - 31] */
	bool is_passthrough;			/**< is passthrough or do gain multiplication */
};

/** \brief Volume processing functions map. */
struct comp_func_map {
	uint16_t frame_fmt;	/**< frame format */
	vol_scale_func func;	/**< volume processing function */
	vol_scale_func passthrough_func;	/**< volume passthrough function */
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
 * \param[in] cd Volume data structure.
 */
static inline vol_scale_func vol_get_processing_function(struct comp_dev *dev,
							 struct comp_buffer *sinkb,
							 struct vol_data *cd)
{
	int i;

	/* map the volume function for source and sink buffers */
	for (i = 0; i < volume_func_count; i++) {
		if (audio_stream_get_frm_fmt(&sinkb->stream) != volume_func_map[i].frame_fmt)
			continue;

		if (cd->is_passthrough)
			return volume_func_map[i].passthrough_func;
		else
			return volume_func_map[i].func;
	}

	return NULL;
}

#else
/**
 * \brief Retrievies volume processing function.
 * \param[in,out] dev Volume base component device.
 * \param[in] cd Volume data structure
 */
static inline vol_scale_func vol_get_processing_function(struct comp_dev *dev,
							 struct vol_data *cd)
{
	struct processing_module *mod = comp_get_drvdata(dev);

	if (cd->is_passthrough) {
		switch (mod->priv.cfg.base_cfg.audio_fmt.valid_bit_depth) {
		case IPC4_DEPTH_16BIT:
			return volume_func_map[0].passthrough_func;
		case IPC4_DEPTH_24BIT:
			return volume_func_map[1].passthrough_func;
		case IPC4_DEPTH_32BIT:
			return volume_func_map[2].passthrough_func;
		default:
			comp_err(dev, "vol_get_processing_function(): unsupported depth %d",
				 mod->priv.cfg.base_cfg.audio_fmt.depth);
			return NULL;
		}
	} else {
		switch (mod->priv.cfg.base_cfg.audio_fmt.valid_bit_depth) {
		case IPC4_DEPTH_16BIT:
			return volume_func_map[0].func;
		case IPC4_DEPTH_24BIT:
			return volume_func_map[1].func;
		case IPC4_DEPTH_32BIT:
			return volume_func_map[2].func;
		default:
			comp_err(dev, "vol_get_processing_function(): unsupported depth %d",
				 mod->priv.cfg.base_cfg.audio_fmt.depth);
			return NULL;
		}
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
void sys_comp_module_volume_interface_init(void);
#endif

/* source_or_sink, true means source, false means sink */
void set_volume_process(struct vol_data *cd, struct comp_dev *dev, bool source_or_sink);

void volume_peak_free(struct vol_data *cd);

int volume_peak_prepare(struct vol_data *cd, struct processing_module *mod);

int volume_init(struct processing_module *mod);

int volume_set_config(struct processing_module *mod, uint32_t config_id,
		      enum module_cfg_fragment_position pos, uint32_t data_offset_size,
		      const uint8_t *fragment, size_t fragment_size, uint8_t *response,
		      size_t response_size);

int volume_get_config(struct processing_module *mod,
		      uint32_t config_id, uint32_t *data_offset_size,
		      uint8_t *fragment, size_t fragment_size);

void volume_update_current_vol_ipc4(struct vol_data *cd);

void volume_reset_state(struct vol_data *cd);

void volume_prepare_ramp(struct comp_dev *dev, struct vol_data *cd);

int volume_set_chan(struct processing_module *mod, int chan,
		    int32_t vol, bool constant_rate_ramp);

void volume_set_chan_mute(struct processing_module *mod, int chan);

void volume_set_chan_unmute(struct processing_module *mod, int chan);

#endif /* __SOF_AUDIO_VOLUME_H__ */
