// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022-2026 Intel Corporation.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

#include <sof/audio/mfcc/mfcc_comp.h>
#ifdef MFCC_GENERIC

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

/*
 * MFCC algorithm code
 */

/**
 * \brief Generic-C copy of the overlap window from the input circular buffer.
 *
 * Copies \p prev_data_length samples from the front of \p buf into
 * \p prev_data and advances the read pointer. Used once on first frame
 * to seed the overlap region for subsequent STFT hops.
 *
 * \param[in,out] buf Input circular buffer (Q1.15 mono).
 * \param[out] prev_data Destination overlap buffer.
 * \param[in] prev_data_length Number of samples to copy.
 */
void mfcc_fill_prev_samples(struct mfcc_buffer *buf, int16_t *prev_data,
			    int prev_data_length)
{
	/* Fill prev_data from input buffer */
	int16_t *r = buf->r_ptr;
	int16_t *p = prev_data;
	int copied;
	int nmax;
	int n;

	for (copied = 0; copied < prev_data_length; copied += n) {
		nmax = prev_data_length - copied;
		n = mfcc_buffer_samples_without_wrap(buf, r);
		n = MIN(n, nmax);
		memcpy(p, r, sizeof(int16_t) * n); /* Not using memcpy_s() due to speed need */
		p += n;
		r += n;
		r = mfcc_buffer_wrap(buf, r);
	}

	buf->s_avail -= copied;
	buf->s_free += copied;
	buf->r_ptr = r;
}

/**
 * \brief Generic-C window function application on the FFT input buffer.
 *
 * Multiplies the real part of \c fft->fft_buf (sized \c fft->fft_size,
 * Q1.15 input upcast to int32) by \c state->window (Q1.15) in-place and
 * left-shifts by \p input_shift + 1 to produce Q1.31 fixed-point input
 * to the FFT.
 *
 * \param[in,out] state MFCC state; \c state->fft.fft_buf is updated in-place.
 * \param[in] input_shift Additional left shift applied to the windowed sample.
 */
void mfcc_apply_window(struct mfcc_state *state, int input_shift)
{
	struct mfcc_fft *fft = &state->fft;
	int j;
	int i = fft->fft_fill_start_idx;

	/* TODO: Use proper multiply and saturate function to make sure no overflows */
	int s = input_shift + 1; /* To convert 16 -> 32 with Q1.15 x Q1.15 -> Q30 -> Q31 */

	for (j = 0; j < fft->fft_size; j++)
		fft->fft_buf[i + j].real = (fft->fft_buf[i + j].real * state->window[j]) << s;
}

#endif /* MFCC_GENERIC */
