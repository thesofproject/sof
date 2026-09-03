// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2025 Intel Corporation.

#include <sof/audio/component.h>
#include <sof/audio/audio_stream.h>
#include <sof/audio/format.h>
#include <sof/audio/sink_api.h>
#include <sof/audio/sink_source_utils.h>
#include <sof/audio/source_api.h>
#include <sof/math/auditory.h>
#include <sof/math/icomplex32.h>
#include <sof/math/matrix.h>
#include <sof/math/sqrt.h>
#include <sof/math/trig.h>
#include <sof/math/window.h>
#include <sof/trace/trace.h>

#include "stft_process.h"

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

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

#if CONFIG_FORMAT_S32LE
int stft_process_source_s32(struct stft_comp_data *cd, struct sof_source *source, int frames)
{
	struct stft_process_state *state = &cd->state;
	struct stft_process_buffer *ibuf;
	int32_t const *x, *x_start, *x_end;
	int x_size;
	int bytes = frames * cd->frame_bytes;
	int frames_left = frames;
	int ret;
	int n1;
	int n2;
	int channels = cd->channels;
	int n;
	int i;
	int j;

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
		ibuf = &state->ibuf[0];
		n1 = (x_end - x) / cd->channels;
		n2 = stft_process_buffer_samples_without_wrap(ibuf, ibuf->w_ptr);
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
			ibuf->w_ptr = stft_process_buffer_wrap(ibuf, ibuf->w_ptr);
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

int stft_process_sink_s32(struct stft_comp_data *cd, struct sof_sink *sink, int frames)
{
	struct stft_process_state *state = &cd->state;
	struct stft_process_buffer *obuf;
	int32_t *y, *y_start, *y_end;
	int frames_remain = frames;
	int channels = cd->channels;
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
		obuf = &state->obuf[0];
		n1 = (y_end - y) / cd->channels;
		n = stft_process_buffer_samples_without_wrap(obuf, obuf->r_ptr);
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
		for (ch = 0; ch < cd->channels; ch++) {
			obuf = &state->obuf[ch];
			obuf->r_ptr = stft_process_buffer_wrap(obuf, obuf->r_ptr);
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
int stft_process_source_s16(struct stft_comp_data *cd, struct sof_source *source, int frames)
{
	struct stft_process_state *state = &cd->state;
	struct stft_process_buffer *ibuf;
	int16_t const *x, *x_start, *x_end;
	int16_t in;
	int x_size;
	int channels = cd->channels;
	int bytes = frames * cd->frame_bytes;
	int frames_left = frames;
	int ret;
	int n1;
	int n2;
	int n;
	int i;
	int j;

	ret = source_get_data_s16(source, bytes, &x, &x_start, &x_size);
	if (ret)
		return ret;

	x_end = x_start + x_size;

	while (frames_left) {
		ibuf = &state->ibuf[0];
		n1 = (x_end - x) / cd->channels;
		n2 = stft_process_buffer_samples_without_wrap(ibuf, ibuf->w_ptr);
		n = MIN(n1, n2);
		n = MIN(n, frames_left);
		for (i = 0; i < n; i++) {
			for (j = 0; j < channels; j++) {
				ibuf = &state->ibuf[j];
				in = *x++;
				*ibuf->w_ptr++ = (int32_t)in << 16;
			}
		}

		for (j = 0; j < channels; j++) {
			ibuf = &state->ibuf[j];
			ibuf->w_ptr = stft_process_buffer_wrap(ibuf, ibuf->w_ptr);
		}

		if (x >= x_end)
			x -= x_size;

		frames_left -= n;
	}

	source_release_data(source, bytes);
	for (j = 0; j < channels; j++) {
		ibuf = &state->ibuf[j];
		ibuf->s_avail += frames;
		ibuf->s_free -= frames;
	}
	return 0;
}

int stft_process_sink_s16(struct stft_comp_data *cd, struct sof_sink *sink, int frames)
{
	struct stft_process_state *state = &cd->state;
	struct stft_process_buffer *obuf;
	int16_t *y, *y_start, *y_end;
	int frames_remain = frames;
	int channels = cd->channels;
	int bytes = frames * cd->frame_bytes;
	int y_size;
	int ret;
	int ch, n1, n, i;

	ret = sink_get_buffer_s16(sink, bytes, &y, &y_start, &y_size);
	if (ret)
		return ret;

	y_end = y_start + y_size;
	while (frames_remain) {
		obuf = &state->obuf[0];
		n1 = (y_end - y) / cd->channels;
		n = stft_process_buffer_samples_without_wrap(obuf, obuf->r_ptr);
		n = MIN(n1, n);
		n = MIN(n, frames_remain);

		for (i = 0; i < n; i++) {
			for (ch = 0; ch < channels; ch++) {
				obuf = &state->obuf[ch];
				*y++ = sat_int16(Q_SHIFT_RND(*obuf->r_ptr, 31, 15));
				*obuf->r_ptr++ = 0; /* clear overlap add mix */
			}
		}

		for (ch = 0; ch < channels; ch++) {
			obuf = &state->obuf[ch];
			obuf->r_ptr = stft_process_buffer_wrap(obuf, obuf->r_ptr);
		}

		if (y >= y_end)
			y -= y_size;

		frames_remain -= n;
	}

	sink_commit_buffer(sink, bytes);
	for (ch = 0; ch < channels; ch++) {
		obuf = &state->obuf[ch];
		obuf->s_avail -= frames;
		obuf->s_free += frames;
	}

	return 0;
}
#endif /* CONFIG_FORMAT_S16LE */

void stft_process_fill_prev_samples(struct stft_process_buffer *buf, int32_t *prev_data,
				    int prev_data_length)
{
	int32_t *r = buf->r_ptr;
	int32_t *p = prev_data;
	int copied;
	int nmax;
	int n;

	for (copied = 0; copied < prev_data_length; copied += n) {
		nmax = prev_data_length - copied;
		n = stft_process_buffer_samples_without_wrap(buf, r);
		n = MIN(n, nmax);
		memcpy(p, r, sizeof(int32_t) * n);
		p += n;
		r += n;
		r = stft_process_buffer_wrap(buf, r);
	}

	buf->s_avail -= copied;
	buf->s_free += copied;
	buf->r_ptr = r;
}

void stft_process_fill_fft_buffer(struct stft_process_state *state, int ch)
{
	struct stft_process_buffer *ibuf = &state->ibuf[ch];
	struct stft_process_fft *fft = &state->fft;
	int32_t *prev_data = state->prev_data[ch];
	int32_t *r = ibuf->r_ptr;
	int copied;
	int nmax;
	int idx;
	int j;
	int n;

	/* Copy overlapped samples from state buffer. Imaginary part of input
	 * remains zero.
	 */
	for (j = 0; j < state->prev_data_size; j++) {
		fft->fft_buf[j].real = prev_data[j];
		fft->fft_buf[j].imag = 0;
	}

	/* Copy hop size of new data from circular buffer */
	idx = state->prev_data_size;
	for (copied = 0; copied < fft->fft_hop_size; copied += n) {
		nmax = fft->fft_hop_size - copied;
		n = stft_process_buffer_samples_without_wrap(ibuf, r);
		n = MIN(n, nmax);
		for (j = 0; j < n; j++) {
			fft->fft_buf[idx].real = *r++;
			fft->fft_buf[idx].imag = 0;
			idx++;
		}
		r = stft_process_buffer_wrap(ibuf, r);
	}

	ibuf->s_avail -= copied;
	ibuf->s_free += copied;
	ibuf->r_ptr = r;

	/* Copy for next time data back to overlap buffer */
	idx = fft->fft_hop_size;
	for (j = 0; j < state->prev_data_size; j++)
		prev_data[j] = fft->fft_buf[idx + j].real;
}

LOG_MODULE_REGISTER(stft_process_common, CONFIG_SOF_LOG_LEVEL);

/*
 * The main processing function for STFT_PROCESS
 */

static int stft_prepare_fft(struct stft_process_state *state, int channel)
{
	struct stft_process_buffer *ibuf = &state->ibuf[channel];
	struct stft_process_fft *fft = &state->fft;

	/* Wait for FFT hop size of new data */
	if (ibuf->s_avail < fft->fft_hop_size)
		return 0;

	return 1;
}

static void stft_do_fft(struct stft_process_state *state, int ch)
{
	struct stft_process_fft *fft = &state->fft;

	/* Copy data to FFT input buffer from overlap buffer and from new samples buffer */
	stft_process_fill_fft_buffer(state, ch);

	/* Window function */
	stft_process_apply_window(state);

#if STFT_DEBUG
	debug_print_to_file_real(stft_debug_fft_in_fh, fft->fft_buf, fft->fft_size);
#endif

	/* Compute FFT. A full scale s16 sine input with 2^N samples period in low
	 * part of s32 real part and zero imaginary part gives to output about 0.5
	 * full scale 32 bit output to real and imaginary. The scaling is same for
	 * all FFT sizes.
	 */
	fft_multi_execute_32(fft->fft_plan, false);

#if STFT_DEBUG
	debug_print_to_file_complex(stft_debug_fft_out_fh, fft->fft_out, fft->fft_size);
#endif
}

static void stft_do_ifft(struct stft_process_state *state, int ch)
{
	struct stft_process_fft *fft = &state->fft;

	/* Compute IFFT */
	fft_multi_execute_32(fft->ifft_plan, true);

#if STFT_DEBUG
	debug_print_to_file_complex(stft_debug_ifft_out_fh, fft->fft_buf, fft->fft_size);
#endif

	/* Window function */
	stft_process_apply_window(state);

	/* Copy to output buffer */
	stft_process_overlap_add_ifft_buffer(state, ch);
}

#if CONFIG_STFT_PROCESS_MAGNITUDE_PHASE
static void stft_convert_to_polar(struct stft_process_fft *fft)
{
	int i;

	for (i = 0; i < fft->half_fft_size; i++)
		sofm_icomplex32_to_polar(&fft->fft_out[i], &fft->fft_polar[i]);
}

static void stft_convert_to_complex(struct stft_process_fft *fft)
{
	int i;

	for (i = 0; i < fft->half_fft_size; i++)
		sofm_ipolar32_to_complex(&fft->fft_polar[i], &fft->fft_out[i]);
}

static void stft_apply_fft_symmetry(struct stft_process_fft *fft)
{
	int i, j, k;

	j = 2 * fft->half_fft_size - 2;
	for (i = fft->half_fft_size; i < fft->fft_size; i++) {
		k = j - i;
		fft->fft_out[i].real = fft->fft_out[k].real;
		fft->fft_out[i].imag = -fft->fft_out[k].imag;
	}
}
#endif

static void stft_do_fft_ifft(const struct processing_module *mod)
{
	struct stft_comp_data *cd = module_get_private_data(mod);
	struct stft_process_state *state = &cd->state;
	int num_fft;
	int ch;

	for (ch = 0; ch < cd->channels; ch++) {
		num_fft = stft_prepare_fft(state, ch);

		if (num_fft) {
			stft_do_fft(state, ch);

#if CONFIG_STFT_PROCESS_MAGNITUDE_PHASE
			/* Convert half-FFT to polar and back, and fix upper part */
			stft_convert_to_polar(&state->fft);
			stft_convert_to_complex(&state->fft);
			stft_apply_fft_symmetry(&state->fft);
#endif

			stft_do_ifft(state, ch);
			cd->fft_done = true;
		}
	}
}

#if CONFIG_FORMAT_S32LE
static int stft_process_output_zeros_s32(struct stft_comp_data *cd, struct sof_sink *sink,
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

static int stft_process_s32(const struct processing_module *mod, struct sof_source *source,
			    struct sof_sink *sink, uint32_t frames)
{
	struct stft_comp_data *cd = module_get_private_data(mod);

	/* Get samples from source buffer */
	stft_process_source_s32(cd, source, frames);

	/* Do STFT, processing and inverse STFT */
	stft_do_fft_ifft(mod);

	/* Get samples from source buffer */
	if (cd->fft_done)
		stft_process_sink_s32(cd, sink, frames);
	else
		stft_process_output_zeros_s32(cd, sink, frames);

	return 0;
}
#endif /* CONFIG_FORMAT_S32LE */

#if CONFIG_FORMAT_S16LE
static int stft_process_output_zeros_s16(struct stft_comp_data *cd, struct sof_sink *sink,
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

static int stft_process_s16(const struct processing_module *mod, struct sof_source *source,
			    struct sof_sink *sink, uint32_t frames)
{
	struct stft_comp_data *cd = module_get_private_data(mod);

	/* Get samples from source buffer */
	stft_process_source_s16(cd, source, frames);

	/* Do STFT, processing and inverse STFT */
	stft_do_fft_ifft(mod);

	/* Get samples from source buffer */
	if (cd->fft_done)
		stft_process_sink_s16(cd, sink, frames);
	else
		stft_process_output_zeros_s16(cd, sink, frames);

	return 0;
}
#endif /* CONFIG_FORMAT_S16LE */

#if CONFIG_FORMAT_S24LE
#endif /* CONFIG_FORMAT_S24LE */

/* This struct array defines the used processing functions for
 * the PCM formats
 */
const struct stft_process_proc_fnmap stft_process_functions[] = {
#if CONFIG_FORMAT_S16LE
	{ SOF_IPC_FRAME_S16_LE, stft_process_s16 },
	{ SOF_IPC_FRAME_S32_LE, stft_process_s32 },
#endif
};

/**
 * stft_process_find_proc_func() - Find suitable processing function.
 * @src_fmt: Enum value for PCM format.
 *
 * This function finds the suitable processing function to use for
 * the used PCM format. If not found, return NULL.
 *
 * Return: Pointer to processing function for the requested PCM format.
 */
stft_process_func stft_process_find_proc_func(enum sof_ipc_frame src_fmt)
{
	int i;

	/* Find suitable processing function from map */
	for (i = 0; i < ARRAY_SIZE(stft_process_functions); i++)
		if (src_fmt == stft_process_functions[i].frame_fmt)
			return stft_process_functions[i].stft_process_function;

	return NULL;
}
