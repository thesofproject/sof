// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Intel Corporation. All rights reserved.
//
// Author: Pin-chih Lin <johnylin@google.com>

#include <stdint.h>
#include <sof/audio/drc/drc_algorithm.h>
#include <sof/audio/format.h>
#include <sof/audio/eq_iir/iir.h>
#include <sof/audio/multiband_drc/multiband_drc.h>
#include <sof/math/iir_df2t.h>

static void multiband_drc_default_pass(const struct comp_dev *dev,
				       const struct audio_stream *source,
				       struct audio_stream *sink,
				       uint32_t frames)
{
	audio_stream_copy(source, 0, sink, 0, source->channels * frames);
}

static void multiband_drc_process_emp_crossover(struct multiband_drc_state *state,
						crossover_split split_func,
						int32_t *buf_src,
						int32_t *buf_sink,
						int enable_emp,
						int nch,
						int nband)
{
	struct iir_state_df2t *emp_s;
	struct crossover_state *crossover_s;
	int32_t *buf_sink_band;
	int ch, band;
	int32_t emp_out;
	int32_t crossover_out[nband];

	for (ch = 0; ch < nch; ch++) {
		emp_s = &state->emphasis[ch];
		crossover_s = &state->crossover[ch];

		if (enable_emp)
			emp_out = iir_df2t(emp_s, *buf_src);
		else
			emp_out = *buf_src;

		split_func(emp_out, crossover_out, crossover_s);
		buf_sink_band = buf_sink;
		for (band = 0; band < nband; band++) {
			*buf_sink_band = crossover_out[band];
			buf_sink_band += PLATFORM_MAX_CHANNELS;
		}

		buf_src++;
		buf_sink++;
	}
}

#if CONFIG_FORMAT_S16LE
static void multiband_drc_s16_process_drc(struct drc_state *state,
					  const struct sof_drc_params *p,
					  int32_t *buf_src,
					  int32_t *buf_sink,
					  int nch)
{
	int16_t *pd_write;
	int16_t *pd_read;
	int ch;
	int pd_write_index;
	int pd_read_index;

	if (p->enabled && !state->processed) {
		drc_update_envelope(state, p);
		drc_compress_output(state, p, 2, nch);
		state->processed = 1;
	}

	pd_write_index = state->pre_delay_write_index;
	pd_read_index = state->pre_delay_read_index;

	for (ch = 0; ch < nch; ++ch) {
		pd_write = (int16_t *)state->pre_delay_buffers[ch] + pd_write_index;
		pd_read = (int16_t *)state->pre_delay_buffers[ch] + pd_read_index;
		*pd_write = sat_int16(Q_SHIFT_RND(*buf_src, 31, 15));
		*buf_sink = *pd_read << 16;

		buf_src++;
		buf_sink++;
	}

	pd_write_index = (pd_write_index + 1) & DRC_MAX_PRE_DELAY_FRAMES_MASK;
	pd_read_index = (pd_read_index + 1) & DRC_MAX_PRE_DELAY_FRAMES_MASK;
	state->pre_delay_write_index = pd_write_index;
	state->pre_delay_read_index = pd_read_index;

	/* Only perform delay frames by early return here if not enabled */
	if (!p->enabled)
		return;

	/* Process the input division (32 frames). */
	if (!(pd_write_index & DRC_DIVISION_FRAMES_MASK)) {
		drc_update_detector_average(state, p, 2, nch);
		drc_update_envelope(state, p);
		drc_compress_output(state, p, 2, nch);
	}
}
#endif /* CONFIG_FORMAT_S16LE */

