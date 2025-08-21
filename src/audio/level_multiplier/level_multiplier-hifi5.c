// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2025 Intel Corporation.

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/component.h>
#include <sof/audio/sink_api.h>
#include <sof/audio/sink_source_utils.h>
#include <sof/audio/source_api.h>
#include <stdint.h>
#include "level_multiplier.h"

#define LEVEL_MULTIPLIER_S32_SHIFT	8	/* See explanation from level_multiplier_s32() */

#if SOF_USE_MIN_HIFI(5, VOLUME)

#include <xtensa/tie/xt_hifi3.h>

#if CONFIG_FORMAT_S16LE
/**
 * level_multiplier_s16() - Process S16_LE format.
 * @mod: Pointer to module data.
 * @source: Source for PCM samples data.
 * @sink: Sink for PCM samples data.
 * @frames: Number of audio data frames to process.
 *
 * This is the processing function for 16-bit signed integer PCM formats. The
 * audio samples are copied from source to sink with gain defined in cd->gain.
 *
 * Return: Value zero for success, otherwise an error code.
 */
static int level_multiplier_s16(const struct processing_module *mod,
				struct sof_source *source,
				struct sof_sink *sink,
				uint32_t frames)
{
	struct level_multiplier_comp_data *cd = module_get_private_data(mod);
	ae_valignx2 x_align;
	ae_valignx2 y_align = AE_ZALIGN128();
	ae_f32x2 tmp0;
	ae_f32x2 tmp1;
	const ae_f32x2 gain = cd->gain;
	ae_f16x4 samples0;
	ae_f16x4 samples1;
	ae_int16x8 const *x;
	ae_int16x8 *y;
	int16_t const *x_start, *x_end;
	int16_t *y_start, *y_end;
	int x_size, y_size;
	int source_samples_without_wrap;
	int samples_without_wrap;
	int remaining_samples = frames * cd->channels;
	int bytes = frames * cd->frame_bytes;
	int ret;
	int n, i;

	ret = source_get_data_s16(source, bytes, (const int16_t **)&x, &x_start, &x_size);
	if (ret)
		return ret;

	/* Similarly get pointer to sink data in circular buffer, buffer start and size. */
	ret = sink_get_buffer_s16(sink, bytes, (int16_t **)&y, &y_start, &y_size);
	if (ret)
		return ret;

	/* Set helper pointers to buffer end for wrap check. Then loop until all
	 * samples are processed.
	 */
	x_end = x_start + x_size;
	y_end = y_start + y_size;
	while (remaining_samples) {
		/* Find out samples to process before first wrap or end of data. */
		source_samples_without_wrap = x_end - (int16_t *)x;
		samples_without_wrap = y_end - (int16_t *)y;
		samples_without_wrap = MIN(samples_without_wrap, source_samples_without_wrap);
		samples_without_wrap = MIN(samples_without_wrap, remaining_samples);
		x_align = AE_LA128_PP(x);

		/* Process with 128 bit loads and stores */
		n = samples_without_wrap >> 3;
		for (i = 0; i < n; i++) {
			AE_LA16X4X2_IP(samples0, samples1, x_align, x);

			AE_MULF2P32X16X4RS(tmp0, tmp1, gain, gain, samples0);
			/* Q9.23 to Q1.31 */
			tmp0 = AE_SLAI32S(tmp0, 8);
			tmp1 = AE_SLAI32S(tmp1, 8);
			samples0 = AE_ROUND16X4F32SSYM(tmp0, tmp1);

			AE_MULF2P32X16X4RS(tmp0, tmp1, gain, gain, samples1);
			/* Q9.23 to Q1.31 */
			tmp0 = AE_SLAI32S(tmp0, 8);
			tmp1 = AE_SLAI32S(tmp1, 8);
			samples1 = AE_ROUND16X4F32SSYM(tmp0, tmp1);

			AE_SA16X4X2_IP(samples0, samples1, y_align, y);
		}

		AE_SA128POS_FP(y_align, y);
		n = samples_without_wrap - (n << 3);
		for (i = 0; i < n; i++) {
			AE_L16_IP(samples0, (ae_f16 *)x, sizeof(ae_f16));
			tmp0 = AE_MULFP32X16X2RS_H(gain, samples0);
			tmp0 = AE_SLAI32S(tmp0, 8);
			samples0 = AE_ROUND16X4F32SSYM(tmp0, tmp0);
			AE_S16_0_IP(samples0, (ae_f16 *)y, sizeof(ae_f16));
		}

		/* One of the buffers needs a wrap (or end of data), so check for wrap */
		x = (x >= (ae_int16x8 *)x_end) ? x - x_size : x;
		y = (y >= (ae_int16x8 *)y_end) ? y - y_size : y;
		remaining_samples -= samples_without_wrap;
	}

	/* Update the source and sink for bytes consumed and produced. Return success. */
	source_release_data(source, bytes);
	sink_commit_buffer(sink, bytes);
	return 0;
}
#endif /* CONFIG_FORMAT_S16LE */

