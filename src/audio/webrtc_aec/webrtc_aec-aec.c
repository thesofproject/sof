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
#include <webrtc/modules/audio_processing/aec/aec_core.h>

LOG_MODULE_DECLARE(webrtc_aec, CONFIG_SOF_LOG_LEVEL);

struct webrtc_aec_float_data {
	void   *aec[WEBRTC_AEC_CHANNELS_MAX];
	int     num_mic_channels;
	int     num_ref_channels;
	int     suppression;
	int     delay_ms;
	float   ref_f[WEBRTC_AEC_CHANNELS_MAX][WEBRTC_AEC_FRAME_SAMPLES_16K];
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

	rd->delay_ms = cd->delay_ms;
	cd->backend_data = rd;
	comp_info(mod->dev, "webrtc_aec: Classic Float AEC backend initialised");
	return 0;
}

static int webrtc_aec_float_apply_cfg(struct processing_module *mod,
					struct webrtc_aec_float_data *rd,
					void *inst, int ch)
{
	struct webrtc_aec_comp_data *cd = module_get_private_data(mod);
	struct AecCore *core = WebRtcAec_aec_core(inst);
	AecConfig config;
	int ret;

	if (core && IS_ENABLED(CONFIG_WEBRTC_AEC_DELAY_AGNOSTIC)) {
		WebRtcAec_enable_delay_agnostic(core, 1);
		WebRtcAec_enable_extended_filter(core, 1);
	}

	config.nlpMode = (cd->high_suppression || rd->suppression > 1) ? kAecNlpAggressive :
			 (rd->suppression == 0) ? kAecNlpConservative : kAecNlpModerate;
	config.skewMode = kAecFalse;
	config.metricsMode = kAecTrue;
	config.delay_logging = kAecFalse;

	ret = WebRtcAec_set_config(inst, config);
	if (ret) {
		comp_err(mod->dev, "webrtc_aec: set_config ch%d failed %d", ch, ret);
		return ret;
	}

	return 0;
}

