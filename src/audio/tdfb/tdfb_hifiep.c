// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Intel Corporation. All rights reserved.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

#include <sof/common.h>
#include <user/tdfb.h>
#include <sof/audio/audio_stream.h>
#include <sof/audio/tdfb/tdfb_comp.h>
#include <user/fir.h>
#include <user/tdfb.h>

#if TDFB_HIFIEP

#include <sof/math/fir_hifi2ep.h>

static inline void tdfb_core(struct tdfb_comp_data *cd, int in_nch, int out_nch)
{
	struct fir_state_32x16 *f;
	int32_t y0;
	int32_t y1;
	int lshift;
	int rshift;
	int is2;
	int is;
	int om;
	int i;
	int k;
	const int num_filters = cd->config->num_filters;

	/* Clear output mix*/
	memset(cd->out, 0, 2 * out_nch * sizeof(int32_t));

	/* Run and mix all filters to their output channel */
	for (i = 0; i < num_filters; i++) {
		is = cd->input_channel_select[i];
		is2 = is + in_nch;
		om = cd->output_channel_mix[i];
		/* Prepare FIR */
		f = &cd->fir[i];
		fir_hifiep_setup_circular(f);
		fir_get_lrshifts(f, &lshift, &rshift);
		/* Process two samples */
		fir_32x16_2x_hifiep(f, cd->in[is], cd->in[is2], &y0, &y1, lshift, rshift);
		/* Mix as Q5.27 */
		for (k = 0; k < out_nch; k++) {
			if (om & 1) {
				cd->out[k] += y0 >> 4;
				cd->out[k + out_nch] += y1 >> 4;
			}
			om = om >> 1;
		}
	}
}

#if CONFIG_FORMAT_S16LE
void tdfb_fir_s16(struct tdfb_comp_data *cd, struct input_stream_buffer *bsource,
		  struct output_stream_buffer *bsink, int frames)
{
	struct audio_stream *source = bsource->data;
	struct audio_stream *sink = bsink->data;
	int16_t *x = audio_stream_get_rptr(source);
	int16_t *y = audio_stream_get_wptr(sink);
	int fmax;
	int i;
	int j;
	int f;
	const int in_nch = audio_stream_get_channels(source);
	const int out_nch = audio_stream_get_channels(sink);
	int remaining_frames = frames;
	int emp_ch = 0;

	while (remaining_frames) {
		fmax = audio_stream_frames_without_wrap(source, x);
		f = MIN(remaining_frames, fmax);
		fmax = audio_stream_frames_without_wrap(sink, y);
		f = MIN(f, fmax);
		for (j = 0; j < f; j += 2) {
			/* Read two frames from all input channels */
			for (i = 0; i < 2 * in_nch; i++) {
				cd->in[i] = *x << 16;
				tdfb_direction_copy_emphasis(cd, in_nch, &emp_ch, *x << 16);
				x++;
			}

			/* Process */
			tdfb_core(cd, in_nch, out_nch);

			/* Write two frames of output */
			for (i = 0; i < 2 * out_nch; i++) {
				*y = sat_int16(Q_SHIFT_RND(cd->out[i], 27, 15));
				y++;
			}
		}
		remaining_frames -= f;
		x = audio_stream_wrap(source, x);
		y = audio_stream_wrap(sink, y);
	}
}
#endif

#if CONFIG_FORMAT_S24LE
void tdfb_fir_s24(struct tdfb_comp_data *cd, struct input_stream_buffer *bsource,
		  struct output_stream_buffer *bsink, int frames)
{
	struct audio_stream *source = bsource->data;
	struct audio_stream *sink = bsink->data;
	int32_t *x = audio_stream_get_rptr(source);
	int32_t *y = audio_stream_get_wptr(sink);
	int fmax;
	int i;
	int j;
	int f;
	const int in_nch = audio_stream_get_channels(source);
	const int out_nch = audio_stream_get_channels(sink);
	int remaining_frames = frames;
	int emp_ch = 0;

	while (remaining_frames) {
		fmax = audio_stream_frames_without_wrap(source, x);
		f = MIN(remaining_frames, fmax);
		fmax = audio_stream_frames_without_wrap(sink, y);
		f = MIN(f, fmax);
		for (j = 0; j < f; j += 2) {
			/* Read two frames from all input channels */
			for (i = 0; i < 2 * in_nch; i++) {
				cd->in[i] = *x << 8;
				tdfb_direction_copy_emphasis(cd, in_nch, &emp_ch, *x << 8);
				x++;
			}

			/* Process */
			tdfb_core(cd, in_nch, out_nch);

			/* Write two frames of output */
			for (i = 0; i < 2 * out_nch; i++) {
				*y = sat_int24(Q_SHIFT_RND(cd->out[i], 27, 23));
				y++;
			}
		}
		remaining_frames -= f;
		x = audio_stream_wrap(source, x);
		y = audio_stream_wrap(sink, y);
	}
}
#endif

#if CONFIG_FORMAT_S32LE
void tdfb_fir_s32(struct tdfb_comp_data *cd, struct input_stream_buffer *bsource,
		  struct output_stream_buffer *bsink, int frames)
{
	struct audio_stream *source = bsource->data;
	struct audio_stream *sink = bsink->data;
	int32_t *x = audio_stream_get_rptr(source);
	int32_t *y = audio_stream_get_wptr(sink);
	int fmax;
	int i;
	int j;
	int f;
	const int in_nch = audio_stream_get_channels(source);
	const int out_nch = audio_stream_get_channels(sink);
	int remaining_frames = frames;
	int emp_ch = 0;

	while (remaining_frames) {
		fmax = audio_stream_frames_without_wrap(source, x);
		f = MIN(remaining_frames, fmax);
		fmax = audio_stream_frames_without_wrap(sink, y);
		f = MIN(f, fmax);
		for (j = 0; j < f; j += 2) {
			/* Clear output mix*/
			memset(cd->out, 0, 2 * out_nch * sizeof(int32_t));

			/* Read two frames from all input channels */
			for (i = 0; i < 2 * in_nch; i++) {
				cd->in[i] = *x;
				tdfb_direction_copy_emphasis(cd, in_nch, &emp_ch, *x);
				x++;
			}

			/* Process */
			tdfb_core(cd, in_nch, out_nch);

			/* Write two frames of output. In Q5.27 to Q1.31 conversion
			 * rounding is not applicable so just shift left by 4.
			 */
			for (i = 0; i < 2 * out_nch; i++) {
				*y = sat_int32((int64_t)cd->out[i] << 4);
				y++;
			}
		}
		remaining_frames -= f;
		x = audio_stream_wrap(source, x);
		y = audio_stream_wrap(sink, y);
	}
}
#endif

#endif /* TDFB_HIFIEP */

