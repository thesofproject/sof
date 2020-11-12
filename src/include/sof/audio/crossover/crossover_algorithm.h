/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2020 Google LLC. All rights reserved.
 *
 * Author: Pin-chih Lin <johnylin@google.com>
 */
#ifndef __SOF_AUDIO_CROSSOVER_CROSSOVER_ALGORITHM_H__
#define __SOF_AUDIO_CROSSOVER_CROSSOVER_ALGORITHM_H__

#include <stdint.h>
#include <sof/audio/crossover/crossover.h>
#include <sof/platform.h>
#include <sof/math/iir_df2t.h>
#include <user/crossover.h>

/* crossover reset function */
void crossover_reset_state_ch(struct crossover_state *ch_state);

/* crossover init function */
int crossover_init_coef_ch(struct sof_eq_iir_biquad_df2t *coef,
			   struct crossover_state *ch_state,
			   int32_t num_sinks);

#endif //  __SOF_AUDIO_CROSSOVER_CROSSOVER_ALGORITHM_H__
