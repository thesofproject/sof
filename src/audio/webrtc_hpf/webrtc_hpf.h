/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2026 Intel Corporation.
 *
 * WebRTC High-Pass Filter (HPF) module for SOF.
 */

#ifndef __SOF_AUDIO_WEBRTC_HPF_H__
#define __SOF_AUDIO_WEBRTC_HPF_H__

#include <sof/audio/module_adapter/module/generic.h>
#include <stdbool.h>
#include <stdint.h>

#define WEBRTC_HPF_CHANNELS_MAX		8

struct webrtc_hpf_state {
	int16_t y[4];
	int16_t x[2];
	const int16_t *ba;
};

struct webrtc_hpf_backend {
	const char *name;
	int (*init)(struct processing_module *mod);
	int (*prepare)(struct processing_module *mod);
	int (*process)(struct processing_module *mod,
		       const void *src, void *dst, int frames,
		       enum sof_ipc_frame fmt);
	int (*reset)(struct processing_module *mod);
	int (*free)(struct processing_module *mod);
};

struct webrtc_hpf_comp_data {
	struct webrtc_hpf_state state[WEBRTC_HPF_CHANNELS_MAX];
	const struct webrtc_hpf_backend *backend;
	int channels;
	int rate;
	bool enabled;
};

extern const struct webrtc_hpf_backend webrtc_hpf_real_backend;
extern const struct webrtc_hpf_backend webrtc_hpf_stub_backend;

void webrtc_hpf_filter_channel_s16(struct webrtc_hpf_state *hpf,
				   const int16_t *src, int16_t *dst,
				   size_t samples, size_t stride);

void webrtc_hpf_filter_channel_s32(struct webrtc_hpf_state *hpf,
				   const int32_t *src, int32_t *dst,
				   size_t samples, size_t stride);

#endif /* __SOF_AUDIO_WEBRTC_HPF_H__ */
