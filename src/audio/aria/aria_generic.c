// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2023 Intel Corporation. All rights reserved.

#include <sof/audio/aria/aria.h>
#include <ipc4/aria.h>

#ifdef ARIA_GENERIC

inline void aria_algo_calc_gain(struct comp_dev *dev, size_t gain_idx,
				int32_t *__restrict data, const size_t src_size)
{
	struct aria_data *cd = comp_get_drvdata(dev);
	int32_t max_data = 0;
	int64_t gain;

	for (size_t i = 0; i < src_size; ++i)
		max_data = MAX(max_data, ABS(data[i]));

	gain = 1ULL << (cd->att + 32);
	if (max_data > (0x7fffffff >> cd->att) && max_data != 0)
		gain = (0x7fffffffULL << 32) / max_data;

	cd->gains[gain_idx] = gain;
}

void aria_algo_get_data(struct comp_dev *dev, int32_t *__restrict  data, size_t size)
{
	struct aria_data *cd = comp_get_drvdata(dev);
	size_t smpl_groups, ii;
	int64_t step, sample;
	// do linear approximation between points gain_begin and gain_end
	int64_t gain_begin = cd->gains[(cd->gain_state + 2) % ARIA_MAX_GAIN_STATES];
	int64_t gain_end = cd->gains[(cd->gain_state + 3) % ARIA_MAX_GAIN_STATES];

	for (size_t i = 1; i < ARIA_MAX_GAIN_STATES - 1; ++i) {
		if (cd->gains[(cd->gain_state + i + 2) % ARIA_MAX_GAIN_STATES] < gain_begin)
			gain_begin = cd->gains[(cd->gain_state + i + 2) % ARIA_MAX_GAIN_STATES];
		if (cd->gains[(cd->gain_state + i + 3) % ARIA_MAX_GAIN_STATES] < gain_end)
			gain_end = cd->gains[(cd->gain_state + i + 3) % ARIA_MAX_GAIN_STATES];
	}
	smpl_groups = size / cd->chan_cnt;
	step = (gain_end - gain_begin) / smpl_groups;

	for (size_t idx = 0, gain = gain_begin, offset = 0;
	     idx < smpl_groups;
	     ++idx, gain += step, offset += cd->chan_cnt) {
		ii = (cd->buff_pos + offset) % cd->buff_size;
		for (size_t ch = 0; ch < cd->chan_cnt; ++ch) {
			sample = cd->data[ii + ch];
			data[offset + ch] = (sample * gain) >> 32;
		}
	}
	cd->gain_state = (cd->gain_state + 1) % ARIA_MAX_GAIN_STATES;
}
#endif
