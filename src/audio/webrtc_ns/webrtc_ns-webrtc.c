// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.
//
// WebRTC NS real backend for the webrtc_ns module.
//
// Wraps the classic WebRTC noise_suppression library extracted from
// webrtc-audio-processing 0.3.x. This version is pure C, has no C++
// or abseil dependency, and uses a simple stateless API:
//
//   WebRtcNs_Create()       — allocate NsHandle
//   WebRtcNs_Init()         — configure sample rate
//   WebRtcNs_set_policy()   — aggressiveness 0..3
//   WebRtcNs_Analyze()      — feed 10ms frame for noise model update
//   WebRtcNs_Process()      — run suppression
//   WebRtcNs_Free()         — release NsHandle
//
// The library works at a single fixed sample rate; if the pipeline runs at
// a different rate, the caller (webrtc_ns.c) is responsible for downsampling
// before and upsampling after (not yet implemented in this backend — for now,
// the backend uses CONFIG_WEBRTC_NS_SAMPLE_RATE_HZ directly and requires the
// pipeline to run at that rate, or to be configured identically).
//
// Requires cross-built libwebrtc_ns.a (west module modules/audio/webrtc-apm).
// See webrtc_ns.cmake for the cross-build recipe.

#include <sof/audio/module_adapter/module/generic.h>
#include <errno.h>
#include <stdlib.h>
#include "webrtc_ns.h"

#include <noise_suppression.h>   /* from cross-built webrtc-audio-processing 0.3.x */

LOG_MODULE_DECLARE(webrtc_ns, CONFIG_SOF_LOG_LEVEL);

/* Per-channel NS state handles. */
struct webrtc_ns_real_data {
	NsHandle *ns[WEBRTC_NS_CHANNELS_MAX];
	int       num_channels;
};

static int webrtc_ns_real_init(struct processing_module *mod)
{
	struct webrtc_ns_comp_data *cd = module_get_private_data(mod);
	struct webrtc_ns_real_data *rd;

	rd = mod_zalloc(mod, sizeof(*rd));
	if (!rd)
		return -ENOMEM;

	cd->backend_data = rd;
	comp_info(mod->dev, "webrtc_ns: WebRTC NS real backend initialised");
	return 0;
}

static int webrtc_ns_real_configure(struct processing_module *mod,
				    int sample_rate_hz, int level, int num_channels)
{
	struct webrtc_ns_comp_data *cd = module_get_private_data(mod);
	struct webrtc_ns_real_data *rd = cd->backend_data;
	int c, ret;

	/* Free any previously allocated handles from a re-prepare. */
	for (c = 0; c < rd->num_channels; c++) {
		if (rd->ns[c]) {
			WebRtcNs_Free(rd->ns[c]);
			rd->ns[c] = NULL;
		}
	}

	for (c = 0; c < num_channels; c++) {
		rd->ns[c] = WebRtcNs_Create();
		if (!rd->ns[c]) {
			comp_err(mod->dev, "webrtc_ns: WebRtcNs_Create() failed ch%d", c);
			goto err;
		}

		ret = WebRtcNs_Init(rd->ns[c], (uint32_t)sample_rate_hz);
		if (ret) {
			comp_err(mod->dev, "webrtc_ns: WebRtcNs_Init(ch%d, %d) failed %d",
				 c, sample_rate_hz, ret);
			goto err;
		}

		ret = WebRtcNs_set_policy(rd->ns[c], level);
		if (ret) {
			comp_err(mod->dev, "webrtc_ns: set_policy(%d) failed %d", level, ret);
			goto err;
		}
	}

	rd->num_channels = num_channels;
	comp_info(mod->dev, "webrtc_ns: WebRTC NS rate=%d level=%d ch=%d",
		  sample_rate_hz, level, num_channels);
	return 0;

err:
	for (c = 0; c < num_channels; c++) {
		if (rd->ns[c]) {
			WebRtcNs_Free(rd->ns[c]);
			rd->ns[c] = NULL;
		}
	}
	return -ENOMEM;
}

static int webrtc_ns_real_process(struct processing_module *mod,
				  const float *const *in, float *const *out,
				  int frame_samples)
{
	struct webrtc_ns_comp_data *cd = module_get_private_data(mod);
	struct webrtc_ns_real_data *rd = cd->backend_data;
	int c;

	for (c = 0; c < rd->num_channels; c++) {
		/* Analyze updates the noise model (non-destructive). */
		WebRtcNs_Analyze(rd->ns[c], in[c]);

		/* Process suppresses noise: in → out. */
		const float *in_ptrs[1]  = { in[c] };
		float       *out_ptrs[1] = { out[c] };

		WebRtcNs_Process(rd->ns[c], in_ptrs, 1, out_ptrs);
	}

	return 0;
}

static int webrtc_ns_real_reset(struct processing_module *mod)
{
	struct webrtc_ns_comp_data *cd = module_get_private_data(mod);
	struct webrtc_ns_real_data *rd = cd->backend_data;
	int c, ret;

	/* Re-initialise each channel to reset internal state. */
	for (c = 0; c < rd->num_channels; c++) {
		if (!rd->ns[c])
			continue;
		ret = WebRtcNs_Init(rd->ns[c], (uint32_t)cd->proc_rate);
		if (ret)
			comp_warn(mod->dev, "webrtc_ns: reset ch%d failed %d", c, ret);
	}
	return 0;
}

static int webrtc_ns_real_free(struct processing_module *mod)
{
	struct webrtc_ns_comp_data *cd = module_get_private_data(mod);
	struct webrtc_ns_real_data *rd = cd->backend_data;
	int c;

	if (!rd)
		return 0;

	for (c = 0; c < rd->num_channels; c++) {
		if (rd->ns[c]) {
			WebRtcNs_Free(rd->ns[c]);
			rd->ns[c] = NULL;
		}
	}

	mod_free(mod, rd);
	cd->backend_data = NULL;
	return 0;
}

const struct webrtc_ns_backend webrtc_ns_backend = {
	.name      = "webrtc-ns",
	.init      = webrtc_ns_real_init,
	.configure = webrtc_ns_real_configure,
	.process   = webrtc_ns_real_process,
	.reset     = webrtc_ns_real_reset,
	.free      = webrtc_ns_real_free,
};
