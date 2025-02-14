/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2017 Intel Corporation. All rights reserved.
 *
 * Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
 */

#ifndef __SOF_MATH_FIR_HIFI2EP_H__
#define __SOF_MATH_FIR_HIFI2EP_H__

#include <sof/math/fir_config.h>
#include <sof/common.h>

#if SOF_USE_HIFI(2, FILTER)

#include <sof/audio/audio_stream.h>
#include <sof/audio/buffer.h>
#include <user/fir.h>
#include <xtensa/config/defs.h>
#include <xtensa/tie/xt_hifi2.h>
#include <stdint.h>

struct comp_buffer;
struct sof_eq_fir_coef_data;

struct fir_state_32x16 {
	ae_p24x2f *rwp; /* Circular read and write pointer */
	ae_p24f *delay; /* Pointer to FIR delay line */
	ae_p24f *delay_end; /* Pointer to FIR delay line end */
	ae_p16x2s *coef; /* Pointer to FIR coefficients */
	int taps; /* Number of FIR taps */
	int length; /* Number of FIR taps plus input length (even) */
	int out_shift; /* Amount of right shifts at output */
};

void fir_reset(struct fir_state_32x16 *fir);

int fir_delay_size(struct sof_fir_coef_data *config);

int fir_init_coef(struct fir_state_32x16 *fir,
		  struct sof_fir_coef_data *config);

void fir_init_delay(struct fir_state_32x16 *fir, int32_t **data);

/* Setup circular buffer for FIR input data delay */
static inline void fir_hifiep_setup_circular(struct fir_state_32x16 *fir)
{
	AE_SETCBEGIN0(fir->delay);
	AE_SETCEND0(fir->delay_end);
}

void fir_get_lrshifts(struct fir_state_32x16 *fir, int *lshift,
		      int *rshift);

void fir_32x16(struct fir_state_32x16 *fir, int32_t x, int32_t *y, int lshift, int rshift);

void fir_32x16_2x(struct fir_state_32x16 *fir, int32_t x0, int32_t x1,
		  int32_t *y0, int32_t *y1, int lshift, int rshift);

#endif
#endif /* __SOF_MATH_FIR_HIFI2EP_H__ */
