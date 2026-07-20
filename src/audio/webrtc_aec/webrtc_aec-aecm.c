// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.
//
// Real AECm backend for the webrtc_aec module.
//
// Wraps WebRtcAecm_Create/Init/BufferFarend/Process from the WebRTC AECm
// (Mobile) fixed-point library extracted from webrtc-audio-processing 0.3.x.
//
// API summary:
//   WebRtcAecm_Create()       — allocate AecmCore handle
//   WebRtcAecm_Init()         — configure sample rate (8000 or 16000 Hz)
//   WebRtcAecm_BufferFarend() — queue 10 ms of echo reference (far-end)
//   WebRtcAecm_Process()      — cancel echo from near-end mic frame
//   WebRtcAecm_Enable()       — configure filter length and CNG suppression
//   WebRtcAecm_Free()         — release handle
//
// All operations are Q15 fixed-point on int16_t. One handle per channel.

#include <sof/audio/module_adapter/module/generic.h>
#include <errno.h>
#include "webrtc_aec.h"

#include <echo_control_mobile.h>   /* from cross-built webrtc-audio-processing 0.3.x */

LOG_MODULE_DECLARE(webrtc_aec, CONFIG_SOF_LOG_LEVEL);

struct webrtc_aec_real_data {
	void   *aecm[WEBRTC_AEC_CHANNELS_MAX];
	int     num_channels;
	int     filter_len_ms;
	int     suppression;
};

static int webrtc_aec_real_init(struct processing_module *mod)
{
	struct webrtc_aec_comp_data *cd = module_get_private_data(mod);
	struct webrtc_aec_real_data *rd;

	rd = mod_zalloc(mod, sizeof(*rd));
	if (!rd)
		return -ENOMEM;

	cd->backend_data = rd;
	comp_info(mod->dev, "webrtc_aec: AECm real backend initialised");
	return 0;
}

static int webrtc_aec_real_configure(struct processing_module *mod, int sample_rate_hz,
				     int filter_len_ms, int suppression, int num_channels)
{
	struct webrtc_aec_comp_data *cd = module_get_private_data(mod);
	struct webrtc_aec_real_data *rd = cd->backend_data;
	AecmConfig config;
	int c, ret;

	/* Free previously allocated handles. */
	for (c = 0; c < rd->num_channels; c++) {
		if (rd->aecm[c]) {
			WebRtcAecm_Free(rd->aecm[c]);
			rd->aecm[c] = NULL;
		}
	}
	rd->num_channels  = 0;
	rd->filter_len_ms = filter_len_ms;
	rd->suppression   = suppression;

	for (c = 0; c < num_channels; c++) {
		rd->aecm[c] = WebRtcAecm_Create();
		if (!rd->aecm[c]) {
			comp_err(mod->dev, "webrtc_aec: WebRtcAecm_Create() failed ch%d", c);
			goto err;
		}

		ret = WebRtcAecm_Init(rd->aecm[c], sample_rate_hz);
		if (ret) {
			comp_err(mod->dev,
				 "webrtc_aec: WebRtcAecm_Init(ch%d, %d) failed %d",
				 c, sample_rate_hz, ret);
			goto err;
		}

		config.cngMode         = suppression;
		config.echoMode        = (filter_len_ms == 128) ? 4 :
					 (filter_len_ms == 64)  ? 3 :
					 (filter_len_ms == 32)  ? 2 : 3;
		ret = WebRtcAecm_set_config(rd->aecm[c], config);
		if (ret) {
			comp_err(mod->dev, "webrtc_aec: set_config ch%d failed %d", c, ret);
			goto err;
		}
	}

	rd->num_channels = num_channels;
	comp_info(mod->dev, "webrtc_aec: AECm rate=%d filter=%dms cng=%d ch=%d",
		  sample_rate_hz, filter_len_ms, suppression, num_channels);
	return 0;

err:
	for (c = 0; c < num_channels; c++) {
		if (rd->aecm[c]) {
			WebRtcAecm_Free(rd->aecm[c]);
			rd->aecm[c] = NULL;
		}
	}
	return -ENOMEM;
}

static int webrtc_aec_real_process_ch(struct processing_module *mod,
				      const int16_t *mic, const int16_t *ref, int16_t *out,
				      int frame_samples, int ch)
{
	struct webrtc_aec_comp_data *cd = module_get_private_data(mod);
	struct webrtc_aec_real_data *rd = cd->backend_data;
	int ret;

	/* Queue the far-end (reference/playback) frame first. */
	ret = WebRtcAecm_BufferFarend(rd->aecm[ch], ref, frame_samples);
	if (ret) {
		comp_err(mod->dev, "webrtc_aec: BufferFarend ch%d failed %d", ch, ret);
		return ret;
	}

	/* Process near-end (mic) and produce echo-cancelled output.
	 * The third parameter (near_end_noiseless) can be NULL. */
	ret = WebRtcAecm_Process(rd->aecm[ch], mic, NULL, out, frame_samples, 0);
	if (ret) {
		comp_err(mod->dev, "webrtc_aec: Process ch%d failed %d", ch, ret);
		return ret;
	}

	return 0;
}

static int webrtc_aec_real_reset(struct processing_module *mod)
{
	struct webrtc_aec_comp_data *cd = module_get_private_data(mod);
	struct webrtc_aec_real_data *rd = cd->backend_data;
	int c, ret;

	for (c = 0; c < rd->num_channels; c++) {
		if (!rd->aecm[c])
			continue;
		ret = WebRtcAecm_Init(rd->aecm[c], cd->proc_rate);
		if (ret)
			comp_warn(mod->dev, "webrtc_aec: reset ch%d failed %d", c, ret);
	}
	return 0;
}

static int webrtc_aec_real_free(struct processing_module *mod)
{
	struct webrtc_aec_comp_data *cd = module_get_private_data(mod);
	struct webrtc_aec_real_data *rd = cd->backend_data;
	int c;

	if (!rd)
		return 0;

	for (c = 0; c < rd->num_channels; c++) {
		if (rd->aecm[c]) {
			WebRtcAecm_Free(rd->aecm[c]);
			rd->aecm[c] = NULL;
		}
	}
	mod_free(mod, rd);
	cd->backend_data = NULL;
	return 0;
}

const struct webrtc_aec_backend webrtc_aec_backend = {
	.name       = "aecm",
	.init       = webrtc_aec_real_init,
	.configure  = webrtc_aec_real_configure,
	.process_ch = webrtc_aec_real_process_ch,
	.reset      = webrtc_aec_real_reset,
	.free       = webrtc_aec_real_free,
};
