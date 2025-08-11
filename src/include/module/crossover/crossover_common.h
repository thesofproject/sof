/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2023 Intel Corporation. All rights reserved.
 *
 * Author: Sebastiano Carlucci <scarlucci@google.com>
 */

#ifndef __SOF_CROSSOVER_COMMON_H__
#define __SOF_CROSSOVER_COMMON_H__

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/math/iir_df1.h>
#include <user/eq.h>

/* Number of sinks for a 2 way crossover filter */
#define CROSSOVER_2WAY_NUM_SINKS 2
/* Number of sinks for a 3 way crossover filter */
#define CROSSOVER_3WAY_NUM_SINKS 3
/* Number of sinks for a 4 way crossover filter */
#define CROSSOVER_4WAY_NUM_SINKS 4
/* Number of delay slots allocated for LR4 Filters */
#define CROSSOVER_NUM_DELAYS_LR4 4
/* Maximum number of LR4 highpass OR lowpass filters */
#define CROSSOVER_MAX_LR4 3
/* Maximum Number of sinks allowed in config */
#define SOF_CROSSOVER_MAX_STREAMS 4

/**
 * Stores the state of one channel of the Crossover filter
 */
struct crossover_state {
	/* Store the state for each LR4 filter. */
	struct iir_state_df1 lowpass[CROSSOVER_MAX_LR4];
	struct iir_state_df1 highpass[CROSSOVER_MAX_LR4];
};

typedef void (*crossover_split)(int32_t in, int32_t out[],
				struct crossover_state *state);

extern const crossover_split crossover_split_fnmap[];

/* crossover init function */
int crossover_init_coef_ch(struct processing_module *mod,
			   struct sof_eq_iir_biquad *coef,
			   struct crossover_state *ch_state,
			   int32_t num_sinks);

/**
 * \brief Reset the state of an LR4 filter.
 */
static inline void crossover_reset_state_lr4(struct processing_module *mod,
					     struct iir_state_df1 *lr4)
{
	mod_free(mod, lr4->coef);
	mod_free(mod, lr4->delay);

	lr4->coef = NULL;
	lr4->delay = NULL;
}

/**
 * \brief Reset the state (coefficients and delay) of the crossover filter
 *	  of a single channel.
 */
static inline void crossover_reset_state_ch(struct processing_module *mod,
					    struct crossover_state *ch_state)
{
	int i;

	for (i = 0; i < CROSSOVER_MAX_LR4; i++) {
		crossover_reset_state_lr4(mod, &ch_state->lowpass[i]);
		crossover_reset_state_lr4(mod, &ch_state->highpass[i]);
	}
}

/**
 * \brief Returns Crossover split function.
 */
static inline crossover_split crossover_find_split_func(int32_t num_sinks)
{
	if (num_sinks < CROSSOVER_2WAY_NUM_SINKS ||
	    num_sinks > CROSSOVER_4WAY_NUM_SINKS)
		return NULL;
	// The functions in the map are offset by 2 indices.
	return crossover_split_fnmap[num_sinks - CROSSOVER_2WAY_NUM_SINKS];
}

#endif /* __SOF_CROSSOVER_COMMON_H__ */
