// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Sound Open Firmware. All rights reserved.

#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/audio/format.h>
#include <sof/common.h>
#include <ipc/stream.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

LOG_MODULE_DECLARE(volume_riscv, CONFIG_SOF_LOG_LEVEL);

#include "volume.h"

#if CONFIG_COMP_PEAK_VOL

static inline int32_t sat_clamp_q31(int64_t x)
{
	if (__builtin_expect(x >= (int64_t)INT32_MIN && x <= (int64_t)INT32_MAX, 1))
		return (int32_t)x;
	return (x > INT32_MAX) ? INT32_MAX : INT32_MIN;
}

static inline int16_t sat_clamp_s16(int32_t x)
{
	if (__builtin_expect(x >= INT16_MIN && x <= INT16_MAX, 1))
		return (int16_t)x;
	return (x > INT16_MAX) ? INT16_MAX : INT16_MIN;
}

static inline int32_t sat_clamp_s24(int32_t x)
{
	if (__builtin_expect(x >= INT24_MINVALUE && x <= INT24_MAXVALUE, 1))
		return x;
	return (x > INT24_MAXVALUE) ? INT24_MAXVALUE : INT24_MINVALUE;
}

#if CONFIG_FORMAT_S16LE
static inline int16_t vol_mult_s16(int16_t x, int32_t vol)
{
#if COMP_VOLUME_Q8_16
	const int32_t prod = ((int32_t)x * vol + 0x8000) >> 16;
	return sat_clamp_s16(prod);
#elif COMP_VOLUME_Q1_31
	const int64_t prod = ((int64_t)x * vol + (1LL << 30)) >> 31;
	return sat_clamp_s16((int32_t)prod);
#else
	const int64_t prod = ((int64_t)x * vol + (1LL << 22)) >> 23;
	return sat_clamp_s16((int32_t)prod);
#endif
}

static void vol_s16_to_s16(struct processing_module *mod, struct cir_buf_source *source,
			   struct cir_buf_sink *sink, uint32_t frames, uint32_t attenuation)
{
	struct vol_data *cd = module_get_private_data(mod);
	const int16_t *x = source->ptr;
	int16_t *y = sink->ptr;
	const int nch = cd->channels;
	int remaining_samples = frames * nch;

	while (remaining_samples) {
		const int nmax_src = cir_buf_samples_without_wrap_s16(x, source->buf_end);
		const int nmax_snk = cir_buf_samples_without_wrap_s16(y, sink->buf_end);
		int n = MIN(remaining_samples, MIN(nmax_src, nmax_snk));

		if (nch == 2) {
			const int32_t vol_l = cd->volume[0];
			const int32_t vol_r = cd->volume[1];
			int32_t peak_l = 0;
			int32_t peak_r = 0;

			int i = 0;
			for (; i <= n - 4; i += 4) {
				const int16_t x0 = x[i + 0];
				const int16_t x1 = x[i + 1];
				const int16_t x2 = x[i + 2];
				const int16_t x3 = x[i + 3];

				y[i + 0] = vol_mult_s16(x0, vol_l);
				y[i + 1] = vol_mult_s16(x1, vol_r);
				y[i + 2] = vol_mult_s16(x2, vol_l);
				y[i + 3] = vol_mult_s16(x3, vol_r);

				peak_l = MAX((int32_t)abs(x0), peak_l);
				peak_r = MAX((int32_t)abs(x1), peak_r);
				peak_l = MAX((int32_t)abs(x2), peak_l);
				peak_r = MAX((int32_t)abs(x3), peak_r);
			}
			for (; i < n; i += 2) {
				const int16_t x0 = x[i + 0];
				const int16_t x1 = x[i + 1];

				y[i + 0] = vol_mult_s16(x0, vol_l);
				y[i + 1] = vol_mult_s16(x1, vol_r);

				peak_l = MAX((int32_t)abs(x0), peak_l);
				peak_r = MAX((int32_t)abs(x1), peak_r);
			}

			peak_l <<= PEAK_16S_32C_ADJUST;
			peak_r <<= PEAK_16S_32C_ADJUST;
			cd->peak_regs.peak_meter[0] = MAX(peak_l, cd->peak_regs.peak_meter[0]);
			cd->peak_regs.peak_meter[1] = MAX(peak_r, cd->peak_regs.peak_meter[1]);
		} else {
			for (int j = 0; j < nch; j++) {
				const int32_t vol = cd->volume[j];
				int32_t peak = 0;
				for (int i = j; i < n; i += nch) {
					y[i] = vol_mult_s16(x[i], vol);
					peak = MAX((int32_t)abs(x[i]), peak);
				}
				peak <<= PEAK_16S_32C_ADJUST;
				cd->peak_regs.peak_meter[j] = MAX(peak, cd->peak_regs.peak_meter[j]);
			}
		}

		remaining_samples -= n;
		x = cir_buf_wrap(x + n, source->buf_start, source->buf_end);
		y = cir_buf_wrap(y + n, sink->buf_start, sink->buf_end);
	}
}

