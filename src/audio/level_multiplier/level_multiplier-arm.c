// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Sound Open Firmware. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/component.h>
#include <sof/audio/sink_api.h>
#include <sof/audio/sink_source_utils.h>
#include <sof/audio/source_api.h>
#include <sof/audio/arm_simd.h>
#include <stdint.h>
#include "level_multiplier.h"

#define LEVEL_MULTIPLIER_S16_SHIFT	Q_SHIFT_BITS_64(15, LEVEL_MULTIPLIER_QXY_Y, 15)
#define LEVEL_MULTIPLIER_S24_SHIFT	Q_SHIFT_BITS_64(23, LEVEL_MULTIPLIER_QXY_Y, 23)
#define LEVEL_MULTIPLIER_S32_SHIFT	Q_SHIFT_BITS_64(31, LEVEL_MULTIPLIER_QXY_Y, 31)

#if SOF_USE_ARM_SIMD(VOLUME)

#if CONFIG_FORMAT_S16LE
static int level_multiplier_s16(const struct processing_module *mod,
				struct sof_source *source,
				struct sof_sink *sink,
				uint32_t frames)
{
	struct level_multiplier_comp_data *cd = module_get_private_data(mod);
	int16_t const *x, *x_start, *x_end;
	int16_t *y, *y_start, *y_end;
	int x_size, y_size;
	int source_samples_without_wrap;
	int samples_without_wrap;
	int remaining_samples = frames * cd->channels;
	int bytes = frames * cd->frame_bytes;
	const int32_t gain = cd->gain;
	int ret, n, i;

	ret = source_get_data_s16(source, bytes, &x, &x_start, &x_size);
	if (ret)
		return ret;

	ret = sink_get_buffer_s16(sink, bytes, &y, &y_start, &y_size);
	if (ret)
		return ret;

	x_end = x_start + x_size;
	y_end = y_start + y_size;

	while (remaining_samples) {
		source_samples_without_wrap = x_end - x;
		samples_without_wrap = y_end - y;
		samples_without_wrap = MIN(samples_without_wrap, source_samples_without_wrap);
		samples_without_wrap = MIN(samples_without_wrap, remaining_samples);

		n = samples_without_wrap & ~3;
		for (i = 0; i < n; i += 4) {
			int32_t s0 = arm_sat_s16((int32_t)(((int64_t)x[i + 0] * gain) >> LEVEL_MULTIPLIER_S16_SHIFT));
			int32_t s1 = arm_sat_s16((int32_t)(((int64_t)x[i + 1] * gain) >> LEVEL_MULTIPLIER_S16_SHIFT));
			int32_t s2 = arm_sat_s16((int32_t)(((int64_t)x[i + 2] * gain) >> LEVEL_MULTIPLIER_S16_SHIFT));
			int32_t s3 = arm_sat_s16((int32_t)(((int64_t)x[i + 3] * gain) >> LEVEL_MULTIPLIER_S16_SHIFT));

			*(uint32_t *)(y + i) = arm_pkhbt(s0, s1, 16);
			*(uint32_t *)(y + i + 2) = arm_pkhbt(s2, s3, 16);
		}
		for (; i < samples_without_wrap; i++) {
			y[i] = arm_sat_s16((int32_t)(((int64_t)x[i] * gain) >> LEVEL_MULTIPLIER_S16_SHIFT));
		}

		remaining_samples -= samples_without_wrap;
		x += samples_without_wrap;
		y += samples_without_wrap;
		if (x == x_end)
			x = x_start;
		if (y == y_end)
			y = y_start;
	}

	source_release_data(source, bytes);
	sink_commit_buffer(sink, bytes);
	return 0;
}
#endif /* CONFIG_FORMAT_S16LE */

