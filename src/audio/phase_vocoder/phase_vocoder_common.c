// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.

#include <sof/audio/component.h>
#include <sof/audio/audio_stream.h>
#include <sof/math/auditory.h>
#include <sof/math/icomplex32.h>
#include <sof/math/matrix.h>
#include <sof/math/sqrt.h>
#include <sof/math/trig.h>
#include <sof/math/window.h>
#include <sof/trace/trace.h>

#include "phase_vocoder.h"

#include <errno.h>
#include <stddef.h>
#include <stdint.h>

#if STFT_DEBUG
extern FILE *stft_debug_fft_in_fh;
extern FILE *stft_debug_fft_out_fh;
extern FILE *stft_debug_ifft_out_fh;

static void debug_print_to_file_real(FILE *fh, struct icomplex32 *c, int n)
{
	for (int i = 0; i < n; i++)
		fprintf(fh, "%d\n", c[i].real);
}

static void debug_print_to_file_complex(FILE *fh, struct icomplex32 *c, int n)
{
	for (int i = 0; i < n; i++)
		fprintf(fh, "%d %d\n", c[i].real, c[i].imag);
}
#endif

LOG_MODULE_REGISTER(phase_vocoder_common, CONFIG_SOF_LOG_LEVEL);

/*
 * The main processing function for PHASE_VOCODER
 */

static int stft_get_num_ffts_avail(struct phase_vocoder_state *state, int channel)
{
	struct phase_vocoder_buffer *ibuf = &state->ibuf[channel];
	struct phase_vocoder_fft *fft = &state->fft;

	/* Wait for FFT hop size of new data */
	return ibuf->s_avail / fft->fft_hop_size;
}

static void stft_do_fft(struct phase_vocoder_state *state, int ch)
{
	struct phase_vocoder_fft *fft = &state->fft;

	/* Copy data to FFT input buffer from overlap buffer and from new samples buffer */
	phase_vocoder_fill_fft_buffer(state, ch);

	/* Window function */
	phase_vocoder_apply_window(state);

#if STFT_DEBUG
	debug_print_to_file_real(stft_debug_fft_in_fh, fft->fft_buf, fft->fft_size);
#endif

	/* Compute FFT. A full scale s16 sine input with 2^N samples period in low
	 * part of s32 real part and zero imaginary part gives to output about 0.5
	 * full scale 32 bit output to real and imaginary. The scaling is same for
	 * all FFT sizes.
	 */
	fft_execute_32(fft->fft_plan, false);

#if STFT_DEBUG
	debug_print_to_file_complex(stft_debug_fft_out_fh, fft->fft_out, fft->fft_size);
#endif
}

static int stft_do_ifft(struct phase_vocoder_state *state, int ch)
{
	struct phase_vocoder_fft *fft = &state->fft;

	/* Compute IFFT */
	fft_execute_32(fft->ifft_plan, true);

#if STFT_DEBUG
	debug_print_to_file_complex(stft_debug_ifft_out_fh, fft->fft_buf, fft->fft_size);
#endif

	/* Window function */
	phase_vocoder_apply_window(state);

	/* Copy to output buffer */
	return phase_vocoder_overlap_add_ifft_buffer(state, ch);
}

static void stft_convert_to_polar(struct phase_vocoder_fft *fft, struct ipolar32 *polar_data)
{
	int i;

	for (i = 0; i < fft->half_fft_size; i++)
		sofm_icomplex32_to_polar(&fft->fft_out[i], &polar_data[i]);
}

static void stft_convert_to_complex(struct ipolar32 *polar_data, struct phase_vocoder_fft *fft)
{
	int i;

	for (i = 0; i < fft->half_fft_size; i++)
		sofm_ipolar32_to_complex(&polar_data[i], &fft->fft_out[i]);
}

static void stft_apply_fft_symmetry(struct phase_vocoder_fft *fft)
{
	int i, j, k;

	j = 2 * fft->half_fft_size - 2;
	for (i = fft->half_fft_size; i < fft->fft_size; i++) {
		k = j - i;
		fft->fft_out[i].real = fft->fft_out[k].real;
		fft->fft_out[i].imag = -fft->fft_out[k].imag;
	}
}

