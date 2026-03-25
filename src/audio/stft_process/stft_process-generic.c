// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2025-2026 Intel Corporation.

#include <sof/audio/format.h>
#include <sof/common.h>
#include <assert.h>
#include <stdint.h>
#include "stft_process.h"

#if SOF_USE_HIFI(NONE, COMP_STFT_PROCESS)
void stft_process_overlap_add_ifft_buffer(struct stft_process_state *state, int ch)
{
	struct stft_process_buffer *obuf = &state->obuf[ch];
	struct stft_process_fft *fft = &state->fft;
	int32_t *w = obuf->w_ptr;
	int32_t sample;
	int i;
	int n;
	int samples_remain = fft->fft_size;
	int idx = 0;

	while (samples_remain) {
		n = stft_process_buffer_samples_without_wrap(obuf, w);
		n = MIN(samples_remain, n);

		/* Abort if n is zero to avoid infinite loop. The assert can
		 * trigger only with incorrect usage of this function.
		 */
		assert(n);
		for (i = 0; i < n; i++) {
			sample = Q_MULTSR_32X32((int64_t)state->gain_comp, fft->fft_buf[idx].real,
						31, 31, 31);
			*w = sat_int32((int64_t)*w + sample);
			w++;
			idx++;
		}
		w = stft_process_buffer_wrap(obuf, w);
		samples_remain -= n;
	}

	w = obuf->w_ptr + fft->fft_hop_size;
	obuf->w_ptr = stft_process_buffer_wrap(obuf, w);
	obuf->s_avail += fft->fft_hop_size;
	obuf->s_free -= fft->fft_hop_size;
}

void stft_process_apply_window(struct stft_process_state *state)
{
	struct stft_process_fft *fft = &state->fft;
	int j;

	/* Multiply Q1.31 by Q1.15 gives Q2.46, shift right by 15 to get Q2.31, no saturate need */
	for (j = 0; j < fft->fft_size; j++)
		fft->fft_buf[j].real =
			sat_int32(Q_MULTSR_32X32((int64_t)fft->fft_buf[j].real,
						 state->window[j], 31, 31, 31));
}
#endif /* SOF_USE_HIFI(NONE, COMP_STFT_PROCESS) */
