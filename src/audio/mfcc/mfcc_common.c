// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2023-2026 Intel Corporation.
//
// Author: Andrula Song <andrula.song@intel.com>

#include <sof/audio/mfcc/mfcc_comp.h>

#include <sof/audio/component.h>
#include <module/audio/sink_api.h>
#include <module/audio/source_api.h>
#include <sof/audio/format.h>
#include <sof/math/auditory.h>
#include <sof/math/fft.h>
#include <sof/math/matrix.h>
#include <sof/math/sqrt.h>
#include <sof/math/trig.h>
#include <sof/math/window.h>
#include <sof/trace/trace.h>
#include <user/mfcc.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <rtos/string.h>

#include <sof/audio/mfcc/mfcc_vad.h>

LOG_MODULE_REGISTER(mfcc_common, CONFIG_SOF_LOG_LEVEL);

/*
 * Source/sink API based source copy functions.
 * These use sof_source API and are compiled on all platforms (generic, HiFi3, HiFi4).
 */

#if CONFIG_FORMAT_S16LE
/**
 * \brief Copy S16_LE input frames from source to MFCC circular buffer.
 *
 * Reads \p frames frames from \p source, selects channel \p source_channel,
 * optionally applies the pre-emphasis filter \p emph, and writes Q1.15 mono
 * samples to the MFCC input circular buffer \p buf. The source read is
 * released after the copy completes. On source read failure the function
 * returns without modifying \p buf state.
 *
 * \param[in,out] source Audio source providing interleaved S16_LE frames.
 * \param[in,out] buf MFCC input circular buffer (mono Q1.15).
 * \param[in,out] emph Pre-emphasis filter state; updated when enabled.
 * \param[in] frames Number of input frames to copy.
 * \param[in] source_channel Channel index to extract from the source.
 */
void mfcc_source_copy_s16(struct sof_source *source, struct mfcc_buffer *buf,
			  struct mfcc_pre_emph *emph, int frames, int source_channel)
{
	int16_t const *x;
	int16_t const *x_start;
	int16_t const *x_end;
	int x_size;
	int num_channels = source_get_channels(source);
	size_t req_bytes = frames * num_channels * sizeof(int16_t);
	int16_t *w = buf->w_ptr;
	int src_without_wrap;
	int dst_without_wrap;
	int samples_without_wrap;
	int remaining = frames;
	int32_t s;
	int ret;
	int i;

	ret = source_get_data_s16(source, req_bytes, &x, &x_start, &x_size);
	if (ret)
		return;

	x += source_channel;
	x_end = x_start + x_size;

	while (remaining) {
		src_without_wrap = (x_end - x) / num_channels;
		dst_without_wrap = buf->end_addr - w;
		samples_without_wrap = MIN(src_without_wrap, dst_without_wrap);
		samples_without_wrap = MIN(samples_without_wrap, remaining);
		if (emph->enable) {
			for (i = 0; i < samples_without_wrap; i++) {
				s = (int32_t)emph->delay * emph->coef +
					Q_SHIFT_LEFT(*x, 15, 30);
				*w = sat_int16(Q_SHIFT_RND(s, 30, 15));
				emph->delay = *x;
				x += num_channels;
				w++;
			}
		} else {
			for (i = 0; i < samples_without_wrap; i++) {
				*w = *x;
				x += num_channels;
				w++;
			}
		}
		x = (x >= x_end) ? x - x_size : x;
		w = mfcc_buffer_wrap(buf, w);
		remaining -= samples_without_wrap;
	}

	buf->s_avail += frames;
	buf->s_free -= frames;
	buf->w_ptr = w;
	source_release_data(source, req_bytes);
}
#endif /* CONFIG_FORMAT_S16LE */

#if CONFIG_FORMAT_S24LE
/**
 * \brief Copy S24_4LE input frames from source to MFCC circular buffer.
 *
 * Same as mfcc_source_copy_s16() but accepts S24_4LE input (24-bit data
 * sign-extended in a 32-bit container). Samples are downscaled to Q1.15
 * for the MFCC pipeline.
 *
 * \param[in,out] source Audio source providing interleaved S24_4LE frames.
 * \param[in,out] buf MFCC input circular buffer (mono Q1.15).
 * \param[in,out] emph Pre-emphasis filter state; updated when enabled.
 * \param[in] frames Number of input frames to copy.
 * \param[in] source_channel Channel index to extract from the source.
 */
