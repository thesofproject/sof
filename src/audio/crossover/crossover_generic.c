// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Google LLC. All rights reserved.
//
// Author: Sebastiano Carlucci <scarlucci@google.com>

#include <ipc/stream.h>
#include <sof/audio/module_adapter/module/module_interface.h>
#include <sof/audio/component.h>
#include <sof/audio/format.h>
#include <sof/audio/sink_api.h>
#include <sof/audio/source_api.h>
#include <sof/audio/sink_source_utils.h>
#include <sof/common.h>
#include <sof/math/iir_df1.h>
#include <stddef.h>
#include <stdint.h>

#include "crossover.h"

/*
 * \brief Splits x into two based on the coefficients set in the lp
 *        and hp filters. The output of the lp is in y1, the output of
 *        the hp is in y2.
 *
 * As a side effect, this function mutates the delay values of both
 * filters.
 */
static inline void crossover_generic_lr4_split(struct iir_state_df1 *lp,
					       struct iir_state_df1 *hp,
					       int32_t x, int32_t *y1,
					       int32_t *y2)
{
	*y1 = crossover_generic_process_lr4(x, lp);
	*y2 = crossover_generic_process_lr4(x, hp);
}

/*
 * \brief Splits input signal into two and merges it back to it's
 *        original form.
 *
 * With 3-way crossovers, one output goes through only one LR4 filter,
 * whereas the other two go through two LR4 filters. This causes the signals
 * to be out of phase. We need to pass the signal through another set of LR4
 * filters to align back the phase.
 */
static inline void crossover_generic_lr4_merge(struct iir_state_df1 *lp,
					       struct iir_state_df1 *hp,
					       int32_t x, int32_t *y)
{
	int32_t z1, z2;

	z1 = crossover_generic_process_lr4(x, lp);
	z2 = crossover_generic_process_lr4(x, hp);
	*y = sat_int32(((int64_t)z1) + z2);
}

static void crossover_generic_split_2way(int32_t in,
					 int32_t out[],
					 struct crossover_state *state)
{
	crossover_generic_lr4_split(&state->lowpass[0], &state->highpass[0],
				    in, &out[0], &out[1]);
}

static void crossover_generic_split_3way(int32_t in,
					 int32_t out[],
					 struct crossover_state *state)
{
	int32_t z1, z2;

	crossover_generic_lr4_split(&state->lowpass[0], &state->highpass[0],
				    in, &z1, &z2);
	/* Realign the phase of z1 */
	crossover_generic_lr4_merge(&state->lowpass[1], &state->highpass[1],
				    z1, &out[0]);
	crossover_generic_lr4_split(&state->lowpass[2], &state->highpass[2],
				    z2, &out[1], &out[2]);
}

static void crossover_generic_split_4way(int32_t in,
					 int32_t out[],
					 struct crossover_state *state)
{
	int32_t z1, z2;

	crossover_generic_lr4_split(&state->lowpass[1], &state->highpass[1],
				    in, &z1, &z2);
	crossover_generic_lr4_split(&state->lowpass[0], &state->highpass[0],
				    z1, &out[0], &out[1]);
	crossover_generic_lr4_split(&state->lowpass[2], &state->highpass[2],
				    z2, &out[2], &out[3]);
}

/*
 * \brief Passthrough copy of the source samples to every assigned sink.
 *
 * Used when the component runs without a configuration blob. The source
 * data are copied to each active sink without being freed (free == false)
 * so that the same input is replicated to all sinks; the source is
 * released once after the last copy.
 *
 * \return 0 on success, negative error code otherwise.
 */
static int crossover_default_pass(struct comp_data *cd,
				  struct sof_source *source,
				  struct sof_sink **sinks,
				  int32_t num_sinks,
				  uint32_t frames)
{
	size_t bytes = frames * source_get_frame_bytes(source);
	int ret;
	int i;

	for (i = 0; i < num_sinks; i++) {
		if (!sinks[i])
			continue;
		ret = source_to_sink_copy(source, sinks[i], false, bytes);
		if (ret)
			return ret;
	}

	return source_release_data(source, bytes);
}

