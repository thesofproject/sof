/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2026 Intel Corporation.
 *
 * FFmpeg lookahead peak limiter (alimiter) module for SOF.
 */

#ifndef __SOF_AUDIO_ALIMITER_H__
#define __SOF_AUDIO_ALIMITER_H__

#include <sof/audio/module_adapter/module/generic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define ALIMITER_CHANNELS_MAX		4
#define ALIMITER_LOOKAHEAD_MAX_MS	10
#define ALIMITER_LOOKAHEAD_MAX_FRAMES	480

struct alimiter_backend {
	const char *name;
	int (*init)(struct processing_module *mod);
	int (*prepare)(struct processing_module *mod);
	int (*process)(struct processing_module *mod,
		       const void *src, void *dst, int frames,
		       enum sof_ipc_frame fmt);
	int (*reset)(struct processing_module *mod);
	int (*free)(struct processing_module *mod);
};

struct alimiter_comp_data {
	int32_t buffer[ALIMITER_LOOKAHEAD_MAX_FRAMES * ALIMITER_CHANNELS_MAX];
	size_t buf_pos;
	size_t buf_len;

	int32_t limit;        /* Q31 ceiling: e.g. 0.95 = 2040109465 */
	int64_t att;          /* Q30 attenuation: 1.0 = (1 << 30) */
	int64_t delta;        /* Q30 delta per frame */
	int64_t release_rate; /* Q30 release slew per frame */

	const struct alimiter_backend *backend;
	int channels;
	int rate;
	int attack_ms;
	int release_ms;
	bool enabled;
};

extern const struct alimiter_backend alimiter_real_backend;
extern const struct alimiter_backend alimiter_stub_backend;

void alimiter_process_channel_s16(struct alimiter_comp_data *cd,
				  const int16_t *src, int16_t *dst,
				  size_t frames);

void alimiter_process_channel_s32(struct alimiter_comp_data *cd,
				  const int32_t *src, int32_t *dst,
				  size_t frames);

#endif /* __SOF_AUDIO_ALIMITER_H__ */
