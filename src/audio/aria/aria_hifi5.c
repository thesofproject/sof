// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2024 Intel Corporation. All rights reserved.

#include "aria.h"

#if SOF_USE_HIFI(5, ARIA)
#include <xtensa/config/defs.h>
#include <xtensa/tie/xt_hifi5.h>

extern const int32_t sof_aria_index_tab[];

/* Setup circular buffer 0 */
static inline void set_circular_buf0(const void *start, const void *end)
{
	AE_SETCBEGIN0(start);
	AE_SETCEND0(end);
}

/* Setup circular for buffer 1 */
static inline void set_circular_buf1(const void *start, const void *end)
{
	AE_SETCBEGIN1(start);
	AE_SETCEND1(end);
}

inline void aria_algo_calc_gain(struct aria_data *cd, size_t gain_idx,
				struct audio_stream *source, int frames)
{
	/* detecting maximum value in data chunk */
	ae_int32x2 in_sample;
	ae_int32x2 in_sample1;
	ae_int32x2 max_data = AE_ZERO32();
	int32_t att = cd->att;
	ae_valignx2 inu = AE_ZALIGN128();
	uint64_t gain = (1ULL << (att + 32)) - 1;
	int32_t *max_ptr = (int32_t *)&max_data;
	int32_t max;
	int samples = frames * audio_stream_get_channels(source);
	ae_int32x4 *in = audio_stream_get_rptr(source);
	int i, n, left;

	while (samples) {
		n = audio_stream_samples_without_wrap_s32(source, in);
		n = MIN(samples, n);
		left = n & 3;
		inu = AE_LA128_PP(in);
		for (i = 0; i < n; i += 4) {
			AE_LA32X2X2_IP(in_sample, in_sample1, inu, in);
			max_data = AE_MAXABS32S(max_data, AE_SLAI32(in_sample1, 8));
			max_data = AE_MAXABS32S(max_data, AE_SLAI32(in_sample, 8));
		}
		for (i = 0; i < left; i++) {
			AE_L32_IP(in_sample, (ae_int32 *)in, sizeof(ae_int32));
			max_data = AE_MAXABS32S(max_data, AE_SLAI32(in_sample, 8));
		}
		in = audio_stream_wrap(source, in);
		samples -= n;
	}

	max = MAX(max_ptr[0], max_ptr[1]) >> 8;

	/*zero check for maxis not needed since att is in range <0;3>*/
	if (max > (0x007fffff >> att))
		gain = (0x007fffffULL << 32) / max;

	/* normalization by attenuation factor to obtain fractional range <1 / (2 pow att), 1> */
	cd->gains[gain_idx] = (int32_t)(gain >> (att + 1));
}

static void aria_algo_get_data_odd_channel(struct processing_module *mod,
					   struct audio_stream *sink,
					   int frames)
{
	struct aria_data *cd = module_get_private_data(mod);
	size_t i, ch;
	ae_int32x2 step;
	int32_t gain_state_add_2 = cd->gain_state + 2;
	int32_t gain_state_add_3 = cd->gain_state + 3;
	int32_t gain_begin = cd->gains[sof_aria_index_tab[gain_state_add_2]];
	/* do linear approximation between points gain_begin and gain_end */
	int32_t gain_end = cd->gains[sof_aria_index_tab[gain_state_add_3]];
	ae_int32 *out = audio_stream_get_wptr(sink);
	ae_int32 *in = (ae_int32 *)cd->data_ptr;
	ae_int32x2 in_sample, out_sample;
	const int inc = sizeof(ae_int32);
	ae_int32x2 gain;
	const int ch_n = cd->chan_cnt;
	const int shift_bits = 31 - cd->att - 24;
	ae_int64 out1;

	for (i = 1; i < ARIA_MAX_GAIN_STATES - 1; i++) {
		if (cd->gains[sof_aria_index_tab[gain_state_add_2 + i]] < gain_begin)
			gain_begin = cd->gains[sof_aria_index_tab[gain_state_add_2 + i]];
		if (cd->gains[sof_aria_index_tab[gain_state_add_3 + i]] < gain_end)
			gain_end = cd->gains[sof_aria_index_tab[gain_state_add_3 + i]];
	}

	step = (gain_end - gain_begin) / frames;
	gain = gain_begin;

	set_circular_buf0(cd->data_addr, cd->data_end);
	set_circular_buf1(audio_stream_get_addr(sink), audio_stream_get_end_addr(sink));

	for (i = 0; i < frames; i++) {
		/*process data one by one if ch_n is odd*/
		for (ch = 0; ch < ch_n; ch++) {
			AE_L32_XC(in_sample, in, inc);
			in_sample = AE_SRAI32(AE_SLAI32(in_sample, 8), 8);
			out1 = AE_MUL32_HH(in_sample, gain);
			out1 = AE_SRAA64(out1, shift_bits);
			out_sample = AE_ROUND24X2F48SSYM(out1, out1);
			AE_S32_L_XC1(out_sample, out, inc);
		}
		gain = AE_ADD32S(gain, step);
	}

	cd->gain_state = sof_aria_index_tab[cd->gain_state + 1];
}

