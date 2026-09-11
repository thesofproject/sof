// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Sound Open Firmware. All rights reserved.

#include <sof/common.h>
#include <sof/math/iir_df1_float.h>
#include <user/eq.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <rtos/symbol.h>

int iir_delay_size_df1_float(struct sof_eq_iir_header *config)
{
	uint32_t n = config->num_sections;

	if (!n || n > SOF_EQ_IIR_BIQUADS_MAX)
		return -EINVAL;

	if (!config->num_sections_in_series ||
	    config->num_sections_in_series > n)
		return -EINVAL;

	return 4 * n * sizeof(float);
}
EXPORT_SYMBOL(iir_delay_size_df1_float);

int iir_coef_size_df1_float(struct sof_eq_iir_header *config)
{
	uint32_t n = config->num_sections;

	if (!n || n > SOF_EQ_IIR_BIQUADS_MAX)
		return -EINVAL;

	return 5 * n * sizeof(float);
}
EXPORT_SYMBOL(iir_coef_size_df1_float);

int iir_init_coef_df1_float(struct iir_state_df1_float *iir,
			    struct sof_eq_iir_header *config,
			    float **coef_storage)
{
	uint32_t n = config->num_sections;
	const int32_t *raw_coef = (const int32_t *)&config->biquads[0];
	float *c_out = *coef_storage;

	iir->biquads = n;
	iir->biquads_in_series = config->num_sections_in_series;
	iir->coef = c_out;

	/* Convert each fixed-point biquad {a2, a1, b2, b1, b0, shift, gain}
	 * to normalized float {b0, b1, b2, a1, a2} */
	const float inv_q30 = 1.0f / 1073741824.0f;
	const float inv_q14 = 1.0f / 16384.0f;

	for (uint32_t j = 0; j < n; j++) {
		int32_t a2_q30 = raw_coef[0];
		int32_t a1_q30 = raw_coef[1];
		int32_t b2_q30 = raw_coef[2];
		int32_t b1_q30 = raw_coef[3];
		int32_t b0_q30 = raw_coef[4];
		int32_t shift   = raw_coef[5];
		int32_t gain    = raw_coef[6];

		float gain_factor = (float)gain * inv_q14;
		if (shift > 0)
			gain_factor /= (float)(1 << shift);
		else if (shift < 0)
			gain_factor *= (float)(1 << (-shift));

		float b_scale = gain_factor * inv_q30;

		c_out[0] = (float)b0_q30 * b_scale;
		c_out[1] = (float)b1_q30 * b_scale;
		c_out[2] = (float)b2_q30 * b_scale;
		c_out[3] = (float)a1_q30 * inv_q30;
		c_out[4] = (float)a2_q30 * inv_q30;

		raw_coef += 7;
		c_out += 5;
	}

	*coef_storage = c_out;
	return 0;
}
EXPORT_SYMBOL(iir_init_coef_df1_float);

void iir_init_delay_df1_float(struct iir_state_df1_float *iir, float **delay_storage)
{
	iir->delay = *delay_storage;
	*delay_storage += 4 * iir->biquads;
}
EXPORT_SYMBOL(iir_init_delay_df1_float);

void iir_reset_df1_float(struct iir_state_df1_float *iir)
{
	iir->biquads = 0;
	iir->biquads_in_series = 0;
	iir->coef = NULL;
	iir->delay = NULL;
}
EXPORT_SYMBOL(iir_reset_df1_float);