static void phase_vocoder_interpolation_parameters(struct phase_vocoder_comp_data *cd)
{
	struct phase_vocoder_state *state = &cd->state;
	int64_t input_frame_num_frac;
	int32_t input_frame_num_rnd;

	input_frame_num_frac = (int64_t)state->num_output_ifft * cd->state.speed; /* Q31.29 */
	input_frame_num_rnd = Q_SHIFT_RND(input_frame_num_frac, 29, 0);
	state->num_input_fft_to_use = input_frame_num_rnd + 1;
	state->interpolate_fraction = input_frame_num_frac - ((int64_t)input_frame_num_rnd << 29);
}

#if 0
static int32_t unwrap_angle(int32_t angle)
{
	if (angle > PHASE_VOCODER_PI_Q28)
		return angle - PHASE_VOCODER_TWO_PI_Q28;
	else if (angle < -PHASE_VOCODER_PI_Q28)
		return angle + PHASE_VOCODER_TWO_PI_Q28;
	else
		return angle;
}
#endif

static int32_t unwrap_angle_q27(int32_t angle)
{
	while (angle > PHASE_VOCODER_PI_Q27)
		angle -= PHASE_VOCODER_TWO_PI_Q27;

	while (angle < -PHASE_VOCODER_PI_Q27)
		angle += PHASE_VOCODER_TWO_PI_Q27;

	return angle;
}

void phase_vocoder_reset_for_new_speed(struct phase_vocoder_comp_data *cd)
{
	cd->state.speed = cd->speed_ctrl;
	cd->state.num_input_fft = 0;
	cd->state.num_output_ifft = 0;
}

// TODO: Civilized input and output counters reset to prevent int32_t wrap
static int stft_do_fft_ifft(const struct processing_module *mod)
{
	struct phase_vocoder_comp_data *cd = module_get_private_data(mod);
	struct phase_vocoder_state *state = &cd->state;
	struct phase_vocoder_polar *polar = &state->polar;
	struct phase_vocoder_fft *fft = &state->fft;
	struct ipolar32 *polar_data_prev_ch;
	struct ipolar32 *polar_data_ch;
	int32_t *angle_delta_prev_ch;
	int32_t *angle_delta_ch;
	int32_t *output_phase_ch;
	int32_t one_minus_frac;
	int32_t frac;
	int32_t p1, p2;
	int32_t a;
	const size_t polar_fft_half_bytes = sizeof(struct ipolar32) * fft->half_fft_size;
	const size_t int32_fft_half_bytes = sizeof(int32_t) * fft->half_fft_size;
	int num_fft;
	int ret = 0;
	int ch;
	int i;

	num_fft = stft_get_num_ffts_avail(state, 0);
	if (!num_fft)
		return 0;

	/* First analysis FFT */
	if (!state->num_input_fft) {
		for (ch = 0; ch < cd->channels; ch++) {
			stft_do_fft(state, ch);

			/* Convert half-FFT to polar */
			polar_data_ch = polar->polar[ch];
			angle_delta_ch = polar->angle_delta[ch];
			stft_convert_to_polar(&state->fft, polar_data_ch);
			for (i = 0; i < fft->half_fft_size; i++)
				angle_delta_ch[i] = polar_data_ch[i].angle >> 2;
		}
		state->num_input_fft++;
		num_fft--;
	}

	phase_vocoder_interpolation_parameters(cd);
	while (state->num_input_fft < state->num_input_fft_to_use && num_fft > 0) {
		for (ch = 0; ch < cd->channels; ch++) {
			stft_do_fft(state, ch);

			/* Update previous polar data.
			 * Note: Not using memcpy_s() since this is hot algorithm code.
			 */
			polar_data_prev_ch = polar->polar_prev[ch];
			polar_data_ch = polar->polar[ch];
			memcpy(polar_data_prev_ch, polar_data_ch, polar_fft_half_bytes);

			/* Convert half-FFT to polar */
			stft_convert_to_polar(&state->fft, polar_data_ch);

			/* Update previous delta phase data */
			angle_delta_ch = polar->angle_delta[ch];
			angle_delta_prev_ch = polar->angle_delta_prev[ch];
			memcpy(angle_delta_prev_ch, angle_delta_ch, int32_fft_half_bytes);

			/* Calculate new delta phase */
			for (i = 0; i < fft->half_fft_size; i++) {
				/* Calculate as Q4.28 */
				a = (polar_data_ch[i].angle >> 2) -
				    (polar_data_prev_ch[i].angle >> 2);
				angle_delta_ch[i] = unwrap_angle_q27(a);
			}
		}
		state->num_input_fft++;
		num_fft--;
	}

	if (state->num_input_fft < state->num_input_fft_to_use)
		return 0;

	/* Interpolate IFFT frame */
	frac = state->interpolate_fraction;
	one_minus_frac = PHASE_VOCODER_ONE_Q29 - frac;

	for (ch = 0; ch < cd->channels; ch++) {
		polar_data_prev_ch = polar->polar_prev[ch];
		polar_data_ch = polar->polar[ch];
		angle_delta_ch = polar->angle_delta[ch];
		angle_delta_prev_ch = polar->angle_delta_prev[ch];
		output_phase_ch = polar->output_phase[ch];

		for (i = 0; i < fft->half_fft_size; i++) {
			p1 = Q_MULTSR_32X32((int64_t)one_minus_frac,
					    polar_data_prev_ch[i].magnitude, 29, 30, 30);
			p2 = Q_MULTSR_32X32((int64_t)frac, polar_data_ch[i].magnitude, 29, 30, 30);
			polar->polar_tmp[i].magnitude = p1 + p2;

			a = output_phase_ch[i];
			p1 = Q_MULTSR_32X32((int64_t)one_minus_frac, angle_delta_prev_ch[i], 29, 27,
					    27);
			p2 = Q_MULTSR_32X32((int64_t)frac, angle_delta_ch[i], 29, 27, 27);
			a = output_phase_ch[i] + p1 + p2;
			output_phase_ch[i] = unwrap_angle_q27(a);
			polar->polar_tmp[i].angle = output_phase_ch[i] << 2; /* Q29 */
		}

		/* Convert back to (re, im) complex, and fix upper part */
		stft_convert_to_complex(polar->polar_tmp, &state->fft);
		stft_apply_fft_symmetry(&state->fft);
		ret = stft_do_ifft(state, ch);
		if (ret) {
			comp_err(mod->dev, "IFFT failure, check output overlap-add buffer size");
			return ret;
		}
	}

	comp_dbg(mod->dev, "no = %d, ni = %d, frac = %d", state->num_output_ifft,
		 state->num_input_fft, frac);
	state->num_output_ifft++;
	state->first_output_ifft_done = true;
	return 0;
}

