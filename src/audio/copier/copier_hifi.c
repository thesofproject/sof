// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 Intel Corporation. All rights reserved.
//
// Author: Andrula Song <xiaoyuan.song@intel.com>
#include "copier.h"
#include <sof/common.h>

#if SOF_USE_HIFI(3, COPIER) || SOF_USE_HIFI(4, COPIER) || SOF_USE_HIFI(5, COPIER)

#include <sof/audio/buffer.h>
#include <sof/audio/component_ext.h>
#include <sof/audio/format.h>
#include <sof/audio/pipeline.h>
#include <sof/audio/component.h>
#include <module/module/base.h>
#include <stddef.h>
#include <errno.h>
#include <stdint.h>
#include <xtensa/tie/xt_hifi3.h>
#include "copier_gain.h"

LOG_MODULE_REGISTER(copier_hifi, CONFIG_SOF_LOG_LEVEL);

int apply_attenuation(struct comp_dev *dev, struct copier_data *cd,
		      struct comp_buffer *sink, int frame)
{
	int i;
	int n;
	int nmax;
	int m;
	ae_int32x2 sample;
	ae_valign uu = AE_ZALIGN64();
	ae_valign su = AE_ZALIGN64();
	int remaining_samples = frame * audio_stream_get_channels(&sink->stream);
	uint32_t bytes = frame * audio_stream_frame_bytes(&sink->stream);
	uint32_t *dst = audio_stream_rewind_wptr_by_bytes(&sink->stream, bytes);
	ae_int32x2 *in = (ae_int32x2 *)dst;
	ae_int32x2 *out = (ae_int32x2 *)dst;

	/* only support attenuation in format of 32bit */
	switch (audio_stream_get_frm_fmt(&sink->stream)) {
	case SOF_IPC_FRAME_S16_LE:
		comp_err(dev, "16bit sample isn't supported by attenuation");
		return -EINVAL;
	case SOF_IPC_FRAME_S24_4LE:
	case SOF_IPC_FRAME_S32_LE:
		while (remaining_samples) {
			nmax = audio_stream_samples_without_wrap_s32(&sink->stream, dst);
			in = (ae_int32x2 *)dst;
			out = (ae_int32x2 *)dst;
			uu = AE_LA64_PP(in);
			n = MIN(remaining_samples, nmax);
			m = n >> 1;
			for (i = 0; i < m; i++) {
				AE_LA32X2_IP(sample, uu, in);
				sample = AE_SRAA32(sample, cd->attenuation);
				AE_SA32X2_IP(sample, su, out);
			}
			AE_SA64POS_FP(su, out);
			if (n & 0x01) {
				AE_L32_IP(sample, (ae_int32 *)in, sizeof(ae_int32));
				sample = AE_SRAA32(sample, cd->attenuation);
				AE_S32_L_IP(sample, (ae_int32 *)out, sizeof(ae_int32));
			}
			remaining_samples -= n;
			dst = audio_stream_wrap(&sink->stream, dst + n);
		}

		return 0;
	default:
		comp_err(dev, "unsupported format %d for attenuation",
			 audio_stream_get_frm_fmt(&sink->stream));
		return -EINVAL;
	}
}

void copier_gain_set_basic_params(struct comp_dev *dev, struct copier_gain_params *gain_params,
				  struct ipc4_base_module_cfg *ipc4_cfg)
{

	/* Set default gain coefficients */
	for (int i = 0; i < ARRAY_SIZE(gain_params->gain_coeffs); ++i)
		gain_params->gain_coeffs[i] = AE_MOVF16X4_FROMINT64(UNITY_GAIN_4X_Q10);

	gain_params->step_f16 = AE_ZERO16();
	gain_params->init_gain = AE_ZERO16();

	/* Set channels count */
	gain_params->channels_count = ipc4_cfg->audio_fmt.channels_count;
}

