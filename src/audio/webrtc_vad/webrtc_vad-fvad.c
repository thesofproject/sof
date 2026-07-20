// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.
//
// libfvad backend for the webrtc_vad module.
//
// libfvad is a standalone pure-C BSD-3 extraction of the WebRTC GMM Voice
// Activity Detection algorithm. It operates in Q15 fixed-point and requires
// no FPU, making it well suited to Xtensa DSPs and Cortex-M class targets.
//
// The backend wraps the following minimal libfvad API:
//
//   fvad_new()             — allocate a FvadState
//   fvad_set_mode()        — aggressiveness 0..3
//   fvad_set_sample_rate() — 8000/16000/32000/48000 Hz
//   fvad_process()         — classify num_samples of int16 mono audio
//   fvad_free()            — release FvadState
//
// Requires cross-compiled libfvad static library and headers (west module
// modules/audio/libfvad; see webrtc_vad.cmake for the cross-build recipe).
// Only compiled when CONFIG_COMP_WEBRTC_VAD_STUB is not selected.

#include <sof/audio/module_adapter/module/generic.h>
#include <errno.h>
#include <stdlib.h>
#include "webrtc_vad.h"

#include <fvad.h>	/* from cross-built libfvad install */

LOG_MODULE_DECLARE(webrtc_vad, CONFIG_SOF_LOG_LEVEL);

static int webrtc_vad_fvad_init(struct processing_module *mod)
{
	struct webrtc_vad_comp_data *cd = module_get_private_data(mod);
	Fvad *fvad;

	fvad = fvad_new();
	if (!fvad) {
		comp_err(mod->dev, "webrtc_vad: fvad_new() failed");
		return -ENOMEM;
	}

	cd->backend_data = fvad;
	comp_info(mod->dev, "webrtc_vad: libfvad backend initialised");
	return 0;
}

static int webrtc_vad_fvad_configure(struct processing_module *mod,
				     int sample_rate_hz, int mode)
{
	struct webrtc_vad_comp_data *cd = module_get_private_data(mod);
	Fvad *fvad = cd->backend_data;
	int ret;

	ret = fvad_set_sample_rate(fvad, sample_rate_hz);
	if (ret < 0) {
		comp_err(mod->dev, "webrtc_vad: fvad_set_sample_rate(%d) failed %d",
			 sample_rate_hz, ret);
		return -EINVAL;
	}

	ret = fvad_set_mode(fvad, mode);
	if (ret < 0) {
		comp_err(mod->dev, "webrtc_vad: fvad_set_mode(%d) failed %d",
			 mode, ret);
		return -EINVAL;
	}

	comp_info(mod->dev, "webrtc_vad: libfvad rate=%d mode=%d",
		  sample_rate_hz, mode);
	return 0;
}

static int webrtc_vad_fvad_classify(struct processing_module *mod,
				    const int16_t *samples, int num_samples)
{
	struct webrtc_vad_comp_data *cd = module_get_private_data(mod);
	Fvad *fvad = cd->backend_data;
	int decision;

	/* fvad_process returns 1 (speech), 0 (non-speech), or <0 on error. */
	decision = fvad_process(fvad, samples, num_samples);
	if (decision < 0) {
		comp_err(mod->dev, "webrtc_vad: fvad_process error %d", decision);
		return -EIO;
	}

	return decision;
}

static int webrtc_vad_fvad_reset(struct processing_module *mod)
{
	struct webrtc_vad_comp_data *cd = module_get_private_data(mod);
	Fvad *fvad = cd->backend_data;

	/* libfvad has no explicit reset API; re-init the state via free+new. */
	int rate = cd->sample_rate;
	int mode = CONFIG_WEBRTC_VAD_MODE;

	fvad_free(fvad);
	fvad = fvad_new();
	if (!fvad)
		return -ENOMEM;

	cd->backend_data = fvad;
	return webrtc_vad_fvad_configure(mod, rate, mode);
}

static int webrtc_vad_fvad_free(struct processing_module *mod)
{
	struct webrtc_vad_comp_data *cd = module_get_private_data(mod);

	if (cd->backend_data) {
		fvad_free(cd->backend_data);
		cd->backend_data = NULL;
	}
	return 0;
}

const struct webrtc_vad_backend webrtc_vad_backend = {
	.name      = "fvad",
	.init      = webrtc_vad_fvad_init,
	.configure = webrtc_vad_fvad_configure,
	.classify  = webrtc_vad_fvad_classify,
	.reset     = webrtc_vad_fvad_reset,
	.free      = webrtc_vad_fvad_free,
};