static int phase_vocoder_check_fft_run_need(struct phase_vocoder_comp_data *cd)
{
	return cd->state.obuf[0].s_avail < cd->state.fft.fft_hop_size;
}

#if CONFIG_FORMAT_S32LE
static int phase_vocoder_output_zeros_s32(struct phase_vocoder_comp_data *cd, struct sof_sink *sink,
					  int frames)
{
	int32_t *y, *y_start, *y_end;
	int samples = frames * cd->channels;
	size_t bytes = samples * sizeof(int32_t);
	int samples_without_wrap;
	int y_size;
	int ret;

	/* Get pointer to sink data in circular buffer, buffer start and size. */
	ret = sink_get_buffer_s32(sink, bytes, &y, &y_start, &y_size);
	if (ret)
		return ret;

	/* Set helper pointers to buffer end for wrap check. Then loop until all
	 * samples are processed.
	 */
	y_end = y_start + y_size;
	while (samples) {
		/* Find out samples to process before first wrap or end of data. */
		samples_without_wrap = y_end - y;
		samples_without_wrap = MIN(samples_without_wrap, samples);
		memset(y, 0, samples_without_wrap * sizeof(int32_t));
		y += samples_without_wrap;

		/* Check for wrap */
		if (y >= y_end)
			y -= y_size;

		/* Update processed samples count for next loop iteration. */
		samples -= samples_without_wrap;
	}

	/* Update the source and sink for bytes consumed and produced. Return success. */
	sink_commit_buffer(sink, bytes);
	return 0;
}

static int phase_vocoder_s32(const struct processing_module *mod, struct sof_source *source,
			     struct sof_sink *sink, uint32_t source_frames, uint32_t sink_frames)
{
	struct phase_vocoder_comp_data *cd = module_get_private_data(mod);
	int ret;

