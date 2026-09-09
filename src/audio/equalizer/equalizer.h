/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2026 Intel Corporation.
 *
 * FFmpeg parametric equalizer (equalizer) module for SOF.
 */

#ifndef __SOF_AUDIO_EQUALIZER_H__
#define __SOF_AUDIO_EQUALIZER_H__

#include <sof/audio/module_adapter/module/generic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define EQUALIZER_CHANNELS_MAX	2
#define EQUALIZER_BANDS_MAX	3

struct biquad_band {
	int32_t b0; /* Q30 coefficients */
	int32_t b1;
	int32_t b2;
	int32_t a1;
	int32_t a2;
	int64_t d1[EQUALIZER_CHANNELS_MAX]; /* state registers per channel */
	int64_t d2[EQUALIZER_CHANNELS_MAX];
};

struct equalizer_backend {
	const char *name;
	int (*init)(struct processing_module *mod);
	int (*prepare)(struct processing_module *mod);
	int (*process)(struct processing_module *mod,
		       const void *src, void *dst, int frames,
		       enum sof_ipc_frame fmt);
	int (*reset)(struct processing_module *mod);
	int (*free)(struct processing_module *mod);
};

struct equalizer_comp_data {
	struct biquad_band bands[EQUALIZER_BANDS_MAX];
	const struct equalizer_backend *backend;
	int channels;
	int rate;
	bool enabled;
	bool bass_boost;
};

extern const struct equalizer_backend equalizer_real_backend;
extern const struct equalizer_backend equalizer_stub_backend;

void equalizer_update_coeffs(struct equalizer_comp_data *cd);

void equalizer_process_s16(struct equalizer_comp_data *cd,
			   const int16_t *src, int16_t *dst,
			   size_t frames);

void equalizer_process_s32(struct equalizer_comp_data *cd,
			   const int32_t *src, int32_t *dst,
			   size_t frames);

#endif /* __SOF_AUDIO_EQUALIZER_H__ */
