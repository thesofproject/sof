// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2025 Intel Corporation.

#include <sof/audio/component.h>
#include <sof/audio/audio_stream.h>
#include <sof/math/auditory.h>
#include <sof/math/trig.h>
#include <sof/math/window.h>
#include <sof/trace/trace.h>
#include "stft_process.h"

#include <errno.h>
#include <stddef.h>
#include <stdint.h>

/* Definitions for cepstral lifter */
#define PI_Q23 Q_CONVERT_FLOAT(3.1415926536, 23)
#define TWO_PI_Q23 Q_CONVERT_FLOAT(6.2831853072, 23)
#define ONE_Q9 Q_CONVERT_FLOAT(1, 9)

#define STFT_MAX_ALLOC_SIZE	65536

LOG_MODULE_REGISTER(stft_process_setup, CONFIG_SOF_LOG_LEVEL);

static void stft_process_init_buffer(struct stft_process_buffer *buf, int32_t *base, int size)
{
	buf->addr = base;
	buf->end_addr = base + size;
	buf->r_ptr = base;
	buf->w_ptr = base;
	buf->s_free = size;
	buf->s_avail = 0;
	buf->s_length = size;
}

static int stft_process_get_window(struct stft_process_state *state,
				   enum sof_stft_process_fft_window_type name)
{
	struct stft_process_fft *fft = &state->fft;

	switch (name) {
	case STFT_RECTANGULAR_WINDOW:
		win_rectangular_32b(state->window, fft->fft_size);
		return 0;
	case STFT_BLACKMAN_WINDOW:
		win_blackman_32b(state->window, fft->fft_size, WIN_BLACKMAN_A0_Q31);
		return 0;
	case STFT_HAMMING_WINDOW:
		win_hamming_32b(state->window, fft->fft_size);
		return 0;
	case STFT_HANN_WINDOW:
		win_hann_32b(state->window, fft->fft_size);
		return 0;

	default:
		return -EINVAL;
	}
}

/* TODO stft_process setup needs to use the config blob, not hard coded parameters.
 * Also this is a too long function. Split to STFT, Mel filter, etc. parts.
 */
