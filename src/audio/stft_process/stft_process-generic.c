// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2025 Intel Corporation.

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/component.h>
#include <sof/audio/format.h>
#include <sof/audio/sink_api.h>
#include <sof/audio/sink_source_utils.h>
#include <sof/audio/source_api.h>
#include <stdint.h>
#include "stft_process.h"

#if CONFIG_FORMAT_S32LE
/**
 * stft_process_source_s32() - Process S16_LE format.
 * @mod: Pointer to module data.
 * @source: Source for PCM samples data.
 * @sink: Sink for PCM samples data.
 * @frames: Number of audio data frames to process.
 *
 * This is the processing function for 16-bit signed integer PCM formats. The
 * audio samples in every frame are re-order to channels order defined in
 * component data channel_map[].
 *
 * Return: Value zero for success, otherwise an error code.
 */
int stft_process_source_s32(struct stft_comp_data *cd, struct sof_source *source, int frames)
{
	struct stft_process_state *state = &cd->state;
	struct stft_process_buffer *ibuf = &state->ibuf;
	int32_t *w = ibuf->w_ptr;
	int32_t const *x, *x_start, *x_end;
	int32_t in;
	int x_size;
	int bytes = frames * cd->frame_bytes;
	int frames_left = frames;
	int ret;
	int n1;
	int n2;
	int n;
	int i;

	/* Get pointer to source data in circular buffer */
	ret = source_get_data_s32(source, bytes, &x, &x_start, &x_size);
	if (ret)
		return ret;

	/* Set helper pointers to buffer end for wrap check. Then loop until all
	 * samples are processed.
	 */
	x_end = x_start + x_size;

	while (frames_left) {
		/* Find out samples to process before first wrap or end of data. */
		n1 = (x_end - x) / cd->channels;
		n2 = stft_process_buffer_samples_without_wrap(ibuf, w);
		n = MIN(n1, n2);
		n = MIN(n, frames_left);
		for (i = 0; i < n; i++) {
			in = *(x + cd->source_channel);
			*w++ = in;
			x += cd->channels;
		}

		/* One of the buffers needs a wrap (or end of data), so check for wrap */
		w = stft_process_buffer_wrap(ibuf, w);
		if (x >= x_end)
			x -= x_size;

		/* Update processed samples count for next loop iteration. */
		frames_left -= n;
	}

	/* Update the source for bytes consumed. Return success. */
	source_release_data(source, bytes);
	ibuf->s_avail += frames;
	ibuf->s_free -= frames;
	ibuf->w_ptr = w;
	return 0;
}

/**
 * stft_process_sink_s32() - Process S16_LE format.
 * @mod: Pointer to module data.
 * @source: Source for PCM samples data.
 * @sink: Sink for PCM samples data.
 * @frames: Number of audio data frames to process.
 *
 * This is the processing function for 16-bit signed integer PCM formats. The
 * audio samples in every frame are re-order to channels order defined in
 * component data channel_map[].
 *
 * Return: Value zero for success, otherwise an error code.
 */
int stft_process_sink_s32(struct stft_comp_data *cd, struct sof_sink *sink, int frames)
{
	struct stft_process_state *state = &cd->state;
	struct stft_process_buffer *obuf = &state->obuf;
	int32_t *r = obuf->r_ptr;
	int32_t *y, *y_start, *y_end;
	int frames_remain = frames;
	int samples = frames * cd->channels;
	int bytes = frames * cd->frame_bytes;
	int y_size;
	int ret;
	int ch, n1, n, i;

	/* Get pointer to sink data in circular buffer */
	ret = sink_get_buffer_s32(sink, bytes, &y, &y_start, &y_size);
	if (ret)
		return ret;

	/* Set helper pointers to buffer end for wrap check. Then loop until all
	 * samples are processed.
	 */
	y_end = y_start + y_size;
	while (frames_remain) {
		/* Find out samples to process before first wrap or end of data. */
		n1 = (y_end - y) / cd->channels;
		n = stft_process_buffer_samples_without_wrap(obuf, r);
		n = MIN(n1, n);
		n = MIN(n, frames_remain);

		for (i = 0; i < n; i++) {
			for (ch = 0; ch < cd->channels; ch++) {
				*y = *r;
				y++;
			}
			*r = 0; /* clear overlap add mix */
			r++;
		}

		/* One of the buffers needs a wrap (or end of data), so check for wrap */
		r = stft_process_buffer_wrap(obuf, r);
		if (y >= y_end)
			y -= y_size;

		/* Update processed samples count for next loop iteration. */
		frames_remain -= n;
	}

	/* Update the sink for bytes produced. Return success. */
	sink_commit_buffer(sink, bytes);
	obuf->r_ptr = r;
	obuf->s_avail -= samples;
	obuf->s_free += samples;

	return 0;
}
#endif /* CONFIG_FORMAT_S32LE */

