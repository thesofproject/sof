// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.
//
// Modern WebRTC AEC3 (C++17) backend for the webrtc_aec module.
//
// Wraps EchoCanceller3 via pure C ABI wrapper functions:
//   webrtc_aec3_create()        — instantiate AEC3 with standard defaults
//   webrtc_aec3_buffer_farend() — queue 10 ms of float echo reference (render)
//   webrtc_aec3_process()       — process capture frame and cancel echo
//   webrtc_aec3_free()          — release handle
//
// Single-precision floating-point operations lowered to native hardware FPU
// instructions on Panther Lake (intel_ace30_ptl).

#include <sof/audio/module_adapter/module/generic.h>
#include <errno.h>
#include "webrtc_aec.h"
#include "webrtc_aec3_wrapper.h"

LOG_MODULE_DECLARE(webrtc_aec, CONFIG_SOF_LOG_LEVEL);

struct webrtc_aec3_data {
	webrtc_aec3_inst_t *aec3;
	int     num_mic_channels;
	int     num_ref_channels;
	int     suppression;
	uint32_t aec3_frames;
	uint64_t aec3_ref_cyc;
	uint64_t aec3_mic_cyc;
	float   ref_f[WEBRTC_AEC_CHANNELS_MAX][WEBRTC_AEC_FRAME_SAMPLES_16K];
	float   mic_f[WEBRTC_AEC_CHANNELS_MAX][WEBRTC_AEC_FRAME_SAMPLES_16K];
	float   out_f[WEBRTC_AEC_CHANNELS_MAX][WEBRTC_AEC_FRAME_SAMPLES_16K];
};

static int webrtc_aec3_backend_init(struct processing_module *mod)
{
	struct webrtc_aec_comp_data *cd = module_get_private_data(mod);
	struct webrtc_aec3_data *rd;

	rd = mod_zalloc(mod, sizeof(*rd));
	if (!rd)
		return -ENOMEM;

	cd->backend_data = rd;
	comp_info(mod->dev, "webrtc_aec: Modern AEC3 (C++17) backend initialised");
	return 0;
}

static int webrtc_aec3_backend_configure(struct processing_module *mod, int sample_rate_hz,
					 int filter_len_ms, int suppression,
					 int num_mic_channels, int num_ref_channels)
{
	struct webrtc_aec_comp_data *cd = module_get_private_data(mod);
	struct webrtc_aec3_data *rd = cd->backend_data;
	struct webrtc_aec3_cfg cfg = {
		.sample_rate_hz = sample_rate_hz,
		.num_render_channels = (num_ref_channels > WEBRTC_AEC_CHANNELS_MAX) ?
					WEBRTC_AEC_CHANNELS_MAX : num_ref_channels,
		.num_capture_channels = (num_mic_channels > WEBRTC_AEC_CHANNELS_MAX) ?
					 WEBRTC_AEC_CHANNELS_MAX : num_mic_channels,
		.delay_ms = cd->delay_ms,
#ifdef CONFIG_WEBRTC_AEC_AEC3_FILTER_BLOCKS
		.filter_length_blocks = CONFIG_WEBRTC_AEC_AEC3_FILTER_BLOCKS,
#else
		.filter_length_blocks = 12,
#endif
		.use_external_delay_estimator = IS_ENABLED(CONFIG_WEBRTC_AEC_AEC3_EXTERNAL_DELAY_EST),
		.enable_coarse_filter = IS_ENABLED(CONFIG_WEBRTC_AEC_AEC3_COARSE_FILTER),
		.detect_stereo_content = IS_ENABLED(CONFIG_WEBRTC_AEC_STEREO_CONTENT_DETECT),
		.high_suppression = (cd->high_suppression || suppression > 1),
	};

	if (rd->aec3) {
		webrtc_aec3_free(rd->aec3);
		rd->aec3 = NULL;
	}

	rd->num_mic_channels = 0;
	rd->num_ref_channels = 0;
	rd->suppression = suppression;
	rd->aec3_frames = 0;
	rd->aec3_ref_cyc = 0;
	rd->aec3_mic_cyc = 0;

	rd->aec3 = webrtc_aec3_create_cfg(&cfg);
	if (!rd->aec3) {
		comp_err(mod->dev, "webrtc_aec: webrtc_aec3_create_cfg() failed rate=%d mic=%d ref=%d",
			 sample_rate_hz, cfg.num_capture_channels, cfg.num_render_channels);
		return -ENOMEM;
	}

	rd->num_mic_channels = cfg.num_capture_channels;
	rd->num_ref_channels = cfg.num_render_channels;
	comp_info(mod->dev, "webrtc_aec: AEC3 configured rate=%d mic=%d ref=%d blocks=%d ext_delay=%d stereo_det=%d",
		  sample_rate_hz, rd->num_mic_channels, rd->num_ref_channels,
		  cfg.filter_length_blocks, cfg.use_external_delay_estimator,
		  cfg.detect_stereo_content);
	return 0;
}