#if CONFIG_FORMAT_S24LE
/**
 * level_multiplier_s24() - Process S24_4LE format.
 * @mod: Pointer to module data.
 * @source: Source for PCM samples data.
 * @sink: Sink for PCM samples data.
 * @frames: Number of audio data frames to process.
 *
 * This is the processing function for 24-bit signed integer PCM formats. The
 * audio samples are copied from source to sink with gain defined in cd->gain.
 *
 * Return: Value zero for success, otherwise an error code.
 */
static int level_multiplier_s24(const struct processing_module *mod,
				struct sof_source *source,
				struct sof_sink *sink,
				uint32_t frames)
{
	struct level_multiplier_comp_data *cd = module_get_private_data(mod);
	ae_valignx2 x_align;
	ae_valignx2 y_align = AE_ZALIGN128();
	const ae_f32x2 gain = cd->gain;
	ae_f32x2 samples0;
	ae_f32x2 samples1;
	ae_f32x2 tmp0;
	ae_f32x2 tmp1;
	ae_int32x4 const *x;
	ae_int32x4 *y;
	int32_t const *x_start, *x_end;
	int32_t *y_start, *y_end;
	int x_size, y_size;
	int source_samples_without_wrap;
	int samples_without_wrap;
	int remaining_samples = frames * cd->channels;
	int bytes = frames * cd->frame_bytes;
	int ret;
	int n, i;

	ret = source_get_data_s32(source, bytes, (const int32_t **)&x, &x_start, &x_size);
	if (ret)
		return ret;

	/* Similarly get pointer to sink data in circular buffer, buffer start and size. */
	ret = sink_get_buffer_s32(sink, bytes, (int32_t **)&y, &y_start, &y_size);
	if (ret)
		return ret;

	/* Set helper pointers to buffer end for wrap check. Then loop until all
	 * samples are processed.
	 */
	x_end = x_start + x_size;
	y_end = y_start + y_size;
	while (remaining_samples) {
		/* Find out samples to process before first wrap or end of data. */
		source_samples_without_wrap = x_end - (int32_t *)x;
		samples_without_wrap = y_end - (int32_t *)y;
		samples_without_wrap = MIN(samples_without_wrap, source_samples_without_wrap);
		samples_without_wrap = MIN(samples_without_wrap, remaining_samples);
		x_align = AE_LA128_PP(x);

		/* Process with 64 bit loads and stores */
		n = samples_without_wrap >> 2;
		for (i = 0; i < n; i++) {
			AE_LA32X2X2_IP(samples0, samples1, x_align, x);
			AE_MULF2P32X4RS(tmp0, tmp1, gain, gain,
					AE_SLAI32(samples0, 8),
					AE_SLAI32(samples1, 8));
			samples0 = AE_SRAI32(AE_SLAI32S(tmp0, 8), 8);
			samples1 = AE_SRAI32(AE_SLAI32S(tmp1, 8), 8);
			AE_SA32X2X2_IP(samples0, samples1, y_align, y);
		}

		AE_SA128POS_FP(y_align, y);
		n = samples_without_wrap - (n << 2);
		for (i = 0; i < n; i++) {
			AE_L32_IP(samples0, (ae_f32 *)x, sizeof(ae_f32));
			samples0 = AE_MULFP32X2RS(gain, AE_SLAI32(samples0, 8));
			samples0 = AE_SRAI32(AE_SLAI32S(samples0, 8), 8);
			AE_S32_L_IP(samples0, (ae_f32 *)y, sizeof(ae_f32));
		}

		/* One of the buffers needs a wrap (or end of data), so check for wrap */
		x = (x >= (ae_int32x4 *)x_end) ? x - x_size : x;
		y = (y >= (ae_int32x4 *)y_end) ? y - y_size : y;
		remaining_samples -= samples_without_wrap;
	}

	/* Update the source and sink for bytes consumed and produced. Return success. */
	source_release_data(source, bytes);
	sink_commit_buffer(sink, bytes);
	return 0;
}
#endif /* CONFIG_FORMAT_S24LE */

#if CONFIG_FORMAT_S32LE
/**
 * level_multiplier_s32() - Process S32_LE format.
 * @mod: Pointer to module data.
 * @source: Source for PCM samples data.
 * @sink: Sink for PCM samples data.
 * @frames: Number of audio data frames to process.
 *
 * This is the processing function for 32-bit signed integer PCM formats. The
 * audio samples are copied from source to sink with gain defined in cd->gain.
 *
 * Return: Value zero for success, otherwise an error code.
 */
