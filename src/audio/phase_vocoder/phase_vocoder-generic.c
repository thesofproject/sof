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
#include "phase_vocoder.h"

#if CONFIG_FORMAT_S32LE
/**
 * phase_vocoder_source_s32() - Process S16_LE format.
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
int phase_vocoder_source_s32(struct phase_vocoder_comp_data *cd, struct sof_source *source,
			     int frames)
{
	struct phase_vocoder_state *state = &cd->state;
	struct phase_vocoder_buffer *ibuf;
	int32_t const *x, *x_start, *x_end;
	int frames_left;
	int x_size;
	int bytes;
	int ret;
	int n1;
	int n2;
	int n;
	int i;
	int j;
	int channels = cd->channels;

	ibuf = &state->ibuf[0];
	frames = MIN(frames, ibuf->s_free);
	bytes = frames * cd->frame_bytes;

	/* Get pointer to source data in circular buffer */
	ret = source_get_data_s32(source, bytes, &x, &x_start, &x_size);
	if (ret)
		return ret;

	/* Set helper pointers to buffer end for wrap check. Then loop until all
	 * samples are processed.
	 */
	x_end = x_start + x_size;

	frames_left = frames;
	while (frames_left) {
		/* Find out samples to process before first wrap or end of data. */
		ibuf = &state->ibuf[0];
		n1 = (x_end - x) / channels;
		n2 = phase_vocoder_buffer_samples_without_wrap(ibuf, ibuf->w_ptr);
		n = MIN(n1, n2);
		n = MIN(n, frames_left);
		for (i = 0; i < n; i++) {
			for (j = 0; j < channels; j++) {
				ibuf = &state->ibuf[j];
				*ibuf->w_ptr++ = *x++;
			}
		}

		/* One of the buffers needs a wrap (or end of data), so check for wrap */
		for (j = 0; j < channels; j++) {
			ibuf = &state->ibuf[j];
			ibuf->w_ptr = phase_vocoder_buffer_wrap(ibuf, ibuf->w_ptr);
		}

		if (x >= x_end)
			x -= x_size;

		/* Update processed samples count for next loop iteration. */
		frames_left -= n;
	}

	/* Update the source for bytes consumed. Return success. */
	source_release_data(source, bytes);
	for (j = 0; j < channels; j++) {
		ibuf = &state->ibuf[j];
		ibuf->s_avail += frames;
		ibuf->s_free -= frames;
	}

	return 0;
}

/**
 * phase_vocoder_sink_s32() - Process S16_LE format.
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
int phase_vocoder_sink_s32(struct phase_vocoder_comp_data *cd, struct sof_sink *sink, int frames)
{
	struct phase_vocoder_state *state = &cd->state;
	struct phase_vocoder_buffer *obuf;
	int32_t *y, *y_start, *y_end;
	int frames_remain;
	int channels = cd->channels;
	int bytes;
	int y_size;
	int ret;
	int ch, n1, n, i;

	obuf = &state->obuf[0];
	frames = MIN(frames, obuf->s_avail);
	if (!frames)
		return 0;

	/* Get pointer to sink data in circular buffer */
	bytes = frames * cd->frame_bytes;
	ret = sink_get_buffer_s32(sink, bytes, &y, &y_start, &y_size);
	if (ret)
		return ret;

	/* Set helper pointers to buffer end for wrap check. Then loop until all
	 * samples are processed.
	 */
	y_end = y_start + y_size;

	frames_remain = frames;
	while (frames_remain) {
		/* Find out samples to process before first wrap or end of data. */
		obuf = &state->obuf[0];
		n1 = (y_end - y) / cd->channels;
		n = phase_vocoder_buffer_samples_without_wrap(obuf, obuf->r_ptr);
		n = MIN(n1, n);
		n = MIN(n, frames_remain);

		for (i = 0; i < n; i++) {
			for (ch = 0; ch < channels; ch++) {
				obuf = &state->obuf[ch];
				*y++ = *obuf->r_ptr;
				*obuf->r_ptr++ = 0; /* clear overlap add mix */
			}
		}

		/* One of the buffers needs a wrap (or end of data), so check for wrap */
		for (ch = 0; ch < channels; ch++) {
			obuf = &state->obuf[ch];
			obuf->r_ptr = phase_vocoder_buffer_wrap(obuf, obuf->r_ptr);
		}

		if (y >= y_end)
			y -= y_size;

		/* Update processed samples count for next loop iteration. */
		frames_remain -= n;
	}

	/* Update the sink for bytes produced. Return success. */
	sink_commit_buffer(sink, bytes);
	for (ch = 0; ch < channels; ch++) {
		obuf = &state->obuf[ch];
		obuf->s_avail -= frames;
		obuf->s_free += frames;
	}

	return 0;
}
#endif /* CONFIG_FORMAT_S32LE */