#if CONFIG_FORMAT_S16LE
/**
 * stft_process_source_s16() - Process S16_LE format.
 * @mod: Pointer to module data.
 * @source: Source for PCM samples data.
 * @sink: Sink for PCM samples data.
 * @frames: Number of audio data frames to process.
 *
 * This is the processing function for 16-bit signed integer PCM formats. The
 * audio samples in every frame are re-order to channels order defined in
 * component data channel_map[].
 *
 * Return: Value zero for success, otherwise an error code.
 */
int stft_process_source_s16(struct stft_comp_data *cd, struct sof_source *source, int frames)
{
	struct stft_process_state *state = &cd->state;
	struct stft_process_buffer *ibuf = &state->ibuf;
	int32_t *w = ibuf->w_ptr;
	int16_t const *x, *x_start, *x_end;
	int16_t in;
	int x_size;
	int bytes = frames * cd->frame_bytes;
	int frames_left = frames;
	int ret;
	int n1;
	int n2;
	int n;
	int i;

	/* Get pointer to source data in circular buffer, get buffer start and size to
	 * check for wrap. The size in bytes is converted to number of s16 samples to
	 * control the samples process loop. If the number of bytes requested is not
	 * possible, an error is returned.
	 */
	ret = source_get_data_s16(source, bytes, &x, &x_start, &x_size);
	if (ret)
		return ret;

	/* Set helper pointers to buffer end for wrap check. Then loop until all
	 * samples are processed.
	 */
	x_end = x_start + x_size;

	while (frames_left) {
		/* Find out samples to process before first wrap or end of data. */
		n1 = (x_end - x) / cd->channels;
		n2 = stft_process_buffer_samples_without_wrap(ibuf, w);
		n = MIN(n1, n2);
		n = MIN(n, frames_left);
		for (i = 0; i < n; i++) {
			in = *(x + cd->source_channel);
			*w = (int32_t)in << 16;
			x += cd->channels;
			w++;
		}

		/* One of the buffers needs a wrap (or end of data), so check for wrap */
		w = stft_process_buffer_wrap(ibuf, w);
		if (x >= x_end)
			x -= x_size;

		/* Update processed samples count for next loop iteration. */
		frames_left -= n;
	}

	/* Update the source for bytes consumed. Return success. */
	source_release_data(source, bytes);
	ibuf->s_avail += frames;
	ibuf->s_free -= frames;
	ibuf->w_ptr = w;
	return 0;
}

/**
 * stft_process_sink_s16() - Process S16_LE format.
 * @mod: Pointer to module data.
 * @source: Source for PCM samples data.
 * @sink: Sink for PCM samples data.
 * @frames: Number of audio data frames to process.
 *
 * This is the processing function for 16-bit signed integer PCM formats. The
 * audio samples in every frame are re-order to channels order defined in
 * component data channel_map[].
 *
 * Return: Value zero for success, otherwise an error code.
 */
