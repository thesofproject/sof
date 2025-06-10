// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2025 Intel Corporation.

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/component.h>
#include <sof/audio/sink_api.h>
#include <sof/audio/sink_source_utils.h>
#include <sof/audio/source_api.h>
#include <sof/math/log.h>
#include <sof/math/iir_df1.h>
#include <user/eq.h>
#include <stdbool.h>
#include <stdint.h>

#include "sound_dose.h"

LOG_MODULE_DECLARE(sound_dose, CONFIG_SOF_LOG_LEVEL);

static void sound_dose_calculate_mel(const struct processing_module *mod, int frames)
{
	struct sound_dose_comp_data *cd = module_get_private_data(mod);
	uint64_t energy_sum;
	uint32_t log_arg;
	int32_t tmp;
	int ch;

	cd->frames_count += frames;
	if (cd->frames_count < cd->report_count)
		return;

	cd->frames_count = 0;
	energy_sum = 0;
	for (ch = 0; ch < cd->channels; ch++) {
		energy_sum += cd->energy[ch];
		cd->energy[ch] = 0;
	}

	/* Log2 argument is Q32.0 unsigned, log2 returns Q16.16 signed.
	 * Energy is Qx.30, so the argument 2^30 times scaled. Also to keep
	 * argument within uint32_t range, need to scale it down by 19.
	 * To compensate these, need to add 19 (Q16.16) and subtract 30 (Q16.16)
	 * from logarithm value.
	 */
	log_arg = (uint32_t)(energy_sum >> SOUND_DOSE_ENERGY_SHIFT);
	log_arg = MAX(log_arg, 1);
	tmp = base2_logarithm(log_arg);
	tmp += SOUND_DOSE_LOG_FIXED_OFFSET; /* Compensate Q2.30 and energy shift */
	tmp += cd->log_offset_for_mean; /* logarithm subtract for mean */
	tmp = Q_MULTSR_32X32((int64_t)tmp, SOUND_DOSE_TEN_OVER_LOG2_10_Q29,
			     SOUND_DOSE_LOGOFFS_Q, SOUND_DOSE_LOGMULT_Q, SOUND_DOSE_LOGOFFS_Q);
	cd->level_dbfs = tmp + SOUND_DOSE_WEIGHT_FILTERS_OFFS_Q16 + SOUND_DOSE_DFBS_OFFS_Q16;

	/* If stereo sum channel level values and subtract 3 dB, to generalize
	 * For stereo or more subtract -1.5 dB per channel.
	 */
	if (cd->channels > 1)
		cd->level_dbfs += cd->channels * SOUND_DOSE_MEL_CHANNELS_SUM_FIX;

	sound_dose_report_mel(mod);
}

#if CONFIG_FORMAT_S16LE

/**
 * sound_dose_s16() - Process S16_LE format.
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
static int sound_dose_s16(const struct processing_module *mod,
			  struct sof_source *source,
			  struct sof_sink *sink,
			  uint32_t frames)
{
	struct sound_dose_comp_data *cd = module_get_private_data(mod);
	struct iir_state_df1 *iir;
	int32_t weighted;
	int16_t sample;
	int16_t const *x0, *x, *x_start, *x_end;
	int16_t *y0, *y, *y_start, *y_end;
	int x_size, y_size;
	int source_samples_without_wrap;
	int samples_without_wrap;
	int samples = frames * cd->channels;
	int bytes = frames * cd->frame_bytes;
	int ret;
	int ch;
	int i;
	const int channels = cd->channels;

	/* Get pointer to source data in circular buffer, get buffer start and size to
	 * check for wrap. The size in bytes is converted to number of s16 samples to
	 * control the samples process loop. If the number of bytes requested is not
	 * possible, an error is returned.
	 */
	ret = source_get_data_s16(source, bytes, &x0, &x_start, &x_size);
	if (ret)
		return ret;

	/* Similarly get pointer to sink data in circular buffer, buffer start and size. */
	ret = sink_get_buffer_s16(sink, bytes, &y0, &y_start, &y_size);
	if (ret)
		return ret;

	/* Set helper pointers to buffer end for wrap check. Then loop until all
	 * samples are processed.
	 */
	x_end = x_start + x_size;
	y_end = y_start + y_size;
	while (samples) {
		/* Find out samples to process before first wrap or end of data. */
		source_samples_without_wrap = x_end - x0;
		samples_without_wrap = y_end - y0;
		samples_without_wrap = MIN(samples_without_wrap, source_samples_without_wrap);
		samples_without_wrap = MIN(samples_without_wrap, samples);
		for (ch = 0; ch < cd->channels; ch++) {
			iir = &cd->iir[ch];
			x = x0++;
			y = y0++;
			for (i = 0; i < samples_without_wrap; i += channels) {
				sample = sat_int16(Q_MULTSR_32X32((int64_t)cd->gain, *x,
								  SOUND_DOSE_GAIN_Q,
								  SOUND_DOSE_S16_Q,
								  SOUND_DOSE_S16_Q));
				*y = sample;
				x += channels;
				y += channels;
				weighted = iir_df1(iir, ((int32_t)sample) << 16) >> 16;

				/* Update sound dose, energy is Q1.15 * Q1.15 --> Q2.30 */
				cd->energy[ch] += weighted * weighted;
			}
		}

		/* One of the buffers needs a wrap (or end of data), so check for wrap */
		x0 += samples_without_wrap;
		y0 += samples_without_wrap;
		x0 = (x0 >= x_end) ? x0 - x_size : x0;
		y0 = (y0 >= y_end) ? y0 - y_size : y0;

		/* Update processed samples count for next loop iteration. */
		samples -= samples_without_wrap;
	}

	/* Update the source and sink for bytes consumed and produced. Return success. */
	source_release_data(source, bytes);
	sink_commit_buffer(sink, bytes);

	sound_dose_calculate_mel(mod, frames);
	return 0;
}
#endif /* CONFIG_FORMAT_S16LE */