static void aria_algo_get_data_even_channel(struct processing_module *mod,
					    struct audio_stream *sink,
					    int frames)
{
	struct aria_data *cd = module_get_private_data(mod);
	size_t i, ch;
	ae_int32x2 step;
	int32_t gain_state_add_2 = cd->gain_state + 2;
	int32_t gain_state_add_3 = cd->gain_state + 3;
	int32_t gain_begin = cd->gains[sof_aria_index_tab[gain_state_add_2]];
	/* do linear approximation between points gain_begin and gain_end */
	int32_t gain_end = cd->gains[sof_aria_index_tab[gain_state_add_3]];
	ae_int32x2 *out = audio_stream_get_wptr(sink);
	ae_int32x2 *in = (ae_int32x2 *)cd->data_ptr;
	ae_int32x2 in_sample, out_sample;
	ae_int32x2 gain;
	const int ch_n = cd->chan_cnt;
	const int inc = sizeof(ae_int32x2);
	const int shift_bits = 31 - cd->att - 24;
	ae_int64 out1, out2;

	for (i = 1; i < ARIA_MAX_GAIN_STATES - 1; i++) {
		if (cd->gains[sof_aria_index_tab[gain_state_add_2 + i]] < gain_begin)
			gain_begin = cd->gains[sof_aria_index_tab[gain_state_add_2 + i]];
		if (cd->gains[sof_aria_index_tab[gain_state_add_3 + i]] < gain_end)
			gain_end = cd->gains[sof_aria_index_tab[gain_state_add_3 + i]];
	}

	step = (gain_end - gain_begin) / frames;
	gain = gain_begin;

	set_circular_buf0(cd->data_addr, cd->data_end);
	set_circular_buf1(audio_stream_get_addr(sink), audio_stream_get_end_addr(sink));

	for (i = 0; i < frames; i++) {
		/*process 2 samples per time if ch_n is even*/
		for (ch = 0; ch < ch_n; ch += 2) {
			AE_L32X2_XC(in_sample, in, inc);
			in_sample = AE_SRAI32(AE_SLAI32(in_sample, 8), 8);
			out1 = AE_MUL32_HH(in_sample, gain);
			out1 = AE_SRAA64(out1, shift_bits);
			out2 = AE_MUL32_LL(in_sample, gain);
			out2 = AE_SRAA64(out2, shift_bits);
			out_sample = AE_ROUND24X2F48SSYM(out1, out2);
			AE_S32X2_XC1(out_sample, out, inc);
		}
		gain = AE_ADD32S(gain, step);
	}
	cd->gain_state = sof_aria_index_tab[cd->gain_state + 1];
}

aria_get_data_func aria_algo_get_data_func(struct processing_module *mod)
{
	struct aria_data *cd = module_get_private_data(mod);

	if (cd->chan_cnt & 1)
		return aria_algo_get_data_odd_channel;
	else
		return aria_algo_get_data_even_channel;
}
#endif