static void vol_passthrough_s16_to_s16(struct processing_module *mod,
				       struct cir_buf_source *source,
				       struct cir_buf_sink *sink, uint32_t frames,
				       uint32_t attenuation)
{
	struct vol_data *cd = module_get_private_data(mod);
	const int16_t *x = source->ptr;
	int16_t *y = sink->ptr;
	const int nch = cd->channels;
	int remaining_samples = frames * nch;

	while (remaining_samples) {
		const int nmax_src = cir_buf_samples_without_wrap_s16(x, source->buf_end);
		const int nmax_snk = cir_buf_samples_without_wrap_s16(y, sink->buf_end);
		int n = MIN(remaining_samples, MIN(nmax_src, nmax_snk));

		for (int j = 0; j < nch; j++) {
			int32_t peak = 0;
			for (int i = j; i < n; i += nch) {
				y[i] = x[i];
				peak = MAX((int32_t)abs(x[i]), peak);
			}
			peak <<= PEAK_16S_32C_ADJUST;
			cd->peak_regs.peak_meter[j] = MAX(peak, cd->peak_regs.peak_meter[j]);
		}

		remaining_samples -= n;
		x = cir_buf_wrap(x + n, source->buf_start, source->buf_end);
		y = cir_buf_wrap(y + n, sink->buf_start, sink->buf_end);
	}
}
#endif /* CONFIG_FORMAT_S16LE */

#if CONFIG_FORMAT_S24LE
static inline int32_t vol_mult_s24(int32_t x, int32_t vol)
{
	const int32_t sx = sign_extend_s24(x);
#if COMP_VOLUME_Q8_16
	const int64_t prod = ((int64_t)sx * vol + (1LL << 15)) >> 16;
#elif COMP_VOLUME_Q1_31
	const int64_t prod = ((int64_t)sx * vol + (1LL << 30)) >> 31;
#else
	const int64_t prod = ((int64_t)sx * vol + (1LL << 22)) >> 23;
#endif
	return sat_clamp_s24((int32_t)prod);
}

static void vol_s24_to_s24(struct processing_module *mod, struct cir_buf_source *source,
			   struct cir_buf_sink *sink, uint32_t frames, uint32_t attenuation)
{
	struct vol_data *cd = module_get_private_data(mod);
	const int32_t *x = source->ptr;
	int32_t *y = sink->ptr;
	const int nch = cd->channels;
	int remaining_samples = frames * nch;

	while (remaining_samples) {
		const int nmax_src = cir_buf_samples_without_wrap_s32(x, source->buf_end);
		const int nmax_snk = cir_buf_samples_without_wrap_s32(y, sink->buf_end);
		int n = MIN(remaining_samples, MIN(nmax_src, nmax_snk));

		if (nch == 2) {
			const int32_t vol_l = cd->volume[0];
			const int32_t vol_r = cd->volume[1];
			int32_t peak_l = 0;
			int32_t peak_r = 0;

			int i = 0;
			for (; i <= n - 4; i += 4) {
				const int32_t x0 = x[i + 0];
				const int32_t x1 = x[i + 1];
				const int32_t x2 = x[i + 2];
				const int32_t x3 = x[i + 3];

				y[i + 0] = vol_mult_s24(x0, vol_l);
				y[i + 1] = vol_mult_s24(x1, vol_r);
				y[i + 2] = vol_mult_s24(x2, vol_l);
				y[i + 3] = vol_mult_s24(x3, vol_r);

				peak_l = MAX(abs(x0), peak_l);
				peak_r = MAX(abs(x1), peak_r);
				peak_l = MAX(abs(x2), peak_l);
				peak_r = MAX(abs(x3), peak_r);
			}
			for (; i < n; i += 2) {
				const int32_t x0 = x[i + 0];
				const int32_t x1 = x[i + 1];

				y[i + 0] = vol_mult_s24(x0, vol_l);
				y[i + 1] = vol_mult_s24(x1, vol_r);

				peak_l = MAX(abs(x0), peak_l);
				peak_r = MAX(abs(x1), peak_r);
			}

			peak_l <<= (attenuation + PEAK_24S_32C_ADJUST);
			peak_r <<= (attenuation + PEAK_24S_32C_ADJUST);
			cd->peak_regs.peak_meter[0] = MAX(peak_l, cd->peak_regs.peak_meter[0]);
			cd->peak_regs.peak_meter[1] = MAX(peak_r, cd->peak_regs.peak_meter[1]);
		} else {
			for (int j = 0; j < nch; j++) {
				const int32_t vol = cd->volume[j];
				int32_t peak = 0;
				for (int i = j; i < n; i += nch) {
					y[i] = vol_mult_s24(x[i], vol);
					peak = MAX(abs(x[i]), peak);
				}
				peak <<= (attenuation + PEAK_24S_32C_ADJUST);
				cd->peak_regs.peak_meter[j] = MAX(peak, cd->peak_regs.peak_meter[j]);
			}
		}

		remaining_samples -= n;
		x = cir_buf_wrap(x + n, source->buf_start, source->buf_end);
		y = cir_buf_wrap(y + n, sink->buf_start, sink->buf_end);
	}
}

