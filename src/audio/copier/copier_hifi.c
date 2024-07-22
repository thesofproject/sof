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

void copier_gain_set_basic_params(struct comp_dev *dev, struct dai_data *dd,
				  struct ipc4_base_module_cfg *ipc4_cfg)
{
	struct copier_gain_params *gain_params = dd->gain_data;

	/* Set default gain coefficients */
	for (int i = 0; i < ARRAY_SIZE(gain_params->gain_coeffs); ++i)
		gain_params->gain_coeffs[i] = AE_MOVF16X4_FROMINT64(UNITY_GAIN_4X_Q10);

	gain_params->step_f16 = AE_ZERO16();
	gain_params->init_gain = AE_ZERO16();

	/* Set channels count */
	gain_params->channels_count = ipc4_cfg->audio_fmt.channels_count;
}

int copier_gain_set_fade_params(struct comp_dev *dev, struct dai_data *dd,
				struct ipc4_base_module_cfg *ipc4_cfg,
				uint32_t fade_period, uint32_t frames)
{
	struct copier_gain_params *gain_params = dd->gain_data;
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

#endif