static int webrtc_aec3_backend_set_suppression(struct processing_module *mod, bool high_suppression)
{
	struct webrtc_aec_comp_data *cd = module_get_private_data(mod);
	struct webrtc_aec3_data *rd = cd->backend_data;

	if (!rd || !rd->aec3)
		return 0;

	webrtc_aec3_set_suppression(rd->aec3, high_suppression);
	comp_info(mod->dev, "webrtc_aec: updated AEC3 suppression high=%d", high_suppression);
	return 0;
}

static int webrtc_aec3_backend_set_delay(struct processing_module *mod, int delay_ms)
{
	struct webrtc_aec_comp_data *cd = module_get_private_data(mod);
	struct webrtc_aec3_data *rd = cd->backend_data;

	if (!rd || !rd->aec3)
		return 0;

	return webrtc_aec3_set_delay(rd->aec3, delay_ms);
}

static int webrtc_aec3_backend_get_metrics(struct processing_module *mod,
					   int *erl_db, int *erle_db, int *delay_ms)
{
	struct webrtc_aec_comp_data *cd = module_get_private_data(mod);
	struct webrtc_aec3_data *rd = cd->backend_data;

	if (!rd || !rd->aec3)
		return -EINVAL;

	return webrtc_aec3_get_metrics(rd->aec3, erl_db, erle_db, delay_ms);
}

