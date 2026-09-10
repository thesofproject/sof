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
	int     num_channels;
	int     suppression;
	float   ref_f[WEBRTC_AEC_FRAME_SAMPLES_16K];
	float   mic_f[WEBRTC_AEC_FRAME_SAMPLES_16K];
	float   out_f[WEBRTC_AEC_FRAME_SAMPLES_16K];
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
					 int filter_len_ms, int suppression, int num_channels)
{
	struct webrtc_aec_comp_data *cd = module_get_private_data(mod);
	struct webrtc_aec3_data *rd = cd->backend_data;

	if (rd->aec3) {
		webrtc_aec3_free(rd->aec3);
		rd->aec3 = NULL;
	}

	rd->num_channels = 0;
	rd->suppression  = suppression;

	int aec_channels = (num_channels > 1) ? 1 : num_channels;

	rd->aec3 = webrtc_aec3_create(sample_rate_hz, aec_channels);
	if (!rd->aec3) {
		comp_err(mod->dev, "webrtc_aec: webrtc_aec3_create() failed rate=%d", sample_rate_hz);
		return -ENOMEM;
	}

	rd->num_channels = aec_channels;
	comp_info(mod->dev, "webrtc_aec: AEC3 configured rate=%d ch=%d (stream_ch=%d)",
		  sample_rate_hz, aec_channels, num_channels);
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

static int webrtc_aec3_backend_process_ch(struct processing_module *mod,
					  const int16_t *mic, const int16_t *ref, int16_t *out,
					  int frame_samples, int ch)
{
	struct webrtc_aec_comp_data *cd = module_get_private_data(mod);
	struct webrtc_aec3_data *rd = cd->backend_data;
	const float *const nearend[1] = { rd->mic_f };
	float *const out_ptrs[1] = { rd->out_f };
	int i, ret;

	static uint32_t aec3_frames = 0;
	static uint64_t aec3_ref_cyc = 0;
	static uint64_t aec3_mic_cyc = 0;
	uint32_t t0, t1, t2;

	if (frame_samples > WEBRTC_AEC_FRAME_SAMPLES_16K)
		return -EINVAL;

	if (ch >= rd->num_channels || !rd->aec3) {
		memcpy(out, mic, (size_t)frame_samples * sizeof(int16_t));
		return 0;
	}

	/* Convert ref to float and buffer far-end */
	for (i = 0; i < frame_samples; i++)
		rd->ref_f[i] = (float)ref[i];

	t0 = k_cycle_get_32();
	ret = webrtc_aec3_buffer_farend(rd->aec3, rd->ref_f, (size_t)frame_samples);
	t1 = k_cycle_get_32();
	if (ret) {
		comp_err(mod->dev, "webrtc_aec: AEC3 BufferFarend ch%d failed %d", ch, ret);
		return ret;
	}

	/* Convert mic to float */
	for (i = 0; i < frame_samples; i++)
		rd->mic_f[i] = (float)mic[i];

	/* Process capture (mic) and cancel echo */
	ret = webrtc_aec3_process(rd->aec3, nearend, 1, out_ptrs, (size_t)frame_samples);
	t2 = k_cycle_get_32();
	if (ret) {
		comp_err(mod->dev, "webrtc_aec: AEC3 Process ch%d failed %d", ch, ret);
		return ret;
	}

	aec3_ref_cyc += (uint64_t)(t1 - t0);
	aec3_mic_cyc += (uint64_t)(t2 - t1);
	aec3_frames++;
	if (aec3_frames % 20 == 0) {
		comp_info(mod->dev, "webrtc_aec3 perf: f=%u ref=%u kcyc mic=%u kcyc tot=%u kcyc",
			  aec3_frames,
			  (uint32_t)(aec3_ref_cyc / (aec3_frames * 1000)),
			  (uint32_t)(aec3_mic_cyc / (aec3_frames * 1000)),
			  (uint32_t)((aec3_ref_cyc + aec3_mic_cyc) / (aec3_frames * 1000)));
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

static int webrtc_aec3_backend_reset(struct processing_module *mod)
{
	struct webrtc_aec_comp_data *cd = module_get_private_data(mod);
	struct webrtc_aec3_data *rd = cd->backend_data;

	if (rd && rd->aec3) {
		webrtc_aec3_init(rd->aec3, cd->proc_rate);
	}
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
	.process_ch      = webrtc_aec3_backend_process_ch,
	.reset           = webrtc_aec3_backend_reset,
	.free            = webrtc_aec3_backend_free,
};
