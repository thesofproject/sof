// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2023-2026 Intel Corporation.
//
// Author: Andrula Song <andrula.song@intel.com>

#include <sof/audio/mfcc/mfcc_comp.h>

#ifdef MFCC_HIFI3

#include <sof/audio/component.h>
#include <sof/math/auditory.h>
#include <sof/math/icomplex16.h>
#include <sof/math/icomplex32.h>
#include <sof/math/matrix.h>
#include <sof/math/sqrt.h>
#include <sof/math/trig.h>
#include <sof/math/window.h>
#include <sof/trace/trace.h>
#include <user/mfcc.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <xtensa/tie/xt_hifi3.h>

/* Setup circular for buffer 0 */
static inline void set_circular_buf0(const void *start, const void *end)
{
	AE_SETCBEGIN0(start);
	AE_SETCEND0(end);
}

/*
 * MFCC algorithm code
 */

/**
 * \brief HiFi3 copy of the overlap window from the input circular buffer.
 *
 * HiFi3 implementation equivalent to the generic mfcc_fill_prev_samples();
 * uses xtensa circular addressing for the source pointer.
 *
 * \param[in,out] buf Input circular buffer (Q1.15 mono).
 * \param[out] prev_data Destination overlap buffer.
 * \param[in] prev_data_length Number of samples to copy.
 */
void mfcc_fill_prev_samples(struct mfcc_buffer *buf, int16_t *prev_data,
			    int prev_data_length)
{
	/* Fill prev_data from input buffer */
	ae_int32 *out = (ae_int32 *)prev_data;
	ae_int32 *in = (ae_int32 *)buf->r_ptr;
	ae_int32x2 in_sample;
	ae_int16x4 sample;
	const int inc = sizeof(ae_int32);
	int n = prev_data_length >> 1;
	int i;

	/* Set buf as circular buffer 0 */
	set_circular_buf0(buf->addr, buf->end_addr);

	/* very strange this align load is unexpected
	 * so use load a 32bit to replace 16x4 align load.
	 */
	for (i = 0; i < n; i++) {
		AE_L32_XC(in_sample, in, inc);
		/* sizeof(ae_int32) = 4 */
		AE_S32_L_IP(in_sample, out, 4);
	}
	if (prev_data_length & 0x01) {
		AE_L16_XC(sample, (ae_int16 *)in, sizeof(ae_int16));
		AE_S16_0_IP(sample, (ae_int16 *)out, sizeof(ae_int16));
	}

	buf->s_avail -= prev_data_length;
	buf->s_free += prev_data_length;
	buf->r_ptr = (void *)in; /* int16_t pointer but direct cast is not possible */
}

/**
 * \brief HiFi3 window function application on the FFT input buffer.
 *
 * HiFi3 implementation equivalent to the generic mfcc_apply_window();
 * uses fractional multiply with saturating shift to produce Q1.31
 * FFT input data.
 *
 * \param[in,out] state MFCC state; \c state->fft.fft_buf is updated in-place.
 * \param[in] input_shift Additional left shift applied to the windowed sample.
 */
void mfcc_apply_window(struct mfcc_state *state, int input_shift)
{
	struct mfcc_fft *fft = &state->fft;
	const int fft_inc = sizeof(fft->fft_buf[0]);
	ae_int16 *win_in = (ae_int16 *)state->window;
	const int win_inc = sizeof(ae_int16);
	ae_int32x2 temp;
	ae_int16x4 win;
	int j;

	ae_int32 *fft_in = (ae_int32 *)&fft->fft_buf[fft->fft_fill_start_idx].real;
	ae_int32x2 sample;

	for (j = 0; j < fft->fft_size; j++) {
		AE_L32_IP(sample, fft_in, 0);
		AE_L16_XP(win, win_in, win_inc);
		/* Data is 16-bit in 32-bit container, shift to Q1.31 for fractional multiply */
		sample = AE_SLAI32S(sample, 16);
		temp = AE_MULFP32X16X2RS_L(sample, win);
		temp = AE_SLAA32S(temp, input_shift);
		AE_S32_L_XP(temp, fft_in, fft_inc);
	}
}

#endif /* MFCC_HIFI3 */