#if CONFIG_FORMAT_S16LE
static int crossover_s16_default(struct comp_data *cd,
				 struct sof_source *source,
				 struct sof_sink **sinks,
				 int32_t num_sinks,
				 uint32_t frames)
{
	int16_t *y[SOF_CROSSOVER_MAX_STREAMS];
	int16_t *y_start[SOF_CROSSOVER_MAX_STREAMS];
	int16_t *y_end[SOF_CROSSOVER_MAX_STREAMS];
	int out_idx[SOF_CROSSOVER_MAX_STREAMS];
	int32_t out[SOF_CROSSOVER_MAX_STREAMS];
	struct crossover_state *state;
	int16_t const *x, *x_start, *x_end;
	int x_samples, y_samples;
	int nch = source_get_channels(source);
	size_t bytes = frames * source_get_frame_bytes(source);
	int active_sinks = 0;
	int remaining_samples;
	int ch, i, j, n;
	int ret;

	ret = source_get_data_s16(source, bytes, &x, &x_start, &x_samples);
	if (ret)
		return ret;
	x_end = x_start + x_samples;

	for (j = 0; j < num_sinks; j++) {
		if (!sinks[j])
			continue;
		ret = sink_get_buffer_s16(sinks[j], bytes, &y[active_sinks],
					  &y_start[active_sinks], &y_samples);
		if (ret) {
			while (active_sinks--) {
				sink_commit_buffer(sinks[out_idx[active_sinks]], 0);
			}
			source_release_data(source, 0);
			return ret;
		}
		out_idx[active_sinks] = j;
		active_sinks++;
	}

	remaining_samples = frames * nch;
	while (remaining_samples) {
		n = MIN(remaining_samples, cir_buf_samples_without_wrap_s16(x, x_end));
		for (j = 0; j < active_sinks; j++)
			n = MIN(n, cir_buf_samples_without_wrap_s16(y[j], y_end[j]));

		for (ch = 0; ch < nch; ch++) {
			state = &cd->state[ch];
			for (i = ch; i < n; i += nch) {
				cd->crossover_split((int32_t)x[i] << 16, out, state);
				for (j = 0; j < active_sinks; j++)
					y[j][i] = sat_int16(Q_SHIFT_RND(out[out_idx[j]], 31, 15));
			}
		}

		remaining_samples -= n;
		x = cir_buf_wrap(x + n, x_start, x_end);
		for (j = 0; j < active_sinks; j++)
			y[j] = cir_buf_wrap(y[j] + n, y_start[j], y_end[j]);
	}

	ret = source_release_data(source, bytes);
	if (ret)
		return ret;
	for (j = 0; j < active_sinks; j++) {
		ret = sink_commit_buffer(sinks[out_idx[j]], bytes);
		if (ret)
			return ret;
	}

	return 0;
}
#endif /* CONFIG_FORMAT_S16LE */

#if CONFIG_FORMAT_S24LE
static int crossover_s24_default(struct comp_data *cd,
				 struct sof_source *source,
				 struct sof_sink **sinks,
				 int32_t num_sinks,
				 uint32_t frames)
{
	int32_t *y[SOF_CROSSOVER_MAX_STREAMS];
	int32_t *y_start[SOF_CROSSOVER_MAX_STREAMS];
	int32_t *y_end[SOF_CROSSOVER_MAX_STREAMS];
	int out_idx[SOF_CROSSOVER_MAX_STREAMS];
	int32_t out[SOF_CROSSOVER_MAX_STREAMS];
	struct crossover_state *state;
	int32_t const *x, *x_start, *x_end;
	int x_samples, y_samples;
	int nch = source_get_channels(source);
	size_t bytes = frames * source_get_frame_bytes(source);
	int active_sinks = 0;
	int remaining_samples;
	int ch, i, j, n;
	int ret;

	ret = source_get_data_s32(source, bytes, &x, &x_start, &x_samples);
	if (ret)
		return ret;
	x_end = x_start + x_samples;

	for (j = 0; j < num_sinks; j++) {
		if (!sinks[j])
			continue;
		ret = sink_get_buffer_s32(sinks[j], bytes, &y[active_sinks],
					  &y_start[active_sinks], &y_samples);
		if (ret) {
			while (active_sinks--) {
				sink_commit_buffer(sinks[out_idx[active_sinks]], 0);
			}
			source_release_data(source, 0);
			return ret;
		}
		out_idx[active_sinks] = j;
		active_sinks++;
	}

	remaining_samples = frames * nch;
	while (remaining_samples) {
		n = MIN(remaining_samples, cir_buf_samples_without_wrap_s32(x, x_end));
		for (j = 0; j < active_sinks; j++)
			n = MIN(n, cir_buf_samples_without_wrap_s32(y[j], y_end[j]));

		for (ch = 0; ch < nch; ch++) {
			state = &cd->state[ch];
			for (i = ch; i < n; i += nch) {
				cd->crossover_split(x[i] << 8, out, state);
				for (j = 0; j < active_sinks; j++)
					y[j][i] = sat_int24(Q_SHIFT_RND(out[out_idx[j]], 31, 23));
			}
		}

		remaining_samples -= n;
		x = cir_buf_wrap(x + n, x_start, x_end);
		for (j = 0; j < active_sinks; j++)
			y[j] = cir_buf_wrap(y[j] + n, y_start[j], y_end[j]);
	}

	ret = source_release_data(source, bytes);
	if (ret)
		return ret;
	for (j = 0; j < active_sinks; j++) {
		ret = sink_commit_buffer(sinks[out_idx[j]], bytes);
		if (ret)
			return ret;
	}

	return 0;
}
#endif /* CONFIG_FORMAT_S24LE */

#if CONFIG_FORMAT_S32LE
/**
 * \brief Processes audio frames with a crossover filter for s32 format.
 *
 * This function reads the interleaved audio data from the source, applies
 * the crossover split for each channel, and writes the per-band results to
 * the assigned sinks using the sink circular buffer API.
 *
 * \param cd Pointer to the component data structure which holds the crossover state.
 * \param source Source handle to read audio data from.
 * \param sinks Array of sink handles, indexed by band; entries may be NULL.
 * \param num_sinks Number of entries in the sinks array.
 * \param frames Number of audio frames to process.
 * \return 0 on success, negative error code otherwise.
 */
