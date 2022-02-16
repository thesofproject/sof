/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2017 Intel Corporation. All rights reserved.
 *
 * Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
 *         Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 */

#ifndef __SOF_MATH_FIR_GENERIC_H__
#define __SOF_MATH_FIR_GENERIC_H__

#include <sof/math/fir_config.h>

#if FIR_GENERIC

#include <sof/audio/audio_stream.h>
#include <sof/audio/format.h>
#include <user/fir.h>
#include <stdint.h>

struct comp_buffer;
struct sof_eq_fir_coef_data;

struct fir_state_32x16 {
	int rwi; /* Circular read and write index */
	int taps; /* Number of FIR taps */
	int length; /* Number of FIR taps plus input length (even) */
	int out_shift; /* Amount of right shifts at output */
	int16_t *coef; /* Pointer to FIR coefficients */
	int32_t *delay; /* Pointer to FIR delay line */
};

void fir_reset(struct fir_state_32x16 *fir);

int fir_delay_size(struct sof_fir_coef_data *config);

int fir_init_coef(struct fir_state_32x16 *fir,
		  struct sof_fir_coef_data *config);

void fir_init_delay(struct fir_state_32x16 *fir, int32_t **data);

int32_t fir_32x16(struct fir_state_32x16 *fir, int32_t x);

void fir_32x16_2x(struct fir_state_32x16 *fir, int32_t x0, int32_t x1, int32_t *y0, int32_t *y1);

#endif
#endif /* __SOF_MATH_FIR_GENERIC_H__ */
