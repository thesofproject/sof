// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.
//
// Real RNNoise backend for webrtc_ns2.
//
// RNNoise public API (rnnoise.h / denoise.c):
//
//   int rnnoise_get_size(void);
//       Returns sizeof(DenoiseState). Use this instead of sizeof() so the
//       wrapper doesn't need to know the internal layout.
//
//   int rnnoise_init(DenoiseState *st, const RNNModel *model);
//       Initialise state in-place. model=NULL uses the built-in weights
//       (rnnoise_model_orig compiled into rnnoise_tables.c).
//
//   DenoiseState *rnnoise_create(const RNNModel *model);
//       Allocate + init. Calls malloc() internally.
//
//   void rnnoise_destroy(DenoiseState *st);
//       Free. Calls free() internally.
//
//   float rnnoise_process_frame(DenoiseState *st, float *out, const float *in);
//       Process one 480-sample (10 ms at 48 kHz) mono frame.
//       Samples are in full-scale float (not normalised to [-1,1]).
//       Returns speech probability in [0, 1].
//
// Memory: ~30 KB per DenoiseState (including RNN state).
// Dependencies: libm (expf, tanhf, sinf, cosf, sqrtf).

#include <sof/audio/module_adapter/module/generic.h>
#include <errno.h>
#include "webrtc_ns2.h"

#include <rnnoise.h>   /* from cross-built rnnoise library */

LOG_MODULE_DECLARE(webrtc_ns2, CONFIG_SOF_LOG_LEVEL);

/* RNNoise uses full-scale float (raw amplitude, not normalised). */
#define RNN_FULL_SCALE	32768.0f

struct webrtc_ns2_rnn_data {
	DenoiseState *st[WEBRTC_NS2_CHANNELS_MAX];
	int	      num_channels;
};

/* Per-frame scratch buffer at full scale. */
static float rnn_in[WEBRTC_NS2_FRAME_SAMPLES];
static float rnn_out[WEBRTC_NS2_FRAME_SAMPLES];

static int webrtc_ns2_rnn_init(struct processing_module *mod)
{
	struct webrtc_ns2_comp_data *cd = module_get_private_data(mod);
	struct webrtc_ns2_rnn_data *rd;

	rd = mod_zalloc(mod, sizeof(*rd));
	if (!rd)
		return -ENOMEM;

	cd->backend_data = rd;
	comp_info(mod->dev, "webrtc_ns2: RNNoise real backend, frame=%d rate=%d",
		  WEBRTC_NS2_FRAME_SAMPLES, WEBRTC_NS2_SAMPLE_RATE);
	return 0;
}

static int webrtc_ns2_rnn_configure(struct processing_module *mod, int num_channels)
{
	struct webrtc_ns2_comp_data *cd = module_get_private_data(mod);
	struct webrtc_ns2_rnn_data *rd = cd->backend_data;
	int c;

	/* Free any previous instances from a re-prepare. */
	for (c = 0; c < rd->num_channels; c++) {
		if (rd->st[c]) {
			rnnoise_destroy(rd->st[c]);
			rd->st[c] = NULL;
		}
	}
	rd->num_channels = 0;

	for (c = 0; c < num_channels; c++) {
		/* Pass NULL to use the built-in model weights. */
		rd->st[c] = rnnoise_create(NULL);
		if (!rd->st[c]) {
			comp_err(mod->dev,
				 "webrtc_ns2: rnnoise_create() failed ch%d", c);
			goto err;
		}
	}

	rd->num_channels = num_channels;
	comp_info(mod->dev, "webrtc_ns2: %d RNNoise instance(s) created", num_channels);
	return 0;

err:
	for (c = 0; c < num_channels; c++) {
		if (rd->st[c]) {
			rnnoise_destroy(rd->st[c]);
			rd->st[c] = NULL;
		}
	}
	return -ENOMEM;
}

static float webrtc_ns2_rnn_process_ch(struct processing_module *mod,
					const float *in, float *out, int ch)
{
	struct webrtc_ns2_comp_data *cd = module_get_private_data(mod);
	struct webrtc_ns2_rnn_data *rd = cd->backend_data;
	float vad_prob;
	int i;

	/*
	 * RNNoise expects full-scale float (raw PCM amplitude), but the SOF
	 * glue normalises to [-1, +1]. Scale up before processing and back
	 * down afterward.
	 */
	for (i = 0; i < WEBRTC_NS2_FRAME_SAMPLES; i++)
		rnn_in[i] = in[i] * RNN_FULL_SCALE;

	vad_prob = rnnoise_process_frame(rd->st[ch], rnn_out, rnn_in);

	for (i = 0; i < WEBRTC_NS2_FRAME_SAMPLES; i++)
		out[i] = rnn_out[i] / RNN_FULL_SCALE;

	return vad_prob;
}

static int webrtc_ns2_rnn_reset(struct processing_module *mod)
{
	struct webrtc_ns2_comp_data *cd = module_get_private_data(mod);
	struct webrtc_ns2_rnn_data *rd = cd->backend_data;
	int c;

	/*
	 * RNNoise has no reset API; re-initialise each instance in-place
	 * using rnnoise_init() to clear the GRU hidden state.
	 */
	for (c = 0; c < rd->num_channels; c++) {
		if (rd->st[c])
			rnnoise_init(rd->st[c], NULL);
	}
	return 0;
}

static int webrtc_ns2_rnn_free(struct processing_module *mod)
{
	struct webrtc_ns2_comp_data *cd = module_get_private_data(mod);
	struct webrtc_ns2_rnn_data *rd = cd->backend_data;
	int c;

	if (!rd)
		return 0;

	for (c = 0; c < rd->num_channels; c++) {
		if (rd->st[c]) {
			rnnoise_destroy(rd->st[c]);
			rd->st[c] = NULL;
		}
	}
	mod_free(mod, rd);
	cd->backend_data = NULL;
	return 0;
}

const struct webrtc_ns2_backend webrtc_ns2_backend = {
	.name       = "rnnoise",
	.init       = webrtc_ns2_rnn_init,
	.configure  = webrtc_ns2_rnn_configure,
	.process_ch = webrtc_ns2_rnn_process_ch,
	.reset      = webrtc_ns2_rnn_reset,
	.free       = webrtc_ns2_rnn_free,
};