static int webrtc_aec_float_configure(struct processing_module *mod, int sample_rate_hz,
				      int filter_len_ms, int suppression,
				      int num_mic_channels, int num_ref_channels)
{
	struct webrtc_aec_comp_data *cd = module_get_private_data(mod);
	struct webrtc_aec_float_data *rd = cd->backend_data;
	int c, ret;

	/* Free previously allocated handles. */
	for (c = 0; c < rd->num_mic_channels; c++) {
		if (rd->aec[c]) {
			WebRtcAec_Free(rd->aec[c]);
			rd->aec[c] = NULL;
		}
	}
	rd->num_mic_channels = 0;
	rd->num_ref_channels = num_ref_channels;
	rd->suppression      = suppression;
	rd->delay_ms         = cd->delay_ms;

	/* Classic Float AEC is single-capture-channel per handle, so allocate one per mic. */
	for (c = 0; c < num_mic_channels; c++) {
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

		if (webrtc_aec_float_apply_cfg(mod, rd, rd->aec[c], c))
			goto err;
	}

	rd->num_mic_channels = num_mic_channels;
	comp_info(mod->dev, "webrtc_aec: Float AEC rate=%d high_supp=%d mic_ch=%d ref_ch=%d delay=%dms",
		  sample_rate_hz, cd->high_suppression, num_mic_channels, num_ref_channels,
		  rd->delay_ms);
	return 0;

err:
	for (c = 0; c < num_mic_channels; c++) {
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
	config.metricsMode = kAecTrue;
	config.delay_logging = kAecFalse;

	for (c = 0; c < rd->num_mic_channels; c++) {
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

static int webrtc_aec_float_set_delay(struct processing_module *mod, int delay_ms)
{
	struct webrtc_aec_comp_data *cd = module_get_private_data(mod);
	struct webrtc_aec_float_data *rd = cd->backend_data;

	if (!rd)
		return 0;

	rd->delay_ms = delay_ms;
	return 0;
}

static int webrtc_aec_float_get_metrics(struct processing_module *mod,
					int *erl_db, int *erle_db, int *delay_ms)
{
	struct webrtc_aec_comp_data *cd = module_get_private_data(mod);
	struct webrtc_aec_float_data *rd = cd->backend_data;
	AecMetrics m;

	if (!rd || !rd->aec[0])
		return -EINVAL;

	if (WebRtcAec_GetMetrics(rd->aec[0], &m) != 0)
		return -EINVAL;

	if (erl_db)
		*erl_db = m.erl.instant;
	if (erle_db)
		*erle_db = m.erle.instant;
	if (delay_ms)
		*delay_ms = rd->delay_ms;
	return 0;
}

static int webrtc_aec_float_run_ch(struct processing_module *mod,
				   struct webrtc_aec_float_data *rd,
				   const int16_t *mic, const float *ref_f,
				   int16_t *out, int frame_samples, int ch)
{
	const float *const nearend[1] = { rd->mic_f };
	float *const out_ptrs[1] = { rd->out_f };
	int i, ret;

	if (ch >= rd->num_mic_channels || !rd->aec[ch]) {
		memcpy(out, mic, (size_t)frame_samples * sizeof(int16_t));
		return 0;
	}

	/* Queue the far-end reference block before running near-end echo cancellation. */
	ret = WebRtcAec_BufferFarend(rd->aec[ch], ref_f, (size_t)frame_samples);
	if (ret) {
		comp_err(mod->dev, "webrtc_aec: BufferFarend ch%d failed %d", ch, ret);
		return ret;
	}

	for (i = 0; i < frame_samples; i++)
		rd->mic_f[i] = (float)mic[i];

	ret = WebRtcAec_Process(rd->aec[ch], nearend, 1, out_ptrs,
				(size_t)frame_samples, (int16_t)rd->delay_ms, 0);
	if (ret) {
		comp_err(mod->dev, "webrtc_aec: Process ch%d failed %d", ch, ret);
		return ret;
	}

	/* Round and saturate float output back into signed 16-bit PCM range. */
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

static int webrtc_aec_float_process_frame(struct processing_module *mod,
					  const int16_t *const *mic,
					  const int16_t *const *ref,
					  int16_t *const *out,
					  int frame_samples,
					  int num_mic_channels, int num_ref_channels)
{
	struct webrtc_aec_comp_data *cd = module_get_private_data(mod);
	struct webrtc_aec_float_data *rd = cd->backend_data;
	int i, c, ret;

	if (frame_samples <= 0 || frame_samples > WEBRTC_AEC_FRAME_SAMPLES_16K ||
	    num_mic_channels <= 0 || num_mic_channels > WEBRTC_AEC_CHANNELS_MAX ||
	    num_ref_channels <= 0 || num_ref_channels > WEBRTC_AEC_CHANNELS_MAX)
		return -EINVAL;

	/*
	 * Convert reference to float once outside the mic loop: use per-channel ref
	 * when ref and mic counts match, otherwise downmix multi-channel ref to mono.
	 */
	if (num_ref_channels == num_mic_channels) {
		for (c = 0; c < num_ref_channels; c++) {
			for (i = 0; i < frame_samples; i++)
				rd->ref_f[c][i] = (float)ref[c][i];
		}
	} else if (num_ref_channels == 1) {
		for (i = 0; i < frame_samples; i++)
			rd->ref_f[0][i] = (float)ref[0][i];
	} else {
		float inv = 1.0f / (float)num_ref_channels;

		for (i = 0; i < frame_samples; i++) {
			float acc = 0.0f;

			for (c = 0; c < num_ref_channels; c++)
				acc += (float)ref[c][i];
			rd->ref_f[0][i] = acc * inv;
		}
	}

	for (c = 0; c < num_mic_channels; c++) {
		const float *ref_f = (num_ref_channels == num_mic_channels) ?
			rd->ref_f[c] : rd->ref_f[0];

		ret = webrtc_aec_float_run_ch(mod, rd, mic[c], ref_f, out[c], frame_samples, c);
		if (ret)
			return ret;
	}

	return 0;
}

static int webrtc_aec_float_process_ch(struct processing_module *mod,
				       const int16_t *mic, const int16_t *ref, int16_t *out,
				       int frame_samples, int ch)
{
	struct webrtc_aec_comp_data *cd = module_get_private_data(mod);
	struct webrtc_aec_float_data *rd = cd->backend_data;
	int i;

	if (frame_samples > WEBRTC_AEC_FRAME_SAMPLES_16K)
		return -EINVAL;

	for (i = 0; i < frame_samples; i++)
		rd->ref_f[0][i] = (float)ref[i];

	return webrtc_aec_float_run_ch(mod, rd, mic, rd->ref_f[0], out, frame_samples, ch);
}

static int webrtc_aec_float_reset(struct processing_module *mod)
{
	struct webrtc_aec_comp_data *cd = module_get_private_data(mod);
	struct webrtc_aec_float_data *rd = cd->backend_data;
	int c, ret;

	for (c = 0; c < rd->num_mic_channels; c++) {
		if (!rd->aec[c])
			continue;
		ret = WebRtcAec_Init(rd->aec[c], cd->proc_rate, cd->proc_rate);
		if (ret) {
			comp_warn(mod->dev, "webrtc_aec: reset ch%d failed %d", c, ret);
			continue;
		}
		webrtc_aec_float_apply_cfg(mod, rd, rd->aec[c], c);
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

	for (c = 0; c < rd->num_mic_channels; c++) {
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
	.set_delay       = webrtc_aec_float_set_delay,
	.get_metrics     = webrtc_aec_float_get_metrics,
	.process_ch      = webrtc_aec_float_process_ch,
	.process_frame   = webrtc_aec_float_process_frame,
	.reset           = webrtc_aec_float_reset,
	.free            = webrtc_aec_float_free,
};