	if (phase_vocoder_check_fft_run_need(cd)) {
		/* Get samples from source buffer */
		phase_vocoder_source_s32(cd, source, source_frames);

		/* Do STFT, processing and inverse STFT */
		ret = stft_do_fft_ifft(mod);
		if (ret)
			return ret;
	}

	/* Get samples from source buffer */
	if (cd->state.first_output_ifft_done)
		ret = phase_vocoder_sink_s32(cd, sink, sink_frames);
	else
		ret = phase_vocoder_output_zeros_s32(cd, sink, sink_frames);

	return ret;
}
#endif /* CONFIG_FORMAT_S32LE */

#if CONFIG_FORMAT_S16LE
static int phase_vocoder_output_zeros_s16(struct phase_vocoder_comp_data *cd, struct sof_sink *sink,
					  int frames)
{
	int16_t *y, *y_start, *y_end;
	int samples = frames * cd->channels;
	size_t bytes = samples * sizeof(int16_t);
	int samples_without_wrap;
	int y_size;
	int ret;

	/* Get pointer to sink data in circular buffer, buffer start and size. */
	ret = sink_get_buffer_s16(sink, bytes, &y, &y_start, &y_size);
	if (ret)
		return ret;

	/* Set helper pointers to buffer end for wrap check. Then loop until all
	 * samples are processed.
	 */
	y_end = y_start + y_size;
	while (samples) {
		/* Find out samples to process before first wrap or end of data. */
		samples_without_wrap = y_end - y;
		samples_without_wrap = MIN(samples_without_wrap, samples);
		memset(y, 0, samples_without_wrap * sizeof(int16_t));
		y += samples_without_wrap;

		/* Check for wrap */
		if (y >= y_end)
			y -= y_size;

		/* Update processed samples count for next loop iteration. */
		samples -= samples_without_wrap;
	}

	/* Update the source and sink for bytes consumed and produced. Return success. */
	sink_commit_buffer(sink, bytes);
	return 0;
}

static int phase_vocoder_s16(const struct processing_module *mod, struct sof_source *source,
			     struct sof_sink *sink, uint32_t source_frames, uint32_t sink_frames)
{
	struct phase_vocoder_comp_data *cd = module_get_private_data(mod);
	int ret;

	if (phase_vocoder_check_fft_run_need(cd)) {
		/* Get samples from source buffer */
		phase_vocoder_source_s16(cd, source, source_frames);

		/* Do STFT, processing and inverse STFT */
		ret = stft_do_fft_ifft(mod);
		if (ret)
			return ret;
	}

	/* Get samples from source buffer */
	if (cd->state.first_output_ifft_done)
		ret = phase_vocoder_sink_s16(cd, sink, sink_frames);
	else
		ret = phase_vocoder_output_zeros_s16(cd, sink, sink_frames);

	return ret;
}
#endif /* CONFIG_FORMAT_S16LE */

#if CONFIG_FORMAT_S24LE
#endif /* CONFIG_FORMAT_S24LE */

/* This struct array defines the used processing functions for
 * the PCM formats
 */
const struct phase_vocoder_proc_fnmap phase_vocoder_functions[] = {
#if CONFIG_FORMAT_S16LE
	{SOF_IPC_FRAME_S16_LE, phase_vocoder_s16},
	{SOF_IPC_FRAME_S32_LE, phase_vocoder_s32},
#endif
};

/**
 * phase_vocoder_find_proc_func() - Find suitable processing function.
 * @src_fmt: Enum value for PCM format.
 *
 * This function finds the suitable processing function to use for
 * the used PCM format. If not found, return NULL.
 *
 * Return: Pointer to processing function for the requested PCM format.
 */
phase_vocoder_func phase_vocoder_find_proc_func(enum sof_ipc_frame src_fmt)
{
	int i;

	/* Find suitable processing function from map */
	for (i = 0; i < ARRAY_SIZE(phase_vocoder_functions); i++)
		if (src_fmt == phase_vocoder_functions[i].frame_fmt)
			return phase_vocoder_functions[i].phase_vocoder_function;

	return NULL;
}
