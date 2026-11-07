/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2026 Intel Corporation.
 *
 * WebRTC Automatic Gain Control (AGC) module for SOF.
 */

#ifndef __SOF_AUDIO_WEBRTC_AGC_H__
#define __SOF_AUDIO_WEBRTC_AGC_H__

#include <sof/audio/module_adapter/module/generic.h>
#include <stdbool.h>
#if !CONFIG_COMP_WEBRTC_AGC_STUB
#include <webrtc/modules/audio_processing/agc/legacy/digital_agc.h>
#else
typedef struct { uint32_t dummy[64]; } DigitalAgc;
#endif

#define WEBRTC_AGC_CHANNELS_MAX			4
/*
 * WebRTC's WebRtcAgc_ProcessDigital() algorithm operates on a fixed 10 ms frame
 * (10 x 1 ms subframes); the module's internal framing FIFO decouples this from
 * the SOF pipeline period.
 */
#define WEBRTC_AGC_FRAME_MS			10
/* 10 x 1 ms subframes per 10 ms frame; 11 envelope boundary points (0..10) */
#define WEBRTC_AGC_SUBFRAMES			10
#define WEBRTC_AGC_GAIN_ENVELOPE_LEN		(WEBRTC_AGC_SUBFRAMES + 1)
#define WEBRTC_AGC_BAND_RATE_HZ			16000
#define WEBRTC_AGC_MAX_RATE_HZ			48000
#define WEBRTC_AGC_BAND_SAMPLES			\
	((WEBRTC_AGC_BAND_RATE_HZ * WEBRTC_AGC_FRAME_MS) / 1000)
#define WEBRTC_AGC_SUBFRAME_BAND_SAMPLES	\
	(WEBRTC_AGC_BAND_SAMPLES / WEBRTC_AGC_SUBFRAMES)
#define WEBRTC_AGC_FRAME_SAMPLES_MAX		\
	((WEBRTC_AGC_MAX_RATE_HZ * WEBRTC_AGC_FRAME_MS) / 1000)
#define WEBRTC_AGC_FIFO_FRAMES			WEBRTC_AGC_FRAME_SAMPLES_MAX
#define WEBRTC_AGC_RESAMP_TMPMEM_LEN		(WEBRTC_AGC_FRAME_SAMPLES_MAX + 16)

struct webrtc_agc_backend {
	const char *name;
	int (*init)(struct processing_module *mod);
	int (*prepare)(struct processing_module *mod);
	int (*process)(struct processing_module *mod,
		       const void *src, void *dst, int frames,
		       enum sof_ipc_frame fmt);
	int (*reset)(struct processing_module *mod);
	int (*free)(struct processing_module *mod);
};

struct webrtc_agc_channel {
	DigitalAgc inst;
#if !CONFIG_COMP_WEBRTC_AGC_STUB
	int32_t down_state_32_to_16[8];
	WebRtcSpl_State48khzTo16khz state_48_to_16;
#endif
	int16_t in_fifo[WEBRTC_AGC_FIFO_FRAMES];
	int16_t out_fifo[WEBRTC_AGC_FIFO_FRAMES];
};

struct webrtc_agc_comp_data {
	struct webrtc_agc_channel ch_data[WEBRTC_AGC_CHANNELS_MAX];
	const struct webrtc_agc_backend *backend;

#if !CONFIG_COMP_WEBRTC_AGC_STUB
	/* Scratch buffers for multi-band (32/48 kHz) processing */
	int16_t vad_buf[WEBRTC_AGC_BAND_SAMPLES];
	int16_t probe[WEBRTC_AGC_BAND_SAMPLES];
	int32_t gains[WEBRTC_AGC_GAIN_ENVELOPE_LEN];
	int32_t resamp_tmpmem[WEBRTC_AGC_RESAMP_TMPMEM_LEN];
#endif

	int channels;
	int rate;
	int frame_samples; /* samples per 10 ms frame */

	/* Framing FIFO state */
	size_t in_fifo_count;
	size_t out_fifo_count;
	size_t out_fifo_rd;

	/* Configuration */
	int16_t target_dbfs;
	int16_t compression_gain_db;
	uint8_t limiter_enable;
	int16_t mode;
	bool enabled;
};

extern const struct webrtc_agc_backend webrtc_agc_real_backend;
extern const struct webrtc_agc_backend webrtc_agc_stub_backend;

#endif /* __SOF_AUDIO_WEBRTC_AGC_H__ */
