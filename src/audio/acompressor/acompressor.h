/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2026 Intel Corporation.
 *
 * FFmpeg dynamic range compressor (acompressor) module for SOF.
 */

#ifndef __SOF_AUDIO_ACOMPRESSOR_H__
#define __SOF_AUDIO_ACOMPRESSOR_H__

#include <sof/audio/module_adapter/module/generic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define ACOMPRESSOR_CHANNELS_MAX	4

struct acompressor_backend {
	const char *name;
	int (*init)(struct processing_module *mod);
	int (*prepare)(struct processing_module *mod);
	int (*process)(struct processing_module *mod,
		       const void *src, void *dst, int frames,
		       enum sof_ipc_frame fmt);
	int (*reset)(struct processing_module *mod);
	int (*free)(struct processing_module *mod);
};

struct acompressor_comp_data {
	int32_t threshold;    /* Q31 linear threshold: -12 dBFS = 539423616 */
	int32_t ratio;        /* 3 for 3:1 compression */
	int32_t attack_coeff; /* Q30 */
	int32_t release_coeff;/* Q30 */
	int32_t makeup;       /* Q30 makeup gain: +2 dB = 1351760896 */
	int32_t envelope;     /* Q31 envelope state */

	const struct acompressor_backend *backend;
	int channels;
	int rate;
	int attack_ms;
	int release_ms;
	bool enabled;
	bool heavy_ratio;
};

extern const struct acompressor_backend acompressor_real_backend;
extern const struct acompressor_backend acompressor_stub_backend;

void acompressor_process_s16(struct acompressor_comp_data *cd,
			     const int16_t *src, int16_t *dst,
			     size_t frames);

void acompressor_process_s32(struct acompressor_comp_data *cd,
			     const int32_t *src, int32_t *dst,
			     size_t frames);

#endif /* __SOF_AUDIO_ACOMPRESSOR_H__ */
