// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.
//
// Stub backend for webrtc_ns2. Passes audio through unchanged.
// Returns VAD probability = 1.0 (always speech).

#include <sof/audio/module_adapter/module/generic.h>
#include <rtos/string.h>
#include "webrtc_ns2.h"

LOG_MODULE_DECLARE(webrtc_ns2, CONFIG_SOF_LOG_LEVEL);

static int webrtc_ns2_stub_init(struct processing_module *mod)
{
	comp_info(mod->dev, "webrtc_ns2 stub: no RNNoise library linked");
	return 0;
}

static int webrtc_ns2_stub_configure(struct processing_module *mod, int num_channels)
{
	comp_info(mod->dev, "webrtc_ns2 stub: ch=%d (ignored)", num_channels);
	return 0;
}

static float webrtc_ns2_stub_process_ch(struct processing_module *mod,
					const float *in, float *out, int ch)
{
	memcpy(out, in, WEBRTC_NS2_FRAME_SAMPLES * sizeof(float));
	(void)ch;
	return 1.0f; /* always report speech */
}

static int webrtc_ns2_stub_reset(struct processing_module *mod)
{
	(void)mod;
	return 0;
}

static int webrtc_ns2_stub_free(struct processing_module *mod)
{
	(void)mod;
	return 0;
}

const struct webrtc_ns2_backend webrtc_ns2_backend = {
	.name       = "stub",
	.init       = webrtc_ns2_stub_init,
	.configure  = webrtc_ns2_stub_configure,
	.process_ch = webrtc_ns2_stub_process_ch,
	.reset      = webrtc_ns2_stub_reset,
	.free       = webrtc_ns2_stub_free,
};