int copier_gain_set_fade_params(struct comp_dev *dev, struct copier_gain_params *gain_params,
				struct ipc4_base_module_cfg *ipc4_cfg,
				uint32_t fade_period, uint32_t frames)
{
	uint16_t init_gain[MAX_GAIN_COEFFS_CNT];
	uint16_t step_i64_to_i16;
	ae_f16 step_f16;

	/* For backward compatibility add a case with default fade transition.
	 * Backward compatibility is referring to clock_on_delay in DMIC blob.
	 */
	if (fade_period == GAIN_DEFAULT_FADE_PERIOD) {
		/* Set fade transition delay to default value*/
		if (ipc4_cfg->audio_fmt.sampling_frequency > IPC4_FS_16000HZ)
			gain_params->fade_sg_length = frames * GAIN_DEFAULT_HQ_TRANS_MS;
		else
			gain_params->fade_sg_length = frames * GAIN_DEFAULT_LQ_TRANS_MS;
	} else if (fade_period == GAIN_ZERO_TRANS_MS) {
		/* Special case for GAIN_ZERO_TRANS_MS to support zero fade in transition time */
		gain_params->fade_sg_length = 0;
		return 0;
	} else {
		gain_params->fade_sg_length = frames * fade_period;
	}

	/* High precision step for fade-in calculation, keeps accurate precision */
	gain_params->step_i64 = INT64_MAX / gain_params->fade_sg_length;
	step_i64_to_i16 = gain_params->step_i64 >> I64_TO_I16_SHIFT;

	step_f16 = step_i64_to_i16 * (MAX_GAIN_COEFFS_CNT / gain_params->channels_count);

	/* Lower precision step for HIFI SIMD fade-in calculation */
	gain_params->step_f16 = step_f16;

	/* Initialization gain for HIFI SIMD addition, depends on channel configuration */
	for (int i = 0; i < MAX_GAIN_COEFFS_CNT; i++)
		init_gain[i] = (i / gain_params->channels_count) * step_i64_to_i16;

	int ret = memcpy_s(&gain_params->init_gain, sizeof(gain_params->init_gain), init_gain,
			   sizeof(init_gain));
	if (ret)
		comp_err(dev, "memcpy_s failed with error code %d", ret);

	return ret;
}

static inline ae_int16x4 copier_load_slots_and_gain16(ae_int16x4 **addr,
						      ae_valign *align_in,
						      const ae_int16x4 gains)
{
	ae_int16x4 d16_1 = AE_ZERO16();
	ae_int32x2 d32_1 = AE_ZERO32();
	ae_int32x2 d32_2 = AE_ZERO32();

	AE_LA16X4_IC(d16_1, align_in[0], addr[0]);
	AE_MUL16X4(d32_1, d32_2, d16_1, gains);

	/* Saturate if exists by moving to Q31 */
	d32_1 = AE_SLAA32S(d32_1, Q10_TO_Q31_SHIFT);
	d32_2 = AE_SLAA32S(d32_2, Q10_TO_Q31_SHIFT);

	/* Returns desired samples selection */
	return AE_TRUNC16X4F32(d32_1, d32_2);
}

static inline void copier_load_slots_and_gain32(ae_int32x2 **addr, ae_valign *align_in,
						const ae_int16x4 gains, ae_int32x2 *out_d32_h,
						ae_int32x2 *out_d32_l)
{
	ae_int32x2 d32tmp_h = AE_ZERO32();
	ae_int32x2 d32tmp_l = AE_ZERO32();

	AE_LA32X2_IC(d32tmp_h, align_in[0], addr[0]);
	AE_LA32X2_IC(d32tmp_l, align_in[0], addr[0]);

	/* Apply gains */
	d32tmp_h = AE_MULFP32X16X2RAS_H(d32tmp_h, gains);
	d32tmp_l = AE_MULFP32X16X2RAS_L(d32tmp_l, gains);

	/* Gain is Q10 but treated in AE_MULFP32X16 as Q15,
	 * so we need to compensate by shifting with saturation
	 */
	*out_d32_h = AE_SLAA32S(d32tmp_h, Q10_TO_Q15_SHIFT);
	*out_d32_l = AE_SLAA32S(d32tmp_l, Q10_TO_Q15_SHIFT);
}

int copier_gain_input16(struct comp_buffer *buff, enum copier_gain_state state,
			enum copier_gain_envelope_dir dir,
			struct copier_gain_params *gain_params, uint32_t frames)
{
	uint16_t *dst = audio_stream_get_rptr(&buff->stream);
	const int nch = audio_stream_get_channels(&buff->stream);
	int samples = frames * nch;
	const ae_int16x4 gain_i16 = gain_params->gain_coeffs[0];
	ae_valign align_in = AE_ZALIGN64();
	ae_valign align_out = AE_ZALIGN64();
	ae_f16x4 gain_env = AE_ZERO16();
	ae_int16x4 *out_ptr;
	ae_int16x4 *in_ptr;
	ae_int16x4 d_r;
	ae_int16x4 d16_1;
	int rest, n, nmax;