int stft_process_setup(struct processing_module *mod, int max_frames,
		       int sample_rate, int channels)
{
	struct stft_comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	struct sof_stft_process_config *config = cd->config;
	struct stft_process_state *state = &cd->state;
	struct stft_process_fft *fft = &state->fft;
	size_t sample_buffers_size;
	size_t ibuf_size;
	size_t obuf_size;
	size_t prev_size;
	int32_t *addr;
	int ret;
	int i;

	comp_dbg(dev, "stft_process_setup()");

	/* Check size */
	if (config->size != sizeof(struct sof_stft_process_config)) {
		comp_err(dev, "Illegal configuration size %d.", config->size);
		return -EINVAL;
	}

	if (config->sample_frequency != sample_rate) {
		comp_err(dev, "Config sample_frequency does not match stream");
		return -EINVAL;
	}

	cd->max_frames = max_frames;
	state->sample_rate = sample_rate;

	comp_info(dev, "source_channel = %d, stream_channels = %d",
		  config->channel, channels);
	if (config->channel >= channels) {
		comp_err(dev, "Illegal channel");
		return -EINVAL;
	}

	if (config->channel < 0)
		state->source_channel = 0;
	else
		state->source_channel = config->channel;

	fft->fft_size = config->frame_length;
	fft->fft_padded_size = fft->fft_size; /* Same */
	fft->fft_hop_size = config->frame_shift;
	fft->half_fft_size = (fft->fft_padded_size >> 1) + 1;

	comp_info(dev, "fft_size = %d, fft_padded_size = %d, fft_hop_size = %d, window = %d",
		  fft->fft_size, fft->fft_padded_size, fft->fft_hop_size, config->window);

	/* Calculated parameters */
	state->prev_data_size = fft->fft_size - fft->fft_hop_size;
	ibuf_size = fft->fft_hop_size + cd->max_frames;
	obuf_size = fft->fft_size + cd->max_frames;
	prev_size = state->prev_data_size;

	/* Allocate buffer input samples, overlap buffer, window */
	sample_buffers_size = sizeof(int32_t) * cd->channels *
		(ibuf_size + obuf_size + prev_size + fft->fft_size);

	if (sample_buffers_size > STFT_MAX_ALLOC_SIZE || sample_buffers_size < 0) {
		comp_err(dev, "Illegal allocation size");
		return -EINVAL;
	}

	state->buffers = mod_balloc(mod, sample_buffers_size);
	if (!state->buffers) {
		comp_err(dev, "Failed buffer allocate");
		ret = -ENOMEM;
		goto exit;
	}

	bzero(state->buffers, sample_buffers_size);
	addr = state->buffers;
	for (i = 0; i < cd->channels; i++) {
		stft_process_init_buffer(&state->ibuf[i], addr, ibuf_size);
		addr += ibuf_size;
		stft_process_init_buffer(&state->obuf[i], addr, obuf_size);
		addr += obuf_size;
		state->prev_data[i] = addr;
		addr += prev_size;
	}
	state->window = addr;

	/* Allocate buffers for FFT input and output data */
	fft->fft_buffer_size = fft->fft_padded_size * sizeof(struct icomplex32);
	fft->fft_buf = mod_balloc(mod, fft->fft_buffer_size);
	if (!fft->fft_buf) {
		comp_err(dev, "Failed FFT buffer allocate");
		ret = -ENOMEM;
		goto free_buffers;
	}

	fft->fft_out = mod_balloc(mod, fft->fft_buffer_size);
	if (!fft->fft_out) {
		comp_err(dev, "Failed FFT output allocate");
		ret = -ENOMEM;
		goto free_fft_buf;
	}

	fft->fft_fill_start_idx = 0; /* From config pad_type */

	/* Setup FFT */
	fft->fft_plan = mod_fft_multi_plan_new(mod, fft->fft_buf, fft->fft_out,
					       fft->fft_padded_size, 32);
	if (!fft->fft_plan) {
		comp_err(dev, "Failed FFT init");
		ret = -EINVAL;
		goto free_fft_out;
	}

	fft->ifft_plan = mod_fft_multi_plan_new(mod, fft->fft_out, fft->fft_buf,
						fft->fft_padded_size, 32);
	if (!fft->ifft_plan) {
		comp_err(dev, "Failed IFFT init");
		ret = -EINVAL;
		goto free_ifft_out;
	}

	/* Setup window */
	ret = stft_process_get_window(state, config->window);
	if (ret < 0) {
		comp_err(dev, "Failed Window function");
		goto free_window_out;
	}

	/* Need to compensate the window function gain */
	state->gain_comp = config->window_gain_comp;

	/* Set initial state for STFT */
	state->waiting_fill = true;
	state->prev_samples_valid = false;

	comp_dbg(dev, "stft_process_setup(), done");
	return 0;

free_window_out:
	mod_free(mod, fft->ifft_plan);

free_ifft_out:
	mod_free(mod, fft->fft_plan);

free_fft_out:
	mod_free(mod, fft->fft_out);

free_fft_buf:
	mod_free(mod, fft->fft_buf);

free_buffers:
	mod_free(mod, state->buffers);

exit:
	return ret;
}

void stft_process_free_buffers(struct processing_module *mod)
{
	struct stft_comp_data *cd = module_get_private_data(mod);
	struct stft_process_state *state = &cd->state;
	struct stft_process_fft *fft = &state->fft;

	mod_fft_multi_plan_free(mod, fft->ifft_plan);
	mod_fft_multi_plan_free(mod, fft->fft_plan);
	mod_free(mod, cd->state.fft.fft_buf);
	mod_free(mod, cd->state.fft.fft_out);
	mod_free(mod, cd->state.buffers);
}
