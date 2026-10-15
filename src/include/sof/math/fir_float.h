/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2026 Sound Open Firmware. All rights reserved.
 */

#ifndef __SOF_MATH_FIR_FLOAT_H__
#define __SOF_MATH_FIR_FLOAT_H__

#include <sof/common.h>
#include <user/fir.h>
#include <stdint.h>
#include <stddef.h>

struct sof_fir_coef_data;

struct fir_state_float {
	int rwi;	/* Circular read and write index */
	int taps;	/* Number of FIR taps */
	int length;	/* Number of FIR taps plus input length (even) */
	int out_shift;	/* Unused in float kernel (folded into coefficients) */
	float *coef;	/* Pointer to FIR float coefficients */
	float *delay;	/* Pointer to FIR float delay line */
};

void fir_reset_float(struct fir_state_float *fir);

int fir_delay_size_float(struct sof_fir_coef_data *config);

int fir_coef_size_float(struct sof_fir_coef_data *config);

int fir_init_coef_float(struct fir_state_float *fir,
			struct sof_fir_coef_data *config,
			float **coef_storage);

void fir_init_delay_float(struct fir_state_float *fir, float **data);

float fir_float(struct fir_state_float *fir, float x);

void fir_float_2x(struct fir_state_float *fir, float x0, float x1, float *y0, float *y1);

#endif /* __SOF_MATH_FIR_FLOAT_H__ */