void mfcc_source_copy_s24(struct sof_source *source, struct mfcc_buffer *buf,
			  struct mfcc_pre_emph *emph, int frames, int source_channel)
{
	int32_t const *x;
	int32_t const *x_start;
	int32_t const *x_end;
	int x_size;
	int num_channels = source_get_channels(source);
	size_t req_bytes = frames * num_channels * sizeof(int32_t);
	int16_t *w = buf->w_ptr;
	int src_without_wrap;
	int dst_without_wrap;
	int samples_without_wrap;
	int remaining = frames;
	int32_t s, tmp;
	int ret;
	int i;

	ret = source_get_data_s32(source, req_bytes, &x, &x_start, &x_size);
	if (ret)
		return;

	x += source_channel;
	x_end = x_start + x_size;

	while (remaining) {
		src_without_wrap = (x_end - x) / num_channels;
		dst_without_wrap = buf->end_addr - w;
		samples_without_wrap = MIN(src_without_wrap, dst_without_wrap);
		samples_without_wrap = MIN(samples_without_wrap, remaining);
		if (emph->enable) {
			for (i = 0; i < samples_without_wrap; i++) {
				s = (int32_t)((uint32_t)*x << 8);
				tmp = (int32_t)emph->delay * emph->coef +
					Q_SHIFT(s, 31, 30);
				*w = sat_int16(Q_SHIFT_RND(tmp, 30, 15));
				emph->delay = sat_int16(Q_SHIFT_RND(s, 31, 15));
				x += num_channels;
				w++;
			}
		} else {
			for (i = 0; i < samples_without_wrap; i++) {
				s = (int32_t)((uint32_t)*x << 8);
				*w = sat_int16(Q_SHIFT_RND(s, 31, 15));
				x += num_channels;
				w++;
			}

		}
		x = (x >= x_end) ? x - x_size : x;
		w = mfcc_buffer_wrap(buf, w);
		remaining -= samples_without_wrap;
	}

	buf->s_avail += frames;
	buf->s_free -= frames;
	buf->w_ptr = w;
	source_release_data(source, req_bytes);
}
#endif /* CONFIG_FORMAT_S24LE */

#if CONFIG_FORMAT_S32LE
/**
 * \brief Copy S32_LE input frames from source to MFCC circular buffer.
 *
 * Same as mfcc_source_copy_s16() but accepts S32_LE input. Samples are
 * downscaled to Q1.15 for the MFCC pipeline.
 *
 * \param[in,out] source Audio source providing interleaved S32_LE frames.
 * \param[in,out] buf MFCC input circular buffer (mono Q1.15).
 * \param[in,out] emph Pre-emphasis filter state; updated when enabled.
 * \param[in] frames Number of input frames to copy.
 * \param[in] source_channel Channel index to extract from the source.
 */
void mfcc_source_copy_s32(struct sof_source *source, struct mfcc_buffer *buf,
			  struct mfcc_pre_emph *emph, int frames, int source_channel)
{
	int32_t const *x;
	int32_t const *x_start;
	int32_t const *x_end;
	int x_size;
	int num_channels = source_get_channels(source);
	size_t req_bytes = frames * num_channels * sizeof(int32_t);
	int16_t *w = buf->w_ptr;
	int src_without_wrap;
	int dst_without_wrap;
	int samples_without_wrap;
	int remaining = frames;
	int32_t s;
	int ret;
	int i;

	ret = source_get_data_s32(source, req_bytes, &x, &x_start, &x_size);
	if (ret)
		return;

