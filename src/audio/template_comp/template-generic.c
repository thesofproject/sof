// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2025 Intel Corporation.

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/component.h>
#include <sof/audio/sink_api.h>
#include <sof/audio/sink_source_utils.h>
#include <sof/audio/source_api.h>
#include <stdint.h>
#include "template.h"

#if CONFIG_FORMAT_S16LE
/**
 * template_comp_s16() - Process S16_LE format.
 * @mod: Pointer to module data.
 * @source: Source for PCM samples data.
 * @sink: Sink for PCM samples data.
 * @frames: Number of audio data frames to process.
 *
 * This is the processing function for 16-bit signed integer PCM formats. The
 * audio samples in every frame are re-order to channels order defined in
 * component data channel_map[].
 *
 * Return: Value zero for success, otherwise an error code.
 */
static int template_comp_s16(const struct processing_module *mod,
			     struct sof_source *source,
			     struct sof_sink *sink,
			     uint32_t frames)
{
	struct template_comp_comp_data *cd = module_get_private_data(mod);
	int16_t const *x, *x_start, *x_end;
	int16_t *y, *y_start, *y_end;
	int x_size, y_size;
	int source_samples_without_wrap;
	int samples_without_wrap;
	int samples = frames * cd->channels;
	int bytes = frames * cd->frame_bytes;
	int ret;
	int ch;
	int i;

	/* Get pointer to source data in circular buffer, get buffer start and size to
	 * check for wrap. The size in bytes is converted to number of s16 samples to
	 * control the samples process loop. If the number of bytes requested is not
	 * possible, an error is returned.
	 */
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
	while (samples) {
		/* Find out samples to process before first wrap or end of data. */
		source_samples_without_wrap = x_end - x;
		samples_without_wrap = y_end - y;
		samples_without_wrap = MIN(samples_without_wrap, source_samples_without_wrap);
		samples_without_wrap = MIN(samples_without_wrap, samples);

		/* Since the example processing is for frames of audio channels, process
		 * with step of channels count.
		 */
		for (i = 0; i < samples_without_wrap; i += cd->channels) {
			/* In inner loop process the frame. As example re-arrange the channels
			 * as defined in array channel_map[].
			 */
			for (ch = 0; ch < cd->channels; ch++) {
				*y = *(x + cd->channel_map[ch]);
				y++;
			}
			x += cd->channels;
		}

		/* One of the buffers needs a wrap (or end of data), so check for wrap */
		x = (x >= x_end) ? x - x_size : x;
		y = (y >= y_end) ? y - y_size : y;

		/* Update processed samples count for next loop iteration. */
		samples -= samples_without_wrap;
	}

	/* Update the source and sink for bytes consumed and produced. Return success. */
	source_release_data(source, bytes);
	sink_commit_buffer(sink, bytes);
	return 0;
}
#endif /* CONFIG_FORMAT_S16LE */

#if CONFIG_FORMAT_S32LE || CONFIG_FORMAT_S24LE
/**
 * template_comp_s32() - Process S32_LE or S24_4LE format.
 * @mod: Pointer to module data.
 * @source: Source for PCM samples data.
 * @sink: Sink for PCM samples data.
 * @frames: Number of audio data frames to process.
 *
 * Processing function for signed integer 32-bit PCM formats. The same
 * function works for s24 and s32 formats since the samples values are
 * not modified in computation. The audio samples in every frame are
 * re-order to channels order defined in component data channel_map[].
 *
 * Return: Value zero for success, otherwise an error code.
 */
static int template_comp_s32(const struct processing_module *mod,
			     struct sof_source *source,
			     struct sof_sink *sink,
			     uint32_t frames)
{
	struct template_comp_comp_data *cd = module_get_private_data(mod);
	int32_t const *x, *x_start, *x_end;
	int32_t *y, *y_start, *y_end;
	int x_size, y_size;
	int source_samples_without_wrap;
	int samples_without_wrap;
	int samples = frames * cd->channels;
	int bytes = frames * cd->frame_bytes;
	int ret;
	int ch;
	int i;

	/* Get pointer to source data in circular buffer, get buffer start and size to
	 * check for wrap. The size in bytes is converted to number of s16 samples to
	 * control the samples process loop. If the number of bytes requested is not
	 * possible, an error is returned.
	 */
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
	while (samples) {
		/* Find out samples to process before first wrap or end of data. */
		source_samples_without_wrap = x_end - x;
		samples_without_wrap = y_end - y;
		samples_without_wrap = MIN(samples_without_wrap, source_samples_without_wrap);
		samples_without_wrap = MIN(samples_without_wrap, samples);

		/* Since the example processing is for frames of audio channels, process
		 * with step of channels count.
		 */
		for (i = 0; i < samples_without_wrap; i += cd->channels) {
			/* In inner loop process the frame. As example re-arrange the channels
			 * as defined in array channel_map[].
			 */
			for (ch = 0; ch < cd->channels; ch++) {
				*y = *(x + cd->channel_map[ch]);
				y++;
			}
			x += cd->channels;
		}

		/* One of the buffers needs a wrap (or end of data), so check for wrap */
		x = (x >= x_end) ? x - x_size : x;
		y = (y >= y_end) ? y - y_size : y;

		/* Update processed samples count for next loop iteration. */
		samples -= samples_without_wrap;
	}

	/* Update the source and sink for bytes consumed and produced. Return success. */
	source_release_data(source, bytes);
	sink_commit_buffer(sink, bytes);
	return 0;
}
#endif /* CONFIG_FORMAT_S32LE || CONFIG_FORMAT_S24LE */

/* This struct array defines the used processing functions for
 * the PCM formats
 */
const struct template_comp_proc_fnmap template_comp_proc_fnmap[] = {
#if CONFIG_FORMAT_S16LE
	{ SOF_IPC_FRAME_S16_LE, template_comp_s16 },
#endif
#if CONFIG_FORMAT_S24LE
	{ SOF_IPC_FRAME_S24_4LE, template_comp_s32 },
#endif
#if CONFIG_FORMAT_S32LE
	{ SOF_IPC_FRAME_S32_LE, template_comp_s32 },
#endif
};

/**
 * template_comp_find_proc_func() - Find suitable processing function.
 * @src_fmt: Enum value for PCM format.
 *
 * This function finds the suitable processing function to use for
 * the used PCM format. If not found, return NULL.
 *
 * Return: Pointer to processing function for the requested PCM format.
 */
template_comp_func template_comp_find_proc_func(enum sof_ipc_frame src_fmt)
{
	int i;

	/* Find suitable processing function from map */
	for (i = 0; i < ARRAY_SIZE(template_comp_proc_fnmap); i++)
		if (src_fmt == template_comp_proc_fnmap[i].frame_fmt)
			return template_comp_proc_fnmap[i].template_comp_proc_func;

	return NULL;
}