static void vol_passthrough_s24_to_s24(struct processing_module *mod,
				       struct cir_buf_source *source,
				       struct cir_buf_sink *sink, uint32_t frames,
				       uint32_t attenuation)
{
	struct vol_data *cd = module_get_private_data(mod);
	const int32_t *x = source->ptr;
	int32_t *y = sink->ptr;
	const int nch = cd->channels;
	int remaining_samples = frames * nch;

	while (remaining_samples) {
		const int nmax_src = cir_buf_samples_without_wrap_s32(x, source->buf_end);
		const int nmax_snk = cir_buf_samples_without_wrap_s32(y, sink->buf_end);
		int n = MIN(remaining_samples, MIN(nmax_src, nmax_snk));

		for (int j = 0; j < nch; j++) {
			int32_t peak = 0;
			for (int i = j; i < n; i += nch) {
				y[i] = x[i];
				peak = MAX(abs(x[i]), peak);
			}
			peak <<= (attenuation + PEAK_24S_32C_ADJUST);
			cd->peak_regs.peak_meter[j] = MAX(peak, cd->peak_regs.peak_meter[j]);
		}

		remaining_samples -= n;
		x = cir_buf_wrap(x + n, source->buf_start, source->buf_end);
		y = cir_buf_wrap(y + n, sink->buf_start, sink->buf_end);
	}
}
#endif /* CONFIG_FORMAT_S24LE */

#if CONFIG_FORMAT_S32LE
static inline int32_t vol_mult_s32(int32_t x, int32_t vol)
{
#if COMP_VOLUME_Q8_16
	const int64_t prod = ((int64_t)x * vol + (1LL << 15)) >> 16;
#elif COMP_VOLUME_Q1_31
	const int64_t prod = ((int64_t)x * vol + (1LL << 30)) >> 31;
#else
	const int64_t prod = ((int64_t)x * vol + (1LL << 22)) >> 23;
#endif
	return sat_clamp_q31(prod);
}

static void vol_s32_to_s32(struct processing_module *mod, struct cir_buf_source *source,
			   struct cir_buf_sink *sink, uint32_t frames, uint32_t attenuation)
{
	struct vol_data *cd = module_get_private_data(mod);
	const int32_t *x = source->ptr;
	int32_t *y = sink->ptr;
	const int nch = cd->channels;
	int remaining_samples = frames * nch;

