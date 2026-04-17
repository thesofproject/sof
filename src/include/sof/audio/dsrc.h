/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright 2026 Intel Corporation. All rights reserved.
 */

#ifndef __SOF_AUDIO_DSRC_H__
#define __SOF_AUDIO_DSRC_H__

#include <stddef.h>
#include <stdint.h>
#include <sof/platform.h>

/**
 * @brief State of a DSRC (drift-compensating sample rate converter).
 *
 * Upsamples by a small rate difference: one interpolated frame is inserted
 * every in_rate / (out_rate - in_rate) input frames.
 */
struct dsrc {
	size_t channels; /**< Number of channels */
	int64_t previous_sample_norm[PLATFORM_MAX_CHANNELS]; /**< Last input sample / max_phase */
	uint64_t phase_acc; /**< Phase accumulator, Q32.32 */
	uint64_t max_phase; /**< Input frames per inserted frame, Q32.32; 0: no resampling */
};

/**
 * @brief Initialize struct dsrc.
 *
 * @param dsrc DSRC state.
 * @param channels Number of channels, at most PLATFORM_MAX_CHANNELS.
 */
void dsrc_init(struct dsrc *dsrc, size_t channels);

/**
 * @brief Set the conversion rate.
 *
 * Equal rates disable resampling and reset the phase.
 *
 * @param dsrc DSRC state.
 * @param in_rate Input rate in Hz.
 * @param out_rate Output rate in Hz, in_rate <= out_rate <= 2 * in_rate.
 */
void dsrc_set_rate(struct dsrc *dsrc, uint32_t in_rate, uint32_t out_rate);

/**
 * @brief Resample 32-bit frames.
 *
 * Neither @p in nor @p out is advanced; the caller consumes @p frames from the input
 * and produces @p frames plus the returned number of frames into the output.
 *
 * @param dsrc DSRC state.
 * @param in Input circular buffer position.
 * @param out Output circular buffer position, with room for @p frames plus the inserted ones.
 * @param frames Number of input frames.
 *
 * @return Number of extra frames inserted on top of @p frames.
 */
size_t dsrc_process(struct dsrc *dsrc, const struct cir_buf_ptr *in,
		  struct cir_buf_ptr *out, size_t frames);

#endif /* __SOF_AUDIO_DSRC_H__ */
