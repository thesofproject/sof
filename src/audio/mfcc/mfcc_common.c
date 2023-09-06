// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2023 Intel Corporation. All rights reserved.
//
// Author: Andrula Song <andrula.song@intel.com>

#include <sof/audio/mfcc/mfcc_comp.h>

#include <sof/audio/component.h>
#include <sof/audio/audio_stream.h>
#include <sof/math/auditory.h>
#include <sof/math/matrix.h>
#include <sof/math/sqrt.h>
#include <sof/math/trig.h>
#include <sof/math/window.h>
#include <sof/trace/trace.h>
#include <user/mfcc.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

LOG_MODULE_REGISTER(mfcc_common, CONFIG_SOF_LOG_LEVEL);
/* MFCC with 16 bit FFT benefits from data normalize, for 32 bits there's no
 * significant impact. The amount of left shifts for FFT input is limited to
 * 10 that equals about 60 dB boost. The boost is compensated in Mel energy
 * calculation.
 */
#if MFCC_FFT_BITS == 16
#define MFCC_NORMALIZE_FFT
#else
#undef MFCC_NORMALIZE_FFT
#endif
#define MFCC_NORMALIZE_MAX_SHIFT	10

/*
 * The main processing function for MFCC
 */

static int mfcc_stft_process(const struct comp_dev *dev, struct mfcc_state *state)
{
	struct mfcc_buffer *buf = &state->buf;
	struct mfcc_fft *fft = &state->fft;
	int mel_scale_shift;
	int input_shift;
	int i;
	int m;
	int cc_count = 0;

	/* Phase 1, wait until whole fft_size is filled with valid data. This way
	 * first output cepstral coefficients originate from streamed data and not
	 * from buffers with zero data.
	 */
	comp_dbg(dev, "mfcc_stft_process(), avail = %d", buf->s_avail);
	if (state->waiting_fill) {
		if (buf->s_avail < fft->fft_size)
			return 0;

		state->waiting_fill = false;
	}

	/* Phase 2, move first prev_size data to previous data buffer, remove
	 * samples from input buffer.
	 */
	if (!state->prev_samples_valid) {
		mfcc_fill_prev_samples(buf, state->prev_data, state->prev_data_size);
		state->prev_samples_valid = true;
	}

	/* Check if enough samples in buffer for FFT hop */
	m = buf->s_avail / fft->fft_hop_size;
	for (i = 0; i < m; i++) {
		/* Clear FFT input buffer because it has been used as scratch */
		bzero(fft->fft_buf, fft->fft_buffer_size);

		/* Copy data to FFT input buffer from overlap buffer and from new samples buffer */
		mfcc_fill_fft_buffer(state);

		/* TODO: remove_dc_offset */

		/* TODO: use_energy & raw_energy */

#ifdef MFCC_NORMALIZE_FFT
		/* Find block scale left shift for FFT input */
		input_shift = mfcc_normalize_fft_buffer(state);
#else
		input_shift = 0;
#endif

		/* Window function */
		mfcc_apply_window(state, input_shift);

		/* TODO: use_energy & !raw_energy */

		/* The FFT out buffer needs to be cleared to avoid to corrupt
		 * the output. TODO: check moving it to FFT lib.
		 */
		bzero(fft->fft_out, fft->fft_buffer_size);

		/* Compute FFT */
#if MFCC_FFT_BITS == 16
		fft_execute_16(fft->fft_plan, false);
#else
		fft_execute_32(fft->fft_plan, false);
#endif

		/* Convert powerspectrum to Mel band logarithmic spectrum */
		mat_init_16b(state->mel_spectra, 1, state->dct.num_in, 7); /* Q8.7 */

		/* Compensate FFT lib scaling to Mel log values, e.g. for 512 long FFT
		 * the fft_plan->len is 9. The scaling is 1/512. Subtract from input_shift it
		 * to add the missing "gain".
		 */
		mel_scale_shift = input_shift - fft->fft_plan->len;
#if MFCC_FFT_BITS == 16
		psy_apply_mel_filterbank_16(&state->melfb, fft->fft_out, state->power_spectra,
					    state->mel_spectra->data, mel_scale_shift);
#else
		psy_apply_mel_filterbank_32(&state->melfb, fft->fft_out, state->power_spectra,
					    state->mel_spectra->data, mel_scale_shift);
#endif

		/* Multiply Mel spectra with DCT matrix to get cepstral coefficients */
		mat_init_16b(state->cepstral_coef, 1, state->dct.num_out, 7); /* Q8.7 */
		mat_multiply(state->mel_spectra, state->dct.matrix, state->cepstral_coef);

		/* Apply cepstral lifter */
		if (state->lifter.cepstral_lifter != 0)
			mat_multiply_elementwise(state->cepstral_coef, state->lifter.matrix,
						 state->cepstral_coef);

		cc_count += state->dct.num_out;

		/* Output to sink buffer */
	}

	/* TODO: This version handles only one FFT run per copy(). How to pass multiple
	 * cepstral coefficients sets return is an open.
	 */
	return cc_count;
}

#if CONFIG_FORMAT_S16LE
void mfcc_s16_default(struct processing_module *mod, struct input_stream_buffer *bsource,
		      struct output_stream_buffer *bsink, int frames)
{
	struct audio_stream *sink = bsink->data;
	struct mfcc_comp_data *cd = module_get_private_data(mod);
	struct mfcc_state *state = &cd->state;
	struct mfcc_buffer *buf = &cd->state.buf;
	uint32_t magic = MFCC_MAGIC;
	int16_t *w_ptr = audio_stream_get_wptr(sink);
	// int num_magic = sizeof(magic) / sizeof(int16_t);
	const int num_magic = 2;
	int num_ceps;
	int zero_samples;

	/* Get samples from source buffer */
	mfcc_source_copy_s16(bsource, buf, &state->emph, frames, state->source_channel);

	/* Run STFT and processing after FFT: Mel auditory filter and DCT. The sink
	 * buffer is updated during STDF processing.
	 */
	num_ceps = mfcc_stft_process(mod->dev, state);

	/* Done, copy data to sink. This works only if the period has room for magic (2)
	 * plus num_ceps int16_t samples. TODO: split ceps over multiple periods.
	 */
	zero_samples = frames * audio_stream_get_channels(sink);
	if (num_ceps > 0) {
		zero_samples -= num_ceps + num_magic;
		w_ptr = mfcc_sink_copy_data_s16(sink, w_ptr, num_magic, (int16_t *)&magic);
		w_ptr = mfcc_sink_copy_data_s16(sink, w_ptr, num_ceps, state->cepstral_coef->data);
	}

	w_ptr = mfcc_sink_copy_zero_s16(sink, w_ptr, zero_samples);
}
#endif /* CONFIG_FORMAT_S16LE */

#if CONFIG_FORMAT_S24LE
#endif /* CONFIG_FORMAT_S24LE */

#if CONFIG_FORMAT_S32LE
#endif /* CONFIG_FORMAT_S32LE */
