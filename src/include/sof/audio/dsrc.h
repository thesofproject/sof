/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright 2026 Intel Corporation. All rights reserved.
 */

#ifndef __SOF_AUDIO_DSRC_H__
#define __SOF_AUDIO_DSRC_H__

#include <stddef.h>
#include <stdint.h>
#include <sof/platform.h>

struct dsrc {
	size_t channels;
	int64_t previous_sample_norm[PLATFORM_MAX_CHANNELS];
	uint64_t phase_acc;
	uint64_t max_phase;
};

void dsrc_init(struct dsrc *dsrc, size_t channels);
void dsrc_set_rate(struct dsrc *dsrc, uint32_t in_rate, uint32_t out_rate);
size_t dsrc_process(struct dsrc *dsrc, const struct cir_buf_ptr *in,
		  struct cir_buf_ptr *out, size_t frames);

#endif /* __SOF_AUDIO_DSRC_H__ */
