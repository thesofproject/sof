/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2020 Google LLC. All rights reserved.
 *
 * Author: Pin-chih Lin <johnylin@google.com>
 */
#ifndef __SOF_AUDIO_DRC_DRC_H__
#define __SOF_AUDIO_DRC_DRC_H__

#include <stdint.h>
#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/drc/drc_plat_conf.h>
#include <sof/audio/buffer.h>
#include <sof/platform.h>
#include <user/drc.h>

struct audio_stream;
struct comp_dev;

/* Define CONFIG_DRC_MAX_PRE_DELAY_FRAMES for the build purposes without Kconfig,
 * e.g. testbench.
 * TODO: Use Kconfig on building the testbench.
 */
#ifdef CONFIG_LIBRARY
/* CONFIG_DRC_MAX_PRE_DELAY_FRAMES needs to be a 2^N number */
#define CONFIG_DRC_MAX_PRE_DELAY_FRAMES 512
#endif
#define DRC_MAX_PRE_DELAY_FRAMES_MASK (CONFIG_DRC_MAX_PRE_DELAY_FRAMES - 1)
#define DRC_DEFAULT_PRE_DELAY_FRAMES (CONFIG_DRC_MAX_PRE_DELAY_FRAMES >> 1)

/* DRC_DIVISION_FRAMES needs to be a 2^N number */
#define DRC_DIVISION_FRAMES 32
#define DRC_DIVISION_FRAMES_MASK (DRC_DIVISION_FRAMES - 1)

/* Stores the state of DRC */
struct drc_state {
	/* The detector_average is the target gain obtained by looking at the
	 * future samples in the lookahead buffer and applying the compression
	 * curve on them. compressor_gain is the gain applied to the current
	 * samples. compressor_gain moves towards detector_average with the
	 * speed envelope_rate which is calculated once for each division (32
	 * frames).
	 */
	int32_t detector_average; /* Q2.30 */
	int32_t compressor_gain;  /* Q2.30 */

	/* Lookahead section. */
	int8_t *pre_delay_buffers[PLATFORM_MAX_CHANNELS];
	int32_t last_pre_delay_frames; /* integer */
	int32_t pre_delay_read_index;  /* integer */
	int32_t pre_delay_write_index; /* integer */

	/* envelope for the current division */
	int32_t envelope_rate;       /* Q2.30 */
	int32_t scaled_desired_gain; /* Q2.30 */

	int32_t processed; /* switch */

	int32_t max_attack_compression_diff_db; /* Q8.24 */
};

typedef void (*drc_func)(struct processing_module *mod,
			 const struct audio_stream *source,
			 struct audio_stream *sink,
			 uint32_t frames);

/* DRC component private data */
struct drc_comp_data {
	struct drc_state state;             /**< compressor state */
	struct comp_data_blob_handler *model_handler;
	struct sof_drc_config *config;      /**< pointer to setup blob */
	bool config_ready;                  /**< set when fully received */
	enum sof_ipc_frame source_format;   /**< source frame format */
	drc_func drc_func;            /**< processing function */
};

struct drc_proc_fnmap {
	enum sof_ipc_frame frame_fmt;
	drc_func drc_proc_func;
};

extern const struct drc_proc_fnmap drc_proc_fnmap[];
extern const size_t drc_proc_fncount;

void drc_default_pass(struct processing_module *mod,
		      const struct audio_stream *source,
		      struct audio_stream *sink, uint32_t frames);
/**
 * \brief Returns DRC processing function.
 */
static inline drc_func drc_find_proc_func(enum sof_ipc_frame src_fmt)
{
	int i;

	/* Find suitable processing function from map */
	for (i = 0; i < drc_proc_fncount; i++)
		if (src_fmt == drc_proc_fnmap[i].frame_fmt)
			return drc_proc_fnmap[i].drc_proc_func;

	return NULL;
}

#endif //  __SOF_AUDIO_DRC_DRC_H__
