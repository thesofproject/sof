// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 Intel Corporation. All rights reserved.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

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

/* Open files for data debug output in testbench */
#define DEBUG_WITH_TESTBENCH
#if defined(CONFIG_LIBRARY) && defined(DEBUG_WITH_TESTBENCH)
#include <stdio.h>
#define DEBUGFILES
#undef DEBUGFILES_READ_FFT /* Override FFT out with file input */
#undef DEBUGFILES_READ_MEL /* Override Mel filterbank with file input */
#endif

#ifdef MFCC_DEBUGFILES
#ifdef DEBUGFILES_READ_FFT
FILE *fh_fft_out_read;
#endif
#ifdef DEBUGFILES_READ_MEL
FILE *fh_mel_read;
#endif
FILE *fh_fft_in;
FILE *fh_fft_out;
FILE *fh_pow;
FILE *fh_mel;
FILE *fh_ceps;

void mfcc_generic_debug_open(void)
{
#ifdef DEBUGFILES_READ_FFT
	fh_fft_out_read = fopen("ref_fft_out.txt", "r");
#endif
#ifdef DEBUGFILES_READ_MEL
	fh_mel_read = fopen("ref_fft_mel.txt", "r");
#endif
	fh_fft_in = fopen("fft_in.txt", "w");
	fh_fft_out = fopen("fft_out.txt", "w");
	fh_pow = fopen("fft_pow.txt", "w");
	fh_mel = fopen("fft_mel.txt", "w");
	fh_ceps = fopen("ceps.txt", "w");
}

void mfcc_generic_debug_close(void)
{
#ifdef DEBUGFILES_READ_FFT
	fclose(fh_fft_out_read);
#endif
#ifdef DEBUGFILES_READ_MEL
	fclose(fh_mel_read);
#endif
	fclose(fh_fft_in);
	fclose(fh_fft_out);
	fclose(fh_pow);
	fclose(fh_mel);
	fclose(fh_ceps);
}
#endif

/*
 * MFCC algorithm code
 */

static void mfcc_source_copy_s16(struct input_stream_buffer *bsource, struct mfcc_buffer *buf,
				 struct mfcc_pre_emph *emph, int frames, int source_channel)
{
	struct audio_stream __sparse_cache *source = bsource->data;
	int32_t s;
	int16_t *x0;
	int16_t *x = source->r_ptr;
	int16_t *w = buf->w_ptr;
	int copied;
	int nmax;
	int n1;
	int n2;
	int n;
	int i;
	int num_channels = source->channels;

	/* Copy from source to pre-buffer for FFT.
	 * The pre-emphasis filter is done in this step.
	 */
	for (copied = 0; copied < frames; copied += n) {
		nmax = frames - copied;
		n1 = audio_stream_frames_without_wrap(source, x);
		n2 = mfcc_buffer_samples_without_wrap(buf, w);
		n = MIN(n1, n2);
		n = MIN(n, nmax);
		x0 = x + source_channel;
		for (i = 0; i < n; i++) {
			if (emph->enable) {
				/* Q1.15 x Q1.15 -> Q2.30 */
				s = (int32_t)emph->delay * emph->coef + Q_SHIFT_LEFT(*x0, 15, 30);
				*w = sat_int16(Q_SHIFT_RND(s, 30, 15));
				emph->delay = *x0;
			} else {
				*w = *x0;
			}
			x0 += num_channels;
			w++;
		}

		x = audio_stream_wrap(source, x + n * source->channels);
		w = mfcc_buffer_wrap(buf, w);
	}
	buf->s_avail += copied;
	buf->s_free -= copied;
	buf->w_ptr = w;
}