static int crossover_s32_default(struct comp_data *cd,
				 struct sof_source *source,
				 struct sof_sink **sinks,
				 int32_t num_sinks,
				 uint32_t frames)
{
	int32_t *y[SOF_CROSSOVER_MAX_STREAMS];
	int32_t *y_start[SOF_CROSSOVER_MAX_STREAMS];
	int32_t *y_end[SOF_CROSSOVER_MAX_STREAMS];
	int out_idx[SOF_CROSSOVER_MAX_STREAMS];
	int32_t out[SOF_CROSSOVER_MAX_STREAMS];
	struct crossover_state *state;
	int32_t const *x, *x_start, *x_end;
	int x_samples, y_samples;
	int nch = source_get_channels(source);
	size_t bytes = frames * source_get_frame_bytes(source);
	int active_sinks = 0;
	int remaining_samples;
	int ch, i, j, n;
	int ret;

	ret = source_get_data_s32(source, bytes, &x, &x_start, &x_samples);
	if (ret)
		return ret;
	x_end = x_start + x_samples;

	/* Identify active sinks, keeping the band index for correct routing */
	for (j = 0; j < num_sinks; j++) {
		if (!sinks[j])
			continue;
		ret = sink_get_buffer_s32(sinks[j], bytes, &y[active_sinks],
					  &y_start[active_sinks], &y_samples);
		if (ret) {
			while (active_sinks--) {
				sink_commit_buffer(sinks[out_idx[active_sinks]], 0);
			}
			source_release_data(source, 0);
			return ret;
		}
		y_end[active_sinks] = y_start[active_sinks] + y_samples;
		out_idx[active_sinks] = j;
		active_sinks++;
	}

	/* Process the largest contiguous block that avoids wrapping in the
	 * source and every active sink, then advance/wrap once per block.
	 */
	remaining_samples = frames * nch;
	while (remaining_samples) {
		n = MIN(remaining_samples, cir_buf_samples_without_wrap_s32(x, x_end));
		for (j = 0; j < active_sinks; j++)
			n = MIN(n, cir_buf_samples_without_wrap_s32(y[j], y_end[j]));

		for (ch = 0; ch < nch; ch++) {
			state = &cd->state[ch];
			for (i = ch; i < n; i += nch) {
				cd->crossover_split(x[i], out, state);
				for (j = 0; j < active_sinks; j++)
					y[j][i] = out[out_idx[j]];
			}
		}

		remaining_samples -= n;
		x = cir_buf_wrap(x + n, x_start, x_end);
		for (j = 0; j < active_sinks; j++)
			y[j] = cir_buf_wrap(y[j] + n, y_start[j], y_end[j]);
	}

	ret = source_release_data(source, bytes);
	if (ret)
		return ret;
	for (j = 0; j < active_sinks; j++) {
		ret = sink_commit_buffer(sinks[out_idx[j]], bytes);
		if (ret)
			return ret;
	}

	return 0;
}
#endif /* CONFIG_FORMAT_S32LE */

const struct crossover_proc_fnmap crossover_proc_fnmap[] = {
/* { SOURCE_FORMAT , PROCESSING FUNCTION } */
#if CONFIG_FORMAT_S16LE
	{ SOF_IPC_FRAME_S16_LE, crossover_s16_default },
#endif /* CONFIG_FORMAT_S16LE */

#if CONFIG_FORMAT_S24LE
	{ SOF_IPC_FRAME_S24_4LE, crossover_s24_default },
#endif /* CONFIG_FORMAT_S24LE */

#if CONFIG_FORMAT_S32LE
	{ SOF_IPC_FRAME_S32_LE, crossover_s32_default },
#endif /* CONFIG_FORMAT_S32LE */
};

const struct crossover_proc_fnmap crossover_proc_fnmap_pass[] = {
/* { SOURCE_FORMAT , PROCESSING FUNCTION } */
#if CONFIG_FORMAT_S16LE
	{ SOF_IPC_FRAME_S16_LE, crossover_default_pass },
#endif /* CONFIG_FORMAT_S16LE */

#if CONFIG_FORMAT_S24LE
	{ SOF_IPC_FRAME_S24_4LE, crossover_default_pass },
#endif /* CONFIG_FORMAT_S24LE */

#if CONFIG_FORMAT_S32LE
	{ SOF_IPC_FRAME_S32_LE, crossover_default_pass },
#endif /* CONFIG_FORMAT_S32LE */
};

const size_t crossover_proc_fncount = ARRAY_SIZE(crossover_proc_fnmap);

const crossover_split crossover_split_fnmap[] = {
	crossover_generic_split_2way,
	crossover_generic_split_3way,
	crossover_generic_split_4way,
};

const size_t crossover_split_fncount = ARRAY_SIZE(crossover_split_fnmap);
