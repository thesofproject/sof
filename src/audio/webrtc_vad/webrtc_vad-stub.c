// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.
//
// Dependency-free stub backend for the webrtc_vad module.
//
// This lets the SOF module glue (init/prepare/process/reset/free, the LLEXT
// manifest and topology wiring) be built and exercised in CI without pulling
// in libfvad. The stub always reports SPEECH (decision = 1) so audio flow
// and notifier events can be validated end-to-end. The real libfvad backend
// lives in webrtc_vad-fvad.c and provides the same webrtc_vad_backend symbol.

#include <sof/audio/module_adapter/module/generic.h>
#include <errno.h>
#include "webrtc_vad.h"

LOG_MODULE_DECLARE(webrtc_vad, CONFIG_SOF_LOG_LEVEL);

static int webrtc_vad_stub_init(struct processing_module *mod)
{
	comp_info(mod->dev, "webrtc_vad stub backend: no libfvad linked");
	return 0;
}

static int webrtc_vad_stub_configure(struct processing_module *mod,
				     int sample_rate_hz, int mode)
{
	comp_info(mod->dev, "webrtc_vad stub: rate=%d mode=%d (ignored)",
		  sample_rate_hz, mode);
	return 0;
}

/* Stub classifier: always reports speech. */
static int webrtc_vad_stub_classify(struct processing_module *mod,
				    const int16_t *samples, int num_samples)
{
	(void)mod; (void)samples; (void)num_samples;
	return WEBRTC_VAD_SPEECH;
}

static int webrtc_vad_stub_reset(struct processing_module *mod)
{
	(void)mod;
	return 0;
}

static int webrtc_vad_stub_free(struct processing_module *mod)
{
	(void)mod;
	return 0;
}

const struct webrtc_vad_backend webrtc_vad_backend = {
	.name      = "stub",
	.init      = webrtc_vad_stub_init,
	.configure = webrtc_vad_stub_configure,
	.classify  = webrtc_vad_stub_classify,
	.reset     = webrtc_vad_stub_reset,
	.free      = webrtc_vad_stub_free,
};
