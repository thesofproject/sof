// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2017 Intel Corporation. All rights reserved.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
//         Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>

#include <sof/common.h>
#include <sof/audio/format.h>
#include <sof/math/iir_df2t.h>
#include <user/eq.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

int iir_delay_size_df2t(struct sof_eq_iir_header_df2t *config)
{
	int n = config->num_sections; /* One section uses two unit delays */

	if (n > SOF_EQ_IIR_DF2T_BIQUADS_MAX || n < 1)
		return -EINVAL;

	return 2 * n * sizeof(int64_t);
}

int iir_init_coef_df2t(struct iir_state_df2t *iir,
		       struct sof_eq_iir_header_df2t *config)
{
	iir->biquads = config->num_sections;
	iir->biquads_in_series = config->num_sections_in_series;
	iir->coef = ASSUME_ALIGNED(&config->biquads[0], 4);

	return 0;
}

void iir_init_delay_df2t(struct iir_state_df2t *iir, int64_t **delay)
{
	/* Set delay line of this IIR */
	iir->delay = *delay;

	/* Point to next IIR delay line start. The DF2T biquad uses two
	 * memory elements.
	 */
	*delay += 2 * iir->biquads;
}

void iir_reset_df2t(struct iir_state_df2t *iir)
{
	iir->biquads = 0;
	iir->biquads_in_series = 0;
	iir->coef = NULL;
	/* Note: May need to know the beginning of dynamic allocation after so
	 * omitting setting iir->delay to NULL.
	 */
}

