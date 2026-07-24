/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2026 Intel Corporation.
 *
 * WebRTC Noise Suppression module for SOF.
 *
 * Wraps the classic WebRTC NS algorithm (spectral subtraction / Wiener filter
 * from webrtc-audio-processing 0.3.x) behind the SOF module_interface as a
 * single-input, single-output PCM effect.
 *
 * The actual suppression is delegated to a pluggable backend so the SOF glue
 * can be validated with a stub (webrtc_ns-stub.c) before the real WebRTC NS
 * backend (webrtc_ns-webrtc.c) is linked in.
 */
#ifndef __SOF_AUDIO_WEBRTC_NS_H__
#define __SOF_AUDIO_WEBRTC_NS_H__

#include <sof/audio/module_adapter/module/generic.h>
#include <stdbool.h>
#include <stdint.h>

/*
 * WebRTC NS requires exactly 10 ms frames at the processing sample rate.
 * Maximum: 10 ms at 48 kHz = 480 samples/channel.
 */
#define WEBRTC_NS_FRAME_SAMPLES_MAX	480

/* Maximum channel count supported. */
#define WEBRTC_NS_CHANNELS_MAX		8

/**
 * struct webrtc_ns_backend - NS backend operations.
 *
 * A backend owns the WebRTC NS state and the per-frame suppression call.
 * All ops return 0 on success or a negative errno.
 */
struct webrtc_ns_backend {
	const char *name;

	/* One-time init, called from module init(). */
	int (*init)(struct processing_module *mod);

	/**
	 * Configure the NS for rate/level. Called from prepare().
	 * @sample_rate_hz: processing rate (8000/16000/32000/48000)
	 * @level:          NS aggressiveness 0..3
	 * @num_channels:   number of channels to suppress independently
	 */
	int (*configure)(struct processing_module *mod,
			 int sample_rate_hz, int level, int num_channels);

	/**
	 * Process one 10 ms frame of float PCM (per channel, planar).
	 * @in:  array of num_channels pointers, each of frame_samples floats
	 * @out: array of num_channels pointers, each of frame_samples floats
	 * @frame_samples: samples per channel (rate * 10 / 1000)
	 */
	int (*process)(struct processing_module *mod,
		       const float *const *in, float *const *out,
		       int frame_samples);

	/* Reset NS state (keep configuration). Called from reset(). */
	int (*reset)(struct processing_module *mod);

	/* Tear down all state from init()/configure(). */
	int (*free)(struct processing_module *mod);
};

/**
 * struct webrtc_ns_comp_data - webrtc_ns module private data.
 * @backend:          Selected backend operations.
 * @backend_data:     Backend-private state handle.
 * @in_rate:          Input sample rate from pipeline (Hz).
 * @proc_rate:        Processing rate used by NS (may be < in_rate).
 * @channels:         Channel count.
 * @in_frame_bytes:   Bytes per frame at pipeline rate.
 * @proc_frame_samples: Samples per 10 ms at proc_rate (per channel).
 * @in_frame_samples: Samples per 10 ms at in_rate (per channel).
 * @buffered_frames:  Samples currently in accumulator.
 * @in_buf:           Float planar input accumulator [ch][frame_samples].
 * @out_buf:          Float planar output buffer [ch][frame_samples].
 * @configured:       True once backend has been opened.
 */
struct webrtc_ns_comp_data {
	const struct webrtc_ns_backend *backend;
	void	*backend_data;
	int	 in_rate;
	int	 proc_rate;
	int	 channels;
	int	 in_frame_bytes;
	int	 proc_frame_samples;
	int	 in_frame_samples;
	int	 buffered_frames;
	float	 in_buf[WEBRTC_NS_CHANNELS_MAX][WEBRTC_NS_FRAME_SAMPLES_MAX];
	float	 out_buf[WEBRTC_NS_CHANNELS_MAX][WEBRTC_NS_FRAME_SAMPLES_MAX];
	const float *in_ptrs[WEBRTC_NS_CHANNELS_MAX];
	float	*out_ptrs[WEBRTC_NS_CHANNELS_MAX];
	bool	 configured;
};

/* Backend instance provided by the selected translation unit. */
extern const struct webrtc_ns_backend webrtc_ns_backend;

#endif /* __SOF_AUDIO_WEBRTC_NS_H__ */
