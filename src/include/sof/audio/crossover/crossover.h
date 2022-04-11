/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2020 Google LLC. All rights reserved.
 *
 * Author: Sebastiano Carlucci <scarlucci@google.com>
 */
#ifndef __SOF_AUDIO_CROSSOVER_CROSSOVER_H__
#define __SOF_AUDIO_CROSSOVER_CROSSOVER_H__

#include <stdint.h>
#include <sof/platform.h>
#include <sof/math/iir_df2t.h>
#include <user/crossover.h>

struct comp_buffer;
struct comp_dev;

/* Maximum number of LR4 highpass OR lowpass filters */
#define CROSSOVER_MAX_LR4 3
/* Number of delay slots allocated for LR4 Filters */
#define CROSSOVER_NUM_DELAYS_LR4 4

/* Number of sinks for a 2 way crossover filter */
#define CROSSOVER_2WAY_NUM_SINKS 2
/* Number of sinks for a 3 way crossover filter */
#define CROSSOVER_3WAY_NUM_SINKS 3
/* Number of sinks for a 4 way crossover filter */
#define CROSSOVER_4WAY_NUM_SINKS 4

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

/**
 * Stores the state of one channel of the Crossover filter
 */
struct crossover_state {
	/* Store the state for each LR4 filter. */
	struct iir_state_df2t lowpass[CROSSOVER_MAX_LR4];
	struct iir_state_df2t highpass[CROSSOVER_MAX_LR4];
};

typedef void (*crossover_process)(const struct comp_dev *dev,
				  const struct comp_buffer __sparse_cache *source,
				  struct comp_buffer __sparse_cache *sinks[],
				  int32_t num_sinks,
				  uint32_t frames);

typedef void (*crossover_split)(int32_t in, int32_t out[],
				struct crossover_state *state);

/* Crossover component private data */
struct comp_data {
	/**< filter state */
	struct crossover_state state[PLATFORM_MAX_CHANNELS];
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

/**
 * \brief Returns Crossover split function.
 */
static inline crossover_split crossover_find_split_func(int32_t num_sinks)
{
	if (num_sinks < CROSSOVER_2WAY_NUM_SINKS &&
	    num_sinks > CROSSOVER_4WAY_NUM_SINKS)
		return NULL;
	// The functions in the map are offset by 2 indices.
	return crossover_split_fnmap[num_sinks - CROSSOVER_2WAY_NUM_SINKS];
}

/*
 * \brief Runs input in through the LR4 filter and returns it's output.
 */
static inline int32_t crossover_generic_process_lr4(int32_t in,
						    struct iir_state_df2t *lr4)
{
	/* Cascade two biquads with same coefficients in series. */
	return iir_df2t(lr4, in);
}

#endif //  __SOF_AUDIO_CROSSOVER_CROSSOVER_H__
