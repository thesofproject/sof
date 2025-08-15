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

#define LEVEL_MULTIPLIER_S16_SHIFT	Q_SHIFT_BITS_32(15, LEVEL_MULTIPLIER_QXY_Y, 15)
#define LEVEL_MULTIPLIER_S24_SHIFT	Q_SHIFT_BITS_64(23, LEVEL_MULTIPLIER_QXY_Y, 23)
#define LEVEL_MULTIPLIER_S32_SHIFT	Q_SHIFT_BITS_64(31, LEVEL_MULTIPLIER_QXY_Y, 31)

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
	const int32_t gain = cd->gain;
	int16_t const *x, *x_start, *x_end;
	int16_t *y, *y_start, *y_end;
	int x_size, y_size;
	int source_samples_without_wrap;
	int samples_without_wrap;
	int remaining_samples = frames * cd->channels;
	int bytes = frames * cd->frame_bytes;
	int ret;
	int i;

	ret = source_get_data_s16(source, bytes, &x, &x_start, &x_size);
	if (ret)
		return ret;

	/* Similarly get pointer to sink data in circular buffer, buffer start and size. */
	ret = sink_get_buffer_s16(sink, bytes, &y, &y_start, &y_size);
	if (ret)
		return ret;

	/* Set helper pointers to buffer end for wrap check. Then loop until all
	 * samples are processed.
	 */
	x_end = x_start + x_size;
	y_end = y_start + y_size;
	while (remaining_samples) {
		/* Find out samples to process before first wrap or end of data. */
		source_samples_without_wrap = x_end - x;
		samples_without_wrap = y_end - y;
		samples_without_wrap = MIN(samples_without_wrap, source_samples_without_wrap);
		samples_without_wrap = MIN(samples_without_wrap, remaining_samples);
		for (i = 0; i < samples_without_wrap; i++) {
			*y = q_multsr_sat_32x32_16(*x, gain, LEVEL_MULTIPLIER_S16_SHIFT);
			x++;
			y++;
		}

		/* One of the buffers needs a wrap (or end of data), so check for wrap */
		x = (x >= x_end) ? x - x_size : x;
		y = (y >= y_end) ? y - y_size : y;

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
	const int32_t gain = cd->gain;
	int32_t const *x, *x_start, *x_end;
	int32_t *y, *y_start, *y_end;
	int x_size, y_size;
	int source_samples_without_wrap;
	int samples_without_wrap;
	int remaining_samples = frames * cd->channels;
	int bytes = frames * cd->frame_bytes;
	int ret;
	int i;

	ret = source_get_data_s32(source, bytes, &x, &x_start, &x_size);
	if (ret)
		return ret;

	/* Similarly get pointer to sink data in circular buffer, buffer start and size. */
	ret = sink_get_buffer_s32(sink, bytes, &y, &y_start, &y_size);
	if (ret)
		return ret;

	/* Set helper pointers to buffer end for wrap check. Then loop until all
	 * samples are processed.
	 */
	x_end = x_start + x_size;
	y_end = y_start + y_size;
	while (remaining_samples) {
		/* Find out samples to process before first wrap or end of data. */
		source_samples_without_wrap = x_end - x;
		samples_without_wrap = y_end - y;
		samples_without_wrap = MIN(samples_without_wrap, source_samples_without_wrap);
		samples_without_wrap = MIN(samples_without_wrap, remaining_samples);
		for (i = 0; i < samples_without_wrap; i++) {
			*y = q_multsr_sat_32x32_24(sign_extend_s24(*x), gain,
						   LEVEL_MULTIPLIER_S24_SHIFT);
			x++;
			y++;
		}

		/* One of the buffers needs a wrap (or end of data), so check for wrap */
		x = (x >= x_end) ? x - x_size : x;
		y = (y >= y_end) ? y - y_size : y;

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
	const int32_t gain = cd->gain;
	int32_t const *x, *x_start, *x_end;
	int32_t *y, *y_start, *y_end;
	int x_size, y_size;
	int source_samples_without_wrap;
	int samples_without_wrap;
	int remaining_samples = frames * cd->channels;
	int bytes = frames * cd->frame_bytes;
	int ret;
	int i;

	ret = source_get_data_s32(source, bytes, &x, &x_start, &x_size);
	if (ret)
		return ret;

	/* Similarly get pointer to sink data in circular buffer, buffer start and size. */
	ret = sink_get_buffer_s32(sink, bytes, &y, &y_start, &y_size);
	if (ret)
		return ret;

	/* Set helper pointers to buffer end for wrap check. Then loop until all
	 * samples are processed.
	 */
	x_end = x_start + x_size;
	y_end = y_start + y_size;
	while (remaining_samples) {
		/* Find out samples to process before first wrap or end of data. */
		source_samples_without_wrap = x_end - x;
		samples_without_wrap = y_end - y;
		samples_without_wrap = MIN(samples_without_wrap, source_samples_without_wrap);
		samples_without_wrap = MIN(samples_without_wrap, remaining_samples);
		for (i = 0; i < samples_without_wrap; i++) {
			*y = q_multsr_sat_32x32(*x, gain, LEVEL_MULTIPLIER_S32_SHIFT);
			x++;
			y++;
		}

		/* One of the buffers needs a wrap (or end of data), so check for wrap */
		x = (x >= x_end) ? x - x_size : x;
		y = (y >= y_end) ? y - y_size : y;

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
