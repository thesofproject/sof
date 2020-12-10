/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2020 Intel Corporation. All rights reserved.
 *
 * Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
 */

#ifndef __SOF_AUDIO_TDFB_CONFIG_H__
#define __SOF_AUDIO_TDFB_CONFIG_H__

#include <sof/platform.h>
#include <sof/audio/audio_stream.h>
#include <sof/math/fir_generic.h>
#include <sof/math/fir_hifi2ep.h>
#include <sof/math/fir_hifi3.h>
#include <user/tdfb.h>

/* Select optimized code variant when xt-xcc compiler is used */
#if defined __XCC__
#include <xtensa/config/core-isa.h>
#if XCHAL_HAVE_HIFI2EP == 1
#define TDFB_GENERIC	0
#define TDFB_HIFIEP	1
#define TDFB_HIFI3	0
#elif XCHAL_HAVE_HIFI3 == 1
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

/* TDFB component private data */

struct tdfb_comp_data {
	struct fir_state_32x16 fir[SOF_TDFB_FIR_MAX_COUNT]; /**< FIR state */
	//struct sof_ipc_comp_event event;
	//struct ipc_msg *msg;
	struct comp_data_blob_handler *model_handler;
	struct sof_tdfb_config *config;	    /**< pointer to setup blob */
	struct sof_tdfb_angle *filter_angles;
	struct sof_tdfb_mic_location *mic_locations;
	int32_t in[TDFB_IN_BUF_LENGTH];	    /**< input samples buffer */
	int32_t out[TDFB_IN_BUF_LENGTH];    /**< output samples mix buffer */
	uint32_t az_value;		    /**< beam steer azimuth as in control enum */
	int32_t *fir_delay;		    /**< pointer to allocated RAM */
	int16_t *input_channel_select;	    /**< For each FIR define in ch */
	int16_t *output_channel_mix;	    /**< For each FIR define out ch */
	int16_t *output_stream_mix;         /**< for each FIR define stream */
	int count;
	size_t fir_delay_size;              /**< allocated size */
	bool config_ready;                  /**< set when fully received */
	bool beam_off;			    /**< set true if beam is off */
	bool update;			    /**< set true if control enum has been received */
	void (*tdfb_func)(struct tdfb_comp_data *cd,
			  const struct audio_stream *source,
			  struct audio_stream *sink,
			  int frames);
};

#if CONFIG_FORMAT_S16LE
void tdfb_fir_s16(struct tdfb_comp_data *cd,
		  const struct audio_stream *source,
		  struct audio_stream *sink, int frames);
#endif

#if CONFIG_FORMAT_S24LE
void tdfb_fir_s24(struct tdfb_comp_data *cd,
		  const struct audio_stream *source,
		  struct audio_stream *sink, int frames);
#endif

#if CONFIG_FORMAT_S32LE
void tdfb_fir_s32(struct tdfb_comp_data *cd,
		  const struct audio_stream *source,
		  struct audio_stream *sink, int frames);
#endif

#endif /* __SOF_AUDIO_EQ_FIR_FIR_CONFIG_H__ */
