// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Intel Corporation. All rights reserved.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

#include <sof/common.h>
#include <sof/audio/audio_stream.h>
#include <user/fir.h>

#include "tdfb.h"
#include "tdfb_comp.h"

#if TDFB_HIFIEP

#include <sof/math/fir_hifi2ep.h>

static inline void tdfb_core(struct tdfb_comp_data *cd, unsigned int in_nch, unsigned int out_nch)
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
	unsigned int k;
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
		fir_32x16_2x(f, cd->in[is], cd->in[is2], &y0, &y1, lshift, rshift);
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
void tdfb_fir_s16(struct tdfb_comp_data *cd, struct cir_buf_source *source, unsigned int in_nch,
		  struct cir_buf_sink *sink, unsigned int out_nch, size_t frames)
{
	const int16_t *x = source->ptr;
	int16_t *y = sink->ptr;
	unsigned int i;
	size_t f, fmax, j;
	size_t remaining_frames = frames;
	unsigned int emp_ch = 0;

	while (remaining_frames) {
		fmax = circ_buf_frames_without_wrap(x, source->buf_end, sizeof(int16_t), in_nch);
		f = MIN(remaining_frames, fmax);
		fmax = circ_buf_frames_without_wrap(y, sink->buf_end, sizeof(int16_t), out_nch);
		f = MIN(f, fmax);
		for (j = 0; j < f; j += 2) {
			/* Read two frames from all input channels */
			for (i = 0; i < 2 * in_nch; i++) {
				cd->in[i] = *x << 16;
				if (cd->direction_updates)
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
		x = source_cir_buf_wrap(x, source->buf_start, source->buf_end);
		y = cir_buf_wrap(y, sink->buf_start, sink->buf_end);
	}
}
#endif

#if CONFIG_FORMAT_S24LE
void tdfb_fir_s24(struct tdfb_comp_data *cd, struct cir_buf_source *source, unsigned int in_nch,
		  struct cir_buf_sink *sink, unsigned int out_nch, size_t frames)
{
	const int32_t *x = source->ptr;
	int32_t *y = sink->ptr;
	unsigned int i;
	size_t f, fmax, j;
	size_t remaining_frames = frames;
	unsigned int emp_ch = 0;

	while (remaining_frames) {
		fmax = circ_buf_frames_without_wrap(x, source->buf_end, sizeof(int32_t), in_nch);
		f = MIN(remaining_frames, fmax);
		fmax = circ_buf_frames_without_wrap(y, sink->buf_end, sizeof(int32_t), out_nch);
		f = MIN(f, fmax);
		for (j = 0; j < f; j += 2) {
			/* Read two frames from all input channels */
			for (i = 0; i < 2 * in_nch; i++) {
				cd->in[i] = *x << 8;
				if (cd->direction_updates)
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
		x = source_cir_buf_wrap(x, source->buf_start, source->buf_end);
		y = cir_buf_wrap(y, sink->buf_start, sink->buf_end);
	}
}
#endif

#if CONFIG_FORMAT_S32LE
void tdfb_fir_s32(struct tdfb_comp_data *cd, struct cir_buf_source *source, unsigned int in_nch,
		  struct cir_buf_sink *sink, unsigned int out_nch, size_t frames)
{
	const int32_t *x = source->ptr;
	int32_t *y = sink->ptr;
	unsigned int i;
	size_t f, fmax, j;
	size_t remaining_frames = frames;
	unsigned int emp_ch = 0;

	while (remaining_frames) {
		fmax = circ_buf_frames_without_wrap(x, source->buf_end, sizeof(int32_t), in_nch);
		f = MIN(remaining_frames, fmax);
		fmax = circ_buf_frames_without_wrap(y, sink->buf_end, sizeof(int32_t), out_nch);
		f = MIN(f, fmax);
		for (j = 0; j < f; j += 2) {
			/* Clear output mix*/
			memset(cd->out, 0, 2 * out_nch * sizeof(int32_t));

			/* Read two frames from all input channels */
			for (i = 0; i < 2 * in_nch; i++) {
				cd->in[i] = *x;
				if (cd->direction_updates)
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
		x = source_cir_buf_wrap(x, source->buf_start, source->buf_end);
		y = cir_buf_wrap(y, sink->buf_start, sink->buf_end);
	}
}
#endif

#endif /* TDFB_HIFIEP */