	x += source_channel;
	x_end = x_start + x_size;
	while (remaining) {
		src_without_wrap = (x_end - x) / num_channels;
		dst_without_wrap = buf->end_addr - w;
		samples_without_wrap = MIN(src_without_wrap, dst_without_wrap);
		samples_without_wrap = MIN(samples_without_wrap, remaining);
		if (emph->enable) {
			for (i = 0; i < samples_without_wrap; i++) {
				s = (int32_t)emph->delay * emph->coef +
					Q_SHIFT(*x, 31, 30);
				*w = sat_int16(Q_SHIFT_RND(s, 30, 15));
				emph->delay = sat_int16(Q_SHIFT_RND(*x, 31, 15));
				x += num_channels;
				w++;
			}
		} else {
			for (i = 0; i < samples_without_wrap; i++) {
				*w = sat_int16(Q_SHIFT_RND(*x, 31, 15));
				x += num_channels;
				w++;
			}
		}

		x = (x >= x_end) ? x - x_size : x;
		w = mfcc_buffer_wrap(buf, w);
		remaining -= samples_without_wrap;
	}

	buf->s_avail += frames;
	buf->s_free -= frames;
	buf->w_ptr = w;
	source_release_data(source, req_bytes);
}
#endif /* CONFIG_FORMAT_S32LE */

/*
 * The main processing function for MFCC
 */

/**
 * \brief Run one STFT hop and Mel/DCT processing.
 *
 * Waits until the input circular buffer has at least one FFT window of
 * samples, then for each available hop: copies the overlap-add window
 * into the FFT scratch buffer, applies the analysis window, runs the
 * 32-bit FFT, computes Mel log spectra, and either keeps the Mel output
 * (when \c state->mel_only is true) or applies DCT and cepstral lifter
 * to produce cepstral coefficients. Optionally runs VAD on the Mel
 * spectrum and sends an IPC notification on VAD state changes.
 *
 * \param[in,out] mod Processing module.
 * \param[in,out] cd  MFCC component data; state and configuration updated
 *                    in-place. Output values land in \c state->mel_log_32
 *                    (Mel mode) or \c state->cepstral_coef (cepstral mode).
 *
 * \return Number of int32_t output values produced this call (0 if not
 *         enough input data was available, or no error condition was
 *         detected).
 */
