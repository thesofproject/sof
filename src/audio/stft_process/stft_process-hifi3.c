// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2025-2026 Intel Corporation.

/**
 * \file
 * \brief HiFi3 SIMD-optimized helpers for the STFT processing component.
 *
 * This compilation unit provides HiFi3 intrinsic versions of selected
 * hot-path helpers. It is guarded by SOF_USE_MIN_HIFI(3, COMP_STFT_PROCESS)
 * so only one of the generic or hifi implementations is active.
 */

#include <sof/audio/format.h>
#include <sof/common.h>
#include <assert.h>
#include <stdint.h>
#include "stft_process.h"

#if SOF_USE_MIN_HIFI(3, COMP_STFT_PROCESS)

#include <xtensa/tie/xt_hifi3.h>

/**
 * stft_process_apply_window() - Multiply FFT buffer by the analysis window.
 * @state: STFT processing state that contains the FFT buffer and window.
 *
 * The real part of each icomplex32 sample in the FFT buffer is multiplied
 * by the corresponding Q1.31 window coefficient.
 */
void stft_process_apply_window(struct stft_process_state *state)
{
	struct stft_process_fft *fft = &state->fft;
	ae_int32 *buf;
	const ae_int32x2 *win;
	ae_f32x2 data01, data23;
	ae_f32x2 win01, win23;
	ae_int32x2 d0, d1;
	int fft_size = fft->fft_size;
	int j;
	int n4;

	/*
	 * buf  points to {real, imag} pairs (struct icomplex32).
	 * win  points to scalar Q1.31 window coefficients.
	 *
	 * We load each complex pair, multiply only the real part by the
	 * window value, then store the pair back with the updated real.
	 * The imaginary part is left untouched.
	 *
	 * Stride for buf  is sizeof(ae_int32x2) = 8 bytes per complex sample.
	 * Stride for win  is sizeof(ae_int32)   = 4 bytes per scalar window value.
	 */
	buf = (ae_int32 *)fft->fft_buf;
	win = (const ae_int32x2 *)state->window;

	assert(!(fft_size & 3));

	/* Main loop: process 4 samples per iteration */
	n4 = fft_size >> 2;
	for (j = 0; j < n4; j++) {
		/* Load four FFT real part values, combine into fft_data,
		 * buf[0] goes to data01 low, buf[1] goes to data01 high.
		 */
		d0 = AE_L32_I(buf, 0 * sizeof(ae_int32x2));
		d1 = AE_L32_I(buf, 1 * sizeof(ae_int32x2));
		data01 = AE_SEL32_HH(d0, d1);
		d0 = AE_L32_I(buf, 2 * sizeof(ae_int32x2));
		d1 = AE_L32_I(buf, 3 * sizeof(ae_int32x2));
		data23 = AE_SEL32_HH(d0, d1);

		/* Load four window coefficients,
		 * win[0] goes to win01 low, win[1] goes to win01 high
		 */
		AE_L32X2_IP(win01, win, sizeof(ae_int32x2));
		AE_L32X2_IP(win23, win, sizeof(ae_int32x2));

		/* Multiply with window function */
		data01 = AE_MULFP32X2RS(data01, win01);
		data23 = AE_MULFP32X2RS(data23, win23);

		/* Store back the updated real parts */
		AE_S32_L_IP(AE_SEL32_LH(data01, data01), buf, sizeof(ae_int32x2));
		AE_S32_L_IP(data01, buf, sizeof(ae_int32x2));
		AE_S32_L_IP(AE_SEL32_LH(data23, data23), buf, sizeof(ae_int32x2));
		AE_S32_L_IP(data23, buf, sizeof(ae_int32x2));
	}
}

/**
 * stft_process_overlap_add_ifft_buffer() - Overlap-add IFFT output to circular output buffer.
 * @state: STFT processing state.
 * @ch: Channel index.
 *
 * Each IFFT output sample is multiplied by gain_comp (Q1.31 x Q1.31) and
 * added with saturation to the existing content of the circular output
 * buffer.  HiFi3 AE_MULF32S_HH handles the multiply and
 * AE_ADD32S provides the saturating accumulation.
 *
 * Note: obuf must be even number of samples and 64-bit aligned.
 */
void stft_process_overlap_add_ifft_buffer(struct stft_process_state *state, int ch)
{
	struct stft_process_buffer *obuf = &state->obuf[ch];
	struct stft_process_fft *fft = &state->fft;
	ae_f32x2 gain = AE_MOVDA32(state->gain_comp);
	ae_f32x2 buffer_data;
	ae_f32x2 fft_data;
	ae_f32x2 d0, d1;
	ae_f32x2 *w = (ae_f32x2 *)obuf->w_ptr;
	ae_f32 *fft_p = (ae_f32 *)fft->fft_buf;
	int samples_remain = fft->fft_size;
	int i, n;

	while (samples_remain) {
		n = stft_process_buffer_samples_without_wrap(obuf, (int32_t *)w);

		/* The samples count must be even and not zero, the latter to avoid infinite
		 * loop. The assert can trigger only with incorrect usage of this function.
		 */
		assert(n && !(n & 1));
		n = MIN(samples_remain, n) >> 1;
		for (i = 0; i < n; i++) {
			/* Load two FFT real part values, combine into fft_data */
			AE_L32_IP(d0, fft_p, sizeof(ae_f32x2));
			AE_L32_IP(d1, fft_p, sizeof(ae_f32x2));
			fft_data = AE_SEL32_HH(d0, d1);

			/* Load buffer data, multiply fft_data with gain and accumulate, and
			 * store to output buffer.
			 */
			buffer_data = AE_L32X2_I(w, 0);
			AE_MULAFP32X2RS(buffer_data, fft_data, gain);
			AE_S32X2_IP(buffer_data, w, sizeof(ae_f32x2));
		}
		w = (ae_f32x2 *)stft_process_buffer_wrap(obuf, (int32_t *)w);
		samples_remain -= n << 1;
	}

	obuf->w_ptr = stft_process_buffer_wrap(obuf, obuf->w_ptr + fft->fft_hop_size);
	obuf->s_avail += fft->fft_hop_size;
	obuf->s_free -= fft->fft_hop_size;
}

#endif /* SOF_USE_MIN_HIFI(3, COMP_STFT_PROCESS) */