	while (remaining_samples) {
		const int nmax_src = cir_buf_samples_without_wrap_s32(x, source->buf_end);
		const int nmax_snk = cir_buf_samples_without_wrap_s32(y, sink->buf_end);
		int n = MIN(remaining_samples, MIN(nmax_src, nmax_snk));

		if (nch == 2) {
			const int32_t vol_l = cd->volume[0];
			const int32_t vol_r = cd->volume[1];
			int32_t peak_l = 0;
			int32_t peak_r = 0;

			int i = 0;
			for (; i <= n - 4; i += 4) {
				const int32_t x0 = x[i + 0];
				const int32_t x1 = x[i + 1];
				const int32_t x2 = x[i + 2];
				const int32_t x3 = x[i + 3];

				y[i + 0] = vol_mult_s32(x0, vol_l);
				y[i + 1] = vol_mult_s32(x1, vol_r);
				y[i + 2] = vol_mult_s32(x2, vol_l);
				y[i + 3] = vol_mult_s32(x3, vol_r);

				peak_l = MAX(abs(x0), peak_l);
				peak_r = MAX(abs(x1), peak_r);
				peak_l = MAX(abs(x2), peak_l);
				peak_r = MAX(abs(x3), peak_r);
			}
			for (; i < n; i += 2) {
				const int32_t x0 = x[i + 0];
				const int32_t x1 = x[i + 1];

				y[i + 0] = vol_mult_s32(x0, vol_l);
				y[i + 1] = vol_mult_s32(x1, vol_r);

				peak_l = MAX(abs(x0), peak_l);
				peak_r = MAX(abs(x1), peak_r);
			}

			peak_l <<= attenuation;
			peak_r <<= attenuation;
			cd->peak_regs.peak_meter[0] = MAX(peak_l, cd->peak_regs.peak_meter[0]);
			cd->peak_regs.peak_meter[1] = MAX(peak_r, cd->peak_regs.peak_meter[1]);
		} else {
			for (int j = 0; j < nch; j++) {
				const int32_t vol = cd->volume[j];
				int32_t peak = 0;
				for (int i = j; i < n; i += nch) {
					y[i] = vol_mult_s32(x[i], vol);
					peak = MAX(abs(x[i]), peak);
				}
				peak <<= attenuation;
				cd->peak_regs.peak_meter[j] = MAX(peak, cd->peak_regs.peak_meter[j]);
			}
		}

		remaining_samples -= n;
		x = cir_buf_wrap(x + n, source->buf_start, source->buf_end);
		y = cir_buf_wrap(y + n, sink->buf_start, sink->buf_end);
	}
}

static void vol_passthrough_s32_to_s32(struct processing_module *mod,
				       struct cir_buf_source *source,
				       struct cir_buf_sink *sink, uint32_t frames,
				       uint32_t attenuation)
{
	struct vol_data *cd = module_get_private_data(mod);
	const int32_t *x = source->ptr;
	int32_t *y = sink->ptr;
	const int nch = cd->channels;
	int remaining_samples = frames * nch;

	while (remaining_samples) {
		const int nmax_src = cir_buf_samples_without_wrap_s32(x, source->buf_end);
		const int nmax_snk = cir_buf_samples_without_wrap_s32(y, sink->buf_end);
		int n = MIN(remaining_samples, MIN(nmax_src, nmax_snk));

		for (int j = 0; j < nch; j++) {
			int32_t peak = 0;
			for (int i = j; i < n; i += nch) {
				y[i] = x[i];
				peak = MAX(abs(x[i]), peak);
			}
			peak <<= attenuation;
			cd->peak_regs.peak_meter[j] = MAX(peak, cd->peak_regs.peak_meter[j]);
		}

		remaining_samples -= n;
		x = cir_buf_wrap(x + n, source->buf_start, source->buf_end);
		y = cir_buf_wrap(y + n, sink->buf_start, sink->buf_end);
	}
}
#endif /* CONFIG_FORMAT_S32LE */

#if CONFIG_FORMAT_FLOAT
static void vol_float_to_float(struct processing_module *mod, struct cir_buf_source *source,
			       struct cir_buf_sink *sink, uint32_t frames, uint32_t attenuation)
{
	struct vol_data *cd = module_get_private_data(mod);
	const float *x = source->ptr;
	float *y = sink->ptr;
	const int nch = cd->channels;
	int remaining_samples = frames * nch;
	float vol_f[PLATFORM_MAX_CHANNELS];

	for (int j = 0; j < nch; j++) {
#if COMP_VOLUME_Q8_16
		vol_f[j] = (float)cd->volume[j] * (1.0f / 65536.0f);
#elif COMP_VOLUME_Q1_31
		vol_f[j] = (float)cd->volume[j] * (1.0f / 2147483648.0f);
#else
		vol_f[j] = (float)cd->volume[j] * (1.0f / 8388608.0f);
#endif
	}