#if CONFIG_FORMAT_S24LE
static int level_multiplier_s24(const struct processing_module *mod,
				struct sof_source *source,
				struct sof_sink *sink,
				uint32_t frames)
{
	struct level_multiplier_comp_data *cd = module_get_private_data(mod);
	int32_t const *x, *x_start, *x_end;
	int32_t *y, *y_start, *y_end;
	int x_size, y_size;
	int source_samples_without_wrap;
	int samples_without_wrap;
	int remaining_samples = frames * cd->channels;
	int bytes = frames * cd->frame_bytes;
	const int32_t gain = cd->gain;
	int ret, n, i;

	ret = source_get_data_s32(source, bytes, &x, &x_start, &x_size);
	if (ret)
		return ret;

	ret = sink_get_buffer_s32(sink, bytes, &y, &y_start, &y_size);
	if (ret)
		return ret;

	x_end = x_start + x_size;
	y_end = y_start + y_size;

	while (remaining_samples) {
		source_samples_without_wrap = x_end - x;
		samples_without_wrap = y_end - y;
		samples_without_wrap = MIN(samples_without_wrap, source_samples_without_wrap);
		samples_without_wrap = MIN(samples_without_wrap, remaining_samples);

		n = samples_without_wrap & ~3;
		for (i = 0; i < n; i += 4) {
			y[i + 0] = arm_sat_s24((int32_t)(((int64_t)sign_extend_s24(x[i + 0]) * gain) >> LEVEL_MULTIPLIER_S24_SHIFT));
			y[i + 1] = arm_sat_s24((int32_t)(((int64_t)sign_extend_s24(x[i + 1]) * gain) >> LEVEL_MULTIPLIER_S24_SHIFT));
			y[i + 2] = arm_sat_s24((int32_t)(((int64_t)sign_extend_s24(x[i + 2]) * gain) >> LEVEL_MULTIPLIER_S24_SHIFT));
			y[i + 3] = arm_sat_s24((int32_t)(((int64_t)sign_extend_s24(x[i + 3]) * gain) >> LEVEL_MULTIPLIER_S24_SHIFT));
		}
		for (; i < samples_without_wrap; i++) {
			y[i] = arm_sat_s24((int32_t)(((int64_t)sign_extend_s24(x[i]) * gain) >> LEVEL_MULTIPLIER_S24_SHIFT));
		}

		remaining_samples -= samples_without_wrap;
		x += samples_without_wrap;
		y += samples_without_wrap;
		if (x == x_end)
			x = x_start;
		if (y == y_end)
			y = y_start;
	}

	source_release_data(source, bytes);
	sink_commit_buffer(sink, bytes);
	return 0;
}
#endif /* CONFIG_FORMAT_S24LE */

#if CONFIG_FORMAT_S32LE
static int level_multiplier_s32(const struct processing_module *mod,
				struct sof_source *source,
				struct sof_sink *sink,
				uint32_t frames)
{
	struct level_multiplier_comp_data *cd = module_get_private_data(mod);
	int32_t const *x, *x_start, *x_end;
	int32_t *y, *y_start, *y_end;
	int x_size, y_size;
	int source_samples_without_wrap;
	int samples_without_wrap;
	int remaining_samples = frames * cd->channels;
	int bytes = frames * cd->frame_bytes;
	const int32_t gain = cd->gain;
	int ret, n, i;

	ret = source_get_data_s32(source, bytes, &x, &x_start, &x_size);
	if (ret)
		return ret;

	ret = sink_get_buffer_s32(sink, bytes, &y, &y_start, &y_size);
	if (ret)
		return ret;

	x_end = x_start + x_size;
	y_end = y_start + y_size;

	while (remaining_samples) {
		source_samples_without_wrap = x_end - x;
		samples_without_wrap = y_end - y;
		samples_without_wrap = MIN(samples_without_wrap, source_samples_without_wrap);
		samples_without_wrap = MIN(samples_without_wrap, remaining_samples);

		n = samples_without_wrap & ~3;
		for (i = 0; i < n; i += 4) {
			int64_t p0 = (int64_t)x[i + 0] * gain;
			int64_t p1 = (int64_t)x[i + 1] * gain;
			int64_t p2 = (int64_t)x[i + 2] * gain;
			int64_t p3 = (int64_t)x[i + 3] * gain;

			y[i + 0] = arm_sat_q31(p0 >> LEVEL_MULTIPLIER_S32_SHIFT);
			y[i + 1] = arm_sat_q31(p1 >> LEVEL_MULTIPLIER_S32_SHIFT);
			y[i + 2] = arm_sat_q31(p2 >> LEVEL_MULTIPLIER_S32_SHIFT);
			y[i + 3] = arm_sat_q31(p3 >> LEVEL_MULTIPLIER_S32_SHIFT);
		}
		for (; i < samples_without_wrap; i++) {
			int64_t p = (int64_t)x[i] * gain;
			y[i] = arm_sat_q31(p >> LEVEL_MULTIPLIER_S32_SHIFT);
		}

		remaining_samples -= samples_without_wrap;
		x += samples_without_wrap;
		y += samples_without_wrap;
		if (x == x_end)
			x = x_start;
		if (y == y_end)
			y = y_start;
	}

	source_release_data(source, bytes);
	sink_commit_buffer(sink, bytes);
	return 0;
}
#endif /* CONFIG_FORMAT_S32LE */

