/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2017 Intel Corporation. All rights reserved.
 *
 * Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
 */

#ifndef __SOF_MATH_FIR_HIFI3_H__
#define __SOF_MATH_FIR_HIFI3_H__

#include <sof/math/fir_config.h>

#if FIR_HIFI3

#include <sof/audio/audio_stream.h>
#include <sof/audio/buffer.h>
#include <user/fir.h>
#include <xtensa/config/defs.h>
#include <xtensa/tie/xt_hifi3.h>

struct sof_eq_fir_coef_data;

struct fir_state_32x16 {
	ae_int32 *rwp; /* Circular read and write pointer */
	ae_int32 *delay; /* Pointer to FIR delay line */
	ae_int32 *delay_end; /* Pointer to FIR delay line end */
	ae_f16x4 *coef; /* Pointer to FIR coefficients */
	int taps; /* Number of FIR taps */
	int length; /* Number of FIR taps plus input length (even) */
	int in_shift; /* Amount of right shifts at input */
	int out_shift; /* Amount of right shifts at output */
};

void fir_reset(struct fir_state_32x16 *fir);

int fir_delay_size(struct sof_fir_coef_data *config);

int fir_init_coef(struct fir_state_32x16 *fir,
		  struct sof_fir_coef_data *config);

void fir_init_delay(struct fir_state_32x16 *fir, int32_t **data);

/* Setup circular buffer for FIR input data delay */
static inline void fir_core_setup_circular(struct fir_state_32x16 *fir)
{
	AE_SETCBEGIN0(fir->delay);
	AE_SETCEND0(fir->delay_end);
}

/* Setup circular for component buffer */
static inline void fir_comp_setup_circular(const struct audio_stream *buffer)
{
	AE_SETCBEGIN0(audio_stream_get_addr(buffer));
	AE_SETCEND0(audio_stream_get_end_addr(buffer));
}

void fir_get_lrshifts(struct fir_state_32x16 *fir, int *lshift,
		      int *rshift);

void fir_32x16_hifi3(struct fir_state_32x16 *fir, ae_int32 x, ae_int32 *y,
		     int shift);

void fir_32x16_2x_hifi3(struct fir_state_32x16 *fir, ae_int32 x0, ae_int32 x1,
			ae_int32 *y0, ae_int32 *y1, int shift);

#endif
#endif /* __SOF_MATH_FIR_HIFI3_H__ */
