// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.
//
// Real Classic Float AEC backend for the webrtc_aec module.
//
// Wraps WebRtcAec_Create/Init/BufferFarend/Process from the WebRTC AEC
// (Partitioned Block Frequency Domain MDF) floating-point library.
//
// API summary:
//   WebRtcAec_Create()       — allocate Aec handle
//   WebRtcAec_Init()         — configure sample rate (8000, 16000, 32000, 48000 Hz)
//   WebRtcAec_BufferFarend() — queue 10 ms of float echo reference (far-end)
//   WebRtcAec_Process()      — cancel echo from near-end mic frame (float)
//   WebRtcAec_set_config()   — configure NLP suppression mode
//   WebRtcAec_Free()         — release handle
//
// Single-precision floating-point operations lowered to native hardware FPU
// instructions on Panther Lake (intel_ace30_ptl). One handle per channel.

#include <sof/audio/module_adapter/module/generic.h>
#include <errno.h>
#include "webrtc_aec.h"

#include <webrtc/modules/audio_processing/aec/include/echo_cancellation.h>

LOG_MODULE_DECLARE(webrtc_aec, CONFIG_SOF_LOG_LEVEL);

struct webrtc_aec_float_data {
	void   *aec[WEBRTC_AEC_CHANNELS_MAX];
	int     num_channels;
	int     suppression;
	float   ref_f[WEBRTC_AEC_FRAME_SAMPLES_16K];
	float   mic_f[WEBRTC_AEC_FRAME_SAMPLES_16K];
	float   out_f[WEBRTC_AEC_FRAME_SAMPLES_16K];
};

static int webrtc_aec_float_init(struct processing_module *mod)
{
	struct webrtc_aec_comp_data *cd = module_get_private_data(mod);
	struct webrtc_aec_float_data *rd;

	rd = mod_zalloc(mod, sizeof(*rd));
	if (!rd)
		return -ENOMEM;

	cd->backend_data = rd;
	comp_info(mod->dev, "webrtc_aec: Classic Float AEC backend initialised");
	return 0;
}

static int webrtc_aec_float_configure(struct processing_module *mod, int sample_rate_hz,
				      int filter_len_ms, int suppression, int num_channels)
{
	struct webrtc_aec_comp_data *cd = module_get_private_data(mod);
	struct webrtc_aec_float_data *rd = cd->backend_data;
	AecConfig config;
	int c, ret;

	/* Free previously allocated handles. */
	for (c = 0; c < rd->num_channels; c++) {
		if (rd->aec[c]) {
			WebRtcAec_Free(rd->aec[c]);
			rd->aec[c] = NULL;
		}
	}
	rd->num_channels = 0;
	rd->suppression  = suppression;

	int aec_channels = (num_channels > 1) ? 1 : num_channels;

	for (c = 0; c < aec_channels; c++) {
		rd->aec[c] = WebRtcAec_Create();
		if (!rd->aec[c]) {
			comp_err(mod->dev, "webrtc_aec: WebRtcAec_Create() failed ch%d", c);
			goto err;
		}

		ret = WebRtcAec_Init(rd->aec[c], sample_rate_hz, sample_rate_hz);
		if (ret) {
			comp_err(mod->dev,
				 "webrtc_aec: WebRtcAec_Init(ch%d, %d) failed %d",
				 c, sample_rate_hz, ret);
			goto err;
		}

		config.nlpMode = (cd->high_suppression || suppression > 1) ? kAecNlpAggressive :
				 (suppression == 0) ? kAecNlpConservative : kAecNlpModerate;
		config.skewMode = kAecFalse;
		config.metricsMode = kAecFalse;
		config.delay_logging = kAecFalse;

		ret = WebRtcAec_set_config(rd->aec[c], config);
		if (ret) {
			comp_err(mod->dev, "webrtc_aec: set_config ch%d failed %d", c, ret);
			goto err;
		}
	}

	rd->num_channels = aec_channels;
	comp_info(mod->dev, "webrtc_aec: Float AEC rate=%d nlpMode=%d ch=%d (stream_ch=%d)",
		  sample_rate_hz, config.nlpMode, aec_channels, num_channels);
	return 0;

err:
	for (c = 0; c < num_channels; c++) {
		if (rd->aec[c]) {
			WebRtcAec_Free(rd->aec[c]);
			rd->aec[c] = NULL;
		}
	}
	return -ENOMEM;
}

