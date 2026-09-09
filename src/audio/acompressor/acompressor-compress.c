/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2026 Intel Corporation.
 *
 * Real FFmpeg dynamic range compressor (acompressor) fixed-point algorithm.
 */

#include "acompressor.h"
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

void acompressor_process_s16(struct acompressor_comp_data *cd,
			     const int16_t *src, int16_t *dst,
			     size_t frames)
{
	int32_t thres = cd->threshold;
	int32_t ratio = cd->ratio;
	int32_t att_c = cd->attack_coeff;
	int32_t rel_c = cd->release_coeff;
	int32_t makeup = cd->makeup;
	int32_t env = cd->envelope;
	size_t ch = cd->channels;
	size_t i, c;

	for (i = 0; i < frames; i++) {
		int32_t peak = 0;
		for (c = 0; c < ch; c++) {
			int32_t s = (int32_t)src[i * ch + c] << 16;
			int32_t a = (s < 0) ? -s : s;
			if (a > peak)
				peak = a;
		}

		if (peak > env) {
			env = (int32_t)(((int64_t)att_c * env + ((int64_t)(1 << 30) - att_c) * peak) >> 30);
		} else {
			env = (int32_t)(((int64_t)rel_c * env + ((int64_t)(1 << 30) - rel_c) * peak) >> 30);
		}

		int32_t gain = 1 << 30; /* 1.0 in Q30 */
		if (env > thres && env > 0) {
			int64_t ratio_term = ((int64_t)thres << 30) / env;
			gain = (int32_t)((1LL << 30) / ratio + ((ratio - 1) * ratio_term) / ratio);
		}
		int32_t total_gain = (int32_t)(((int64_t)gain * makeup) >> 30);

		for (c = 0; c < ch; c++) {
			int32_t s = (int32_t)src[i * ch + c] << 16;
			int32_t out = (int32_t)(((int64_t)s * total_gain) >> 30);
			dst[i * ch + c] = sat16(sat32(out) >> 16);
		}
	}
	cd->envelope = env;
}

void acompressor_process_s32(struct acompressor_comp_data *cd,
			     const int32_t *src, int32_t *dst,
			     size_t frames)
{
	int32_t thres = cd->threshold;
	int32_t ratio = cd->ratio;
	int32_t att_c = cd->attack_coeff;
	int32_t rel_c = cd->release_coeff;
	int32_t makeup = cd->makeup;
	int32_t env = cd->envelope;
	size_t ch = cd->channels;
	size_t i, c;

	for (i = 0; i < frames; i++) {
		int32_t peak = 0;
		for (c = 0; c < ch; c++) {
			int32_t s = src[i * ch + c];
			int32_t a = (s < 0) ? -s : s;
			if (a > peak)
				peak = a;
		}

		if (peak > env) {
			env = (int32_t)(((int64_t)att_c * env + ((int64_t)(1 << 30) - att_c) * peak) >> 30);
		} else {
			env = (int32_t)(((int64_t)rel_c * env + ((int64_t)(1 << 30) - rel_c) * peak) >> 30);
		}

		int32_t gain = 1 << 30; /* 1.0 in Q30 */
		if (env > thres && env > 0) {
			int64_t ratio_term = ((int64_t)thres << 30) / env;
			gain = (int32_t)((1LL << 30) / ratio + ((ratio - 1) * ratio_term) / ratio);
		}
		int32_t total_gain = (int32_t)(((int64_t)gain * makeup) >> 30);

		for (c = 0; c < ch; c++) {
			int32_t s = src[i * ch + c];
			int32_t out = (int32_t)(((int64_t)s * total_gain) >> 30);
			dst[i * ch + c] = sat32(out);
		}
	}
	cd->envelope = env;
}

static int acompressor_compress_init(struct processing_module *mod)
{
	return 0;
}

static int acompressor_compress_prepare(struct processing_module *mod)
{
	struct acompressor_comp_data *cd = module_get_private_data(mod);

	/* Calculate attack and release coefficients in Q30 */
	int32_t att_denom = (cd->attack_ms * cd->rate) / 1000;
	int32_t rel_denom = (cd->release_ms * cd->rate) / 1000;

	if (att_denom <= 0) att_denom = 1;
	if (rel_denom <= 0) rel_denom = 1;

	/* 1 - 1/N approximation in Q30 */
	cd->attack_coeff = (int32_t)((1LL << 30) - (1LL << 30) / att_denom);
	cd->release_coeff = (int32_t)((1LL << 30) - (1LL << 30) / rel_denom);
	cd->envelope = 0;

	return 0;
}

static int acompressor_compress_process(struct processing_module *mod,
					const void *src, void *dst, int frames,
					enum sof_ipc_frame fmt)
{
	struct acompressor_comp_data *cd = module_get_private_data(mod);

	if (!cd->enabled) {
		memcpy(dst, src, frames * cd->channels *
		       (fmt == SOF_IPC_FRAME_S16_LE ? sizeof(int16_t) : sizeof(int32_t)));
		return 0;
	}

	if (fmt == SOF_IPC_FRAME_S16_LE)
		acompressor_process_s16(cd, src, dst, frames);
	else
		acompressor_process_s32(cd, src, dst, frames);

	return 0;
}

static int acompressor_compress_reset(struct processing_module *mod)
{
	struct acompressor_comp_data *cd = module_get_private_data(mod);

	if (cd)
		cd->envelope = 0;

	return 0;
}

static int acompressor_compress_free(struct processing_module *mod)
{
	return 0;
}

const struct acompressor_backend acompressor_real_backend = {
	.name = "acompressor",
	.init = acompressor_compress_init,
	.prepare = acompressor_compress_prepare,
	.process = acompressor_compress_process,
	.reset = acompressor_compress_reset,
	.free = acompressor_compress_free,
};