int mfcc_stft_process(struct processing_module *mod, struct mfcc_comp_data *cd)
{
	const struct comp_dev *dev = mod->dev;
	struct sof_mfcc_config *config = cd->config;
	struct mfcc_state *state = &cd->state;
	struct mfcc_buffer *buf = &state->buf;
	struct mfcc_fft *fft = &state->fft;
	int mel_scale_shift;
	int input_shift;
	int j;
	int m;
	int cc_count = 0;
	int64_t s;
	int32_t mel_value;
	int32_t peak;
	int32_t clamp_value;
	bool vad_now;

	/* Phase 1, wait until whole fft_size is filled with valid data. This way
	 * first output cepstral coefficients originate from streamed data and not
	 * from buffers with zero data.
	 */
	comp_dbg(dev, "avail = %d", buf->s_avail);
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

	/* Check if enough samples in buffer for one FFT hop */
	m = buf->s_avail / fft->fft_hop_size;
	if (m > 0) {
		/* Clear FFT input buffer because it has been used as scratch */
		bzero(fft->fft_buf, fft->fft_buffer_size);

		/* Copy data to FFT input buffer from overlap buffer and from new samples buffer */
		mfcc_fill_fft_buffer(state);

		/* TODO: remove_dc_offset */

		/* TODO: use_energy & raw_energy */

		input_shift = 0;

		/* Window function */
		mfcc_apply_window(state, input_shift);

		/* TODO: use_energy & !raw_energy */

		/* The FFT out buffer needs to be cleared to avoid to corrupt
		 * the output. TODO: check moving it to FFT lib.
		 */
		bzero(fft->fft_out, fft->fft_buffer_size);

		/* Compute FFT */
		fft_execute_32(fft->fft_plan, false);

		/* Initialize 16-bit Mel log spectrum buffer in Q9.7. The Mel values
		 * are converted from Q9.23 to Q9.7 for DCT matrix multiplication.
		 */
		mat_init_16b(state->mel_spectra, 1, state->dct.num_in, 7); /* Q9.7 */

		/* Compensate FFT lib scaling to Mel log values, e.g. for 512 long FFT
		 * the fft_plan->len is 9. The scaling is 1/512. Subtract from input_shift it
		 * to add the missing "gain".
		 */
		mel_scale_shift = input_shift - fft->fft_plan->len;
		psy_apply_mel_filterbank_32(&state->melfb, fft->fft_out, state->power_spectra,
					    state->mel_log_32, mel_scale_shift);

		if (state->mel_only) {
			/* In Mel-only mode output Mel log spectra directly */
			cc_count += state->dct.num_in;

			/* Find peak mel value and track state->mmax in Q9.23 */
			if (config->dynamic_mmax) {
				peak = state->mel_log_32[0];
				for (j = 1; j < state->dct.num_in; j++) {
					if (state->mel_log_32[j] > peak)
						peak = state->mel_log_32[j];
				}

				/* Jump to peak immediately if higher, decay otherwise */
				if (peak > state->mmax) {
					state->mmax = peak;
				} else {
					/* Q9.23 * Q1.15, result Q9.23. The coefficient is small
					 * so no need for saturation.
					 */
					s = (int64_t)peak - state->mmax;
					state->mmax +=
						Q_MULTSR_32X32(s, config->mmax_coef, 23, 15, 23);
				}
			}

			/* Clamp Mel values lower than mmax - top_db, add offset, and scale.
			 * Config top_db and mel_offset are Q9.7, shift to Q9.23.
			 */
			clamp_value = state->mmax - ((int32_t)config->top_db << 16);
			for (j = 0; j < state->dct.num_in; j++) {
				mel_value = state->mel_log_32[j];
				if (mel_value < clamp_value)
					mel_value = clamp_value;

				/* Q9.23 * Q4.12, result Q9.23 */
				s = (int64_t)mel_value + ((int32_t)config->mel_offset << 16);
				state->mel_log_32[j] =
					sat_int32(Q_MULTSR_32X32(s, config->mel_scale, 23, 12, 23));
			}

			/* Enable this to check mmax decay */
			comp_dbg(dev, "state->mmax = %d", state->mmax);
		} else {
			/* Convert Q9.23 to Q9.7 for 16-bit DCT */
			for (j = 0; j < state->dct.num_in; j++)
				state->mel_spectra->data[j] =
					sat_int16(state->mel_log_32[j] >> 16);

			/* Multiply Mel spectra with DCT matrix to get cepstral coefficients */
			mat_init_16b(state->cepstral_coef, 1, state->dct.num_out, 7); /* Q9.7 */
			mat_multiply(state->mel_spectra, state->dct.matrix, state->cepstral_coef);

			/* Apply cepstral lifter */
			if (state->lifter.cepstral_lifter != 0) {
				mat_multiply_elementwise(state->cepstral_coef, state->lifter.matrix,
							 state->cepstral_coef);
			}

			cc_count += state->dct.num_out;
		}

		/* Use hop counter for frame numbering (independent of VAD enable) */
		state->header.frame_number = state->hop_count;

		/* Run VAD on the mel log spectrum (available in both modes) */
		if (config->enable_vad) {
			mfcc_vad_update(&cd->vad, state->mel_log_32);

			/* Populate data header for this output frame */
			state->header.energy = cd->vad.energy;
			state->header.noise_energy = cd->vad.noise_energy;
			state->header.vad_flag = cd->vad.is_speech ? 1 : 0;
		}

		/* Increment hop counter at end of hop processing */
		state->hop_count++;

		/* Send notification when VAD state changes */
		if (config->enable_vad && config->update_controls) {
			vad_now = cd->vad.is_speech;

			if (vad_now != cd->vad_prev) {
				mfcc_send_vad_notification(mod, vad_now ? 1 : 0);
				cd->vad_prev = vad_now;
			}
		}
	}

	return cc_count;
}

/**
 * \brief Write bytes into a possibly wrapped sink buffer.
 *
 * Caller must ensure max_bytes <= buf_size; otherwise the wrap branch
 * would write past the end of the buffer. Returns 0 on bound violation.
 */