#if CONFIG_FORMAT_S24LE || CONFIG_FORMAT_S32LE
static void multiband_drc_s32_process_drc(struct drc_state *state,
					  const struct sof_drc_params *p,
					  int32_t *buf_src,
					  int32_t *buf_sink,
					  int nch)
{
	int32_t *pd_write;
	int32_t *pd_read;
	int ch;
	int pd_write_index;
	int pd_read_index;

	if (p->enabled && !state->processed) {
		drc_update_envelope(state, p);
		drc_compress_output(state, p, 4, nch);
		state->processed = 1;
	}

	pd_write_index = state->pre_delay_write_index;
	pd_read_index = state->pre_delay_read_index;

	for (ch = 0; ch < nch; ++ch) {
		pd_write = (int32_t *)state->pre_delay_buffers[ch] + pd_write_index;
		pd_read = (int32_t *)state->pre_delay_buffers[ch] + pd_read_index;
		*pd_write = *buf_src;
		*buf_sink = *pd_read;

		buf_src++;
		buf_sink++;
	}

	pd_write_index = (pd_write_index + 1) & DRC_MAX_PRE_DELAY_FRAMES_MASK;
	pd_read_index = (pd_read_index + 1) & DRC_MAX_PRE_DELAY_FRAMES_MASK;
	state->pre_delay_write_index = pd_write_index;
	state->pre_delay_read_index = pd_read_index;

	/* Only perform delay frames by early return here if not enabled */
	if (!p->enabled)
		return;

	/* Process the input division (32 frames). */
	if (!(pd_write_index & DRC_DIVISION_FRAMES_MASK)) {
		drc_update_detector_average(state, p, 4, nch);
		drc_update_envelope(state, p);
		drc_compress_output(state, p, 4, nch);
	}
}
#endif /* CONFIG_FORMAT_S24LE || CONFIG_FORMAT_S32LE */

static void multiband_drc_process_deemp(struct multiband_drc_state *state,
					int32_t *buf_src,
					int32_t *buf_sink,
					int enable_deemp,
					int nch,
					int nband)
{
	struct iir_state_df2t *deemp_s;
	int32_t *buf_src_band;
	int ch, band;
	int32_t mix_out;

	for (ch = 0; ch < nch; ch++) {
		deemp_s = &state->deemphasis[ch];

		buf_src_band = buf_src;
		mix_out = 0;
		for (band = 0; band < nband; band++) {
			mix_out = sat_int32((int64_t)mix_out + *buf_src_band);
			buf_src_band += PLATFORM_MAX_CHANNELS;
		}

		if (enable_deemp)
			*buf_sink = iir_df2t(deemp_s, mix_out);
		else
			*buf_sink = mix_out;

		buf_src++;
		buf_sink++;
	}
}

 /* This graph illustrates the buffers declared in the following default functions, as the example
  * of a 3-band Multiband DRC:
  *
  *            :buf_src[nch]                            :buf_drc_sink[nch*nband]
  *            :                                        :
  *            :                           o-[]-> DRC0 -[]--o
  *            :                           | :          :   |
  *            :                 3-WAY     | :          :   |
  *    source -[]-> EQ EMP --> CROSSOVER --o-[]-> DRC1 -[]-(+)--> EQ DEEMP -[]-> sink
  *                                        | :          :   |               :
  *                                        | :          :   |               :
  *                                        o-[]-> DRC2 -[]--o               :
  *                                          :                              :
  *                                          :buf_drc_src[nch*nband]        :buf_sink[nch]
  */
