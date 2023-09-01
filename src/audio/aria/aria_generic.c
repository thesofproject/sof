// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2023 Intel Corporation. All rights reserved.

#include <sof/audio/aria/aria.h>
#include <ipc4/aria.h>

#ifdef ARIA_GENERIC

/**
 * \brief Aria gain index mapping table
 */
const uint8_t INDEX_TAB[] = {
		0,    1,    2,    3,
		4,    5,    6,    7,
		8,    9,    0,    1,
		2,    3,    4,    5,
		6,    7,    8,    9,
		0,    1,    2,    3
};

inline void aria_algo_calc_gain(struct aria_data *cd, size_t gain_idx,
				struct audio_stream *source, int frames)
{
	int32_t max_data = 0;
	int32_t sample_abs;
	uint32_t att = cd->att;
	int64_t gain = (1ULL << (att + 32)) - 1;
	int samples = frames * audio_stream_get_channels(source);
	int32_t *src = audio_stream_get_rptr(source);
	int i, n;

	while (samples) {
		n = audio_stream_samples_without_wrap_s32(source, src);
		n = MIN(samples, n);
		for (i = 0; i < n; i++) {
			sample_abs = ABS(src[i]);
			max_data = MAX(max_data, sample_abs);
		}

		src = audio_stream_wrap(source, src + n);
		samples -= n;
	}

	/*zero check for maxis not needed since att is in range <0;3>*/
	if (max_data > (0x7fffffff >> att))
		gain = (0x7fffffffULL << 32) / max_data;

	cd->gains[gain_idx] = (int32_t)(gain >> (att + 1));
}

void aria_algo_get_data(struct processing_module *mod,
			struct audio_stream *sink, int frames)
{
	struct aria_data *cd = module_get_private_data(mod);
	int32_t step, in_sample;
	int32_t gain_state_add_2 = cd->gain_state + 2;
	int32_t gain_state_add_3 = cd->gain_state + 3;
	int32_t gain_begin = cd->gains[INDEX_TAB[gain_state_add_2]];
	/* do linear approximation between points gain_begin and gain_end */
	int32_t gain_end = cd->gains[INDEX_TAB[gain_state_add_3]];
	int32_t m, n, i, ch;
	int32_t samples = frames * audio_stream_get_channels(sink);
	int32_t *out = audio_stream_get_wptr(sink);
	int32_t *in = cd->data_ptr;
	int32_t gain;
	const int ch_n = cd->chan_cnt;
	const int shift = 31 - cd->att;

	for (i = 1; i < ARIA_MAX_GAIN_STATES - 1; i++) {
		if (cd->gains[INDEX_TAB[gain_state_add_2 + i]] < gain_begin)
			gain_begin = cd->gains[INDEX_TAB[gain_state_add_2 + i]];
		if (cd->gains[INDEX_TAB[gain_state_add_3 + i]] < gain_end)
			gain_end = cd->gains[INDEX_TAB[gain_state_add_3 + i]];
	}
	step = (gain_end - gain_begin) / frames;
	gain = gain_begin;

	while (samples) {
		m = audio_stream_samples_without_wrap_s32(sink, out);
		n = MIN(m, samples);
		m = cir_buf_samples_without_wrap_s32(cd->data_ptr, cd->data_end);
		n = MIN(m, n);
		for (i = 0; i < n; i += ch_n) {
			for (ch = 0; ch < ch_n; ch++) {
				in_sample = *in++;
				out[ch] = q_multsr_sat_32x32(in_sample, gain, shift);
			}
			gain += step;
			out += ch_n;
		}
		samples -= n;
		in = cir_buf_wrap(in, cd->data_addr, cd->data_end);
		out = audio_stream_wrap(sink, out);
	}
	cd->gain_state = INDEX_TAB[cd->gain_state + 1];
}

aria_get_data_func aria_algo_get_data_func(struct processing_module *mod)
{
	return aria_algo_get_data;
}
#endif