static size_t mfcc_sink_write_bytes(uint8_t **dst, uint8_t *buf_start,
				    size_t buf_size, const uint8_t *src,
				    size_t max_bytes)
{
	uint8_t *buf_end = buf_start + buf_size;
	size_t chunk;

	if (max_bytes == 0)
		return 0;

	/* Guard: a single write must not exceed total sink buffer size. */
	if (max_bytes > buf_size)
		return 0;

	chunk = MIN(max_bytes, (size_t)(buf_end - *dst));
	memcpy(*dst, src, chunk);
	if (chunk < max_bytes) {
		memcpy(buf_start, src + chunk, max_bytes - chunk);
		*dst = buf_start + (max_bytes - chunk);
	} else {
		*dst += chunk;
		if (*dst >= buf_end)
			*dst = buf_start;
	}

	return max_bytes;
}

/**
 * \brief Prepare the next MFCC output frame after STFT processing.
 *
 * Copies Mel (int32 Q9.23) or cepstral (int16 Q9.7 widened to int32 Q9.23)
 * output into \c state->out_stage, sets up the read pointer for staged output,
 * and marks the data header as pending.
 *
 * \param[in,out] state MFCC state.
 * \param[in] num_ceps Number of int32_t values to publish; no-op if <= 0.
 */
static void mfcc_prepare_output(struct mfcc_state *state, int num_ceps)
{
	int k;

	if (num_ceps <= 0)
		return;

	/* Copy into out_stage so the next STFT hop may freely reuse
	 * mel_log_32 / cepstral_coef while this frame is still pending.
	 */
	if (state->mel_only) {
		for (k = 0; k < num_ceps; k++)
			state->out_stage[k] = state->mel_log_32[k];
	} else {
		/* Widen int16 Q9.7 cepstral coefficients to int32 Q9.23. */
		for (k = 0; k < num_ceps; k++)
			state->out_stage[k] = (int32_t)state->cepstral_coef->data[k] << 16;
	}

	state->out_data_ptr = state->out_stage;
	state->out_remain = num_ceps;
	state->header_pending = true;
}

/**
 * \brief Commit MFCC output in compress mode.
 *
 * Writes the pending data header (if any) followed by the pending
 * int32_t Mel/cepstral payload into the sink as a contiguous blob,
 * without zero padding. When VAD + DTX are enabled, suppresses silence
 * frames after a configurable trailing-silence count and optionally
 * sends periodic keep-alive silence updates.
 *
 * \param[in,out] mod Processing module.
 * \param[in,out] cd MFCC component data.
 * \param[in,out] sinks Sink array; only sinks[0] is used.
 * \param[in] num_ceps Number of int32_t values prepared for this hop.
 *
 * \return 0 on success, -ENOSPC if the sink cannot accept the frame,
 *         or a negative error from the sink API.
 */
static int mfcc_output_compress(struct processing_module *mod, struct mfcc_comp_data *cd,
				struct sof_sink **sinks, int num_ceps)
{
	struct mfcc_state *state = &cd->state;
	size_t out_bytes;
	size_t commit_bytes;
	void *sink_ptr;
	void *sink_start;
	size_t sink_buf_size;
	uint8_t *dst;
	int ret;
	bool pending;
	bool send_periodic;

	pending = state->header_pending || state->out_remain > 0;
	if (!pending)
		return 0;

	if (num_ceps > 0 && cd->config->enable_vad && !cd->vad.is_speech) {
		state->vad_silence_count++;
		/* With DTX enabled, send trailing silence frames
		 * (configurable count) then suppress. After trailing
		 * frames, optionally send periodic silence updates
		 * at the configured interval. This gives the host
		 * enough silence to detect end-of-speech while
		 * keeping alive updates during long silence.
		 * Without DTX, output every frame regardless of VAD.
		 */
		if (cd->config->enable_dtx &&
		    state->vad_silence_count > state->dtx_trailing_silence) {
			send_periodic = false;

			/* Check periodic silence frame send */
			if (state->dtx_silence_interval > 0) {
				state->dtx_silence_counter++;
				if (state->dtx_silence_counter >=
				    state->dtx_silence_interval) {
					state->dtx_silence_counter = 0;
					send_periodic = true;
				}
			}

			if (!send_periodic) {
				/* Suppress this frame */
				state->header_pending = false;
				state->out_remain = 0;
				return 0;
			}
		}
	} else {
		state->vad_silence_count = 0;
		state->dtx_silence_counter = 0;
	}

