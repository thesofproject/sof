// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 Intel Corporation. All rights reserved.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

#include <sof/audio/mfcc/mfcc_comp.h>
#include <sof/audio/component.h>
#include <sof/audio/audio_stream.h>
#include <sof/math/auditory.h>
#include <sof/math/trig.h>
#include <sof/math/window.h>
#include <sof/trace/trace.h>
#include <user/mfcc.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

/* Definitions for cepstral lifter */
#define PI_Q23 Q_CONVERT_FLOAT(3.1415926536, 23)
#define TWO_PI_Q23 Q_CONVERT_FLOAT(6.2831853072, 23)
#define ONE_Q9 Q_CONVERT_FLOAT(1, 9)

LOG_MODULE_REGISTER(mfcc_setup, CONFIG_SOF_LOG_LEVEL);

static void mfcc_init_buffer(struct mfcc_buffer *buf, int16_t *base, int size)
{
	buf->addr = base;
	buf->end_addr = base + size;
	buf->r_ptr = base;
	buf->w_ptr = base;
	buf->s_free = size;
	buf->s_avail = 0;
	buf->s_length = size;
}

static int mfcc_get_window(struct mfcc_state *state, enum sof_mfcc_fft_window_type name)
{
	struct mfcc_fft *fft = &state->fft;

	switch (name) {
	case MFCC_RECTANGULAR_WINDOW:
		win_rectangular_16b(state->window, fft->fft_size);
		return 0;
	case MFCC_BLACKMAN_WINDOW:
		win_blackman_16b(state->window, fft->fft_size, MFCC_BLACKMAN_A0);
		return 0;
	case MFCC_HAMMING_WINDOW:
		win_hamming_16b(state->window, fft->fft_size);
		return 0;
	case MFCC_POVEY_WINDOW:
		win_povey_16b(state->window, fft->fft_size);
		return 0;

	default:
		return -EINVAL;
	}
}

/* The function returns a vector for multiplying the cepstral coefficients when
 * cepstral lifter option is enabled. The cepstral lifter value is Q7.9, e.g. 22.0.
 * The output vector is Q7.9 also and is size (1, num_ceps).
 *
 * The lifter function is
 * coef[i] = 1.0 + 0.5 * lifter * sin(pi * i / lifter), i = 0 to num_ceps-1
 */

static int mfcc_get_cepstral_lifter(struct processing_module *mod, struct mfcc_cepstral_lifter *cl)
{
	int32_t inv_cepstral_lifter;
	int32_t val;
	int32_t sin;
	int i;

	if (cl->num_ceps > DCT_MATRIX_SIZE_MAX)
		return -EINVAL;

	cl->matrix = mod_mat_matrix_alloc_16b(mod, 1, cl->num_ceps, 9); /* Use Q7.9 */
	if (!cl->matrix)
		return -ENOMEM;

	inv_cepstral_lifter = (1 << 30) / cl->cepstral_lifter; /* Q2.30 / Q7.9 -> Q1.21 */

	for (i = 0; i < cl->num_ceps; i++) {
		val = Q_MULTSR_32X32((int64_t)inv_cepstral_lifter, PI_Q23 * i, 21, 23, 23);
		val %= TWO_PI_Q23;
		sin = sin_fixed_32b(Q_SHIFT_LEFT(val, 23, 28)); /* Q4.28 -> Q1.31 */
		/* Val is Q7.9 make 0.5 multiply with additional shift */
		val = Q_MULTSR_32X32((int64_t)sin, cl->cepstral_lifter, 31, 9, 9 - 1);
		val += ONE_Q9;
		mat_set_scalar_16b(cl->matrix, 0, i, sat_int16(val));
	}

	return 0;
}

/* TODO mfcc setup needs to use the config blob, not hard coded parameters.
 * Also this is a too long function. Split to STFT, Mel filter, etc. parts.
 */
