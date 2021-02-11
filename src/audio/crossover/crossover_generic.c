// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Google LLC. All rights reserved.
//
// Author: Sebastiano Carlucci <scarlucci@google.com>

#include <stdint.h>
#include <sof/audio/format.h>
#include <sof/audio/eq_iir/iir.h>
#include <sof/audio/component.h>
#include <sof/audio/format.h>
#include <sof/audio/crossover/crossover.h>

/*
 * \brief Splits x into two based on the coefficients set in the lp
 *        and hp filters. The output of the lp is in y1, the output of
 *        the hp is in y2.
 *
 * As a side effect, this function mutates the delay values of both
 * filters.
 */
static inline void crossover_generic_lr4_split(struct iir_state_df2t *lp,
					       struct iir_state_df2t *hp,
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
static inline void crossover_generic_lr4_merge(struct iir_state_df2t *lp,
					       struct iir_state_df2t *hp,
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

#if CONFIG_FORMAT_S16LE
static void crossover_s16_default_pass(const struct comp_dev *dev,
				       const struct comp_buffer *source,
				       struct comp_buffer *sinks[],
				       int32_t num_sinks,
				       uint32_t frames)
{
	const struct audio_stream *source_stream = &source->stream;
	int16_t *x;
	int32_t *y;
	int i, j;
	int n = source_stream->channels * frames;

	for (i = 0; i < n; i++) {
		x = audio_stream_read_frag_s16(source_stream, i);
		for (j = 0; j < num_sinks; j++) {
			if (!sinks[j])
				continue;
			y = audio_stream_read_frag_s16((&sinks[j]->stream), i);
			*y = *x;
		}
	}
}
#endif /* CONFIG_FORMAT_S16LE */

#if CONFIG_FORMAT_S24LE || CONFIG_FORMAT_S32LE
static void crossover_s32_default_pass(const struct comp_dev *dev,
				       const struct comp_buffer *source,
				       struct comp_buffer *sinks[],
				       int32_t num_sinks,
				       uint32_t frames)
{
	const struct audio_stream *source_stream = &source->stream;
	int32_t *x, *y;
	int i, j;
	int n = source_stream->channels * frames;

	for (i = 0; i < n; i++) {
		x = audio_stream_read_frag_s32(source_stream, i);
		for (j = 0; j < num_sinks; j++) {
			if (!sinks[j])
				continue;
			y = audio_stream_read_frag_s32((&sinks[j]->stream), i);
			*y = *x;
		}
	}
}
#endif /* CONFIG_FORMAT_S24LE || CONFIG_FORMAT_S32LE */

#if CONFIG_FORMAT_S16LE
static void crossover_s16_default(const struct comp_dev *dev,
				  const struct comp_buffer *source,
				  struct comp_buffer *sinks[],
				  int32_t num_sinks,
				  uint32_t frames)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct crossover_state *state;
	const struct audio_stream *source_stream = &source->stream;
	struct audio_stream *sink_stream;
	int16_t *x, *y;
	int ch, i, j;
	int idx;
	int nch = source_stream->channels;
	int32_t out[num_sinks];

	for (ch = 0; ch < nch; ch++) {
		idx = ch;
		state = &cd->state[ch];
		for (i = 0; i < frames; i++) {
			x = audio_stream_read_frag_s16(source_stream, idx);
			cd->crossover_split(*x << 16, out, state);

			for (j = 0; j < num_sinks; j++) {
				if (!sinks[j])
					continue;
				sink_stream = &sinks[j]->stream;
				y = audio_stream_read_frag_s16(sink_stream,
							       idx);
				*y = sat_int16(Q_SHIFT_RND(out[j], 31, 15));
			}

			idx += nch;
		}
	}
}
#endif /* CONFIG_FORMAT_S16LE */

#if CONFIG_FORMAT_S24LE
static void crossover_s24_default(const struct comp_dev *dev,
				  const struct comp_buffer *source,
				  struct comp_buffer *sinks[],
				  int32_t num_sinks,
				  uint32_t frames)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct crossover_state *state;
	const struct audio_stream *source_stream = &source->stream;
	struct audio_stream *sink_stream;
	int32_t *x, *y;
	int ch, i, j;
	int idx;
	int nch = source_stream->channels;
	int32_t out[num_sinks];

	for (ch = 0; ch < nch; ch++) {
		idx = ch;
		state = &cd->state[ch];
		for (i = 0; i < frames; i++) {
			x = audio_stream_read_frag_s32(source_stream, idx);
			cd->crossover_split(*x << 8, out, state);

			for (j = 0; j < num_sinks; j++) {
				if (!sinks[j])
					continue;
				sink_stream = &sinks[j]->stream;
				y = audio_stream_read_frag_s32(sink_stream,
							       idx);
				*y = sat_int24(Q_SHIFT_RND(out[j], 31, 23));
			}

			idx += nch;
		}
	}
}
#endif /* CONFIG_FORMAT_S24LE */

#if CONFIG_FORMAT_S32LE
static void crossover_s32_default(const struct comp_dev *dev,
				  const struct comp_buffer *source,
				  struct comp_buffer *sinks[],
				  int32_t num_sinks,
				  uint32_t frames)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct crossover_state *state;
	const struct audio_stream *source_stream = &source->stream;
	struct audio_stream *sink_stream;
	int32_t *x, *y;
	int ch, i, j;
	int idx;
	int nch = source_stream->channels;
	int32_t out[num_sinks];

	for (ch = 0; ch < nch; ch++) {
		idx = ch;
		state = &cd->state[0];
		for (i = 0; i < frames; i++) {
			x = audio_stream_read_frag_s32(source_stream, idx);
			cd->crossover_split(*x, out, state);

			for (j = 0; j < num_sinks; j++) {
				if (!sinks[j])
					continue;
				sink_stream = &sinks[j]->stream;
				y = audio_stream_read_frag_s32(sink_stream,
							       idx);
				*y = out[j];
			}

			idx += nch;
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
	{ SOF_IPC_FRAME_S16_LE, crossover_s16_default_pass },
#endif /* CONFIG_FORMAT_S16LE */

#if CONFIG_FORMAT_S24LE
	{ SOF_IPC_FRAME_S24_4LE, crossover_s32_default_pass },
#endif /* CONFIG_FORMAT_S24LE */

#if CONFIG_FORMAT_S32LE
	{ SOF_IPC_FRAME_S32_LE, crossover_s32_default_pass },
#endif /* CONFIG_FORMAT_S32LE */
};

const size_t crossover_proc_fncount = ARRAY_SIZE(crossover_proc_fnmap);

const crossover_split crossover_split_fnmap[] = {
	crossover_generic_split_2way,
	crossover_generic_split_3way,
	crossover_generic_split_4way,
};

const size_t crossover_split_fncount = ARRAY_SIZE(crossover_split_fnmap);