#if CONFIG_FORMAT_S16LE
static void multiband_drc_s16_default(const struct comp_dev *dev,
				      const struct audio_stream *source,
				      struct audio_stream *sink,
				      uint32_t frames)
{
	struct multiband_drc_comp_data *cd = comp_get_drvdata(dev);
	struct multiband_drc_state *state = &cd->state;
	int32_t buf_src[PLATFORM_MAX_CHANNELS];
	int32_t buf_sink[PLATFORM_MAX_CHANNELS];
	int32_t buf_drc_src[PLATFORM_MAX_CHANNELS * SOF_MULTIBAND_DRC_MAX_BANDS];
	int32_t buf_drc_sink[PLATFORM_MAX_CHANNELS * SOF_MULTIBAND_DRC_MAX_BANDS];
	int32_t *band_buf_drc_src;
	int32_t *band_buf_drc_sink;
	int16_t *x;
	int16_t *y;
	int idx_src = 0;
	int idx_sink = 0;
	int ch;
	int band;
	int i;
	int nch = source->channels;
	int nband = cd->config->num_bands;
	int enable_emp_deemp = cd->config->enable_emp_deemp;

	for (i = 0; i < frames; ++i) {
		for (ch = 0; ch < nch; ch++) {
			x = audio_stream_read_frag_s16(source, idx_src);
			buf_src[ch] = *x << 16;
			idx_src++;
		}

		multiband_drc_process_emp_crossover(state, cd->crossover_split,
						    buf_src, buf_drc_src,
						    enable_emp_deemp, nch, nband);

		band_buf_drc_src = buf_drc_src;
		band_buf_drc_sink = buf_drc_sink;
		for (band = 0; band < nband; ++band) {
			multiband_drc_s16_process_drc(&state->drc[band],
						      &cd->config->drc_coef[band],
						      band_buf_drc_src, band_buf_drc_sink, nch);
			band_buf_drc_src += PLATFORM_MAX_CHANNELS;
			band_buf_drc_sink += PLATFORM_MAX_CHANNELS;
		}

		multiband_drc_process_deemp(state, buf_drc_sink, buf_sink,
					    enable_emp_deemp, nch, nband);

		for (ch = 0; ch < nch; ch++) {
			y = audio_stream_write_frag_s16(sink, idx_sink);
			*y = sat_int16(Q_SHIFT_RND(buf_sink[ch], 31, 15));
			idx_sink++;
		}
	}
}
#endif /* CONFIG_FORMAT_S16LE */

#if CONFIG_FORMAT_S24LE
static void multiband_drc_s24_default(const struct comp_dev *dev,
				      const struct audio_stream *source,
				      struct audio_stream *sink,
				      uint32_t frames)
{
	struct multiband_drc_comp_data *cd = comp_get_drvdata(dev);
	struct multiband_drc_state *state = &cd->state;
	int32_t buf_src[PLATFORM_MAX_CHANNELS];
	int32_t buf_sink[PLATFORM_MAX_CHANNELS];
	int32_t buf_drc_src[PLATFORM_MAX_CHANNELS * SOF_MULTIBAND_DRC_MAX_BANDS];
	int32_t buf_drc_sink[PLATFORM_MAX_CHANNELS * SOF_MULTIBAND_DRC_MAX_BANDS];
	int32_t *band_buf_drc_src;
	int32_t *band_buf_drc_sink;
	int32_t *x;
	int32_t *y;
	int idx_src = 0;
	int idx_sink = 0;
	int ch;
	int band;
	int i;
	int nch = source->channels;
	int nband = cd->config->num_bands;
	int enable_emp_deemp = cd->config->enable_emp_deemp;

	for (i = 0; i < frames; ++i) {
		for (ch = 0; ch < nch; ch++) {
			x = audio_stream_read_frag_s32(source, idx_src);
			buf_src[ch] = *x << 8;
			idx_src++;
		}

		multiband_drc_process_emp_crossover(state, cd->crossover_split,
						    buf_src, buf_drc_src,
						    enable_emp_deemp, nch, nband);

		band_buf_drc_src = buf_drc_src;
		band_buf_drc_sink = buf_drc_sink;
		for (band = 0; band < nband; ++band) {
			multiband_drc_s32_process_drc(&state->drc[band],
						      &cd->config->drc_coef[band],
						      band_buf_drc_src, band_buf_drc_sink, nch);
			band_buf_drc_src += PLATFORM_MAX_CHANNELS;
			band_buf_drc_sink += PLATFORM_MAX_CHANNELS;
		}

		multiband_drc_process_deemp(state, buf_drc_sink, buf_sink,
					    enable_emp_deemp, nch, nband);

		for (ch = 0; ch < nch; ch++) {
			y = audio_stream_write_frag_s32(sink, idx_sink);
			*y = sat_int24(Q_SHIFT_RND(buf_sink[ch], 31, 23));
			idx_sink++;
		}
	}
}
#endif /* CONFIG_FORMAT_S24LE */