int mfcc_setup(struct processing_module *mod, int max_frames, int sample_rate, int channels)
{
	struct mfcc_comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	struct sof_mfcc_config *config = cd->config;
	struct mfcc_state *state = &cd->state;
	struct mfcc_fft *fft = &state->fft;
	struct psy_mel_filterbank *fb = &state->melfb;
	struct dct_plan_16 *dct = &state->dct;
	int ret;

	comp_dbg(dev, "mfcc_setup()");

	/* Check size */
	if (config->size != sizeof(struct sof_mfcc_config)) {
		comp_err(dev, "Illegal configuration size %d.", config->size);
		return -EINVAL;
	}

	/* Check currently hard-coded features to match configuration request */
	if (!config->round_to_power_of_two || !config->snip_edges ||
	    config->subtract_mean || config->use_energy) {
		comp_err(dev, "Can't change currently hard-coded features");
		return -EINVAL;
	}

	if (config->sample_frequency != sample_rate) {
		comp_err(dev, "Config sample_frequency does not match stream");
		return -EINVAL;
	}

	cd->max_frames = max_frames;
	state->sample_rate = sample_rate;
	state->low_freq = config->low_freq;
	state->high_freq = (config->high_freq == 0) ? (sample_rate >> 1) : config->high_freq;
	if (state->low_freq > state->high_freq) {
		comp_err(dev, "Config high_freq must be larger than low_freq");
		return -EINVAL;
	}

	comp_info(dev, "mfcc_setup(), source_channel = %d, stream_channels = %d",
		  config->channel, channels);
	if (config->channel >= channels) {
		comp_err(dev, "Illegal channel");
		return -EINVAL;
	}

	if (config->channel < 0)
		state->source_channel = 0;
	else
		state->source_channel = config->channel;

	state->emph.enable = config->preemphasis_coefficient > 0;
	state->emph.coef = -config->preemphasis_coefficient; /* Negate config parameter */
	fft->fft_size = config->frame_length;
	fft->fft_padded_size = 1 << (31 - norm_int32(fft->fft_size)); /* Round up to nearest 2^N */
	fft->fft_hop_size = config->frame_shift;
	fft->half_fft_size = (fft->fft_padded_size >> 1) + 1;

	comp_info(dev, "mfcc_setup(), emphasis = %d, fft_size = %d, fft_padded_size = %d, fft_hop_size = %d",
		  config->preemphasis_coefficient,
		  fft->fft_size, fft->fft_padded_size, fft->fft_hop_size);

	/* Calculated parameters */
	state->prev_data_size = fft->fft_size - fft->fft_hop_size;
	state->buffer_size = fft->fft_size + max_frames;

	/* Allocate buffer input samples and overlap buffer */
	state->sample_buffers_size = sizeof(int16_t) *
		(state->buffer_size + state->prev_data_size + fft->fft_size);

	comp_info(dev, "mfcc_setup(), buffer_size = %d, prev_size = %d",
		  state->buffer_size, state->prev_data_size);

	state->buffers = mod_zalloc(mod, state->sample_buffers_size);
	if (!state->buffers) {
		comp_err(dev, "Failed buffer allocate");
		return -ENOMEM;
	}

	mfcc_init_buffer(&state->buf, state->buffers, state->buffer_size);
	state->prev_data = state->buffers + state->buffer_size;
	state->window = state->prev_data + state->prev_data_size;

	/* Allocate buffers for FFT input and output data */
#if MFCC_FFT_BITS == 16
	fft->fft_buffer_size = fft->fft_padded_size * sizeof(struct icomplex16);
#else
	fft->fft_buffer_size = fft->fft_padded_size * sizeof(struct icomplex32);
#endif
	fft->fft_buf = mod_zalloc(mod, fft->fft_buffer_size);
	if (!fft->fft_buf) {
		comp_err(dev, "Failed FFT buffer allocate");
		return -ENOMEM;
	}

	fft->fft_out = mod_zalloc(mod, fft->fft_buffer_size);
	if (!fft->fft_out) {
		comp_err(dev, "Failed FFT output allocate");
		return -ENOMEM;
	}

	fft->fft_fill_start_idx = 0; /* From config pad_type */

	/* Setup FFT */
	fft->fft_plan = mod_fft_plan_new(mod, fft->fft_buf, fft->fft_out, fft->fft_padded_size,
					 MFCC_FFT_BITS);
	if (!fft->fft_plan) {
		comp_err(dev, "Failed FFT init");
		return -EINVAL;
	}

	comp_info(dev, "mfcc_setup(), window = %d, num_mel_bins = %d, num_ceps = %d, norm = %d",
		  config->window, config->num_mel_bins, config->num_ceps, config->norm);
	comp_info(dev, "mfcc_setup(), low_freq = %d, high_freq = %d",
		  state->low_freq, state->high_freq);

	/* Setup window */
	ret = mfcc_get_window(state, config->window);
	if (ret < 0) {
		comp_err(dev, "Failed Window function");
		return ret;
	}

	/* Setup Mel auditory filterbank. FFT input and output buffers are used
	 * as scratch in Mel filterbank initialization. Filterbank get function will
	 * return error if not sufficient size.
	 */
	fb->samplerate = sample_rate;
	fb->start_freq = state->low_freq;
	fb->end_freq = state->high_freq;
	fb->mel_bins = config->num_mel_bins;
	fb->slaney_normalize = config->norm == MFCC_MEL_NORM_SLANEY; /* True if slaney */
	fb->mel_log_scale = (enum psy_mel_log_scale)((int)config->mel_log);  /* LOG, LOG10 or DB */
	fb->fft_bins = fft->fft_padded_size;
	fb->half_fft_bins = (fft->fft_padded_size >> 1) + 1;
	fb->scratch_data1 = (int16_t *)fft->fft_buf;
	fb->scratch_data2 = (int16_t *)fft->fft_out;
	fb->scratch_length1 = fft->fft_buffer_size / sizeof(int16_t);
	fb->scratch_length2 = fft->fft_buffer_size / sizeof(int16_t);
	ret = mod_psy_get_mel_filterbank(mod, fb);
	if (ret < 0) {
		comp_err(dev, "Failed Mel filterbank");
		return ret;
	}

	/* Setup DCT */
	dct->num_in = config->num_mel_bins;
	dct->num_out = config->num_ceps;
	dct->type = (enum dct_type)config->dct;
	dct->ortho = true;
	ret = mod_dct_initialize_16(mod, dct);
	if (ret < 0) {
		comp_err(dev, "Failed DCT init");
		return ret;
	}

	state->lifter.num_ceps = config->num_ceps;
	state->lifter.cepstral_lifter = config->cepstral_lifter; /* Q7.9 max 64.0*/
	ret = mfcc_get_cepstral_lifter(mod, &state->lifter);
	if (ret < 0) {
		comp_err(dev, "Failed cepstral lifter");
		return ret;
	}

	/* Scratch overlay during runtime
	 *
	 *  +--------------------------------------------------------+
	 *  | 1. fft_buf[], 16 bits,size x 4, e.g. 512 -> 2048 bytes |
	 *  +-------------------------------------+------------------+
	 *  | 3. power_spectra[],                 |
	 *  |    32 bits, e.g. x257 -> 1028 bytes |
	 *  +-------------------------------------+
	 *
	 *  +---------------------------------------------------------------------------------+
	 *  | 2. fft_out[], 16 bits,size x 4, e.g. 512 -> 2048 bytes                          |
	 *  +----------------------------------+----------------------------------+-----------+
	 *  | 4. mel_spectra[],                | 5. cepstral_coef[],              |
	 *  |    16 bits, e.g. x23 -> 46 bytes |    16 bits, e.g. 13x -> 26 bytes |
	 *  +----------------------------------+----------------------------------+
	 *
	 */

	/* Use FFT buffer as scratch for later computed data */
	state->power_spectra = (int32_t *)&fft->fft_buf[0];
	state->mel_spectra = (struct mat_matrix_16b *)&fft->fft_out[0];
	state->cepstral_coef = (struct mat_matrix_16b *)
		&state->mel_spectra->data[state->dct.num_in];

	/* Set initial state for STFT */
	state->waiting_fill = true;
	state->prev_samples_valid = false;

	comp_dbg(dev, "mfcc_setup(), done");
	return 0;
}

void mfcc_free_buffers(struct processing_module *mod)
{
	struct mfcc_comp_data *cd = module_get_private_data(mod);

	mod_fft_plan_free(mod, cd->state.fft.fft_plan);
	mod_free(mod, cd->state.fft.fft_buf);
	mod_free(mod, cd->state.fft.fft_out);
	mod_free(mod, cd->state.buffers);
	mod_free(mod, cd->state.melfb.data);
	mod_free(mod, cd->state.dct.matrix);
	mod_free(mod, cd->state.lifter.matrix);
}