#if CONFIG_FORMAT_S16LE
/**
 * phase_vocoder_source_s16() - Process S16_LE format.
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
int phase_vocoder_source_s16(struct phase_vocoder_comp_data *cd, struct sof_source *source,
			     int frames)
{
	struct phase_vocoder_state *state = &cd->state;
	struct phase_vocoder_buffer *ibuf;
	int16_t const *x, *x_start, *x_end;
	int16_t in;
	int x_size;
	int channels = cd->channels;
	int bytes;

	ibuf = &state->ibuf[0];
	frames = MIN(frames, ibuf->s_free);
	bytes = frames * cd->frame_bytes;
	int frames_left = frames;
	int ret;
	int n1;
	int n2;
	int n;
	int i;
	int j;

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
		ibuf = &state->ibuf[0];
		n1 = (x_end - x) / cd->channels;
		n2 = phase_vocoder_buffer_samples_without_wrap(ibuf, ibuf->w_ptr);
		n = MIN(n1, n2);
		n = MIN(n, frames_left);
		for (i = 0; i < n; i++) {
			for (j = 0; j < channels; j++) {
				ibuf = &state->ibuf[j];
				in = *x++;
				*ibuf->w_ptr++ = (int32_t)in << 16;
			}
		}

		/* One of the buffers needs a wrap (or end of data), so check for wrap */
		for (j = 0; j < channels; j++) {
			ibuf = &state->ibuf[j];
			ibuf->w_ptr = phase_vocoder_buffer_wrap(ibuf, ibuf->w_ptr);
		}

		if (x >= x_end)
			x -= x_size;

		/* Update processed samples count for next loop iteration. */
		frames_left -= n;
	}

	/* Update the source for bytes consumed. Return success. */
	source_release_data(source, bytes);
	for (j = 0; j < channels; j++) {
		ibuf = &state->ibuf[j];
		ibuf->s_avail += frames;
		ibuf->s_free -= frames;
	}
	return 0;
}

/**
 * phase_vocoder_sink_s16() - Process S16_LE format.
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
int phase_vocoder_sink_s16(struct phase_vocoder_comp_data *cd, struct sof_sink *sink, int frames)
{
	struct phase_vocoder_state *state = &cd->state;
	struct phase_vocoder_buffer *obuf;
	int16_t *y, *y_start, *y_end;
	int frames_remain;
	int channels = cd->channels;
	int y_size;
	int bytes;
	int ret;
	int ch, n1, n, i;

	obuf = &state->obuf[0];
	frames = MIN(frames, obuf->s_avail);
	if (!frames)
		return 0;

	/* Get pointer to sink data in circular buffer */
	bytes = frames * cd->frame_bytes;
	frames_remain = frames;
	ret = sink_get_buffer_s16(sink, bytes, &y, &y_start, &y_size);
	if (ret)
		return ret;

	/* Set helper pointers to buffer end for wrap check. Then loop until all
	 * samples are processed.
	 */
	y_end = y_start + y_size;
	while (frames_remain) {
		/* Find out samples to process before first wrap or end of data. */
		obuf = &state->obuf[0];
		n1 = (y_end - y) / cd->channels;
		n = phase_vocoder_buffer_samples_without_wrap(obuf, obuf->r_ptr);
		n = MIN(n1, n);
		n = MIN(n, frames_remain);

		for (i = 0; i < n; i++) {
			for (ch = 0; ch < channels; ch++) {
				obuf = &state->obuf[ch];
				*y++ = sat_int16(Q_SHIFT_RND(*obuf->r_ptr, 31, 15));
				*obuf->r_ptr++ = 0; /* clear overlap add mix */
			}
		}

		/* One of the buffers needs a wrap (or end of data), so check for wrap */
		for (ch = 0; ch < channels; ch++) {
			obuf = &state->obuf[ch];
			obuf->r_ptr = phase_vocoder_buffer_wrap(obuf, obuf->r_ptr);
		}

		if (y >= y_end)
			y -= y_size;

		/* Update processed samples count for next loop iteration. */
		frames_remain -= n;
	}

	/* Update the sink for bytes produced. Return success. */
	sink_commit_buffer(sink, bytes);
	for (ch = 0; ch < channels; ch++) {
		obuf = &state->obuf[ch];
		obuf->s_avail -= frames;
		obuf->s_free += frames;
	}

	return 0;
}
#endif /* CONFIG_FORMAT_S16LE */

