/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2026 Intel Corporation.
 *
 * WebRTC Acoustic Echo Canceller (AEC) module for SOF (IPC4).
 *
 * Two input pins (resolved via IPC4 sink queue IDs):
 *   queue 0 (mic_src) — microphone capture (same pipeline as the sink)
 *   queue 1 (ref_src) — playback echo reference (from the render pipeline)
 * One output pin:
 *   sinks[0]          — echo-cancelled microphone
 *
 * Processing is delegated to a pluggable backend selected at build time:
 *   - Modern C++17 WebRTC AEC3 (webrtc_aec-aec3.c + webrtc_aec3_wrapper.cpp)
 *   - Classic Float MDF AEC    (webrtc_aec-aec.c)
 *   - Fixed-point AECm         (webrtc_aec-aecm.c)
 *   - Dependency-free stub     (webrtc_aec-stub.c)
 */
#ifndef __SOF_AUDIO_WEBRTC_AEC_H__
#define __SOF_AUDIO_WEBRTC_AEC_H__

#include <sof/audio/module_adapter/module/generic.h>
#include <stdbool.h>
#include <stdint.h>

/* WebRTC AEC operates on 10 ms frames.
 * At 16 kHz: 160 samples/channel.
 * At 32 kHz: 320 samples/channel (resampled to/from 16 kHz).
 * At 48 kHz: 480 samples/channel (resampled to/from 16 kHz).
 */
#define WEBRTC_AEC_FRAME_SAMPLES_16K	160
#define WEBRTC_AEC_FRAME_SAMPLES_32K	320
#define WEBRTC_AEC_FRAME_SAMPLES_48K	480
#define WEBRTC_AEC_FRAME_SAMPLES_MAX	WEBRTC_AEC_FRAME_SAMPLES_48K
#define WEBRTC_AEC_FIFO_FRAMES		(WEBRTC_AEC_FRAME_SAMPLES_MAX * 2)

/* Maximum supported channel count per stream (mic or ref). */
#define WEBRTC_AEC_CHANNELS_MAX		4

/* Maximum DMA buffer alignment. */
#define WEBRTC_AEC_MEM_ALIGN		64

/*
 * IPC4 SOF_IPC4_SWITCH_CONTROL_PARAM_ID switch control IDs.
 * These match the 0-based order of Object.Control.mixer instances defined in
 * tools/topology/topology2/include/components/webrtc-aec.conf:
 *   mixer."1" (enable switch)           -> WEBRTC_AEC_CTL_ID_ENABLE
 *   mixer."2" (high suppression switch) -> WEBRTC_AEC_CTL_ID_HIGH_SUPPRESSION
 */
#define WEBRTC_AEC_CTL_ID_ENABLE		0
#define WEBRTC_AEC_CTL_ID_HIGH_SUPPRESSION	1
#define WEBRTC_AEC_CTL_NUM_ELEMS		1

enum webrtc_aec_resamp_mode {
	WEBRTC_AEC_RESAMP_NONE = 0,
	WEBRTC_AEC_RESAMP_32_TO_16,
	WEBRTC_AEC_RESAMP_48_TO_16,
};

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
 * struct webrtc_aec_backend - AEC backend operations.
 */
struct webrtc_aec_backend {
	const char *name;

	/* One-time allocation, called from init(). */
	int (*init)(struct processing_module *mod);

	/**
	 * Open/configure AEC state. Called from prepare().
	 * @sample_rate_hz:    8000 or 16000
	 * @filter_len_ms:     adaptive filter length (32/64/128)
	 * @suppression:       CNG/NLP level 0..2
	 * @num_mic_channels:  capture microphone channel count (1..MAX)
	 * @num_ref_channels:  playback echo reference channel count (1..MAX)
	 */
	int (*configure)(struct processing_module *mod, int sample_rate_hz,
			 int filter_len_ms, int suppression,
			 int num_mic_channels, int num_ref_channels);

	/* Runtime update of suppression / echo mode. */
	int (*set_suppression)(struct processing_module *mod, bool high_suppression);

	/* Runtime update of nominal echo path delay (ms). */
	int (*set_delay)(struct processing_module *mod, int delay_ms);

	/* Optional metrics readback (ERL, ERLE in dB, estimated delay in ms). */
	int (*get_metrics)(struct processing_module *mod,
			   int *erl_db, int *erle_db, int *delay_ms);

	/**
	 * Process one 10 ms frame (per-channel S16 mic + ref -> S16 out).
	 * All arrays are mono; caller provides per-channel slices.
	 * @mic:  mic input S16 samples (frame_samples)
	 * @ref:  echo reference S16 samples (frame_samples)
	 * @out:  denoised output S16 samples (frame_samples)
	 * @ch:   mic channel index (for multi-channel instances)
	 */
	int (*process_ch)(struct processing_module *mod,
			  const int16_t *mic, const int16_t *ref, int16_t *out,
			  int frame_samples, int ch);