	while (samples) {
		nmax = audio_stream_samples_without_wrap_s16(&buff->stream, dst);
		out_ptr = (ae_int16x4 *)(dst);
		in_ptr = (ae_int16x4 *)(dst);
		nmax = MIN(samples, nmax);
		rest = nmax & 0x3;

		AE_LA16X4POS_PC(align_in, in_ptr);

		switch (state) {
		case STATIC_GAIN:
			for (n = 0; n < (nmax >> 2); n++) {
				d16_1 = copier_load_slots_and_gain16(&in_ptr, &align_in, gain_i16);
				AE_SA16X4_IC(d16_1, align_out, out_ptr);
			}
			break;
		case MUTE:
			d16_1 = AE_ZERO16();
			for (size_t n = 0; n < (nmax >> 2); n++)
				AE_SA16X4_IC(d16_1, align_out, out_ptr);
			break;
		case TRANS_GAIN:
			gain_env = (int16_t)(gain_params->gain_env >> I64_TO_I16_SHIFT);
			gain_env = AE_ADD16S(gain_env, gain_params->init_gain);
			for (n = 0; n < (nmax >> 2); n++) {
				/* static gain part */
				if (!gain_params->unity_gain)
					d16_1 = copier_load_slots_and_gain16(&in_ptr, &align_in,
									     gain_i16);
				else
					AE_LA16X4_IC(d16_1, align_in, in_ptr);

				/* quadratic fade-in part */
				d16_1 = AE_MULFP16X4S(d16_1, gain_env);
				d16_1 = AE_MULFP16X4S(d16_1, gain_env);

				AE_SA16X4_IC(d16_1, align_out, out_ptr);
				if (dir == GAIN_ADD)
					gain_env = AE_ADD16S(gain_env, gain_params->step_f16);
				else
					gain_env = AE_SUB16S(gain_env, gain_params->step_f16);
			}
			break;
		}

		/* Process rest samples */
		AE_SA64POS_FP(align_out, out_ptr);
		if (rest) {
			switch (state) {
			case STATIC_GAIN:
				d_r = copier_load_slots_and_gain16(&in_ptr, &align_in, gain_i16);
				break;
			case MUTE:
				d_r = AE_ZERO16();
				break;
			case TRANS_GAIN:
				if (!gain_params->unity_gain)
					d_r = copier_load_slots_and_gain16(&in_ptr, &align_in,
									   gain_i16);
				else
					AE_LA16X4_IC(d_r, align_in, in_ptr);

				d_r = AE_MULFP16X4S(d_r, gain_env);
				d_r = AE_MULFP16X4S(d_r, gain_env);
				break;
			}

			AE_S16_0_IP(AE_MOVAD16_3(d_r), (ae_int16 *)(out_ptr), sizeof(uint16_t));
			if (rest > 1) {
				AE_S16_0_IP(AE_MOVAD16_2(d_r), (ae_int16 *)(out_ptr),
					    sizeof(uint16_t));
				if (rest > 2)
					AE_S16_0_IP(AE_MOVAD16_1(d_r), (ae_int16 *)(out_ptr), 0);
			}
		}
		samples -= nmax;
		dst = audio_stream_wrap(&buff->stream, dst + nmax);
	}

	if (state == MUTE) {
		gain_params->silence_sg_count += frames;
	} else if (state == TRANS_GAIN) {
		gain_params->fade_in_sg_count += frames;
		if (dir == GAIN_ADD)
			gain_params->gain_env += gain_params->step_i64 * frames;
		else
			gain_params->gain_env -= gain_params->step_i64 * frames;
	}
	return 0;
}

int copier_gain_input32(struct comp_buffer *buff, enum copier_gain_state state,
			enum copier_gain_envelope_dir dir,
			struct copier_gain_params *gain_params, uint32_t frames)
{
	uint32_t *dst = audio_stream_get_rptr(&buff->stream);
	const int nch = audio_stream_get_channels(&buff->stream);
	int samples = frames * nch;
	ae_int16x4 gain_i16 = gain_params->gain_coeffs[0];
	ae_valign align_in = AE_ZALIGN64();
	ae_valign align_out = AE_ZALIGN64();
	ae_int32x2 d32_h = AE_ZERO32();
	ae_int32x2 d32_l = AE_ZERO32();
	ae_int32x2 r_d32_h = AE_ZERO32();
	ae_int32x2 r_d32_l = AE_ZERO32();
	ae_f16x4 gain_env = AE_ZERO16();
	ae_int32x2 *out_ptr;
	ae_int32x2 *in_ptr;
	int rest, n, nmax;