static int level_multiplier_s32(const struct processing_module *mod,
				struct sof_source *source,
				struct sof_sink *sink,
				uint32_t frames)
{
	struct level_multiplier_comp_data *cd = module_get_private_data(mod);
	ae_valignx2 x_align;
	ae_valignx2 y_align = AE_ZALIGN128();
	ae_f64 mult0;
	ae_f64 mult1;
	const ae_f32x2 gain = cd->gain;
	ae_f32x2 samples0;
	ae_f32x2 samples1;
	ae_int32x4 const *x;
	ae_int32x4 *y;
	int32_t const *x_start, *x_end;
	int32_t *y_start, *y_end;
	int x_size, y_size;
	int source_samples_without_wrap;
	int samples_without_wrap;
	int remaining_samples = frames * cd->channels;
	int bytes = frames * cd->frame_bytes;
	int ret;
	int n, i;

	ret = source_get_data_s32(source, bytes, (const int32_t **)&x, &x_start, &x_size);
	if (ret)
		return ret;

	/* Similarly get pointer to sink data in circular buffer, buffer start and size. */
	ret = sink_get_buffer_s32(sink, bytes, (int32_t **)&y, &y_start, &y_size);
	if (ret)
		return ret;

	/* Set helper pointers to buffer end for wrap check. Then loop until all
	 * samples are processed.
	 */
	x_end = x_start + x_size;
	y_end = y_start + y_size;
	while (remaining_samples) {
		/* Find out samples to process before first wrap or end of data. */
		source_samples_without_wrap = x_end - (int32_t *)x;
		samples_without_wrap = y_end - (int32_t *)y;
		samples_without_wrap = MIN(samples_without_wrap, source_samples_without_wrap);
		samples_without_wrap = MIN(samples_without_wrap, remaining_samples);
		x_align = AE_LA128_PP(x);

		/* Process with 64 bit loads and stores */
		n = samples_without_wrap >> 2;
		for (i = 0; i < n; i++) {
			AE_LA32X2X2_IP(samples0, samples1, x_align, x);

			AE_MULF32X2R_HH_LL(mult0, mult1, gain, samples0);
			mult0 = AE_SLAI64(mult0, LEVEL_MULTIPLIER_S32_SHIFT);
			mult1 = AE_SLAI64(mult1, LEVEL_MULTIPLIER_S32_SHIFT);
			samples0 = AE_ROUND32X2F48SSYM(mult0, mult1);	/* Q2.47 -> Q1.31 */

			AE_MULF32X2R_HH_LL(mult0, mult1, gain, samples1);
			mult0 = AE_SLAI64(mult0, LEVEL_MULTIPLIER_S32_SHIFT);
			mult1 = AE_SLAI64(mult1, LEVEL_MULTIPLIER_S32_SHIFT);
			samples1 = AE_ROUND32X2F48SSYM(mult0, mult1); /* Q2.47 -> Q1.31 */

			AE_SA32X2X2_IP(samples0, samples1, y_align, y);
		}

		AE_SA128POS_FP(y_align, y);
		n = samples_without_wrap - (n << 2);
		for (i = 0; i < n; i++) {
			AE_L32_IP(samples0, (ae_f32 *)x, sizeof(ae_f32));
			mult0 = AE_MULF32R_HH(gain, samples0);
			mult0 = AE_SLAI64(mult0, LEVEL_MULTIPLIER_S32_SHIFT);
			samples0 = AE_ROUND32F48SSYM(mult0);
			AE_S32_L_IP(samples0, (ae_f32 *)y, sizeof(ae_f32));
		}

		/* One of the buffers needs a wrap (or end of data), so check for wrap */
		x = (x >= (ae_int32x4 *)x_end) ? x - x_size : x;
		y = (y >= (ae_int32x4 *)y_end) ? y - y_size : y;
		remaining_samples -= samples_without_wrap;
	}

	/* Update the source and sink for bytes consumed and produced. Return success. */
	source_release_data(source, bytes);
	sink_commit_buffer(sink, bytes);
	return 0;
}
#endif /* CONFIG_FORMAT_S32LE */

/* This struct array defines the used processing functions for
 * the PCM formats
 */
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
};

/**
 * level_multiplier_find_proc_func() - Find suitable processing function.
 * @src_fmt: Enum value for PCM format.
 *
 * This function finds the suitable processing function to use for
 * the used PCM format. If not found, return NULL.
 *
 * Return: Pointer to processing function for the requested PCM format.
 */
level_multiplier_func level_multiplier_find_proc_func(enum sof_ipc_frame src_fmt)
{
	int i;

	/* Find suitable processing function from map */
	for (i = 0; i < ARRAY_SIZE(level_multiplier_proc_fnmap); i++)
		if (src_fmt == level_multiplier_proc_fnmap[i].frame_fmt)
			return level_multiplier_proc_fnmap[i].level_multiplier_proc_func;

	return NULL;
}

#endif /* SOF_USE_MIN_HIFI(5, VOLUME) */
