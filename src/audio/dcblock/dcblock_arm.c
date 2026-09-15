// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Sound Open Firmware. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>

#include <stdint.h>
#include <sof/audio/component.h>
#include <sof/audio/format.h>
#include <sof/audio/audio_stream.h>
#include <sof/audio/arm_simd.h>

#include "dcblock.h"

#if SOF_USE_ARM_SIMD(DCBLOCK)

LOG_MODULE_DECLARE(dcblock, CONFIG_SOF_LOG_LEVEL);

static inline int32_t dcblock_arm_process(struct dcblock_state *state, int64_t R, int32_t x)
{
	/*
	 * R: Q2.30, y_prev: Q1.31 -> R * y_prev: Q3.61
	 * Q_SHIFT_RND(R * y_prev, 61, 31) shifts right by 30 bits with rounding.
	 */
	int64_t prod = R * state->y_prev;
	int64_t out = ((int64_t)x) - state->x_prev + ((prod + (1LL << 29)) >> 30);

	state->y_prev = arm_sat_q31(out);
	state->x_prev = x;

	return state->y_prev;
}

#if CONFIG_FORMAT_S16LE
static int dcblock_s16_arm(struct comp_data *cd,
			   struct cir_buf_source *source,
			   struct cir_buf_sink *sink,
			   uint32_t frames)
{
	const int16_t *x = source->ptr;
	int16_t *y = sink->ptr;
	int samples_without_wrap;
	int nch = cd->channels;
	int remaining_samples = frames * nch;
	int32_t tmp;
	int ch = 0;
	int i;

	while (remaining_samples) {
		samples_without_wrap = cir_buf_samples_without_wrap_s16(x, source->buf_end);
		samples_without_wrap = MIN(samples_without_wrap,
					   cir_buf_samples_without_wrap_s16(y, sink->buf_end));
		samples_without_wrap = MIN(samples_without_wrap, remaining_samples);
		for (i = 0; i < samples_without_wrap; i++) {
			tmp = dcblock_arm_process(&cd->state[ch], cd->R_coeffs[ch], (int32_t)*x << 16);
			*y = arm_sat_s16(Q_SHIFT_RND(tmp, 31, 15));
			x++;
			y++;
			if (++ch == nch)
				ch = 0;
		}
		x = cir_buf_wrap((void *)x, source->buf_start, source->buf_end);
		y = cir_buf_wrap(y, sink->buf_start, sink->buf_end);
		remaining_samples -= samples_without_wrap;
	}

	return 0;
}
#endif /* CONFIG_FORMAT_S16LE */

#if CONFIG_FORMAT_S24LE
static int dcblock_s24_arm(struct comp_data *cd,
			   struct cir_buf_source *source,
			   struct cir_buf_sink *sink,
			   uint32_t frames)
{
	const int32_t *x = source->ptr;
	int32_t *y = sink->ptr;
	int samples_without_wrap;
	int nch = cd->channels;
	int remaining_samples = frames * nch;
	int32_t tmp;
	int ch = 0;
	int i;

	while (remaining_samples) {
		samples_without_wrap = cir_buf_samples_without_wrap_s32(x, source->buf_end);
		samples_without_wrap = MIN(samples_without_wrap,
					   cir_buf_samples_without_wrap_s32(y, sink->buf_end));
		samples_without_wrap = MIN(samples_without_wrap, remaining_samples);
		for (i = 0; i < samples_without_wrap; i++) {
			tmp = dcblock_arm_process(&cd->state[ch], cd->R_coeffs[ch], *x << 8);
			*y = arm_sat_s24(Q_SHIFT_RND(tmp, 31, 23));
			x++;
			y++;
			if (++ch == nch)
				ch = 0;
		}
		x = cir_buf_wrap((void *)x, source->buf_start, source->buf_end);
		y = cir_buf_wrap(y, sink->buf_start, sink->buf_end);
		remaining_samples -= samples_without_wrap;
	}

	return 0;
}
#endif /* CONFIG_FORMAT_S24LE */

#if CONFIG_FORMAT_S32LE
static int dcblock_s32_arm(struct comp_data *cd,
			   struct cir_buf_source *source,
			   struct cir_buf_sink *sink,
			   uint32_t frames)
{
	const int32_t *x = source->ptr;
	int32_t *y = sink->ptr;
	int samples_without_wrap;
	int nch = cd->channels;
	int remaining_samples = frames * nch;
	int ch = 0;
	int i;

	while (remaining_samples) {
		samples_without_wrap = cir_buf_samples_without_wrap_s32(x, source->buf_end);
		samples_without_wrap = MIN(samples_without_wrap,
					   cir_buf_samples_without_wrap_s32(y, sink->buf_end));
		samples_without_wrap = MIN(samples_without_wrap, remaining_samples);
		for (i = 0; i < samples_without_wrap; i++) {
			*y = dcblock_arm_process(&cd->state[ch], cd->R_coeffs[ch], *x);
			x++;
			y++;
			if (++ch == nch)
				ch = 0;
		}
		x = cir_buf_wrap((void *)x, source->buf_start, source->buf_end);
		y = cir_buf_wrap(y, sink->buf_start, sink->buf_end);
		remaining_samples -= samples_without_wrap;
	}

	return 0;
}
#endif /* CONFIG_FORMAT_S32LE */

#if CONFIG_FORMAT_FLOAT
static int dcblock_float_arm(struct comp_data *cd,
			     struct cir_buf_source *source,
			     struct cir_buf_sink *sink,
			     uint32_t frames)
{
	const float *x = source->ptr;
	float *y = sink->ptr;
	int samples_without_wrap;
	int nch = cd->channels;
	int remaining_samples = frames * nch;
	int ch = 0;
	int i;

	while (remaining_samples) {
		samples_without_wrap = cir_buf_samples_without_wrap_s32(x, source->buf_end);
		samples_without_wrap = MIN(samples_without_wrap,
					   cir_buf_samples_without_wrap_s32(y, sink->buf_end));
		samples_without_wrap = MIN(samples_without_wrap, remaining_samples);
		for (i = 0; i < samples_without_wrap; i++) {
			float out = *x - cd->state[ch].x_prev_f + cd->R_coeffs_f[ch] * cd->state[ch].y_prev_f;
			cd->state[ch].x_prev_f = *x;
			cd->state[ch].y_prev_f = out;
			*y = out;
			x++;
			y++;
			if (++ch == nch)
				ch = 0;
		}
		x = cir_buf_wrap((void *)x, source->buf_start, source->buf_end);
		y = cir_buf_wrap(y, sink->buf_start, sink->buf_end);
		remaining_samples -= samples_without_wrap;
	}

	return 0;
}
#endif /* CONFIG_FORMAT_FLOAT */

const struct dcblock_func_map dcblock_fnmap[] = {
#if CONFIG_FORMAT_S16LE
	{ SOF_IPC_FRAME_S16_LE, dcblock_s16_arm },
#endif
#if CONFIG_FORMAT_S24LE
	{ SOF_IPC_FRAME_S24_4LE, dcblock_s24_arm },
#endif
#if CONFIG_FORMAT_S32LE
	{ SOF_IPC_FRAME_S32_LE, dcblock_s32_arm },
#endif
#if CONFIG_FORMAT_FLOAT
	{ SOF_IPC_FRAME_FLOAT, dcblock_float_arm },
#endif
};

const size_t dcblock_fncount = ARRAY_SIZE(dcblock_fnmap);

#endif /* SOF_USE_ARM_SIMD(DCBLOCK) */
