// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.
//
// Stub backend for webrtc_aec: passes mic audio through unchanged.
// Reference audio is consumed and discarded.

#include <sof/audio/module_adapter/module/generic.h>
#include <rtos/string.h>
#include <errno.h>
#include "webrtc_aec.h"

LOG_MODULE_DECLARE(webrtc_aec, CONFIG_SOF_LOG_LEVEL);

static int webrtc_aec_stub_init(struct processing_module *mod)
{
	comp_info(mod->dev, "webrtc_aec stub: no AECm library linked");
	return 0;
}

static int webrtc_aec_stub_configure(struct processing_module *mod, int sample_rate_hz,
				     int filter_len_ms, int suppression, int num_channels)
{
	comp_info(mod->dev, "webrtc_aec stub: rate=%d filter=%dms sup=%d ch=%d (ignored)",
		  sample_rate_hz, filter_len_ms, suppression, num_channels);
	return 0;
}

static int webrtc_aec_stub_process_ch(struct processing_module *mod,
				      const int16_t *mic, const int16_t *ref, int16_t *out,
				      int frame_samples, int ch)
{
	/* Pass mic straight to output; ref is silently discarded. */
	memcpy(out, mic, (size_t)frame_samples * sizeof(int16_t));
	(void)ref;
	(void)ch;
	return 0;
}

static int webrtc_aec_stub_reset(struct processing_module *mod)
{
	(void)mod;
	return 0;
}

static int webrtc_aec_stub_free(struct processing_module *mod)
{
	(void)mod;
	return 0;
}

const struct webrtc_aec_backend webrtc_aec_backend = {
	.name       = "stub",
	.init       = webrtc_aec_stub_init,
	.configure  = webrtc_aec_stub_configure,
	.process_ch = webrtc_aec_stub_process_ch,
	.reset      = webrtc_aec_stub_reset,
	.free       = webrtc_aec_stub_free,
};
