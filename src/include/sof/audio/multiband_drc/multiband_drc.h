/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2020 Google LLC. All rights reserved.
 *
 * Author: Pin-chih Lin <johnylin@google.com>
 */
#ifndef __SOF_AUDIO_MULTIBAND_DRC_MULTIBAND_DRC_H__
#define __SOF_AUDIO_MULTIBAND_DRC_MULTIBAND_DRC_H__

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/crossover/crossover.h>
#include <sof/audio/drc/drc.h>
#include <sof/math/iir_df2t.h>
#include <sof/audio/component.h>
#include <sof/audio/data_blob.h>
#include <sof/platform.h>
#include <user/multiband_drc.h>
#include <stdint.h>

/**
 * Stores the state of the sub-components in Multiband DRC
 */
struct multiband_drc_state {
	struct iir_state_df2t emphasis[PLATFORM_MAX_CHANNELS];
	struct crossover_state crossover[PLATFORM_MAX_CHANNELS];
	struct drc_state drc[SOF_MULTIBAND_DRC_MAX_BANDS];
	struct iir_state_df2t deemphasis[PLATFORM_MAX_CHANNELS];
};

typedef void (*multiband_drc_func)(const struct processing_module *mod,
				   const struct audio_stream *source,
				   struct audio_stream *sink,
				   uint32_t frames);

/* Multiband DRC component private data */
struct multiband_drc_comp_data {
	struct multiband_drc_state state;        /**< compressor state */
	struct comp_data_blob_handler *model_handler;
	struct sof_multiband_drc_config *config; /**< pointer to setup blob */
	bool config_ready;                       /**< set when fully received */
	enum sof_ipc_frame source_format;        /**< source frame format */
	bool process_enabled;                    /**< true if component is enabled */
	multiband_drc_func multiband_drc_func;   /**< processing function */
	crossover_split crossover_split;         /**< crossover n-way split func */
};

struct multiband_drc_proc_fnmap {
	enum sof_ipc_frame frame_fmt;
	multiband_drc_func multiband_drc_proc_func;
};

extern const struct multiband_drc_proc_fnmap multiband_drc_proc_fnmap[];
extern const struct multiband_drc_proc_fnmap multiband_drc_proc_fnmap_pass[];
extern const size_t multiband_drc_proc_fncount;

/**
 * \brief Returns Multiband DRC processing function.
 */
static inline multiband_drc_func multiband_drc_find_proc_func(enum sof_ipc_frame src_fmt)
{
	int i;

	/* Find suitable processing function from map */
	for (i = 0; i < multiband_drc_proc_fncount; i++)
		if (src_fmt == multiband_drc_proc_fnmap[i].frame_fmt)
			return multiband_drc_proc_fnmap[i].multiband_drc_proc_func;

	return NULL;
}

/**
 * \brief Returns Multiband DRC passthrough functions.
 */
static inline multiband_drc_func multiband_drc_find_proc_func_pass(enum sof_ipc_frame src_fmt)
{
	int i;

	/* Find suitable processing function from map */
	for (i = 0; i < drc_proc_fncount; i++)
		if (src_fmt == multiband_drc_proc_fnmap_pass[i].frame_fmt)
			return multiband_drc_proc_fnmap_pass[i].multiband_drc_proc_func;

	return NULL;
}

#ifdef UNIT_TEST
void sys_comp_module_multiband_drc_interface_init(void);
#endif

#endif //  __SOF_AUDIO_MULTIBAND_DRC_MULTIBAND_DRC_H__
