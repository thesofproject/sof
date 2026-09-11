/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2026 Intel Corporation.
 *
 * WebRTC AECm (fixed-point Acoustic Echo Canceller Mobile) for SOF.
 *
 * Two input pins:
 *   sources[mic_src]  — microphone capture (same pipeline as the sink)
 *   sources[ref_src]  — playback reference  (different render pipeline)
 * One output pin:
 *   sinks[0]          — echo-cancelled microphone
 *
 * The mic/ref source indices are resolved at prepare() time using the same
 * pipeline-ID heuristic as google_rtc_audio_processing.c.
 *
 * The actual AEC processing is delegated to a pluggable backend so the
 * module can be validated with a stub (webrtc_aec-stub.c) before the
 * real AECm backend (webrtc_aec-webrtc.c) is linked in.
 */
#ifndef __SOF_AUDIO_WEBRTC_AEC_H__
#define __SOF_AUDIO_WEBRTC_AEC_H__

#include <sof/audio/module_adapter/module/generic.h>
#include <ipc4/aec.h>
#include <stdbool.h>
#include <stdint.h>

/* AECm operates on 10 ms frames.
 * At 16 kHz: 160 samples/channel.
 * At 48 kHz: 480 samples/channel (resampled to/from 16 kHz for AECm).
 */
#define WEBRTC_AEC_FRAME_SAMPLES_16K	160
#define WEBRTC_AEC_FRAME_SAMPLES_48K	480
#define WEBRTC_AEC_FRAME_SAMPLES_MAX	WEBRTC_AEC_FRAME_SAMPLES_48K
#define WEBRTC_AEC_FIFO_FRAMES		(WEBRTC_AEC_FRAME_SAMPLES_MAX * 2)

/* Maximum supported channel count (AECm is mono; multi-ch runs N instances). */
#define WEBRTC_AEC_CHANNELS_MAX		4

/* Maximum DMA buffer alignment. */
#define WEBRTC_AEC_MEM_ALIGN		64

/* Fixed-point 48kHz <-> 16kHz resampler states (WebRTC APM SPL). */
struct webrtc_aec_state_48_to_16 {
	int32_t s_48_48[16];
	int32_t s_48_32[8];
	int32_t s_32_16[8];
};

struct webrtc_aec_state_16_to_48 {
	int32_t s_16_32[8];
	int32_t s_32_24[8];
	int32_t s_24_48[8];
};

/**
 * struct webrtc_aec_backend - AECm backend operations.
 */
struct webrtc_aec_backend {
	const char *name;

	/* One-time allocation, called from init(). */
	int (*init)(struct processing_module *mod);

	/**
	 * Open/configure AECm state. Called from prepare().
	 * @sample_rate_hz:  8000 or 16000
	 * @filter_len_ms:   adaptive filter length (32/64/128)
	 * @suppression:     CNG level 0..2
	 * @num_channels:    mic and ref channel count (same; AECm runs per-ch)
	 */
	int (*configure)(struct processing_module *mod, int sample_rate_hz,
			 int filter_len_ms, int suppression, int num_channels);

	/* Runtime update of suppression / echo mode. */
	int (*set_suppression)(struct processing_module *mod, bool high_suppression);

	/**
	 * Process one 10 ms frame (interleaved S16 mic + ref → interleaved S16 out).
	 * All arrays are mono; caller provides per-channel slices.
	 * @mic:  mic input S16 samples (frame_samples)
	 * @ref:  echo reference S16 samples (frame_samples)
	 * @out:  denoised output S16 samples (frame_samples)
	 * @ch:   channel index (for multi-channel instances)
	 */
	int (*process_ch)(struct processing_module *mod,
			  const int16_t *mic, const int16_t *ref, int16_t *out,
			  int frame_samples, int ch);

	/**
	 * Process complete multi-channel 10 ms frame.
	 * If provided, called instead of per-channel process_ch loop.
	 * @mic:  array of mic channel pointers
	 * @ref:  array of echo reference channel pointers
	 * @out:  array of output channel pointers
	 */
	int (*process_frame)(struct processing_module *mod,
			     const int16_t *const *mic,
			     const int16_t *const *ref,
			     int16_t *const *out,
			     int frame_samples, int num_channels);

	/* Reset adaptive filters (keep configuration). Called from reset(). */
	int (*reset)(struct processing_module *mod);

	/* Tear down all state from configure(). */
	int (*free)(struct processing_module *mod);
};

/**
 * struct webrtc_aec_comp_data - webrtc_aec module private data.
 *
 * All buffers are S16 because AECm is a fixed-point Q15 algorithm.
 * We do S32→S16 downshift on input and S16→S32 upshift on output when
 * the pipeline format is S32.
 */
struct webrtc_aec_comp_data {
	const struct webrtc_aec_backend *backend;
	void *backend_data;

	/* Runtime ALSA mixer controls */
	bool enabled;          /* master bypass switch (ctl->id == 0) */
	bool high_suppression; /* suppression level switch (ctl->id == 1) */

	/* Source routing (resolved in prepare). */
	int mic_src;          /* index into sources[] for microphone */
	int ref_src;          /* index into sources[] for echo reference */

	/* Negotiated format. */
	int rate;             /* pipeline sample rate (Hz) */
	int proc_rate;        /* AECm processing rate (8000 or 16000 Hz) */
	int channels;
	int mic_frame_bytes;  /* bytes per pipeline frame on mic input */
	int ref_frame_bytes;  /* bytes per pipeline frame on ref input */
	int out_frame_bytes;
	bool is_s32;          /* true for S32_LE pipeline */
	bool needs_resample;  /* true when rate == 48000 and proc_rate == 16000 */

	/* Processing frame size at proc_rate. */
	int frame_samples;    /* pipeline frames per 10 ms */

	/* Accumulation and lookahead FIFO state. */
	int buffered_in_frames;
	int buffered_out_frames;

	/* Per-channel S16 scratch buffers. */
	int16_t mic_buf[WEBRTC_AEC_CHANNELS_MAX][WEBRTC_AEC_FIFO_FRAMES];
	int16_t ref_buf[WEBRTC_AEC_CHANNELS_MAX][WEBRTC_AEC_FIFO_FRAMES];
	int16_t out_fifo[WEBRTC_AEC_CHANNELS_MAX][WEBRTC_AEC_FIFO_FRAMES];

	/* Resampler state for 48kHz <-> 16kHz */
	struct webrtc_aec_state_48_to_16 mic_resamp[WEBRTC_AEC_CHANNELS_MAX];
	struct webrtc_aec_state_48_to_16 ref_resamp[WEBRTC_AEC_CHANNELS_MAX];
	struct webrtc_aec_state_16_to_48 out_resamp[WEBRTC_AEC_CHANNELS_MAX];
	int32_t resamp_tmpmem[512];

	/* Ref stream liveness (IPC4: always active). */
	bool last_ref_ok;

	/* IPC4 tuning (echo path delay, etc.) — reserved for set_config. */
	struct sof_ipc4_aec_config config;

	bool configured;
};

/* Backend instance exported from the selected translation unit. */
extern const struct webrtc_aec_backend webrtc_aec_backend;

#endif /* __SOF_AUDIO_WEBRTC_AEC_H__ */