#if CONFIG_FORMAT_FLOAT
static int level_multiplier_float(const struct processing_module *mod,
				  struct sof_source *source,
				  struct sof_sink *sink,
				  uint32_t frames)
{
	struct level_multiplier_comp_data *cd = module_get_private_data(mod);
	float const *x, *x_start, *x_end;
	float *y, *y_start, *y_end;
	int x_size, y_size;
	int source_samples_without_wrap;
	int samples_without_wrap;
	int remaining_samples = frames * cd->channels;
	int bytes = frames * cd->frame_bytes;
	const float gain = cd->gain_f;
	int ret, n, i;

	ret = source_get_data_s32(source, bytes, (int32_t const **)&x, (int32_t const **)&x_start, &x_size);
	if (ret)
		return ret;

	ret = sink_get_buffer_s32(sink, bytes, (int32_t **)&y, (int32_t **)&y_start, &y_size);
	if (ret)
		return ret;

	x_end = x_start + x_size;
	y_end = y_start + y_size;

	while (remaining_samples) {
		source_samples_without_wrap = x_end - x;
		samples_without_wrap = y_end - y;
		samples_without_wrap = MIN(samples_without_wrap, source_samples_without_wrap);
		samples_without_wrap = MIN(samples_without_wrap, remaining_samples);

		n = samples_without_wrap & ~3;
		for (i = 0; i < n; i += 4) {
			y[i + 0] = x[i + 0] * gain;
			y[i + 1] = x[i + 1] * gain;
			y[i + 2] = x[i + 2] * gain;
			y[i + 3] = x[i + 3] * gain;
		}
		for (; i < samples_without_wrap; i++) {
			y[i] = x[i] * gain;
		}

		remaining_samples -= samples_without_wrap;
		x += samples_without_wrap;
		y += samples_without_wrap;
		if (x == x_end)
			x = x_start;
		if (y == y_end)
			y = y_start;
	}

	source_release_data(source, bytes);
	sink_commit_buffer(sink, bytes);
	return 0;
}
#endif /* CONFIG_FORMAT_FLOAT */

const struct level_multiplier_proc_fnmap level_multiplier_proc_fnmap[] = {
#if CONFIG_FORMAT_S16LE
	{ SOF_IPC_FRAME_S16_LE, level_multiplier_s16 },
#endif
#if CONFIG_FORMAT_S24LE
	{ SOF_IPC_FRAME_S24_4LE, level_multiplier_s24 },
#endif
#if CONFIG_FORMAT_S32LE
	{ SOF_IPC_FRAME_S32_LE, level_multiplier_s32 },
#endif
#if CONFIG_FORMAT_FLOAT
	{ SOF_IPC_FRAME_FLOAT, level_multiplier_float },
#endif
};

level_multiplier_func level_multiplier_find_proc_func(enum sof_ipc_frame src_fmt)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(level_multiplier_proc_fnmap); i++)
		if (src_fmt == level_multiplier_proc_fnmap[i].frame_fmt)
			return level_multiplier_proc_fnmap[i].level_multiplier_proc_func;

	return NULL;
}

#endif /* SOF_USE_ARM_SIMD(VOLUME) */
