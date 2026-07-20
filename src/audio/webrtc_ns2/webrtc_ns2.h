/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2026 Intel Corporation.
 *
 * webrtc_ns2 — RNNoise deep-learning noise suppressor for SOF.
 *
 * Single-input, single-output PCM effect. Processes interleaved S16/S32
 * at 48 kHz in 480-sample (10 ms) mono frames. One RNNoise instance is
 * allocated per channel and processes channel audio independently.
 *
 * In addition to denoising, the module broadcasts a NOTIFIER_ID_VAD event
 * from the per-frame speech probability returned by rnnoise_process_frame().
 */
#ifndef __SOF_AUDIO_WEBRTC_NS2_H__
#define __SOF_AUDIO_WEBRTC_NS2_H__

#include <sof/audio/module_adapter/module/generic.h>
#include <stdbool.h>
#include <stdint.h>

/* RNNoise is strictly 48 kHz, 10 ms frames. */
#define WEBRTC_NS2_SAMPLE_RATE		48000
#define WEBRTC_NS2_FRAME_SAMPLES	480    /* 10 ms at 48 kHz */
#define WEBRTC_NS2_CHANNELS_MAX		CONFIG_WEBRTC_NS2_CHANNELS_MAX

/**
 * struct webrtc_ns2_backend - NS2 backend operations.
 */
struct webrtc_ns2_backend {
	const char *name;

	/* One-time init called from module init(). */
	int (*init)(struct processing_module *mod);

	/**
	 * Open and configure one RNNoise instance per channel.
	 * Called from prepare().
	 * @num_channels: number of channels to process
	 */
	int (*configure)(struct processing_module *mod, int num_channels);

	/**
	 * Process one 480-sample mono frame for a single channel.
	 * @in:    480 floats in [-1.0, +1.0] (pipeline samples normalised)
	 * @out:   480 floats out (denoised)
	 * @ch:    channel index
	 * Returns speech probability in [0.0, 1.0], or < 0 on error.
	 */
	float (*process_ch)(struct processing_module *mod,
			    const float *in, float *out, int ch);

	/* Reset per-channel state (history buffers). Called from reset(). */
	int (*reset)(struct processing_module *mod);

	/* Deallocate all state. Called from free(). */
	int (*free)(struct processing_module *mod);
};

/**
 * struct webrtc_ns2_comp_data - webrtc_ns2 module private data.
 */
struct webrtc_ns2_comp_data {
	const struct webrtc_ns2_backend *backend;
	void    *backend_data;

	int      channels;
	bool     is_s32;           /* true when pipeline format is S32_LE */
	int      buffered_frames;  /* frames accumulated toward 480 */

	/* Per-channel float scratch buffers (480 samples each). */
	float    in_buf[WEBRTC_NS2_CHANNELS_MAX][WEBRTC_NS2_FRAME_SAMPLES];
	float    out_buf[WEBRTC_NS2_CHANNELS_MAX][WEBRTC_NS2_FRAME_SAMPLES];

	/* VAD: last decision and threshold. */
	int      last_vad;
	float    vad_threshold;

	bool     configured;
};

/* Backend instance provided by the selected translation unit. */
extern const struct webrtc_ns2_backend webrtc_ns2_backend;

#endif /* __SOF_AUDIO_WEBRTC_NS2_H__ */