void phase_vocoder_fill_fft_buffer(struct phase_vocoder_state *state, int ch)
{
	struct phase_vocoder_buffer *ibuf = &state->ibuf[ch];
	struct phase_vocoder_fft *fft = &state->fft;
	struct icomplex32 *fft_buf_ptr;
	int32_t *prev_data = state->prev_data[ch];
	int32_t *r = ibuf->r_ptr;
	const int prev_data_size = state->prev_data_size;
	const int fft_hop_size = fft->fft_hop_size;
	int samples_remain = fft_hop_size;
	int j;
	int n;

	/* Copy overlapped samples from state buffer. Imaginary part of input
	 * remains zero.
	 */
	fft_buf_ptr = fft->fft_buf;
	for (j = 0; j < prev_data_size; j++) {
		fft_buf_ptr->real = prev_data[j];
		fft_buf_ptr->imag = 0;
		fft_buf_ptr++;
	}

	/* Copy hop size of new data from circular buffer */
	fft_buf_ptr = &fft->fft_buf[prev_data_size];
	while (samples_remain) {
		n = phase_vocoder_buffer_samples_without_wrap(ibuf, r);
		n = MIN(n, samples_remain);
		for (j = 0; j < n; j++) {
			fft_buf_ptr->real = *r++;
			fft_buf_ptr->imag = 0;
			fft_buf_ptr++;
		}
		r = phase_vocoder_buffer_wrap(ibuf, r);
		samples_remain -= n;
	}

	ibuf->r_ptr = r;
	ibuf->s_avail -= fft_hop_size;
	ibuf->s_free += fft_hop_size;

	/* Copy for next time data back to input data overlap buffer */
	fft_buf_ptr = &fft->fft_buf[fft_hop_size];
	for (j = 0; j < prev_data_size; j++)
		*prev_data++ = fft_buf_ptr++->real;
}

int phase_vocoder_overlap_add_ifft_buffer(struct phase_vocoder_state *state, int ch)
{
	struct phase_vocoder_buffer *obuf = &state->obuf[ch];
	struct phase_vocoder_fft *fft = &state->fft;
	int32_t *w = obuf->w_ptr;
	int32_t sample;
	int i;
	int n;
	int samples_remain = fft->fft_size;
	int idx = 0;

	if (obuf->s_free < samples_remain)
		return -EINVAL;

	while (samples_remain) {
		n = phase_vocoder_buffer_samples_without_wrap(obuf, w);
		n = MIN(samples_remain, n);
		for (i = 0; i < n; i++) {
			sample = Q_MULTSR_32X32((int64_t)state->gain_comp, fft->fft_buf[idx].real,
						31, 31, 31);
			*w = sat_int32((int64_t)*w + sample);
			w++;
			idx++;
		}
		w = phase_vocoder_buffer_wrap(obuf, w);
		samples_remain -= n;
	}

	w = obuf->w_ptr + fft->fft_hop_size;
	obuf->w_ptr = phase_vocoder_buffer_wrap(obuf, w);
	obuf->s_avail += fft->fft_hop_size;
	obuf->s_free -= fft->fft_hop_size;
	return 0;
}

void phase_vocoder_apply_window(struct phase_vocoder_state *state)
{
	struct phase_vocoder_fft *fft = &state->fft;
	struct icomplex32 *fft_buf_ptr = fft->fft_buf;
	const int32_t *window = state->window;
	const int fft_size = fft->fft_size;
	int i;

	for (i = 0; i < fft_size; i++) {
		fft_buf_ptr->real =
		    sat_int32(Q_MULTSR_32X32((int64_t)fft_buf_ptr->real, window[i], 31, 31, 31));
		fft_buf_ptr++;
	}
}