	out_bytes = (state->header_pending ? sizeof(state->header) : 0) +
		    state->out_remain * sizeof(int32_t);
	if (out_bytes == 0)
		return 0;

	commit_bytes = out_bytes;
	if (sink_get_free_size(sinks[0]) < commit_bytes)
		return -ENOSPC;

	ret = sink_get_buffer(sinks[0], commit_bytes, &sink_ptr,
			      &sink_start, &sink_buf_size);
	if (ret)
		return ret;

	dst = sink_ptr;
	if (state->header_pending) {
		mfcc_sink_write_bytes(&dst, sink_start, sink_buf_size,
				      (uint8_t *)&state->header,
				      sizeof(state->header));
	}

	if (state->out_remain > 0) {
		mfcc_sink_write_bytes(&dst, sink_start, sink_buf_size,
				      (uint8_t *)state->out_data_ptr,
				      state->out_remain * sizeof(int32_t));
	}

	ret = sink_commit_buffer(sinks[0], commit_bytes);
	if (ret)
		return ret;

	state->header_pending = false;
	state->out_remain = 0;
	return 0;
}

/**
 * \brief Commit MFCC output in legacy PCM mode.
 *
 * Acquires a sink period sized as \p frames audio frames, zero-fills it,
 * then writes any pending data header and pending int32_t Mel/cepstral
 * payload. Any payload that does not fit in the current period stays in
 * \c state->out_data_ptr / \c state->out_remain for the next call.
 *
 * \param[in,out] mod Processing module.
 * \param[in,out] cd MFCC component data.
 * \param[in,out] sources Sources array (unused; reserved for symmetry).
 * \param[in,out] sinks Sink array; only sinks[0] is used.
 * \param[in] frames Number of audio frames the sink expects this period.
 *
 * \return 0 on success, -ENOSPC if the sink cannot accept the period,
 *         or a negative error from the sink API.
 */
static int mfcc_output_legacy(struct processing_module *mod, struct mfcc_comp_data *cd,
			      struct sof_source **sources, struct sof_sink **sinks,
			      int frames)
{
	struct mfcc_state *state = &cd->state;
	size_t commit_bytes;
	size_t bytes_to_end;
	size_t avail;
	size_t hdr_size;
	size_t data_bytes;
	size_t to_write;
	void *sink_ptr;
	void *sink_start;
	size_t sink_buf_size;
	uint8_t *dst;
	int n32;
	int ret;

	/* The MFCC sink is treated as an opaque byte container: the period
	 * carries an MFCC blob (header + int32 features), not PCM audio.
	 * Sizing the commit as sink_frame_bytes * frames keeps the period
	 * size matched to whatever the sink advertises (S16_LE / S24_4LE /
	 * S32_LE), so no format-specific conversion is needed. Any payload
	 * that does not fit in the current period is carried over via
	 * state->out_remain and drained in the next call(s). The host side
	 * decodes the bytes as MFCC features regardless of the sink's
	 * nominal PCM format, so non-S32 sinks (e.g. bench S16 topologies)
	 * remain supported.
	 */
	commit_bytes = sink_get_frame_bytes(sinks[0]) * frames;
	if (sink_get_free_size(sinks[0]) < commit_bytes)
		return -ENOSPC;

	ret = sink_get_buffer(sinks[0], commit_bytes, &sink_ptr,
			      &sink_start, &sink_buf_size);
	if (ret)
		return ret;

	/* Zero-fill entire period first */
	bytes_to_end = (size_t)((uint8_t *)sink_start + sink_buf_size -
				(uint8_t *)sink_ptr);

	if (bytes_to_end >= commit_bytes) {
		memset(sink_ptr, 0, commit_bytes);
	} else {
		memset(sink_ptr, 0, bytes_to_end);
		memset(sink_start, 0, commit_bytes - bytes_to_end);
	}

	dst = sink_ptr;
	avail = commit_bytes;

	/* Write pending header */
	if (state->header_pending && avail > 0) {
		hdr_size = sizeof(state->header);

		if (avail >= hdr_size) {
			mfcc_sink_write_bytes(&dst, sink_start, sink_buf_size,
					      (uint8_t *)&state->header, hdr_size);
			avail -= hdr_size;
			state->header_pending = false;
		}
	}

	/* Write pending feature data (always int32) */
	if (state->out_remain > 0 && avail > 0) {
		data_bytes = state->out_remain * sizeof(int32_t);
		to_write = MIN(data_bytes, avail) & ~(size_t)3;
		if (to_write > 0) {
			mfcc_sink_write_bytes(&dst, sink_start, sink_buf_size,
					      (uint8_t *)state->out_data_ptr,
					      to_write);
			n32 = to_write / sizeof(int32_t);
			state->out_data_ptr += n32;
			state->out_remain -= n32;
		}
	}

	return sink_commit_buffer(sinks[0], commit_bytes);
}

