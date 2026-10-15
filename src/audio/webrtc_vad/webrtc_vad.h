/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2026 Intel Corporation.
 *
 * WebRTC Voice Activity Detection (VAD) module for SOF.
 *
 * Wraps libfvad — a standalone pure-C BSD-3 port of the WebRTC GMM VAD —
 * behind the SOF module_interface. The module inspects each incoming PCM
 * frame and emits a binary speech/non-speech decision as a SOF notifier
 * event, leaving the audio data stream unmodified (pass-through).
 *
 * The actual VAD work is delegated to a pluggable backend so the SOF glue
 * can be validated with a dependency-free stub (webrtc_vad-stub.c) before
 * the real libfvad backend (webrtc_vad-fvad.c) is linked in.
 */
#ifndef __SOF_AUDIO_WEBRTC_VAD_H__
#define __SOF_AUDIO_WEBRTC_VAD_H__

#include <sof/audio/module_adapter/module/generic.h>
#include <stdbool.h>
#include <stdint.h>

/* Maximum accumulation buffer: 30 ms at 48 kHz, one channel of int16. */
#define WEBRTC_VAD_MAX_FRAME_SAMPLES	(30 * 48)

/* VAD decision values emitted via notifier. */
#define WEBRTC_VAD_SILENCE	0
#define WEBRTC_VAD_SPEECH	1

struct webrtc_vad_comp_data;

/**
 * struct webrtc_vad_backend - VAD backend operations.
 *
 * A backend owns the real VAD state and the frame-level classification.
 * All ops return 0 on success or a negative errno; process() additionally
 * returns the VAD decision (0 = silence, 1 = speech) via *decision.
 */
struct webrtc_vad_backend {
	const char *name;

	/* One-time backend init, called from module init(). */
	int (*init)(struct processing_module *mod);

	/* Configure the VAD for rate/mode, called from prepare(). */
	int (*configure)(struct processing_module *mod,
			 int sample_rate_hz, int mode);

	/**
	 * Classify one complete VAD frame (frame_ms worth of int16 samples,
	 * mono). Returns 1 for speech, 0 for non-speech, negative on error.
	 */
	int (*classify)(struct processing_module *mod,
			const int16_t *samples, int num_samples);

	/* Reset VAD state, keep configuration. Called from reset(). */
	int (*reset)(struct processing_module *mod);

	/* Tear down any state allocated by init()/configure(). */
	int (*free)(struct processing_module *mod);
};

/**
 * struct webrtc_vad_comp_data - webrtc_vad module private data.
 * @backend:          Selected VAD backend ops.
 * @backend_data:     Backend-private state (e.g. FvadState*).
 * @sample_rate:      Input sample rate (Hz).
 * @channels:         Input channel count (VAD runs on channel 0 only).
 * @frame_samples:    Samples per VAD frame (frame_ms * sample_rate / 1000).
 * @sample_bytes:     Bytes per sample (2 for S16, 4 for S32).
 * @accumulator:      S16 mono ring buffer collecting sub-frame periods.
 * @accum_samples:    Number of S16 samples currently in @accumulator.
 * @last_decision:    Most recent VAD classification (0/1).
 * @configured:       True once the backend has been opened.
 */
struct webrtc_vad_comp_data {
	const struct webrtc_vad_backend *backend;
	void	*backend_data;
	int	 sample_rate;
	int	 channels;
	int	 frame_samples;
	int	 sample_bytes;
	int16_t	 accumulator[WEBRTC_VAD_MAX_FRAME_SAMPLES];
	int	 accum_samples;
	int	 last_decision;
	bool	 configured;
};

/* Backend instance provided by the selected translation unit. */
extern const struct webrtc_vad_backend webrtc_vad_backend;

#endif /* __SOF_AUDIO_WEBRTC_VAD_H__ */
