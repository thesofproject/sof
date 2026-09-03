// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 Intel Corporation. All rights reserved.
//
// Author: Andrula Song <andrula.song@intel.com>

#include <sof/common.h>
#include <sof/audio/format.h>
#include <sof/math/iir_df1.h>
#include <user/eq.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

#include <rtos/symbol.h>

int iir_delay_size_df1(struct sof_eq_iir_header *config)
{
	uint32_t n = config->num_sections; /* One section uses four unit delays */

	if (!n || n > SOF_EQ_IIR_BIQUADS_MAX)
		return -EINVAL;

	if (!config->num_sections_in_series ||
	    config->num_sections_in_series > n)
		return -EINVAL;

	return 4 * n * sizeof(int32_t);
}
EXPORT_SYMBOL(iir_delay_size_df1);

int iir_init_coef_df1(struct iir_state_df1 *iir,
		      struct sof_eq_iir_header *config)
{
	iir->biquads = config->num_sections;
	iir->biquads_in_series = config->num_sections_in_series;
	iir->coef = ASSUME_ALIGNED(&config->biquads[0], 4);

	return 0;
}
EXPORT_SYMBOL(iir_init_coef_df1);

void iir_init_delay_df1(struct iir_state_df1 *iir, int32_t **delay)
{
	/* Set state line of this IIR */
	iir->delay = *delay;

	/* Point to next IIR delay line start. */
	*delay += 4 * iir->biquads;
}
EXPORT_SYMBOL(iir_init_delay_df1);

void iir_reset_df1(struct iir_state_df1 *iir)
{
	iir->biquads = 0;
	iir->biquads_in_series = 0;
	iir->coef = NULL;
	/* Note: May need to know the beginning of dynamic allocation after so
	 * omitting setting iir->delay to NULL.
	 */
}
EXPORT_SYMBOL(iir_reset_df1);
