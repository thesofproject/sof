// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Intel Corporation. All rights reserved.

#include <sof/audio/aria/aria.h>
#include <xtensa/config/defs.h>
#include <xtensa/tie/xt_hifi3.h>

inline void aria_algo_calc_gain(struct comp_dev *dev, size_t gain_idx,
				int32_t *__restrict data, const size_t src_size)
{
	struct aria_data *cd = comp_get_drvdata(dev);
	/* detecting maximum value in data chunk */
	size_t pairs_size = src_size;
	size_t i;
	ae_f32x2 d1;
	ae_int32x2 *ptr_in = (ae_int32x2 *)data;
	ae_f32x2 d0 = AE_ZERO32();

	__aligned(8) uint32_t max_data[2];

	ae_int32x2 *ptr_out = (ae_int32x2 *)&max_data[0];
	/* below condition maintains buffer alignment to 8 bytes */
	if (!IS_ALIGNED((uintptr_t)ptr_in, 8)) {
		AE_L32_XC(d1, (ae_int32 *)ptr_in, 4);
		d0 = AE_MAXABS32S(d0, d1);
		pairs_size--;
	}
	for (i = 0; i < src_size >> 1; ++i) {
		AE_L32X2_XP(d1, ptr_in, 8);
		d0 = AE_MAXABS32S(d0, d1);
	}
	/* maintains odd sample if any left */
	if (pairs_size % 2) {
		AE_L32_XC(d1, (ae_int32 *)ptr_in, 0);
		d0 = AE_MAXABS32S(d0, d1);
	}
	AE_S32X2_XP(d0, ptr_out, 0);
	max_data[0] = MAX(max_data[0], max_data[1]);

	uint64_t gain = (1ULL << (cd->att + 32)) - 1;
	/* currently att_ value is checked on initialization and is in range <0;3>
	 * so eventual zero check for max_data[0] is not needed (prevention from division by 0)
	 */
	if (max_data[0] > (0x7fffffffUL >> cd->att))
		gain = (0x7fffffffULL << 32) / max_data[0];

	/* normalization by attenuation factor to obtain fractional range <1 / (2 pow att), 1> */
	cd->gains[gain_idx] = (int32_t)(gain >> (cd->att + 1));
}

void aria_algo_get_data(struct comp_dev *dev, int32_t *__restrict  data, size_t size)
{
	struct aria_data *cd = comp_get_drvdata(dev);
	/* do linear approximation between points gain_begin and gain_end */
	int32_t gain_begin = cd->gains[(cd->gain_state + 2) % ARIA_MAX_GAIN_STATES];
	int32_t gain_end = cd->gains[(cd->gain_state + 3) % ARIA_MAX_GAIN_STATES];
	size_t idx, ch;

	for (idx = 1; idx < ARIA_MAX_GAIN_STATES - 1; ++idx) {
		gain_begin = MIN(gain_begin, cd->gains[(cd->gain_state + idx + 2) %
						ARIA_MAX_GAIN_STATES]);
		gain_end = MIN(gain_end, cd->gains[(cd->gain_state + idx + 3) %
						ARIA_MAX_GAIN_STATES]);
	}

	ae_f32x2 d1, d2, d3;
	const size_t smpl_groups = size / cd->chan_cnt;
	const int32_t step = (gain_end - gain_begin) / (int32_t)smpl_groups;
	/* ensure index is always positive */
	ae_int32x2 *ptr_in = (ae_int32x2 *)&cd->data[(cd->buff_pos + cd->offset) % cd->buff_size];
	ae_int32x2 *ptr_out = (ae_int32x2 *)data;

	d1 = (ae_f32x2)gain_begin;

	int32_t next_gain;
	int32_t prev_gain = gain_begin;

	AE_SETCBEGIN0(cd->data);
	AE_SETCEND0(cd->data + cd->buff_size);
	ae_valign align_out = AE_ZALIGN64();
	/* variable for odd sample detection, detection when exceeded */
	size_t odd_detect = ALIGN_DOWN(size, 2);
	/* variable accumulates samples being processed, helps to identify odd sample */
	size_t acc = 0;

	/* below condition maintains buffer alignment to 8 bytes */
	if (!IS_ALIGNED((uintptr_t)ptr_in, 8)) {
		AE_L32_XC(d2, (ae_int32 *)(ptr_in), 4);
		d3 = AE_MULFP32X2RS(d1, d2);
		d3 = AE_SLAA32S(d3, cd->att);
		AE_S32_L_XP(d3, (ae_int32 *)(ptr_out), 4);
		/* acc overflow expected and used as a condition later in loop */
		acc = (size_t)(-cd->chan_cnt);
		odd_detect = ALIGN_DOWN(size - 1, 2);
	}
	const size_t chan_pairs = cd->chan_cnt >> 1;

	for (idx = 0; idx < smpl_groups; ++idx) {
		/* below condition process channel pairs from current sample group
		 * to be amplified with the same gain
		 */
		for (ch = 0; ch < chan_pairs; ++ch) {
			AE_L32X2_XC(d2, ptr_in, 8);
			/* d1 - Normalized gain of signal in range <1 / (2 pow att), 1>*/
			d3 = AE_MULFP32X2RS(d1, d2);
			/* denormalization */
			d3 = AE_SLAA32S(d3, cd->att);
			AE_SA32X2_IP(d3, align_out, ptr_out);
		}
		acc += cd->chan_cnt;
		next_gain = (gain_begin + (idx + 1) * step);
		/* below condition process odd channel from current sample group and next
		 * sample group when channels count is odd, it process current
		 * and next sample group corresponding to (idx) and (idx+1)
		 */
		if ((acc % 2) && !(acc > odd_detect)) {
			d1 = AE_MOVDA32X2(prev_gain, next_gain);
			AE_L32X2_XC(d2, ptr_in, 8);
			/* d1 - Normalized gain of signal in range <1 / (2 pow att), 1>*/
			d3 = AE_MULFP32X2RS(d1, d2);
			/* denormalization */
			d3 = AE_SLAA32S(d3, cd->att);
			AE_SA32X2_IP(d3, align_out, ptr_out);
		}
		d1 = (ae_f32x2)next_gain;
		prev_gain = next_gain;
	}
	AE_SA64POS_FP(align_out, ptr_out);
	/* maintains odd sample if any left */
	if (acc > odd_detect) {
		AE_L32_XC(d2, (ae_int32 *)(ptr_in), 0);
		d3 = AE_MULFP32X2RS(d1, d2);
		d3 = AE_SLAA32S(d3, cd->att);
		AE_S32_L_XP(d3, (ae_int32 *)(ptr_out), 0);
	}
	cd->gain_state = (cd->gain_state + 1) % ARIA_MAX_GAIN_STATES;
}
