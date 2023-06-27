/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2020 Intel Corporation. All rights reserved.
 *
 * Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
 */

#ifndef __SOF_AUDIO_TDFB_CONFIG_H__
#define __SOF_AUDIO_TDFB_CONFIG_H__

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/audio_stream.h>
#include <sof/math/fir_generic.h>
#include <sof/math/fir_hifi2ep.h>
#include <sof/math/fir_hifi3.h>
#include <sof/math/iir_df1.h>
#include <sof/platform.h>
#include <user/tdfb.h>

/* Select optimized code variant when xt-xcc compiler is used */
#if defined __XCC__
#include <xtensa/config/core-isa.h>
#if XCHAL_HAVE_HIFI2EP == 1
#define TDFB_GENERIC	0
#define TDFB_HIFIEP	1
#define TDFB_HIFI3	0
#elif XCHAL_HAVE_HIFI3 == 1 || XCHAL_HAVE_HIFI4 == 1
#define TDFB_HIFI3	1
#define TDFB_HIFIEP	0
#define TDFB_GENERIC	0
#else
#error "No HIFIEP or HIFI3 found. Cannot build TDFB module."
#endif
#else
/* GCC */
#define TDFB_GENERIC	1
#define TDFB_HIFIEP	0
#define TDFB_HIFI3	0
#endif

#define TDFB_IN_BUF_LENGTH (2 * PLATFORM_MAX_CHANNELS)
#define TDFB_OUT_BUF_LENGTH (2 * PLATFORM_MAX_CHANNELS)

/* When set to one only one IPC is sent to host. There is not other requests
 * triggered. If set to zero the IPC sent will be empty and the driver will
 * issue an actual control get. In simple  case with known # of control channels
 * including is more efficient.
 */
#define TDFB_ADD_DIRECTION_TO_GET_CMD 1

/* Allocate size is header plus single control value */
#define TDFB_GET_CTRL_DATA_SIZE (sizeof(struct sof_ipc_ctrl_data) + \
	sizeof(struct sof_ipc_ctrl_value_chan))

/* Process max 10% more frames than one period */
#define TDFB_MAX_FRAMES_MULT_Q14 Q_CONVERT_FLOAT(1.10, 14)

/* TDFB component private data */

struct tdfb_direction_data {
	struct iir_state_df1 emphasis[PLATFORM_MAX_CHANNELS];
	int32_t timediff[PLATFORM_MAX_CHANNELS];
	int32_t timediff_iter[PLATFORM_MAX_CHANNELS];
	int64_t level_ambient;
	uint32_t trigger;
	int32_t level;
	int32_t unit_delay; /* Q1.31 seconds */
	int32_t frame_count_since_control;
	int32_t *df1_delay;
	int32_t *r;
	int16_t *d;
	int16_t *d_end;
	int16_t *wp;
	int16_t *rp;
	int16_t step_sign;
	int16_t az_slow;
	int16_t az;
	int16_t max_lag;
	size_t d_size;
	size_t r_size;
	bool line_array; /* Limit scan to -90 to 90 degrees */
};

struct tdfb_comp_data {
	struct fir_state_32x16 fir[SOF_TDFB_FIR_MAX_COUNT]; /**< FIR state */
	struct comp_data_blob_handler *model_handler;
	struct sof_tdfb_config *config;	    /**< pointer to setup blob */
	struct sof_tdfb_angle *filter_angles;
	struct sof_tdfb_mic_location *mic_locations;
	struct sof_ipc_ctrl_data *ctrl_data;
	struct ipc_msg *msg;
	struct tdfb_direction_data direction;
	int32_t in[TDFB_IN_BUF_LENGTH];	    /**< input samples buffer */
	int32_t out[TDFB_IN_BUF_LENGTH];    /**< output samples mix buffer */
	int32_t *fir_delay;		    /**< pointer to allocated RAM */
	int16_t *input_channel_select;	    /**< For each FIR define in ch */
	int16_t *output_channel_mix;	    /**< For each FIR define out ch */
	int16_t *output_stream_mix;         /**< for each FIR define stream */
	int16_t az_value;		    /**< beam steer azimuth as in control enum */
	int16_t az_value_estimate;	    /**< beam steer azimuth as in control enum */
	size_t fir_delay_size;              /**< allocated size */
	unsigned int max_frames;	    /**< max frames to process */
	bool direction_updates:1;	    /**< set true if direction angle control is updated */
	bool direction_change:1;	    /**< set if direction value has significant change */
	bool beam_on:1;			    /**< set true if beam is off */
	bool update:1;			    /**< set true if control enum has been received */
	void (*tdfb_func)(struct tdfb_comp_data *cd,
			  struct input_stream_buffer *bsource,
			  struct output_stream_buffer *bsink,
			  int frames);
};

#if CONFIG_FORMAT_S16LE
void tdfb_fir_s16(struct tdfb_comp_data *cd,
		  struct input_stream_buffer *bsource,
		  struct output_stream_buffer *bsink, int frames);
#endif

#if CONFIG_FORMAT_S24LE
void tdfb_fir_s24(struct tdfb_comp_data *cd,
		  struct input_stream_buffer *bsource,
		  struct output_stream_buffer *bsink, int frames);
#endif

#if CONFIG_FORMAT_S32LE
void tdfb_fir_s32(struct tdfb_comp_data *cd,
		  struct input_stream_buffer *bsource,
		  struct output_stream_buffer *bsink, int frames);
#endif

int tdfb_direction_init(struct tdfb_comp_data *cd, int32_t fs, int channels);
void tdfb_direction_copy_emphasis(struct tdfb_comp_data *cd, int channels, int *channel, int32_t x);
void tdfb_direction_estimate(struct tdfb_comp_data *cd, int frames, int channels);
void tdfb_direction_free(struct tdfb_comp_data *cd);

static inline void tdfb_cinc_s16(int16_t **ptr, int16_t *end, size_t size)
{
	if (*ptr >= end)
		*ptr = (int16_t *)((uint8_t *)*ptr - size);
}

static inline void tdfb_cdec_s16(int16_t **ptr, int16_t *start, size_t size)
{
	if (*ptr < start)
		*ptr = (int16_t *)((uint8_t *)*ptr + size);
}

#ifdef UNIT_TEST
void sys_comp_module_tdfb_interface_init(void);
#endif

#endif /* __SOF_AUDIO_TDFB_CONFIG_H__ */
