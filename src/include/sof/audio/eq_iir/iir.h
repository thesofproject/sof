/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2017 Intel Corporation. All rights reserved.
 *
 * Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
 *         Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 */

#ifndef __SOF_AUDIO_EQ_IIR_IIR_H__
#define __SOF_AUDIO_EQ_IIR_IIR_H__

#include <stddef.h>
#include <stdint.h>
#include <sof/audio/format.h>
#include <sof/math/iir_df2t.h>

struct sof_eq_iir_header_df2t;

int iir_init_coef_df2t(struct iir_state_df2t *iir,
		       struct sof_eq_iir_header_df2t *config);

int iir_delay_size_df2t(struct sof_eq_iir_header_df2t *config);

void iir_init_delay_df2t(struct iir_state_df2t *iir, int64_t **delay);

void iir_mute_df2t(struct iir_state_df2t *iir);

void iir_unmute_df2t(struct iir_state_df2t *iir);

void iir_reset_df2t(struct iir_state_df2t *iir);

#endif /* __SOF_AUDIO_EQ_IIR_IIR_H__ */