	if (nch == 2) {
		const float vol_l = vol_f[0];
		const float vol_r = vol_f[1];
		float peak_l = 0.0f;
		float peak_r = 0.0f;

		while (remaining_samples) {
			const int nmax_src = cir_buf_samples_without_wrap_s32(x, source->buf_end);
			const int nmax_snk = cir_buf_samples_without_wrap_s32(y, sink->buf_end);
			int n = MIN(remaining_samples, MIN(nmax_src, nmax_snk));

			int i = 0;
			for (; i <= n - 4; i += 4) {
				const float x0 = x[i + 0];
				const float x1 = x[i + 1];
				const float x2 = x[i + 2];
				const float x3 = x[i + 3];

				y[i + 0] = x0 * vol_l;
				y[i + 1] = x1 * vol_r;
				y[i + 2] = x2 * vol_l;
				y[i + 3] = x3 * vol_r;

				peak_l = MAX(fabsf(x0), peak_l);
				peak_r = MAX(fabsf(x1), peak_r);
				peak_l = MAX(fabsf(x2), peak_l);
				peak_r = MAX(fabsf(x3), peak_r);
			}
			for (; i < n; i += 2) {
				const float x0 = x[i + 0];
				const float x1 = x[i + 1];

				y[i + 0] = x0 * vol_l;
				y[i + 1] = x1 * vol_r;

				peak_l = MAX(fabsf(x0), peak_l);
				peak_r = MAX(fabsf(x1), peak_r);
			}

			remaining_samples -= n;
			x = cir_buf_wrap(x + n, source->buf_start, source->buf_end);
			y = cir_buf_wrap(y + n, sink->buf_start, sink->buf_end);
		}

		int32_t ipeak_l = (int32_t)(peak_l * 2147483647.0f);
		int32_t ipeak_r = (int32_t)(peak_r * 2147483647.0f);
		cd->peak_regs.peak_meter[0] = MAX(ipeak_l, cd->peak_regs.peak_meter[0]);
		cd->peak_regs.peak_meter[1] = MAX(ipeak_r, cd->peak_regs.peak_meter[1]);
	} else {
		while (remaining_samples) {
			const int nmax_src = cir_buf_samples_without_wrap_s32(x, source->buf_end);
			const int nmax_snk = cir_buf_samples_without_wrap_s32(y, sink->buf_end);
			int n = MIN(remaining_samples, MIN(nmax_src, nmax_snk));

			for (int j = 0; j < nch; j++) {
				const float vol = vol_f[j];
				float peak = 0.0f;
				for (int i = j; i < n; i += nch) {
					y[i] = x[i] * vol;
					peak = MAX(fabsf(x[i]), peak);
				}
				int32_t ipeak = (int32_t)(peak * 2147483647.0f);
				cd->peak_regs.peak_meter[j] = MAX(ipeak, cd->peak_regs.peak_meter[j]);
			}

			remaining_samples -= n;
			x = cir_buf_wrap(x + n, source->buf_start, source->buf_end);
			y = cir_buf_wrap(y + n, sink->buf_start, sink->buf_end);
		}
	}
}

static void vol_passthrough_float_to_float(struct processing_module *mod,
					   struct cir_buf_source *source,
					   struct cir_buf_sink *sink, uint32_t frames,
					   uint32_t attenuation)
{
	struct vol_data *cd = module_get_private_data(mod);
	const float *x = source->ptr;
	float *y = sink->ptr;
	const int nch = cd->channels;
	int remaining_samples = frames * nch;

	while (remaining_samples) {
		const int nmax_src = cir_buf_samples_without_wrap_s32(x, source->buf_end);
		const int nmax_snk = cir_buf_samples_without_wrap_s32(y, sink->buf_end);
		int n = MIN(remaining_samples, MIN(nmax_src, nmax_snk));

		for (int j = 0; j < nch; j++) {
			float peak = 0.0f;
			for (int i = j; i < n; i += nch) {
				y[i] = x[i];
				peak = MAX(fabsf(x[i]), peak);
			}
			int32_t ipeak = (int32_t)(peak * 2147483647.0f);
			cd->peak_regs.peak_meter[j] = MAX(ipeak, cd->peak_regs.peak_meter[j]);
		}

		remaining_samples -= n;
		x = cir_buf_wrap(x + n, source->buf_start, source->buf_end);
		y = cir_buf_wrap(y + n, sink->buf_start, sink->buf_end);
	}
}
#endif /* CONFIG_FORMAT_FLOAT */

const struct comp_func_map volume_func_map[] = {
#if CONFIG_FORMAT_S16LE
	{ SOF_IPC_FRAME_S16_LE, vol_s16_to_s16, vol_passthrough_s16_to_s16 },
#endif
#if CONFIG_FORMAT_S24LE
	{ SOF_IPC_FRAME_S24_4LE, vol_s24_to_s24, vol_passthrough_s24_to_s24 },
#endif
#if CONFIG_FORMAT_S32LE
	{ SOF_IPC_FRAME_S32_LE, vol_s32_to_s32, vol_passthrough_s32_to_s32 },
#endif
#if CONFIG_FORMAT_FLOAT
	{ SOF_IPC_FRAME_FLOAT, vol_float_to_float, vol_passthrough_float_to_float },
#endif
};

const size_t volume_func_count = ARRAY_SIZE(volume_func_map);

#endif /* CONFIG_COMP_PEAK_VOL */