#if CONFIG_FORMAT_S32LE || CONFIG_FORMAT_S24LE

/**
 * sound_dose_s32() - Process S32_LE or S24_4LE format.
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
static int sound_dose_s32(const struct processing_module *mod,
			  struct sof_source *source,
			  struct sof_sink *sink,
			  uint32_t frames)
{
	struct sound_dose_comp_data *cd = module_get_private_data(mod);
	struct iir_state_df1 *iir;
	int32_t sample;
	int32_t const *x0, *x, *x_start, *x_end;
	int32_t *y0, *y, *y_start, *y_end;
	int32_t weighted;
	int x_size, y_size;
	int source_samples_without_wrap;
	int samples_without_wrap;
	int samples = frames * cd->channels;
	int bytes = frames * cd->frame_bytes;
	int ret;
	int ch;
	int i;
	const int channels = cd->channels;

	/* Get pointer to source data in circular buffer, get buffer start and size to
	 * check for wrap. The size in bytes is converted to number of s32 samples to
	 * control the samples process loop. If the number of bytes requested is not
	 * possible, an error is returned.
	 */
	ret = source_get_data_s32(source, bytes, &x0, &x_start, &x_size);
	if (ret)
		return ret;

	/* Similarly get pointer to sink data in circular buffer, buffer start and size. */
	ret = sink_get_buffer_s32(sink, bytes, &y0, &y_start, &y_size);
	if (ret)
		return ret;

	/* Set helper pointers to buffer end for wrap check. Then loop until all
	 * samples are processed.
	 */
	x_end = x_start + x_size;
	y_end = y_start + y_size;
	while (samples) {
		/* Find out samples to process before first wrap or end of data. */
		source_samples_without_wrap = x_end - x0;
		samples_without_wrap = y_end - y0;
		samples_without_wrap = MIN(samples_without_wrap, source_samples_without_wrap);
		samples_without_wrap = MIN(samples_without_wrap, samples);
		for (ch = 0; ch < cd->channels; ch++) {
			iir = &cd->iir[ch];
			x = x0++;
			y = y0++;
			for (i = 0; i < samples_without_wrap; i += channels) {
				sample = sat_int32(Q_MULTSR_32X32((int64_t)cd->gain, *x,
								  SOUND_DOSE_GAIN_Q,
								  SOUND_DOSE_S32_Q,
								  SOUND_DOSE_S32_Q));
				*y = sample;
				x += channels;
				y += channels;
				weighted = iir_df1(iir, sample) >> 16;

				/* Update sound dose, energy is Q1.15 * Q1.15 --> Q2.30 */
				cd->energy[ch] += weighted * weighted;
			}
		}

		/* One of the buffers needs a wrap (or end of data), so check for wrap */
		x0 += samples_without_wrap;
		y0 += samples_without_wrap;
		x0 = (x0 >= x_end) ? x0 - x_size : x0;
		y0 = (y0 >= y_end) ? y0 - y_size : y0;

		/* Update processed samples count for next loop iteration. */
		samples -= samples_without_wrap;
	}

	/* Update the source and sink for bytes consumed and produced. Return success. */
	source_release_data(source, bytes);
	sink_commit_buffer(sink, bytes);

	sound_dose_calculate_mel(mod, frames);
	return 0;
}
#endif /* CONFIG_FORMAT_S32LE || CONFIG_FORMAT_S24LE */

/* This struct array defines the used processing functions for
 * the PCM formats
 */
const struct sound_dose_proc_fnmap sound_dose_proc_fnmap[] = {
#if CONFIG_FORMAT_S16LE
	{ SOF_IPC_FRAME_S16_LE, sound_dose_s16},
#endif
#if CONFIG_FORMAT_S24LE
	{ SOF_IPC_FRAME_S24_4LE, sound_dose_s32},
#endif
#if CONFIG_FORMAT_S32LE
	{ SOF_IPC_FRAME_S32_LE, sound_dose_s32},
#endif
};

/**
 * sound_dose_find_proc_func() - Find suitable processing function.
 * @src_fmt: Enum value for PCM format.
 *
 * This function finds the suitable processing function to use for
 * the used PCM format. If not found, return NULL.
 *
 * Return: Pointer to processing function for the requested PCM format.
 */
sound_dose_func sound_dose_find_proc_func(enum sof_ipc_frame src_fmt)
{
	int i;

	/* Find suitable processing function from map */
	for (i = 0; i < ARRAY_SIZE(sound_dose_proc_fnmap); i++)
		if (src_fmt == sound_dose_proc_fnmap[i].frame_fmt)
			return sound_dose_proc_fnmap[i].sound_dose_proc_func;

	return NULL;
}