static void mfcc_fill_prev_samples(struct mfcc_buffer *buf, int16_t *prev_data,
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

static void mfcc_fill_fft_buffer(struct mfcc_state *state)
{
	struct mfcc_buffer *buf = &state->buf;
	struct mfcc_fft *fft = &state->fft;
	int16_t *r = buf->r_ptr;
	int copied;
	int nmax;
	int idx = fft->fft_fill_start_idx;
	int j;
	int n;

	/* Copy overlapped samples from state buffer. Imaginary part of input
	 * remains zero.
	 */
	for (j = 0; j < state->prev_data_size; j++)
		fft->fft_buf[idx + j].real = state->prev_data[j];

	/* Copy hop size of new data from circular buffer */
	idx += state->prev_data_size;
	for (copied = 0; copied < fft->fft_hop_size; copied += n) {
		nmax = fft->fft_hop_size - copied;
		n = mfcc_buffer_samples_without_wrap(buf, r);
		n = MIN(n, nmax);
		for (j = 0; j < n; j++) {
			fft->fft_buf[idx].real = *r;
			r++;
			idx++;
		}
		r = mfcc_buffer_wrap(buf, r);
	}

	buf->s_avail -= copied;
	buf->s_free += copied;
	buf->r_ptr = r;

	/* Copy for next time data back to overlap buffer */
	idx = fft->fft_fill_start_idx + fft->fft_hop_size;
	for (j = 0; j < state->prev_data_size; j++)
		state->prev_data[j] = fft->fft_buf[idx + j].real;
}

#ifdef MFCC_NORMALIZE_FFT
static int mfcc_normalize_fft_buffer(struct mfcc_state *state)
{
	struct mfcc_fft *fft = &state->fft;
	int32_t absx;
	int32_t smax = 0;
	int32_t x;
	int shift;
	int j;
	int i = fft->fft_fill_start_idx;

	for (j = 0; j < fft->fft_size; j++) {
		x = fft->fft_buf[i + j].real;
		absx = (x < 0) ? -x : x;
		if (smax < absx)
			smax = absx;
	}

	shift = norm_int32(smax << 15) - 1; /* 16 bit data */
	shift = MAX(shift, 0);
	shift = MIN(shift, MFCC_NORMALIZE_MAX_SHIFT);
	return shift;
}
#endif

static void mfcc_apply_window(struct mfcc_state *state, int input_shift)
{
	struct mfcc_fft *fft = &state->fft;
	int j;
	int i = fft->fft_fill_start_idx;

#if MFCC_FFT_BITS == 16
	/* TODO: Use proper multiply and saturate function to make sure no overflows */
	int32_t x;
	int s = 14 - input_shift; /* Q1.15 x Q1.15 -> Q30 -> Q15, shift by 15 - 1 for round */

	for (j = 0; j < fft->fft_size; j++) {
		x = (int32_t)fft->fft_buf[i + j].real * state->window[j];
		fft->fft_buf[i + j].real = ((x >> s) + 1) >> 1;
	}
#else
	/* TODO: Use proper multiply and saturate function to make sure no overflows */
	int s = input_shift + 1; /* To convert 16 -> 32 with Q1.15 x Q1.15 -> Q30 -> Q31 */

	for (j = 0; j < fft->fft_size; j++)
		fft->fft_buf[i + j].real = (fft->fft_buf[i + j].real * state->window[j]) << s;
#endif
}

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
#ifdef DEBUGFILES
	int j;
#endif

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

#ifdef DEBUGFILES
		for (j = 0; j < fft->fft_padded_size; j++)
			fprintf(fh_fft_in, "%d %d\n", fft->fft_buf[j].real, fft->fft_buf[j].imag);
#endif

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

#ifdef DEBUGFILES_READ_FFT
		double re, im;
		int ret;

		for (j = 0; j < fft->half_fft_size; j++) {
			ret = fscanf(fh_fft_out_read, "%lf %lf", &re, &im);
			if (ret != 2)
				break;

			fft->fft_out[j].real = sat_int16((int32_t)(32768.0 * re));
			fft->fft_out[j].imag = sat_int16((int32_t)(32768.0 * im));
		}
#endif
#ifdef DEBUGFILES
		for (j = 0; j < fft->half_fft_size; j++)
			fprintf(fh_fft_out, "%d %d\n", fft->fft_out[j].real, fft->fft_out[j].imag);
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
#ifdef DEBUGFILES_READ_MEL
		double val;
		int tmp;

		for (j = 0; j < state->dct.num_in; j++) {
			tmp = fscanf(fh_mel_read, "%lf", &val);
			if (tmp != 1)
				break;

			state->mel_spectra->data[j] = sat_int16((int32_t)(128.0 * val));
		}
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

#ifdef DEBUGFILES
		for (j = 0; j < fft->half_fft_size; j++)
			fprintf(fh_pow, "%d\n", state->power_spectra[j]);

		for (j = 0; j < state->dct.num_in; j++)
			fprintf(fh_mel, " %d\n", state->mel_spectra->data[j]);

		for (j = 0; j < state->dct.num_out; j++)
			fprintf(fh_ceps, " %d\n", state->cepstral_coef->data[j]);
#endif
	}

	/* TODO: This version handles only one FFT run per copy(). How to pass multiple
	 * cepstral coefficients sets return is an open.
	 */
	return cc_count;
}

#if CONFIG_FORMAT_S16LE

static int16_t *mfcc_sink_copy_zero_s16(const struct audio_stream *sink,
					int16_t *w_ptr, int samples)
{
	int copied;
	int nmax;
	int i;
	int n;

	for (copied = 0; copied < samples; copied += n) {
		nmax = samples - copied;
		n = audio_stream_samples_without_wrap_s16(sink, w_ptr);
		n = MIN(n, nmax);
		for (i = 0; i < n; i++) {
			*w_ptr = 0;
			w_ptr++;
		}

		w_ptr = audio_stream_wrap(sink, w_ptr);
	}

	return w_ptr;
}

static int16_t *mfcc_sink_copy_data_s16(const struct audio_stream *sink, int16_t *w_ptr,
					int samples, int16_t *r_ptr)
{
	int copied;
	int nmax;
	int i;
	int n;

	for (copied = 0; copied < samples; copied += n) {
		nmax = samples - copied;
		n = audio_stream_samples_without_wrap_s16(sink, w_ptr);
		n = MIN(n, nmax);
		for (i = 0; i < n; i++) {
			*w_ptr = *r_ptr;
			r_ptr++;
			w_ptr++;
		}

		w_ptr = audio_stream_wrap(sink, w_ptr);
	}

	return w_ptr;
}

void mfcc_s16_default(struct processing_module *mod, struct input_stream_buffer *bsource,
		      struct output_stream_buffer *bsink, int frames)
{
	struct audio_stream __sparse_cache *sink = bsink->data;
	struct mfcc_comp_data *cd = module_get_private_data(mod);
	struct mfcc_state *state = &cd->state;
	struct mfcc_buffer *buf = &cd->state.buf;
	uint32_t magic = MFCC_MAGIC;
	int16_t *w_ptr = sink->w_ptr;
	int num_magic = sizeof(magic) / sizeof(int16_t);
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
	zero_samples = frames * sink->channels;
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
