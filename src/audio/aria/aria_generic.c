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

inline void aria_algo_calc_gain(struct comp_dev *dev, size_t gain_idx,
				struct audio_stream __sparse_cache *source, int frames)
{
	struct aria_data *cd = comp_get_drvdata(dev);
	int32_t max_data = 0;
	int64_t gain = 1ULL << (cd->att + 32);
	int samples = frames * audio_stream_get_channels(source);
	int32_t *src = audio_stream_get_rptr(source);
	int i, n;

	while (samples) {
		n = audio_stream_samples_without_wrap_s32(source, src);
		n = MIN(samples, n);
		for (i = 0; i < n; i++)
			max_data = MAX(max_data, ABS(src[i]));
		src = audio_stream_wrap(source, src + n);
		samples -= n;
	}

	if (max_data > (0x7fffffff >> cd->att) && max_data != 0)
		gain = (0x7fffffffULL << 32) / max_data;

	cd->gains[gain_idx] = gain;
}

void aria_algo_get_data(struct comp_dev *dev, struct audio_stream __sparse_cache *sink, int frames)
{
	struct aria_data *cd = comp_get_drvdata(dev);
	int64_t step, in_sample;
	// do linear approximation between points gain_begin and gain_end
	int64_t gain_begin = cd->gains[INDEX_TAB[cd->gain_state + 2]];
	int64_t gain_end = cd->gains[INDEX_TAB[cd->gain_state + 3]];
	size_t ii, m, n;
	size_t i, idx, ch;
	size_t samples = frames * audio_stream_get_channels(sink);
	int32_t *out = audio_stream_get_wptr(sink);
	int32_t *in = cd->data_ptr;
	size_t gain;
	size_t offset = 0;
	int ch_n = cd->chan_cnt;

	for (i = 1; i < ARIA_MAX_GAIN_STATES - 1; i++) {
		if (cd->gains[INDEX_TAB[cd->gain_state + i + 2]] < gain_begin)
			gain_begin = cd->gains[INDEX_TAB[cd->gain_state + i + 2]];
		if (cd->gains[INDEX_TAB[cd->gain_state + i + 3]] < gain_end)
			gain_end = cd->gains[INDEX_TAB[cd->gain_state + i + 3]];
	}
	step = (gain_end - gain_begin) / frames;

	while (samples) {
		m = audio_stream_samples_without_wrap_s32(sink, out);
		n = MIN(m, samples);
		m = cir_buf_samples_without_wrap_s32(cd->data_ptr, cd->data_end);
		n = MIN(m, n);
		for (i = 0; i < n; i += ch_n) {
			for (ch = 0; ch < ch_n; ch++) {
				in_sample = *in++;
				out[ch] = (in_sample * gain) >> 32;
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
#endif
