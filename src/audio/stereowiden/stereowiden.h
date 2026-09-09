/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2026 Intel Corporation.
 *
 * FFmpeg stereo widener (stereowiden) module for SOF.
 */

#ifndef __SOF_AUDIO_STEREOWIDEN_H__
#define __SOF_AUDIO_STEREOWIDEN_H__

#include <sof/audio/module_adapter/module/generic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define STEREOWIDEN_CHANNELS_MAX	2
#define STEREOWIDEN_DELAY_MAX_MS	20
#define STEREOWIDEN_DELAY_MAX_FRAMES	960

struct stereowiden_backend {
	const char *name;
	int (*init)(struct processing_module *mod);
	int (*prepare)(struct processing_module *mod);
	int (*process)(struct processing_module *mod,
		       const void *src, void *dst, int frames,
		       enum sof_ipc_frame fmt);
	int (*reset)(struct processing_module *mod);
	int (*free)(struct processing_module *mod);
};

struct stereowiden_comp_data {
	int32_t buffer[STEREOWIDEN_DELAY_MAX_FRAMES * STEREOWIDEN_CHANNELS_MAX];
	size_t buf_pos;
	size_t delay_frames;

	int32_t drymix;    /* Q30 */
	int32_t crossfeed; /* Q30 */
	int32_t feedback;  /* Q30 */

	const struct stereowiden_backend *backend;
	int channels;
	int rate;
	int delay_ms;
	bool enabled;
};

extern const struct stereowiden_backend stereowiden_real_backend;
extern const struct stereowiden_backend stereowiden_stub_backend;

void stereowiden_process_s16(struct stereowiden_comp_data *cd,
			     const int16_t *src, int16_t *dst,
			     size_t frames);

void stereowiden_process_s32(struct stereowiden_comp_data *cd,
			     const int32_t *src, int32_t *dst,
			     size_t frames);

#endif /* __SOF_AUDIO_STEREOWIDEN_H__ */