#if CONFIG_FORMAT_S32LE
static void multiband_drc_s32_default(const struct comp_dev *dev,
				      const struct audio_stream *source,
				      struct audio_stream *sink,
				      uint32_t frames)
{
	struct multiband_drc_comp_data *cd = comp_get_drvdata(dev);
	struct multiband_drc_state *state = &cd->state;
	int32_t buf_src[PLATFORM_MAX_CHANNELS];
	int32_t buf_sink[PLATFORM_MAX_CHANNELS];
	int32_t buf_drc_src[PLATFORM_MAX_CHANNELS * SOF_MULTIBAND_DRC_MAX_BANDS];
	int32_t buf_drc_sink[PLATFORM_MAX_CHANNELS * SOF_MULTIBAND_DRC_MAX_BANDS];
	int32_t *band_buf_drc_src;
	int32_t *band_buf_drc_sink;
	int32_t *x;
	int32_t *y;
	int idx_src = 0;
	int idx_sink = 0;
	int ch;
	int band;
	int i;
	int nch = source->channels;
	int nband = cd->config->num_bands;
	int enable_emp_deemp = cd->config->enable_emp_deemp;

	for (i = 0; i < frames; ++i) {
		for (ch = 0; ch < nch; ch++) {
			x = audio_stream_read_frag_s32(source, idx_src);
			buf_src[ch] = *x;
			idx_src++;
		}

		multiband_drc_process_emp_crossover(state, cd->crossover_split,
						    buf_src, buf_drc_src,
						    enable_emp_deemp, nch, nband);

		band_buf_drc_src = buf_drc_src;
		band_buf_drc_sink = buf_drc_sink;
		for (band = 0; band < nband; ++band) {
			multiband_drc_s32_process_drc(&state->drc[band],
						      &cd->config->drc_coef[band],
						      band_buf_drc_src, band_buf_drc_sink, nch);
			band_buf_drc_src += PLATFORM_MAX_CHANNELS;
			band_buf_drc_sink += PLATFORM_MAX_CHANNELS;
		}

		multiband_drc_process_deemp(state, buf_drc_sink, buf_sink,
					    enable_emp_deemp, nch, nband);

		for (ch = 0; ch < nch; ch++) {
			y = audio_stream_write_frag_s32(sink, idx_sink);
			*y = buf_sink[ch];
			idx_sink++;
		}
	}
}
#endif /* CONFIG_FORMAT_S32LE */

const struct multiband_drc_proc_fnmap multiband_drc_proc_fnmap[] = {
/* { SOURCE_FORMAT , PROCESSING FUNCTION } */
#if CONFIG_FORMAT_S16LE
	{ SOF_IPC_FRAME_S16_LE, multiband_drc_s16_default },
#endif /* CONFIG_FORMAT_S16LE */

#if CONFIG_FORMAT_S24LE
	{ SOF_IPC_FRAME_S24_4LE, multiband_drc_s24_default },
#endif /* CONFIG_FORMAT_S24LE */

#if CONFIG_FORMAT_S32LE
	{ SOF_IPC_FRAME_S32_LE, multiband_drc_s32_default },
#endif /* CONFIG_FORMAT_S32LE */
};

const struct multiband_drc_proc_fnmap multiband_drc_proc_fnmap_pass[] = {
/* { SOURCE_FORMAT , PROCESSING FUNCTION } */
#if CONFIG_FORMAT_S16LE
	{ SOF_IPC_FRAME_S16_LE, multiband_drc_default_pass },
#endif /* CONFIG_FORMAT_S16LE */

#if CONFIG_FORMAT_S24LE
	{ SOF_IPC_FRAME_S24_4LE, multiband_drc_default_pass },
#endif /* CONFIG_FORMAT_S24LE */

#if CONFIG_FORMAT_S32LE
	{ SOF_IPC_FRAME_S32_LE, multiband_drc_default_pass },
#endif /* CONFIG_FORMAT_S32LE */
};

const size_t multiband_drc_proc_fncount = ARRAY_SIZE(multiband_drc_proc_fnmap);
