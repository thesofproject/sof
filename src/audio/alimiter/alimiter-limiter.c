/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2026 Intel Corporation.
 *
 * Real FFmpeg lookahead peak limiter (alimiter) fixed-point algorithm.
 */

#include "alimiter.h"
#include <rtos/string.h>

void alimiter_process_channel_s16(struct alimiter_comp_data *cd,
				  const int16_t *src, int16_t *dst,
				  size_t frames)
{
	size_t ch = cd->channels;
	size_t blen = cd->buf_len;
	int32_t lim_val = cd->limit;
	size_t i, c;

	for (i = 0; i < frames; i++) {
		int32_t peak = 0;
		for (c = 0; c < ch; c++) {
			int32_t s = (int32_t)src[i * ch + c] << 16;
			cd->buffer[cd->buf_pos * ch + c] = s;
			int32_t a = (s < 0) ? -s : s;
			if (a > peak)
				peak = a;
		}

		if (peak > lim_val) {
			int64_t target_att = ((int64_t)lim_val << 30) / peak;
			int64_t needed_delta = (target_att - cd->att) / (int64_t)blen;
			if (needed_delta < cd->delta)
				cd->delta = needed_delta;
		}

		size_t read_pos = (cd->buf_pos + 1) % blen;
		cd->att += cd->delta;
		int64_t min_att = ((int64_t)1 << 30) / 100; /* 0.01 floor */
		if (cd->att < min_att)
			cd->att = min_att;

		if (cd->delta >= 0) {
			if (cd->att < ((int64_t)1 << 30)) {
				cd->att += cd->release_rate;
				if (cd->att > ((int64_t)1 << 30))
					cd->att = (int64_t)1 << 30;
			}
			cd->delta = 0;
		} else {
			int32_t delayed_peak = 0;
			for (c = 0; c < ch; c++) {
				int32_t ds = cd->buffer[read_pos * ch + c];
				int32_t a = (ds < 0) ? -ds : ds;
				if (a > delayed_peak)
					delayed_peak = a;
			}
			if (((int64_t)delayed_peak * cd->att >> 30) <= lim_val)
				cd->delta = 0;
		}

		for (c = 0; c < ch; c++) {
			int32_t ds = cd->buffer[read_pos * ch + c];
			int32_t out = (int32_t)(((int64_t)ds * cd->att) >> 30);
			if (out > lim_val)
				out = lim_val;
			else if (out < -lim_val)
				out = -lim_val;
			dst[i * ch + c] = (int16_t)(out >> 16);
		}

		cd->buf_pos = (cd->buf_pos + 1) % blen;
	}
}

void alimiter_process_channel_s32(struct alimiter_comp_data *cd,
				  const int32_t *src, int32_t *dst,
				  size_t frames)
{
	size_t ch = cd->channels;
	size_t blen = cd->buf_len;
	int32_t lim_val = cd->limit;
	size_t i, c;

	for (i = 0; i < frames; i++) {
		int32_t peak = 0;
		for (c = 0; c < ch; c++) {
			int32_t s = src[i * ch + c];
			cd->buffer[cd->buf_pos * ch + c] = s;
			int32_t a = (s < 0) ? -s : s;
			if (a > peak)
				peak = a;
		}

		if (peak > lim_val) {
			int64_t target_att = ((int64_t)lim_val << 30) / peak;
			int64_t needed_delta = (target_att - cd->att) / (int64_t)blen;
			if (needed_delta < cd->delta)
				cd->delta = needed_delta;
		}

		size_t read_pos = (cd->buf_pos + 1) % blen;
		cd->att += cd->delta;
		int64_t min_att = ((int64_t)1 << 30) / 100; /* 0.01 floor */
		if (cd->att < min_att)
			cd->att = min_att;

		if (cd->delta >= 0) {
			if (cd->att < ((int64_t)1 << 30)) {
				cd->att += cd->release_rate;
				if (cd->att > ((int64_t)1 << 30))
					cd->att = (int64_t)1 << 30;
			}
			cd->delta = 0;
		} else {
			int32_t delayed_peak = 0;
			for (c = 0; c < ch; c++) {
				int32_t ds = cd->buffer[read_pos * ch + c];
				int32_t a = (ds < 0) ? -ds : ds;
				if (a > delayed_peak)
					delayed_peak = a;
			}
			if (((int64_t)delayed_peak * cd->att >> 30) <= lim_val)
				cd->delta = 0;
		}

		for (c = 0; c < ch; c++) {
			int32_t ds = cd->buffer[read_pos * ch + c];
			int32_t out = (int32_t)(((int64_t)ds * cd->att) >> 30);
			if (out > lim_val)
				out = lim_val;
			else if (out < -lim_val)
				out = -lim_val;
			dst[i * ch + c] = out;
		}

		cd->buf_pos = (cd->buf_pos + 1) % blen;
	}
}

static int alimiter_real_init(struct processing_module *mod)
{
	return 0;
}

static int alimiter_real_prepare(struct processing_module *mod)
{
	struct alimiter_comp_data *cd = module_get_private_data(mod);

	cd->buf_pos = 0;
	cd->buf_len = (size_t)(cd->rate * cd->attack_ms) / 1000;
	if (cd->buf_len > ALIMITER_LOOKAHEAD_MAX_FRAMES)
		cd->buf_len = ALIMITER_LOOKAHEAD_MAX_FRAMES;
	if (cd->buf_len < 1)
		cd->buf_len = 1;

	memset(cd->buffer, 0, sizeof(cd->buffer));

	/* limit = 0.950 in Q31 = 2040109465 */
	cd->limit = 2040109465;
	cd->att = (int64_t)1 << 30;
	cd->delta = 0;

	size_t release_samples = (size_t)(cd->rate * cd->release_ms) / 1000;
	if (release_samples < 1)
		release_samples = 1;
	cd->release_rate = ((int64_t)1 << 30) / (int64_t)release_samples;

	return 0;
}

static int alimiter_real_process(struct processing_module *mod,
				 const void *src, void *dst, int frames,
				 enum sof_ipc_frame fmt)
{
	struct alimiter_comp_data *cd = module_get_private_data(mod);

	if (fmt == SOF_IPC_FRAME_S16_LE)
		alimiter_process_channel_s16(cd, src, dst, frames);
	else
		alimiter_process_channel_s32(cd, src, dst, frames);

	return 0;
}

static int alimiter_real_reset(struct processing_module *mod)
{
	struct alimiter_comp_data *cd = module_get_private_data(mod);

	cd->buf_pos = 0;
	memset(cd->buffer, 0, sizeof(cd->buffer));
	cd->att = (int64_t)1 << 30;
	cd->delta = 0;

	return 0;
}

static int alimiter_real_free(struct processing_module *mod)
{
	return 0;
}

const struct alimiter_backend alimiter_real_backend = {
	.name    = "alimiter_lookahead",
	.init    = alimiter_real_init,
	.prepare = alimiter_real_prepare,
	.process = alimiter_real_process,
	.reset   = alimiter_real_reset,
	.free    = alimiter_real_free,
};
