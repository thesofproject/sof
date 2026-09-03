// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Google LLC. All rights reserved.
//
// Author: Sebastiano Carlucci <scarlucci@google.com>

#include <ipc/stream.h>
#include <sof/audio/module_adapter/module/module_interface.h>
#include <sof/audio/component.h>
#include <sof/audio/format.h>
#include <sof/math/iir_df1.h>
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

static void crossover_default_pass(struct comp_data *cd,
				   struct input_stream_buffer *bsource,
				   struct output_stream_buffer **bsinks,
				   int32_t num_sinks,
				   uint32_t frames)
{
	const struct audio_stream *source_stream = bsource->data;
	uint32_t samples = audio_stream_get_channels(source_stream) * frames;
	int i;

	for (i = 0; i < num_sinks; i++) {
		if (!bsinks[i])
			continue;
		audio_stream_copy(source_stream, 0, bsinks[i]->data, 0, samples);
	}
}

#if CONFIG_FORMAT_S16LE
static void crossover_s16_default(struct comp_data *cd,
				  struct input_stream_buffer *bsource,
				  struct output_stream_buffer **bsinks,
				  int32_t num_sinks,
				  uint32_t frames)
{
	struct crossover_state *state;
	const struct audio_stream *source_stream = bsource->data;
	struct audio_stream *sink_stream;
	int16_t *x, *y;
	int ch, i, j;
	int idx;
	int nch = audio_stream_get_channels(source_stream);
	int32_t out[num_sinks];

	for (ch = 0; ch < nch; ch++) {
		idx = ch;
		state = &cd->state[ch];
		for (i = 0; i < frames; i++) {
			x = audio_stream_read_frag_s16(source_stream, idx);
			cd->crossover_split(*x << 16, out, state);

			for (j = 0; j < num_sinks; j++) {
				if (!bsinks[j])
					continue;
				sink_stream = bsinks[j]->data;
				y = audio_stream_write_frag_s16(sink_stream,
								idx);
				*y = sat_int16(Q_SHIFT_RND(out[j], 31, 15));
			}

			idx += nch;
		}
	}
}
#endif /* CONFIG_FORMAT_S16LE */

#if CONFIG_FORMAT_S24LE
static void crossover_s24_default(struct comp_data *cd,
				  struct input_stream_buffer *bsource,
				  struct output_stream_buffer **bsinks,
				  int32_t num_sinks,
				  uint32_t frames)
{
	struct crossover_state *state;
	const struct audio_stream *source_stream = bsource->data;
	struct audio_stream *sink_stream;
	int32_t *x, *y;
	int ch, i, j;
	int idx;
	int nch = audio_stream_get_channels(source_stream);
	int32_t out[num_sinks];

	for (ch = 0; ch < nch; ch++) {
		idx = ch;
		state = &cd->state[ch];
		for (i = 0; i < frames; i++) {
			x = audio_stream_read_frag_s32(source_stream, idx);
			cd->crossover_split(*x << 8, out, state);

			for (j = 0; j < num_sinks; j++) {
				if (!bsinks[j])
					continue;
				sink_stream = bsinks[j]->data;
				y = audio_stream_write_frag_s32(sink_stream,
								idx);
				*y = sat_int24(Q_SHIFT_RND(out[j], 31, 23));
			}

			idx += nch;
		}
	}
}
#endif /* CONFIG_FORMAT_S24LE */

#if CONFIG_FORMAT_S32LE
/**
 * \brief Processes audio frames with a crossover filter for s32 format.
 *
 * This function divides audio data from an input stream into multiple output
 * streams based on a crossover filter. It reads the input audio data, applies
 * the crossover filter, and writes the processed audio data to active output
 * streams.
 *
 * \param cd Pointer to the component data structure which holds the crossover state.
 * \param bsource Pointer to the input stream buffer structure.
 * \param bsinks Array of pointers to output stream buffer structures.
 * \param num_sinks Number of output stream buffers in the bsinks array.
 * \param frames Number of audio frames to process.
 */
static void crossover_s32_default(struct comp_data *cd,
				  struct input_stream_buffer *bsource,
				  struct output_stream_buffer **bsinks,
				  int32_t num_sinks,
				  uint32_t frames)
{
	/* Array to hold active sink streams; initialized to null */
	struct audio_stream *sink_stream[SOF_CROSSOVER_MAX_STREAMS] = { NULL };
	struct crossover_state *state;
	/* Source stream to read audio data from */
	const struct audio_stream *source_stream = bsource->data;
	int32_t *x, *y;
	int ch, i, j;
	int idx;
	/* Counter for active sink streams */
	int active_sinks = 0;
	/* Number of channels in the source stream */
	int nch = audio_stream_get_channels(source_stream);
	/* Output buffer for processed data */
	int32_t out[num_sinks];

	/* Identify active sinks, avoid processing null sinks later */
	for (j = 0; j < num_sinks; j++) {
		if (bsinks[j])
			sink_stream[active_sinks++] = bsinks[j]->data;
	}

	/* Process for each channel */
	/* Loop through each channel in the source stream */
	for (ch = 0; ch < nch; ch++) {
		/* Set current crossover state for this channel */
		state = &cd->state[ch];
		/* Iterate over frames */
		/* Loop through each frame */
		for (i = 0, idx = ch; i < frames; i++, idx += nch) {
			/* Read source */
			/* Read the current audio frame for the channel */
			x = audio_stream_read_frag_s32(source_stream, idx);
			/* Apply the crossover split logic to the audio data */
			cd->crossover_split(*x, out, state);

			/* Write output to active sinks */
			/* Write processed output to active sinks */
			for (j = 0; j < active_sinks; j++) {
				/* Write processed data to sink */
				y = audio_stream_write_frag_s32(sink_stream[j], idx);
				*y = out[j];
			}

		}
	}
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
