/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2020 Google LLC. All rights reserved.
 *
 * Author: Pin-chih Lin <johnylin@google.com>
 */
#ifndef __SOF_AUDIO_DRC_DRC_ALGORITHM_H__
#define __SOF_AUDIO_DRC_DRC_ALGORITHM_H__

#include <stdint.h>
#include <sof/audio/drc/drc.h>
#include <sof/platform.h>
#include <user/drc.h>

/* drc reset function */
void drc_reset_state(struct drc_state *state);

/* drc init functions */
int drc_init_pre_delay_buffers(struct drc_state *state,
			       size_t sample_bytes,
			       int channels);
int drc_set_pre_delay_time(struct drc_state *state,
			   int32_t pre_delay_time,
			   int32_t rate);

/* drc process functions */
void drc_update_detector_average(struct drc_state *state,
				 const struct sof_drc_params *p,
				 int nbyte,
				 int nch);
void drc_update_envelope(struct drc_state *state, const struct sof_drc_params *p);
void drc_compress_output(struct drc_state *state,
			 const struct sof_drc_params *p,
			 int nbyte,
			 int nch);

#endif //  __SOF_AUDIO_DRC_DRC_ALGORITHM_H__