int mfcc_process_output(struct processing_module *mod, struct mfcc_comp_data *cd,
			struct sof_source **sources, struct sof_sink **sinks,
			int num_ceps, int frames)
{
	bool pending;

	pending = cd->state.header_pending || cd->state.out_remain > 0;

	if (cd->config->compress_output) {
		/* Keep retrying pending frame first; don't overwrite pending state. */
		if (!pending && num_ceps > 0)
			mfcc_prepare_output(&cd->state, num_ceps);
		else if (pending)
			num_ceps = 0;

		return mfcc_output_compress(mod, cd, sinks, num_ceps);
	}

	/* Legacy PCM mode: same guard. Overwriting out_data_ptr / out_remain
	 * while a previous period's data is still pending would drop samples.
	 */
	if (!pending && num_ceps > 0)
		mfcc_prepare_output(&cd->state, num_ceps);

	return mfcc_output_legacy(mod, cd, sources, sinks, frames);
}

/**
 * \brief Refill the FFT input scratch from the input circular buffer.
 *
 * Copies the saved overlap (\c state->prev_data) into the first samples
 * of \c fft->fft_buf, appends one \c fft_hop_size of new data from the
 * input circular buffer (consuming it), and finally saves the trailing
 * window of the FFT input back to \c state->prev_data for the next hop.
 * The caller is expected to have zeroed \c fft->fft_buf so that the
 * complex imaginary parts remain 0.
 *
 * \param[in,out] state MFCC state. Input buffer pointers/counters and
 *                      \c prev_data are updated; \c fft->fft_buf is filled.
 */
void mfcc_fill_fft_buffer(struct mfcc_state *state)
{
	struct mfcc_buffer *buf = &state->buf;
	struct mfcc_fft *fft = &state->fft;
	int32_t *d = &fft->fft_buf[fft->fft_fill_start_idx].real;
	const int fft_elem_inc = sizeof(fft->fft_buf[0]) / sizeof(int32_t);
	int16_t *prev = state->prev_data;
	int16_t *prev_end = prev + state->prev_data_size;
	int16_t *r = buf->r_ptr;
	int copied;
	int nmax;
	int n;
	int j;

	/* Copy overlapped samples from state buffer. The fft_buf has been
	 * cleared by caller so imaginary part remains zero.
	 */
	while (prev < prev_end) {
		*d = *prev++;
		d += fft_elem_inc;
	}

	/* Copy hop size of new data from circular buffer */
	for (copied = 0; copied < fft->fft_hop_size; copied += n) {
		nmax = fft->fft_hop_size - copied;
		n = mfcc_buffer_samples_without_wrap(buf, r);
		n = MIN(n, nmax);
		for (j = 0; j < n; j++) {
			*d = *r++;
			d += fft_elem_inc;
		}
		r = mfcc_buffer_wrap(buf, r);
	}

	buf->s_avail -= copied;
	buf->s_free += copied;
	buf->r_ptr = r;

	/* Copy for next time data back to overlap buffer */
	d = (int32_t *)&fft->fft_buf[fft->fft_fill_start_idx + fft->fft_hop_size].real;
	prev = state->prev_data;
	while (prev < prev_end) {
		*prev++ = *d;
		d += fft_elem_inc;
	}
}