int stft_process_sink_s16(struct stft_comp_data *cd, struct sof_sink *sink, int frames)
{
	struct stft_process_state *state = &cd->state;
	struct stft_process_buffer *obuf = &state->obuf;
	int32_t *r = obuf->r_ptr;
	int16_t *y, *y_start, *y_end;
	int frames_remain = frames;
	int samples = frames * cd->channels;
	int bytes = frames * cd->frame_bytes;
	int y_size;
	int ret;
	int ch, n1, n, i;

	/* Get pointer to sink data in circular buffer */
	ret = sink_get_buffer_s16(sink, bytes, &y, &y_start, &y_size);
	if (ret)
		return ret;

	/* Set helper pointers to buffer end for wrap check. Then loop until all
	 * samples are processed.
	 */
	y_end = y_start + y_size;
	while (frames_remain) {
		/* Find out samples to process before first wrap or end of data. */
		n1 = (y_end - y) / cd->channels;
		n = stft_process_buffer_samples_without_wrap(obuf, r);
		n = MIN(n1, n);
		n = MIN(n, frames_remain);

		for (i = 0; i < n; i++) {
			for (ch = 0; ch < cd->channels; ch++) {
				*y = sat_int16(Q_SHIFT_RND(*r, 31, 15));
				y++;
			}
			*r = 0; /* clear overlap add mix */
			r++;
		}

		/* One of the buffers needs a wrap (or end of data), so check for wrap */
		r = stft_process_buffer_wrap(obuf, r);
		if (y >= y_end)
			y -= y_size;

		/* Update processed samples count for next loop iteration. */
		frames_remain -= n;
	}

	/* Update the sink for bytes produced. Return success. */
	sink_commit_buffer(sink, bytes);
	obuf->r_ptr = r;
	obuf->s_avail -= samples;
	obuf->s_free += samples;

	return 0;
}
#endif /* CONFIG_FORMAT_S16LE */

void stft_process_fill_prev_samples(struct stft_process_buffer *buf, int32_t *prev_data,
				    int prev_data_length)
{
	/* Fill prev_data from input buffer */
	int32_t *r = buf->r_ptr;
	int32_t *p = prev_data;
	int copied;
	int nmax;
	int n;

	for (copied = 0; copied < prev_data_length; copied += n) {
		nmax = prev_data_length - copied;
		n = stft_process_buffer_samples_without_wrap(buf, r);
		n = MIN(n, nmax);
		memcpy(p, r, sizeof(int32_t) * n); /* Not using memcpy_s() due to speed need */
		p += n;
		r += n;
		r = stft_process_buffer_wrap(buf, r);
	}

	buf->s_avail -= copied;
	buf->s_free += copied;
	buf->r_ptr = r;
}

void stft_process_fill_fft_buffer(struct stft_process_state *state)
{
	struct stft_process_buffer *ibuf = &state->ibuf;
	struct stft_process_fft *fft = &state->fft;
	int32_t *r = ibuf->r_ptr;
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
		n = stft_process_buffer_samples_without_wrap(ibuf, r);
		n = MIN(n, nmax);
		for (j = 0; j < n; j++) {
			fft->fft_buf[idx].real = *r;
			r++;
			idx++;
		}
		r = stft_process_buffer_wrap(ibuf, r);
	}

	ibuf->s_avail -= copied;
	ibuf->s_free += copied;
	ibuf->r_ptr = r;

	/* Copy for next time data back to overlap buffer */
	idx = fft->fft_fill_start_idx + fft->fft_hop_size;
	for (j = 0; j < state->prev_data_size; j++)
		state->prev_data[j] = fft->fft_buf[idx + j].real;
}

void stft_process_overlap_add_ifft_buffer(struct stft_process_state *state)
{
	struct stft_process_buffer *obuf = &state->obuf;
	struct stft_process_fft *fft = &state->fft;
	int32_t *w = obuf->w_ptr;
	int32_t sample;
	int i;
	int n;
	int samples_remain = fft->fft_size;
	int idx = fft->fft_fill_start_idx;

	while (samples_remain) {
		n = stft_process_buffer_samples_without_wrap(obuf, w);
		n = MIN(samples_remain, n);
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
	int i = fft->fft_fill_start_idx;

	/* Multiply Q1.31 by Q1.15 gives Q2.46, shift right by 15 to get Q2.31, no saturate need */
	for (j = 0; j < fft->fft_size; j++)
		fft->fft_buf[i + j].real =
			sat_int32(Q_MULTSR_32X32((int64_t)fft->fft_buf[i + j].real,
						 state->window[j], 31, 31, 31));
}
