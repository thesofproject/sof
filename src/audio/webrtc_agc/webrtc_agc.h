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
#include <stdint.h>
#include <webrtc/modules/audio_processing/agc/legacy/digital_agc.h>

#define WEBRTC_AGC_CHANNELS_MAX		4
#define WEBRTC_AGC_FRAME_SAMPLES_MAX	480 /* 10 ms @ 48 kHz */
#define WEBRTC_AGC_FIFO_FRAMES		(WEBRTC_AGC_FRAME_SAMPLES_MAX * 2)

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
	int16_t in_fifo[WEBRTC_AGC_FIFO_FRAMES];
	int16_t out_fifo[WEBRTC_AGC_FIFO_FRAMES];
};

struct webrtc_agc_comp_data {
	struct webrtc_agc_channel ch_data[WEBRTC_AGC_CHANNELS_MAX];
	const struct webrtc_agc_backend *backend;

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