	while (samples) {
		nmax = audio_stream_samples_without_wrap_s32(&buff->stream, dst);
		out_ptr = (ae_int32x2 *)(dst);
		in_ptr = (ae_int32x2 *)(dst);
		nmax = MIN(samples, nmax);
		rest = nmax & 0x3;

		/* Align input pointer access */
		AE_LA32X2POS_PC(align_in, in_ptr);

		switch (state) {
		case STATIC_GAIN:
			for (n = 0; n < (nmax >> 2); n++) {
				copier_load_slots_and_gain32(&in_ptr, &align_in, gain_i16,
							     &d32_h, &d32_l);
				AE_SA32X2_IC(d32_h, align_out, out_ptr);
				AE_SA32X2_IC(d32_l, align_out, out_ptr);
			}
			break;
		case MUTE:
			d32_l = AE_ZERO32();
			for (size_t n = 0; n < (nmax >> 2); n++) {
				AE_SA32X2_IC(d32_l, align_out, out_ptr);
				AE_SA32X2_IC(d32_l, align_out, out_ptr);
			}
			break;
		case TRANS_GAIN:
			gain_env = (int16_t)(gain_params->gain_env >> I64_TO_I16_SHIFT);
			gain_env = AE_ADD16S(gain_env, gain_params->init_gain);
			for (n = 0; n < (nmax >> 2); n++) {
				/* static gain part */
				if (!gain_params->unity_gain) {
					copier_load_slots_and_gain32(&in_ptr, &align_in, gain_i16,
								     &d32_h, &d32_l);
				} else {
					AE_LA32X2_IC(d32_h, align_in, in_ptr);
					AE_LA32X2_IC(d32_l, align_in, in_ptr);
				}
				/* quadratic fade-in part */
				d32_h = AE_MULFP32X16X2RAS_H(d32_h, gain_env);
				d32_h = AE_MULFP32X16X2RAS_H(d32_h, gain_env);
				d32_l = AE_MULFP32X16X2RAS_L(d32_l, gain_env);
				d32_l = AE_MULFP32X16X2RAS_L(d32_l, gain_env);
				AE_SA32X2_IC(d32_h, align_out, out_ptr);
				AE_SA32X2_IC(d32_l, align_out, out_ptr);

				if (dir == GAIN_ADD)
					gain_env = AE_ADD16S(gain_env, gain_params->step_f16);
				else
					gain_env = AE_SUB16S(gain_env, gain_params->step_f16);
			}
			break;
		default:
			return -EINVAL;
		}

		AE_SA64POS_FP(align_out, out_ptr);
		if (rest) {
			switch (state) {
			case STATIC_GAIN:
				copier_load_slots_and_gain32(&in_ptr, &align_in, gain_i16,
							     &r_d32_h, &r_d32_l);
				break;
			case MUTE:
				break;
			case TRANS_GAIN:
				if (!gain_params->unity_gain) {
					copier_load_slots_and_gain32(&in_ptr, &align_in, gain_i16,
								     &r_d32_h, &r_d32_l);
				} else {
					AE_LA32X2_IC(r_d32_h, align_in, in_ptr);
					AE_LA32X2_IC(r_d32_l, align_in, in_ptr);
				}
				r_d32_h = AE_MULFP32X16X2RAS_H(r_d32_h, gain_env);
				r_d32_h = AE_MULFP32X16X2RAS_H(r_d32_h, gain_env);
				r_d32_l = AE_MULFP32X16X2RAS_L(r_d32_l, gain_env);
				r_d32_l = AE_MULFP32X16X2RAS_L(r_d32_l, gain_env);
				break;
			}

			if (rest > 1) {
				AE_SA32X2_IC(r_d32_h, align_out, out_ptr);
				AE_SA64POS_FP(align_out, out_ptr);

				if (rest > 2) {
					ae_int32 tmp = AE_MOVAD32_H(r_d32_l);

					AE_S32_L_XC(tmp, (ae_int32 *)out_ptr, 0);
				}
			} else {
				ae_int32 tmp = AE_MOVAD32_H(r_d32_h);

				AE_S32_L_XC(tmp, (ae_int32 *)out_ptr, 0);
			}
		}
		samples -= nmax;
		dst = audio_stream_wrap(&buff->stream, dst + nmax);
	}

	if (state == MUTE) {
		gain_params->silence_sg_count += frames;
	} else if (state == TRANS_GAIN) {
		gain_params->fade_in_sg_count += frames;
		if (dir == GAIN_ADD)
			gain_params->gain_env += gain_params->step_i64 * frames;
		else
			gain_params->gain_env -= gain_params->step_i64 * frames;
	}

	return 0;
}

bool copier_is_unity_gain(struct copier_gain_params *gain_params)
{
	ae_int16x4 gain_coeffs = AE_MOVF16X4_FROMINT64(UNITY_GAIN_4X_Q10);
	xtbool4 unity_gain_check = AE_EQ16(gain_params->gain_coeffs[0], gain_coeffs);

	return XT_ALL4(unity_gain_check) ? true : false;
}

#endif
