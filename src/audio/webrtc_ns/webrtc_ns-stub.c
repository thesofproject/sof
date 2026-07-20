// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.
//
// Dependency-free stub backend for the webrtc_ns module.
//
// Always passes audio through unmodified (identity transform). Used in CI
// and for topology/LLEXT packaging validation without the WebRTC NS library.

#include <sof/audio/module_adapter/module/generic.h>
#include <rtos/string.h>
#include <errno.h>
#include "webrtc_ns.h"

LOG_MODULE_DECLARE(webrtc_ns, CONFIG_SOF_LOG_LEVEL);

static int webrtc_ns_stub_init(struct processing_module *mod)
{
	comp_info(mod->dev, "webrtc_ns stub backend: no WebRTC NS linked");
	return 0;
}

static int webrtc_ns_stub_configure(struct processing_module *mod,
				    int sample_rate_hz, int level, int num_channels)
{
	comp_info(mod->dev, "webrtc_ns stub: rate=%d level=%d ch=%d (ignored)",
		  sample_rate_hz, level, num_channels);
	return 0;
}

/* Stub: copy input to output (pass-through). */
static int webrtc_ns_stub_process(struct processing_module *mod,
				  const float *const *in, float *const *out,
				  int frame_samples)
{
	struct webrtc_ns_comp_data *cd = module_get_private_data(mod);
	int c;

	for (c = 0; c < cd->channels; c++)
		memcpy(out[c], in[c], (size_t)frame_samples * sizeof(float));

	return 0;
}

static int webrtc_ns_stub_reset(struct processing_module *mod)
{
	(void)mod;
	return 0;
}

static int webrtc_ns_stub_free(struct processing_module *mod)
{
	(void)mod;
	return 0;
}

const struct webrtc_ns_backend webrtc_ns_backend = {
	.name      = "stub",
	.init      = webrtc_ns_stub_init,
	.configure = webrtc_ns_stub_configure,
	.process   = webrtc_ns_stub_process,
	.reset     = webrtc_ns_stub_reset,
	.free      = webrtc_ns_stub_free,
};
