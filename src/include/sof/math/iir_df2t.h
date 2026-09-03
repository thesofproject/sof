/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2017 Intel Corporation. All rights reserved.
 *
 * Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
 *         Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 */

#ifndef __SOF_MATH_IIR_DF2T_H__
#define __SOF_MATH_IIR_DF2T_H__

#include <stddef.h>
#include <stdint.h>
#include <sof/common.h>

#define IIR_DF2T_NUM_DELAYS 2

struct iir_state_df2t {
	unsigned int biquads; /* Number of IIR 2nd order sections total */
	unsigned int biquads_in_series; /* Number of IIR 2nd order sections
					 * in series.
					 */
	int32_t *coef; /* Pointer to IIR coefficients */
	int64_t *delay; /* Pointer to IIR delay line */
};

struct sof_eq_iir_header;

int iir_init_coef_df2t(struct iir_state_df2t *iir,
		       struct sof_eq_iir_header *config);

int iir_delay_size_df2t(struct sof_eq_iir_header *config);

void iir_init_delay_df2t(struct iir_state_df2t *iir, int64_t **delay);

void iir_mute_df2t(struct iir_state_df2t *iir);

void iir_unmute_df2t(struct iir_state_df2t *iir);

void iir_reset_df2t(struct iir_state_df2t *iir);

int32_t iir_df2t(struct iir_state_df2t *iir, int32_t x);

/* Inline functions with or without HiFi3 intrinsics */
#if SOF_USE_HIFI(3, FILTER) || SOF_USE_HIFI(4, FILTER)
#include "iir_df2t_hifi3.h"
#else
#include "iir_df2t_generic.h"
#endif

#endif /* __SOF_MATH_IIR_DF2T_H__ */
