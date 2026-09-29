// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Intel Corporation. All rights reserved.

#include "aria.h"

#if SOF_USE_HIFI(3, ARIA) || SOF_USE_HIFI(4, ARIA)
#include <xtensa/config/defs.h>
#include <xtensa/tie/xt_hifi3.h>

extern const int32_t sof_aria_index_tab[];

inline void aria_algo_calc_gain(struct aria_data *cd, size_t gain_idx,
				struct cir_buf_source *source, size_t frames)
{
	/* detecting maximum value in data chunk */
	ae_int32x2 in_sample;
	ae_int32x2 max_data = AE_ZERO32();
	int32_t att = cd->att;
	ae_valign inu = AE_ZALIGN64();
	uint64_t gain = (1ULL << (att + 32)) - 1;
	int32_t *max_ptr = (int32_t *)&max_data;
	int32_t max;
	size_t samples = frames * cd->chan_cnt;
	const ae_int32x2 *in = source->ptr;
	size_t i, n, m;

	while (samples) {
		n = cir_buf_samples_without_wrap_s32(in, source->buf_end);
		n = MIN(samples, n);
		m = n >> 1;
		inu = AE_LA64_PP(in);
		for (i = 0; i < m; i++) {
			AE_LA32X2_IP(in_sample, inu, in);
			max_data = AE_MAXABS32S(max_data, AE_SLAI32(in_sample, 8));
		}
		if (n & 1) {
			AE_L32_IP(in_sample, (ae_int32 *)in, sizeof(ae_int32));
			max_data = AE_MAXABS32S(max_data, AE_SLAI32(in_sample, 8));
		}
		in = source_cir_buf_wrap(in, source->buf_start, source->buf_end);
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
					   struct cir_buf_sink *sink,
					   size_t frames)
{
	struct aria_data *cd = module_get_private_data(mod);
	size_t i, m, n;
	ae_int32x2 step;
	int32_t gain_state_add_2 = cd->gain_state + 2;
	int32_t gain_state_add_3 = cd->gain_state + 3;
	int32_t gain_begin = cd->gains[sof_aria_index_tab[gain_state_add_2]];
	/* do linear approximation between points gain_begin and gain_end */
	int32_t gain_end = cd->gains[sof_aria_index_tab[gain_state_add_3]];
	size_t samples = frames * cd->chan_cnt;
	ae_int32x2 *out = sink->ptr;
	const ae_int32x2 *in = (const ae_int32x2 *)cd->data_ptr;
	ae_valign inu = AE_ZALIGN64();
	ae_valign outu = AE_ZALIGN64();
	ae_int32x2 in_sample, out_sample;
	const int inc = sizeof(ae_int32);
	ae_int32x2 gain;
	const unsigned int ch_n = cd->chan_cnt;
	const int shift_bits = 31 - cd->att - 24;
	unsigned int ch;
	ae_int64 out1;
	int idx;

	for (idx = 1; idx < ARIA_MAX_GAIN_STATES - 1; idx++) {
		if (cd->gains[sof_aria_index_tab[gain_state_add_2 + idx]] < gain_begin)
			gain_begin = cd->gains[sof_aria_index_tab[gain_state_add_2 + idx]];
		if (cd->gains[sof_aria_index_tab[gain_state_add_3 + idx]] < gain_end)
			gain_end = cd->gains[sof_aria_index_tab[gain_state_add_3 + idx]];
	}

	step = (gain_end - gain_begin) / (int32_t)frames;
	gain = gain_begin;
	while (samples) {
		m = cir_buf_samples_without_wrap_s32(out, sink->buf_end);
		n = MIN(m, samples);
		m = cir_buf_samples_without_wrap_s32(cd->data_ptr, cd->data_end);
		n = MIN(m, n);
		inu = AE_LA64_PP(in);
		for (i = 0; i < n; i += ch_n) {
			/*process data one by one if ch_n is odd*/
			for (ch = 0; ch < ch_n; ch++) {
				AE_L32_XP(in_sample, (ae_int32 *)in, inc);
				in_sample = AE_SRAI32(AE_SLAI32(in_sample, 8), 8);
				out1 = AE_MUL32_HH(in_sample, gain);
				out1 = AE_SRAA64(out1, shift_bits);
				out_sample = AE_ROUND24X2F48SSYM(out1, out1);
				AE_S32_L_XP(out_sample, (ae_int32 *)out, inc);
			}
			gain = AE_ADD32S(gain, step);
		}
		AE_SA64POS_FP(outu, out);
		samples -= n;
		in = cir_buf_wrap(in, cd->data_addr, cd->data_end);
		out = cir_buf_wrap(out, sink->buf_start, sink->buf_end);
	}
	cd->gain_state = sof_aria_index_tab[cd->gain_state + 1];
}

static void aria_algo_get_data_even_channel(struct processing_module *mod,
					    struct cir_buf_sink *sink,
					    size_t frames)
{
	struct aria_data *cd = module_get_private_data(mod);
	size_t i, m, n;
	ae_int32x2 step;
	int32_t gain_state_add_2 = cd->gain_state + 2;
	int32_t gain_state_add_3 = cd->gain_state + 3;
	int32_t gain_begin = cd->gains[sof_aria_index_tab[gain_state_add_2]];
	/* do linear approximation between points gain_begin and gain_end */
	int32_t gain_end = cd->gains[sof_aria_index_tab[gain_state_add_3]];
	size_t samples = frames * cd->chan_cnt;
	ae_int32x2 *out = sink->ptr;
	const ae_int32x2 *in = (const ae_int32x2 *)cd->data_ptr;
	ae_valign inu = AE_ZALIGN64();
	ae_valign outu = AE_ZALIGN64();
	ae_int32x2 in_sample, out_sample;
	ae_int32x2 gain;
	const unsigned int ch_n = cd->chan_cnt;
	const int shift_bits = 31 - cd->att - 24;
	unsigned int ch;
	ae_int64 out1, out2;
	int idx;

	for (idx = 1; idx < ARIA_MAX_GAIN_STATES - 1; idx++) {
		if (cd->gains[sof_aria_index_tab[gain_state_add_2 + idx]] < gain_begin)
			gain_begin = cd->gains[sof_aria_index_tab[gain_state_add_2 + idx]];
		if (cd->gains[sof_aria_index_tab[gain_state_add_3 + idx]] < gain_end)
			gain_end = cd->gains[sof_aria_index_tab[gain_state_add_3 + idx]];
	}

	step = (gain_end - gain_begin) / (int32_t)frames;
	gain = gain_begin;
	while (samples) {
		m = cir_buf_samples_without_wrap_s32(out, sink->buf_end);
		n = MIN(m, samples);
		m = cir_buf_samples_without_wrap_s32(cd->data_ptr, cd->data_end);
		n = MIN(m, n);
		inu = AE_LA64_PP(in);
		for (i = 0; i < n; i += ch_n) {
			/*process 2 samples per time if ch_n is even*/
			for (ch = 0; ch < ch_n; ch += 2) {
				AE_LA32X2_IP(in_sample, inu, in);
				in_sample = AE_SRAI32(AE_SLAI32(in_sample, 8), 8);
				out1 = AE_MUL32_HH(in_sample, gain);
				out1 = AE_SRAA64(out1, shift_bits);
				out2 = AE_MUL32_LL(in_sample, gain);
				out2 = AE_SRAA64(out2, shift_bits);
				out_sample = AE_ROUND24X2F48SSYM(out1, out2);
				AE_SA32X2_IP(out_sample, outu, out);
			}
			gain = AE_ADD32S(gain, step);
		}
		AE_SA64POS_FP(outu, out);
		samples -= n;
		in = cir_buf_wrap(in, cd->data_addr, cd->data_end);
		out = cir_buf_wrap(out, sink->buf_start, sink->buf_end);
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
