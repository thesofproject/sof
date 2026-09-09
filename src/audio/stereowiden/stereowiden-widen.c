/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2026 Intel Corporation.
 *
 * Real FFmpeg stereo widener (stereowiden) fixed-point algorithm.
 */

#include "stereowiden.h"
#include <rtos/string.h>

static inline int32_t sat32(int64_t x)
{
	if (x > 2147483647LL)
		return 2147483647;
	if (x < -2147483648LL)
		return -2147483648;
	return (int32_t)x;
}

static inline int16_t sat16(int32_t x)
{
	if (x > 32767)
		return 32767;
	if (x < -32768)
		return -32768;
	return (int16_t)x;
}

void stereowiden_process_s16(struct stereowiden_comp_data *cd,
			     const int16_t *src, int16_t *dst,
			     size_t frames)
{
	size_t dlen = cd->delay_frames;
	int32_t drymix = cd->drymix;
	int32_t crossfeed = cd->crossfeed;
	int32_t feedback = cd->feedback;
	size_t i;

	if (cd->channels != 2 || dlen == 0) {
		/* Mono or passthrough */
		memcpy(dst, src, frames * cd->channels * sizeof(int16_t));
		return;
	}

	for (i = 0; i < frames; i++) {
		int32_t left = (int32_t)src[i * 2 + 0] << 16;
		int32_t right = (int32_t)src[i * 2 + 1] << 16;

		int32_t cur_left = cd->buffer[cd->buf_pos * 2 + 0];
		int32_t cur_right = cd->buffer[cd->buf_pos * 2 + 1];

		int64_t out_l = ((int64_t)left * drymix - (int64_t)right * crossfeed - (int64_t)cur_right * feedback) >> 30;
		int64_t out_r = ((int64_t)right * drymix - (int64_t)left * crossfeed - (int64_t)cur_left * feedback) >> 30;

		cd->buffer[cd->buf_pos * 2 + 0] = left;
		cd->buffer[cd->buf_pos * 2 + 1] = right;
		cd->buf_pos = (cd->buf_pos + 1) % dlen;

		dst[i * 2 + 0] = sat16((int32_t)(sat32(out_l) >> 16));
		dst[i * 2 + 1] = sat16((int32_t)(sat32(out_r) >> 16));
	}
}

void stereowiden_process_s32(struct stereowiden_comp_data *cd,
			     const int32_t *src, int32_t *dst,
			     size_t frames)
{
	size_t dlen = cd->delay_frames;
	int32_t drymix = cd->drymix;
	int32_t crossfeed = cd->crossfeed;
	int32_t feedback = cd->feedback;
	size_t i;

	if (cd->channels != 2 || dlen == 0) {
		/* Mono or passthrough */
		memcpy(dst, src, frames * cd->channels * sizeof(int32_t));
		return;
	}

	for (i = 0; i < frames; i++) {
		int32_t left = src[i * 2 + 0];
		int32_t right = src[i * 2 + 1];

		int32_t cur_left = cd->buffer[cd->buf_pos * 2 + 0];
		int32_t cur_right = cd->buffer[cd->buf_pos * 2 + 1];

		int64_t out_l = ((int64_t)left * drymix - (int64_t)right * crossfeed - (int64_t)cur_right * feedback) >> 30;
		int64_t out_r = ((int64_t)right * drymix - (int64_t)left * crossfeed - (int64_t)cur_left * feedback) >> 30;

		cd->buffer[cd->buf_pos * 2 + 0] = left;
		cd->buffer[cd->buf_pos * 2 + 1] = right;
		cd->buf_pos = (cd->buf_pos + 1) % dlen;

		dst[i * 2 + 0] = sat32(out_l);
		dst[i * 2 + 1] = sat32(out_r);
	}
}

static int stereowiden_widen_init(struct processing_module *mod)
{
	return 0;
}

static int stereowiden_widen_prepare(struct processing_module *mod)
{
	struct stereowiden_comp_data *cd = module_get_private_data(mod);

	cd->delay_frames = (cd->delay_ms * cd->rate) / 1000;
	if (cd->delay_frames > STEREOWIDEN_DELAY_MAX_FRAMES)
		cd->delay_frames = STEREOWIDEN_DELAY_MAX_FRAMES;
	if (cd->delay_frames == 0)
		cd->delay_frames = 1;

	cd->buf_pos = 0;
	memset(cd->buffer, 0, sizeof(cd->buffer));

	return 0;
}

static int stereowiden_widen_process(struct processing_module *mod,
				     const void *src, void *dst, int frames,
				     enum sof_ipc_frame fmt)
{
	struct stereowiden_comp_data *cd = module_get_private_data(mod);

	if (!cd->enabled) {
		memcpy(dst, src, frames * cd->channels *
		       (fmt == SOF_IPC_FRAME_S16_LE ? sizeof(int16_t) : sizeof(int32_t)));
		return 0;
	}

	if (fmt == SOF_IPC_FRAME_S16_LE)
		stereowiden_process_s16(cd, src, dst, frames);
	else
		stereowiden_process_s32(cd, src, dst, frames);

	return 0;
}

static int stereowiden_widen_reset(struct processing_module *mod)
{
	struct stereowiden_comp_data *cd = module_get_private_data(mod);

	if (cd) {
		cd->buf_pos = 0;
		memset(cd->buffer, 0, sizeof(cd->buffer));
	}

	return 0;
}

static int stereowiden_widen_free(struct processing_module *mod)
{
	return 0;
}

const struct stereowiden_backend stereowiden_real_backend = {
	.name = "stereowiden",
	.init = stereowiden_widen_init,
	.prepare = stereowiden_widen_prepare,
	.process = stereowiden_widen_process,
	.reset = stereowiden_widen_reset,
	.free = stereowiden_widen_free,
};