static int webrtc_aec3_backend_process_frame(struct processing_module *mod,
					     const int16_t *const *mic,
					     const int16_t *const *ref,
					     int16_t *const *out,
					     int frame_samples,
					     int num_mic_channels, int num_ref_channels)
{
	struct webrtc_aec_comp_data *cd = module_get_private_data(mod);
	struct webrtc_aec3_data *rd = cd->backend_data;
	const float *nearend[WEBRTC_AEC_CHANNELS_MAX];
	const float *farend[WEBRTC_AEC_CHANNELS_MAX];
	float *out_ptrs[WEBRTC_AEC_CHANNELS_MAX];
	int i, ch, ret;
#if IS_ENABLED(CONFIG_WEBRTC_AEC_PERF_TELEMETRY)
	uint32_t t0, t1, t2;
#endif

	if (frame_samples <= 0 || frame_samples > WEBRTC_AEC_FRAME_SAMPLES_16K ||
	    num_mic_channels <= 0 || num_mic_channels > WEBRTC_AEC_CHANNELS_MAX ||
	    num_ref_channels <= 0 || num_ref_channels > WEBRTC_AEC_CHANNELS_MAX)
		return -EINVAL;

	if (num_mic_channels != rd->num_mic_channels ||
	    num_ref_channels != rd->num_ref_channels || !rd->aec3) {
		for (ch = 0; ch < num_mic_channels; ch++)
			memcpy(out[ch], mic[ch], (size_t)frame_samples * sizeof(int16_t));
		return 0;
	}

	/* WebRTC AudioBuffer operates on float samples in the [-32768, 32767] range. */
	for (ch = 0; ch < num_ref_channels; ch++) {
		for (i = 0; i < frame_samples; i++)
			rd->ref_f[ch][i] = (float)ref[ch][i];
		farend[ch] = rd->ref_f[ch];
	}

	for (ch = 0; ch < num_mic_channels; ch++) {
		for (i = 0; i < frame_samples; i++)
			rd->mic_f[ch][i] = (float)mic[ch][i];
		nearend[ch] = rd->mic_f[ch];
		out_ptrs[ch] = rd->out_f[ch];
	}

	/* Feed all render channels first so AEC3's block aligner sees the current far-end frame. */
#if IS_ENABLED(CONFIG_WEBRTC_AEC_PERF_TELEMETRY)
	t0 = k_cycle_get_32();
#endif
	ret = webrtc_aec3_buffer_farend(rd->aec3, farend, (size_t)num_ref_channels,
					(size_t)frame_samples);
#if IS_ENABLED(CONFIG_WEBRTC_AEC_PERF_TELEMETRY)
	t1 = k_cycle_get_32();
#endif
	if (ret) {
		comp_err(mod->dev, "webrtc_aec: AEC3 BufferFarend failed %d", ret);
		return ret;
	}

	ret = webrtc_aec3_process(rd->aec3, nearend, (size_t)num_mic_channels,
				  out_ptrs, (size_t)frame_samples);
#if IS_ENABLED(CONFIG_WEBRTC_AEC_PERF_TELEMETRY)
	t2 = k_cycle_get_32();
#endif
	if (ret) {
		comp_err(mod->dev, "webrtc_aec: AEC3 Process failed %d", ret);
		return ret;
	}

#if IS_ENABLED(CONFIG_WEBRTC_AEC_PERF_TELEMETRY)
	rd->aec3_ref_cyc += (uint64_t)(t1 - t0);
	rd->aec3_mic_cyc += (uint64_t)(t2 - t1);
	rd->aec3_frames++;
	if (rd->aec3_frames % 20 == 0) {
		comp_info(mod->dev, "webrtc_aec3 (%dx%d) perf: f=%u ref=%u kcyc mic=%u kcyc tot=%u kcyc",
			  num_ref_channels, num_mic_channels, rd->aec3_frames,
			  (uint32_t)(rd->aec3_ref_cyc / (rd->aec3_frames * 1000)),
			  (uint32_t)(rd->aec3_mic_cyc / (rd->aec3_frames * 1000)),
			  (uint32_t)((rd->aec3_ref_cyc + rd->aec3_mic_cyc) / (rd->aec3_frames * 1000)));
	}
#endif

	/* Saturate float capture output back to S16 before pipeline FIFO writeback. */
	for (ch = 0; ch < num_mic_channels; ch++) {
		for (i = 0; i < frame_samples; i++) {
			float val = rd->out_f[ch][i];
			if (val > 32767.0f)
				out[ch][i] = 32767;
			else if (val < -32768.0f)
				out[ch][i] = -32768;
			else
				out[ch][i] = (int16_t)(val + (val >= 0.0f ? 0.5f : -0.5f));
		}
	}

	return 0;
}

static int webrtc_aec3_backend_process_ch(struct processing_module *mod,
					  const int16_t *mic, const int16_t *ref, int16_t *out,
					  int frame_samples, int ch)
{
	const int16_t *mic_arr[1] = { mic };
	const int16_t *ref_arr[1] = { ref };
	int16_t *out_arr[1] = { out };

	(void)ch;
	return webrtc_aec3_backend_process_frame(mod, mic_arr, ref_arr, out_arr,
						 frame_samples, 1, 1);
}

static int webrtc_aec3_backend_reset(struct processing_module *mod)
{
	struct webrtc_aec_comp_data *cd = module_get_private_data(mod);
	struct webrtc_aec3_data *rd = cd->backend_data;

	if (rd && rd->aec3)
		webrtc_aec3_init(rd->aec3, cd->proc_rate);
	return 0;
}

static int webrtc_aec3_backend_free(struct processing_module *mod)
{
	struct webrtc_aec_comp_data *cd = module_get_private_data(mod);
	struct webrtc_aec3_data *rd = cd->backend_data;

	if (!rd)
		return 0;

	if (rd->aec3) {
		webrtc_aec3_free(rd->aec3);
		rd->aec3 = NULL;
	}
	mod_free(mod, rd);
	cd->backend_data = NULL;
	return 0;
}

const struct webrtc_aec_backend webrtc_aec_backend = {
	.name            = "aec3",
	.init            = webrtc_aec3_backend_init,
	.configure       = webrtc_aec3_backend_configure,
	.set_suppression = webrtc_aec3_backend_set_suppression,
	.set_delay       = webrtc_aec3_backend_set_delay,
	.get_metrics     = webrtc_aec3_backend_get_metrics,
	.process_ch      = webrtc_aec3_backend_process_ch,
	.process_frame   = webrtc_aec3_backend_process_frame,
	.reset           = webrtc_aec3_backend_reset,
	.free            = webrtc_aec3_backend_free,
};
