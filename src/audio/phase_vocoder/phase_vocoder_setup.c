// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.

#include <sof/audio/component.h>
#include <sof/audio/audio_stream.h>
#include <sof/math/auditory.h>
#include <sof/math/icomplex32.h>
#include <sof/math/trig.h>
#include <sof/math/window.h>
#include <sof/trace/trace.h>
#include "phase_vocoder.h"

#include <errno.h>
#include <stddef.h>
#include <stdint.h>

#define STFT_MAX_ALLOC_SIZE 65536

LOG_MODULE_REGISTER(phase_vocoder_setup, CONFIG_SOF_LOG_LEVEL);

static void phase_vocoder_init_buffer(struct phase_vocoder_buffer *buf, int32_t *base, int size)
{
	buf->addr = base;
	buf->end_addr = base + size;
	buf->r_ptr = base;
	buf->w_ptr = base;
	buf->s_free = size;
	buf->s_avail = 0;
	buf->s_length = size;
}

static int phase_vocoder_get_window(struct phase_vocoder_state *state,
				    enum sof_phase_vocoder_fft_window_type name)
{
	struct phase_vocoder_fft *fft = &state->fft;

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

int phase_vocoder_setup(struct processing_module *mod)
{
	struct phase_vocoder_comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	struct sof_phase_vocoder_config *config = cd->config;
	struct phase_vocoder_state *state = &cd->state;
	struct phase_vocoder_fft *fft = &state->fft;
	struct phase_vocoder_polar *polar = &state->polar;
	size_t sample_buffers_size;
	size_t ibuf_size;
	size_t obuf_size;
	size_t prev_size;
	int32_t *addr;
	int channels;
	int ret;
	int i;

	comp_dbg(dev, "phase_vocoder_setup()");

	/* Check size */
	if (config->size != sizeof(struct sof_phase_vocoder_config)) {
		comp_err(dev, "Illegal configuration size %d.", config->size);
		return -EINVAL;
	}

	if (config->sample_frequency != cd->sample_rate)
		comp_warn(dev, "Config sample_frequency does not match stream");

	if (config->mono) {
		cd->process_channels = 1;
		if (cd->stream_channels > 1)
			cd->mono_mix_coef = (int32_t)((1LL << 31) / cd->stream_channels);
		else
			cd->mono_mix_coef = INT32_MAX; /* Q1.31 1.0 */

		comp_info(dev, "mono_mix_coef %d", cd->mono_mix_coef);
	} else {
		cd->process_channels = cd->stream_channels;
		cd->mono_mix_coef = 0;
	}

	fft->fft_size = config->frame_length;
	fft->fft_hop_size = config->frame_shift;
	fft->half_fft_size = (fft->fft_size >> 1) + 1;

	/* FFT size needs to be a multiple of 4 for Xtensa HiFi SIMD,
	 * and FFT hop size needs to be a multiple of 2. Check also
	 * for otherwise sane values.
	 */
	if (fft->fft_size <= 0 || fft->fft_hop_size <= 0 ||
	    fft->fft_hop_size > fft->fft_size ||
	    (fft->fft_size & 3) || (fft->fft_hop_size & 1)) {
		comp_err(dev, "FFT size %d or hop size %d are invalid.",
			 fft->fft_size, fft->fft_hop_size);
		return -EINVAL;
	}

	comp_info(dev, "fft_size = %d, fft_hop_size = %d, window = %d", fft->fft_size,
		  fft->fft_hop_size, config->window);

	/* Calculated parameters */
	prev_size = fft->fft_size - fft->fft_hop_size;
	ibuf_size = fft->fft_hop_size + cd->max_input_frames;
	obuf_size = fft->fft_size + fft->fft_hop_size;
	state->prev_data_size = prev_size;

	/* Allocate buffer for input samples, overlap buffer, window */
	channels = cd->process_channels;
	sample_buffers_size =
		sizeof(int32_t) * (channels * (ibuf_size + obuf_size + prev_size) + fft->fft_size);

	if (sample_buffers_size > STFT_MAX_ALLOC_SIZE) {
		comp_err(dev, "Illegal allocation size");
		return -EINVAL;
	}

	addr = mod_balloc(mod, sample_buffers_size);
	if (!addr) {
		comp_err(dev, "Failed buffer allocate");
		ret = -ENOMEM;
		goto exit;
	}

	memset(addr, 0, sample_buffers_size);
	state->buffers = addr;
	for (i = 0; i < channels; i++) {
		phase_vocoder_init_buffer(&state->ibuf[i], addr, ibuf_size);
		addr += ibuf_size;
		phase_vocoder_init_buffer(&state->obuf[i], addr, obuf_size);
		addr += obuf_size;
		state->prev_data[i] = addr;
		addr += prev_size;
	}
	state->window = addr;

	/* Allocate buffers for FFT input and output data */
	fft->fft_buffer_size = fft->fft_size * sizeof(struct icomplex32);
	fft->fft_buf = mod_balloc(mod, fft->fft_buffer_size);
	if (!fft->fft_buf) {
		comp_err(dev, "Failed FFT buffer allocate");
		ret = -ENOMEM;
		goto cleanup;
	}

	fft->fft_out = mod_balloc(mod, fft->fft_buffer_size);
	if (!fft->fft_out) {
		comp_err(dev, "Failed FFT output allocate");
		ret = -ENOMEM;
		goto cleanup;
	}

	/* Setup FFT */
	fft->fft_plan = mod_fft_plan_new(mod, fft->fft_buf, fft->fft_out, fft->fft_size, 32);
	if (!fft->fft_plan) {
		comp_err(dev, "Failed FFT init");
		ret = -EINVAL;
		goto cleanup;
	}

	fft->ifft_plan = mod_fft_plan_new(mod, fft->fft_out, fft->fft_buf, fft->fft_size, 32);
	if (!fft->ifft_plan) {
		comp_err(dev, "Failed IFFT init");
		ret = -EINVAL;
		goto cleanup;
	}

	/* Setup window */
	ret = phase_vocoder_get_window(state, config->window);
	if (ret < 0) {
		comp_err(dev, "Failed Window function");
		goto cleanup;
	}

	/* Need to compensate the window function gain */
	state->gain_comp = config->window_gain_comp;

	/* Allocate buffers for polar format data for magnitude and phase interpolation */
	state->phase_vocoder_polar_bytes =
		channels * fft->half_fft_size * (2 * sizeof(struct ipolar32) + 3 * sizeof(int32_t));
	comp_info(dev, "polar buffers size %zu", state->phase_vocoder_polar_bytes);
	addr = mod_balloc(mod, state->phase_vocoder_polar_bytes);
	if (!addr) {
		comp_err(dev, "Failed polar data buffer allocate");
		ret = -ENOMEM;
		goto cleanup;
	}

	memset(addr, 0, state->phase_vocoder_polar_bytes);
	polar->polar_buffer = addr;
	for (i = 0; i < channels; i++) {
		polar->polar[i] = (struct ipolar32 *)addr;
		addr = (int32_t *)((struct ipolar32 *)addr + fft->half_fft_size);
	}

	for (i = 0; i < channels; i++) {
		polar->polar_prev[i] = (struct ipolar32 *)addr;
		addr = (int32_t *)((struct ipolar32 *)addr + fft->half_fft_size);
	}

	for (i = 0; i < channels; i++) {
		polar->angle_delta[i] = addr;
		addr += fft->half_fft_size;
	}

	for (i = 0; i < channels; i++) {
		polar->angle_delta_prev[i] = addr;
		addr += fft->half_fft_size;
	}

	for (i = 0; i < channels; i++) {
		polar->output_phase[i] = addr;
		addr += fft->half_fft_size;
	}

	/* Use FFT buffer as scratch */
	polar->polar_tmp = (struct ipolar32 *)fft->fft_out;
	comp_dbg(dev, "phase_vocoder_setup(), done");
	return 0;

cleanup:
	phase_vocoder_free_buffers(mod);
exit:
	return ret;
}

void phase_vocoder_free_buffers(struct processing_module *mod)
{
	struct phase_vocoder_comp_data *cd = module_get_private_data(mod);
	struct phase_vocoder_state *state = &cd->state;
	struct phase_vocoder_fft *fft = &state->fft;
	struct phase_vocoder_polar *polar = &state->polar;

	/* All free helpers tolerate NULL; clear pointers so a subsequent reset
	 * does not double-free if setup() failed mid-way.
	 */
	mod_fft_plan_free(mod, fft->ifft_plan);
	fft->ifft_plan = NULL;
	mod_fft_plan_free(mod, fft->fft_plan);
	fft->fft_plan = NULL;
	mod_free(mod, fft->fft_buf);
	fft->fft_buf = NULL;
	mod_free(mod, fft->fft_out);
	fft->fft_out = NULL;
	mod_free(mod, state->buffers);
	state->buffers = NULL;
	mod_free(mod, polar->polar_buffer);
	polar->polar_buffer = NULL;
}
