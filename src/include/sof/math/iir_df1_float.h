/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2026 Sound Open Firmware. All rights reserved.
 */

#ifndef __SOF_MATH_IIR_DF1_FLOAT_H__
#define __SOF_MATH_IIR_DF1_FLOAT_H__

#include <stddef.h>
#include <stdint.h>
#include <sof/common.h>

struct sof_eq_iir_header;

struct iir_state_df1_float {
	unsigned int biquads;           /* Number of IIR 2nd order sections */
	unsigned int biquads_in_series; /* Number of sections in series */
	float *coef;                    /* {b0, b1, b2, a1, a2} per biquad (5 floats) */
	float *delay;                   /* {x1, x2, y1, y2} per biquad (4 floats) */
};

int iir_delay_size_df1_float(struct sof_eq_iir_header *config);
int iir_coef_size_df1_float(struct sof_eq_iir_header *config);
int iir_init_coef_df1_float(struct iir_state_df1_float *iir, struct sof_eq_iir_header *config, float **coef_storage);
void iir_init_delay_df1_float(struct iir_state_df1_float *iir, float **delay_storage);
void iir_reset_df1_float(struct iir_state_df1_float *iir);

static inline float iir_df1_float(struct iir_state_df1_float *iir, float x)
{
	if (!iir->biquads)
		return x;

	const float *coefp = iir->coef;
	float *delay = iir->delay;
	float in = x;
	int c = 0;
	int d = 0;

	for (unsigned int j = 0; j < iir->biquads; j++) {
		const float b0 = coefp[c + 0];
		const float b1 = coefp[c + 1];
		const float b2 = coefp[c + 2];
		const float a1 = coefp[c + 3];
		const float a2 = coefp[c + 4];

		const float x1 = delay[d + 0];
		const float x2 = delay[d + 1];
		const float y1 = delay[d + 2];
		const float y2 = delay[d + 3];

		/* 5 single-cycle FMA operations on RV32F */
		float out = b0 * in + b1 * x1 + b2 * x2 + a1 * y1 + a2 * y2;

		delay[d + 1] = x1;
		delay[d + 0] = in;
		delay[d + 3] = y1;
		delay[d + 2] = out;

		in = out;
		c += 5;
		d += 4;
	}

	return in;
}

#endif /* __SOF_MATH_IIR_DF1_FLOAT_H__ */