	/**
	 * Process complete multi-channel 10 ms frame (supports asymmetric R x C).
	 * If provided, called instead of per-channel process_ch loop.
	 * @mic:               array of mic channel pointers (num_mic_channels)
	 * @ref:               array of echo reference channel pointers (num_ref_channels)
	 * @out:               array of output channel pointers (num_mic_channels)
	 * @frame_samples:     samples per channel at proc_rate
	 * @num_mic_channels:  number of mic/output channels
	 * @num_ref_channels:  number of echo reference channels
	 */
	int (*process_frame)(struct processing_module *mod,
			     const int16_t *const *mic,
			     const int16_t *const *ref,
			     int16_t *const *out,
			     int frame_samples,
			     int num_mic_channels, int num_ref_channels);

	/* Reset adaptive filters (keep configuration). Called from reset(). */
	int (*reset)(struct processing_module *mod);

	/* Tear down all state from configure(). */
	int (*free)(struct processing_module *mod);
};

/**
 * struct webrtc_aec_comp_data - webrtc_aec module private data.
 */
struct webrtc_aec_comp_data {
	const struct webrtc_aec_backend *backend;
	void *backend_data;

	/* Runtime ALSA mixer controls */
	bool enabled;          /* master bypass switch (WEBRTC_AEC_CTL_ID_ENABLE) */
	bool high_suppression; /* suppression switch (WEBRTC_AEC_CTL_ID_HIGH_SUPPRESSION) */
	int delay_ms;          /* echo path / soundcard buffer delay (ms) */
	bool downmix_ref;      /* downmix multi-ch ref to mono before backend */

	/* Source routing (resolved in prepare / process). */
	int mic_src;          /* index into sources[] for microphone */
	int ref_src;          /* index into sources[] for echo reference */

	/* Negotiated format. */
	int rate;             /* pipeline sample rate (Hz) */
	int proc_rate;        /* AEC processing rate (8000 or 16000 Hz) */
	int mic_channels;     /* microphone / output channel count */
	int ref_channels;     /* playback reference channel count */
	int effective_ref_ch; /* ref channels fed to backend (1 if downmixed) */
	int mic_frame_bytes;  /* bytes per pipeline frame on mic input */
	int ref_frame_bytes;  /* bytes per pipeline frame on ref input */
	int out_frame_bytes;
	bool mic_is_s32;      /* true for S32_LE mic input */
	bool ref_is_s32;      /* true for S32_LE ref input */
	bool out_is_s32;      /* true for S32_LE sink output */
	enum webrtc_aec_resamp_mode resamp_mode;

	/* Processing frame size at pipeline rate. */
	int frame_samples;    /* pipeline frames per 10 ms */

	/* Accumulation and lookahead FIFO state. */
	int buffered_in_frames;
	int buffered_out_frames;

	/* Per-channel S16 scratch buffers. */
	int16_t mic_buf[WEBRTC_AEC_CHANNELS_MAX][WEBRTC_AEC_FIFO_FRAMES];
	int16_t ref_buf[WEBRTC_AEC_CHANNELS_MAX][WEBRTC_AEC_FIFO_FRAMES];
	int16_t out_fifo[WEBRTC_AEC_CHANNELS_MAX][WEBRTC_AEC_FIFO_FRAMES];
	int16_t mic16[WEBRTC_AEC_CHANNELS_MAX][WEBRTC_AEC_FRAME_SAMPLES_16K];
	int16_t ref16[WEBRTC_AEC_CHANNELS_MAX][WEBRTC_AEC_FRAME_SAMPLES_16K];
	int16_t out16[WEBRTC_AEC_CHANNELS_MAX][WEBRTC_AEC_FRAME_SAMPLES_16K];

	/* Resampler state for 48kHz <-> 16kHz */
	struct webrtc_aec_state_48_to_16 mic_resamp[WEBRTC_AEC_CHANNELS_MAX];
	struct webrtc_aec_state_48_to_16 ref_resamp[WEBRTC_AEC_CHANNELS_MAX];
	struct webrtc_aec_state_16_to_48 out_resamp[WEBRTC_AEC_CHANNELS_MAX];
	int32_t resamp_tmpmem[512];

	/* Resampler state for 32kHz <-> 16kHz (WebRtcSpl_DownsampleBy2/UpsampleBy2) */
	int32_t mic_resamp32[WEBRTC_AEC_CHANNELS_MAX][8];
	int32_t ref_resamp32[WEBRTC_AEC_CHANNELS_MAX][8];
	int32_t out_resamp32[WEBRTC_AEC_CHANNELS_MAX][8];

	/* Ref stream liveness. */
	bool last_ref_ok;
	bool ref_mismatch_warned;

	bool configured;
};

/* Backend instance exported from the selected translation unit. */
extern const struct webrtc_aec_backend webrtc_aec_backend;

#if !defined(CONFIG_COMP_WEBRTC_AEC_STUB)
void webrtc_aec_set_active_mod(struct processing_module *mod);
struct processing_module *webrtc_aec_get_active_mod(void);
#else
static inline void webrtc_aec_set_active_mod(struct processing_module *mod) { (void)mod; }
static inline struct processing_module *webrtc_aec_get_active_mod(void) { return NULL; }
#endif

#endif /* __SOF_AUDIO_WEBRTC_AEC_H__ */
