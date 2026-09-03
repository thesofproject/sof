/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2020 Google LLC. All rights reserved.
 *
 * Author: Sebastiano Carlucci <scarlucci@google.com>
 */
#ifndef __SOF_AUDIO_CROSSOVER_CROSSOVER_H__
#define __SOF_AUDIO_CROSSOVER_CROSSOVER_H__

#include <sof/audio/module_adapter/module/module_interface.h>
#include <sof/math/iir_df1.h>
#include <module/crossover/crossover_common.h>
#include <sof/platform.h>
#include <stdint.h>

#include "crossover_user.h"

#define CROSSOVER_LR4_NUM_BIQUADS 2

struct comp_buffer;
struct comp_dev;

/**
 * The Crossover filter will have from 2 to 4 outputs.
 * Diagram of a 4-way Crossover filter (6 LR4 Filters).
 *
 *                             o---- LR4 LO-PASS --> y1(n)
 *                             |
 *          o--- LR4 LO-PASS --o
 *          |                  |
 *          |                  o--- LR4 HI-PASS --> y2(n)
 * x(n) --- o
 *          |                  o--- LR4 LO-PASS --> y3(n)
 *          |                  |
 *          o--- LR4 HI-PASS --o
 *                             |
 *                             o--- LR4 HI-PASS --> y4(n)
 *
 * Refer to include/user/crossover.h for diagrams of 2-way and 3-way crossovers
 * The low and high pass LR4 filters have opposite phase responses, causing
 * the intermediary outputs to be out of phase by 180 degrees.
 * For 2-way and 3-way, the phases of the signals need to be synchronized.
 *
 * Each LR4 is made of two butterworth filters in series with the same params.
 *
 * x(n) --> BIQUAD --> z(n) --> BIQUAD --> y(n)
 *
 * In total, we keep track of the state of at most 6 IIRs each made of two
 * biquads in series.
 *
 */

struct comp_data;

typedef void (*crossover_process)(struct comp_data *cd,
				  struct input_stream_buffer *bsource,
				  struct output_stream_buffer *bsinks[],
				  int32_t num_sinks,
				  uint32_t frames);

/* Crossover component private data */
struct comp_data {
	/**< filter state */
	struct crossover_state state[PLATFORM_MAX_CHANNELS];
#if CONFIG_IPC_MAJOR_4
	uint32_t output_pin_index[SOF_CROSSOVER_MAX_STREAMS];
	uint32_t num_output_pins;
#endif
	struct comp_data_blob_handler *model_handler;
	struct sof_crossover_config *config;      /**< pointer to setup blob */
	enum sof_ipc_frame source_format;         /**< source frame format */
	crossover_process crossover_process;      /**< processing function */
	crossover_split crossover_split;          /**< split function */
};

struct crossover_proc_fnmap {
	enum sof_ipc_frame frame_fmt;
	crossover_process crossover_proc_func;
};

extern const struct crossover_proc_fnmap crossover_proc_fnmap[];
extern const struct crossover_proc_fnmap crossover_proc_fnmap_pass[];
extern const size_t crossover_proc_fncount;

/**
 * \brief Returns Crossover processing function.
 */
static inline crossover_process
	crossover_find_proc_func(enum sof_ipc_frame src_fmt)
{
	int i;

	/* Find suitable processing function from map */
	for (i = 0; i < crossover_proc_fncount; i++)
		if (src_fmt == crossover_proc_fnmap[i].frame_fmt)
			return crossover_proc_fnmap[i].crossover_proc_func;

	return NULL;
}

/**
 * \brief Returns Crossover passthrough functions.
 */
static inline crossover_process
	crossover_find_proc_func_pass(enum sof_ipc_frame src_fmt)
{
	int i;

	/* Find suitable processing function from map */
	for (i = 0; i < crossover_proc_fncount; i++)
		if (src_fmt == crossover_proc_fnmap_pass[i].frame_fmt)
			return crossover_proc_fnmap_pass[i].crossover_proc_func;

	return NULL;
}

extern const crossover_split crossover_split_fnmap[];
extern const size_t crossover_split_fncount;

/*
 * \brief Runs input in through the LR4 filter and returns it's output.
 */
static inline int32_t crossover_generic_process_lr4(int32_t in,
						    struct iir_state_df1 *lr4)
{
	/* Cascade two biquads with same coefficients in series. */
	return iir_df1_4th(lr4, in);
}

static inline void crossover_free_config(struct sof_crossover_config **config)
{
	rfree(*config);
	*config = NULL;
}

int crossover_get_sink_id(struct comp_data *cd, uint32_t pipeline_id, uint32_t index);
int crossover_output_pin_init(struct processing_module *mod);
int crossover_check_sink_assign(struct processing_module *mod,
				struct sof_crossover_config *config);
int crossover_check_config(struct processing_module *mod, const uint8_t *fragment);
void crossover_params(struct processing_module *mod);
int crossover_get_stream_index(struct processing_module *mod,
			       struct sof_crossover_config *config, uint32_t pipe_id);

#endif //  __SOF_AUDIO_CROSSOVER_CROSSOVER_H__