static int webrtc_aec_float_set_suppression(struct processing_module *mod, bool high_suppression)
{
	struct webrtc_aec_comp_data *cd = module_get_private_data(mod);
	struct webrtc_aec_float_data *rd = cd->backend_data;
	AecConfig config;
	int c, ret;

	if (!rd)
		return 0;

	config.nlpMode = high_suppression ? kAecNlpAggressive : kAecNlpModerate;
	config.skewMode = kAecFalse;
	config.metricsMode = kAecFalse;
	config.delay_logging = kAecFalse;

	for (c = 0; c < rd->num_channels; c++) {
		if (!rd->aec[c])
			continue;
		ret = WebRtcAec_set_config(rd->aec[c], config);
		if (ret) {
			comp_err(mod->dev, "webrtc_aec: set_config failed %d ch%d", ret, c);
			return ret;
		}
	}

	comp_info(mod->dev, "webrtc_aec: updated float suppression high=%d nlpMode=%d",
		  high_suppression, config.nlpMode);
	return 0;
}

static int webrtc_aec_float_process_ch(struct processing_module *mod,
				       const int16_t *mic, const int16_t *ref, int16_t *out,
				       int frame_samples, int ch)
{
	struct webrtc_aec_comp_data *cd = module_get_private_data(mod);
	struct webrtc_aec_float_data *rd = cd->backend_data;
	const float *const nearend[1] = { rd->mic_f };
	float *const out_ptrs[1] = { rd->out_f };
	int i, ret;

	if (frame_samples > WEBRTC_AEC_FRAME_SAMPLES_16K)
		return -EINVAL;

	if (ch >= rd->num_channels || !rd->aec[ch]) {
		memcpy(out, mic, (size_t)frame_samples * sizeof(int16_t));
		return 0;
	}

	/* Convert ref to float and buffer far-end */
	for (i = 0; i < frame_samples; i++)
		rd->ref_f[i] = (float)ref[i];

	ret = WebRtcAec_BufferFarend(rd->aec[ch], rd->ref_f, (size_t)frame_samples);
	if (ret) {
		comp_err(mod->dev, "webrtc_aec: BufferFarend ch%d failed %d", ch, ret);
		return ret;
	}

	/* Convert mic to float */
	for (i = 0; i < frame_samples; i++)
		rd->mic_f[i] = (float)mic[i];


	/* Process near-end (mic) and produce echo-cancelled output.
	 * Pass 20 ms nominal sound card buffer delay. */
	ret = WebRtcAec_Process(rd->aec[ch], nearend, 1, out_ptrs,
				(size_t)frame_samples, 20, 0);
	if (ret) {
		comp_err(mod->dev, "webrtc_aec: Process ch%d failed %d", ch, ret);
		return ret;
	}

	/* Convert float output back to int16_t with saturation and rounding */
	for (i = 0; i < frame_samples; i++) {
		float val = rd->out_f[i];
		if (val > 32767.0f)
			out[i] = 32767;
		else if (val < -32768.0f)
			out[i] = -32768;
		else
			out[i] = (int16_t)(val + (val >= 0.0f ? 0.5f : -0.5f));
	}

	return 0;
}

static int webrtc_aec_float_reset(struct processing_module *mod)
{
	struct webrtc_aec_comp_data *cd = module_get_private_data(mod);
	struct webrtc_aec_float_data *rd = cd->backend_data;
	int c, ret;

	for (c = 0; c < rd->num_channels; c++) {
		if (!rd->aec[c])
			continue;
		ret = WebRtcAec_Init(rd->aec[c], cd->proc_rate, cd->proc_rate);
		if (ret)
			comp_warn(mod->dev, "webrtc_aec: reset ch%d failed %d", c, ret);
	}
	return 0;
}

static int webrtc_aec_float_free(struct processing_module *mod)
{
	struct webrtc_aec_comp_data *cd = module_get_private_data(mod);
	struct webrtc_aec_float_data *rd = cd->backend_data;
	int c;

	if (!rd)
		return 0;

	for (c = 0; c < rd->num_channels; c++) {
		if (rd->aec[c]) {
			WebRtcAec_Free(rd->aec[c]);
			rd->aec[c] = NULL;
		}
	}
	mod_free(mod, rd);
	cd->backend_data = NULL;
	return 0;
}

const struct webrtc_aec_backend webrtc_aec_backend = {
	.name            = "aec_float",
	.init            = webrtc_aec_float_init,
	.configure       = webrtc_aec_float_configure,
	.set_suppression = webrtc_aec_float_set_suppression,
	.process_ch      = webrtc_aec_float_process_ch,
	.reset           = webrtc_aec_float_reset,
	.free            = webrtc_aec_float_free,
};
