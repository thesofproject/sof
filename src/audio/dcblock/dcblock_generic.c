// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Google LLC. All rights reserved.
//
// Author: Sebastiano Carlucci <scarlucci@google.com>

#include <stdint.h>
#include <sof/audio/component.h>
#include <sof/audio/format.h>
#include <sof/audio/audio_stream.h>

#include "dcblock.h"

#if SOF_USE_HIFI(NONE, DCBLOCK)

LOG_MODULE_DECLARE(dcblock, CONFIG_SOF_LOG_LEVEL);

/**
 *
 * Genereric processing function. Input is 32 bits.
 *
 */
static int32_t dcblock_generic(struct dcblock_state *state,
			       int64_t R, int32_t x)
{
	/*
	 * R: Q2.30, y_prev: Q1.31
	 * R * y_prev: Q3.61
	 */
	int64_t out = ((int64_t)x) - state->x_prev +
		      Q_SHIFT_RND(R * state->y_prev, 61, 31);

	state->y_prev = sat_int32(out);
	state->x_prev = x;

	return state->y_prev;
}

#if CONFIG_FORMAT_S16LE
/**
 * dcblock_s16_default() - Process S16_LE format.
 * @cd: DC blocking filter component private data.
 * @source: Source for PCM samples data.
 * @sink: Sink for PCM samples data.
 * @frames: Number of audio data frames to process.
 *
 * Return: Value zero for success, otherwise an error code.
 */
static int dcblock_s16_default(struct comp_data *cd,
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
			tmp = dcblock_generic(&cd->state[ch], cd->R_coeffs[ch],
					      *x << 16);
			*y = sat_int16(Q_SHIFT_RND(tmp, 31, 15));
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
/**
 * dcblock_s24_default() - Process S24_4LE format.
 * @cd: DC blocking filter component private data.
 * @source: Source for PCM samples data.
 * @sink: Sink for PCM samples data.
 * @frames: Number of audio data frames to process.
 *
 * Return: Value zero for success, otherwise an error code.
 */
static int dcblock_s24_default(struct comp_data *cd,
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
			tmp = dcblock_generic(&cd->state[ch], cd->R_coeffs[ch],
					      *x << 8);
			*y = sat_int24(Q_SHIFT_RND(tmp, 31, 23));
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
/**
 * dcblock_s32_default() - Process S32_LE format.
 * @cd: DC blocking filter component private data.
 * @source: Source for PCM samples data.
 * @sink: Sink for PCM samples data.
 * @frames: Number of audio data frames to process.
 *
 * Return: Value zero for success, otherwise an error code.
 */
static int dcblock_s32_default(struct comp_data *cd,
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
			*y = dcblock_generic(&cd->state[ch], cd->R_coeffs[ch],
					     *x);
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

const struct dcblock_func_map dcblock_fnmap[] = {
/* { SOURCE_FORMAT , PROCESSING FUNCTION } */
#if CONFIG_FORMAT_S16LE
	{ SOF_IPC_FRAME_S16_LE, dcblock_s16_default },
#endif /* CONFIG_FORMAT_S16LE */
#if CONFIG_FORMAT_S24LE
	{ SOF_IPC_FRAME_S24_4LE, dcblock_s24_default },
#endif /* CONFIG_FORMAT_S24LE */
#if CONFIG_FORMAT_S32LE
	{ SOF_IPC_FRAME_S32_LE, dcblock_s32_default },
#endif /* CONFIG_FORMAT_S32LE */
};

const size_t dcblock_fncount = ARRAY_SIZE(dcblock_fnmap);
#endif
