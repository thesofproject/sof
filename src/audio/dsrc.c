/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright 2026 Intel Corporation. All rights reserved.
 */

#include <rtos/string.h>
#include <sof/audio/audio_stream.h>
#include <sof/audio/dsrc.h>

void dsrc_init(struct dsrc *dsrc, size_t channels)
{
	memset(dsrc, 0, sizeof(*dsrc));
	dsrc->channels = channels;
}

void dsrc_set_rate(struct dsrc *dsrc, uint32_t in_rate, uint32_t out_rate)
{
	uint64_t max_phase = 0;

	if (out_rate > in_rate)
		max_phase = ((uint64_t)in_rate << 32) / (out_rate - in_rate);
	else
		assert(out_rate == in_rate);

	if (max_phase == dsrc->max_phase)
		return;

	if (max_phase && dsrc->max_phase) {
		uint32_t old_32 = dsrc->max_phase >> 32;
		uint32_t new_32 = max_phase >> 32;

		/* Used below as divisors, non-zero as long as out_rate <= 2 * in_rate */
		assert(old_32 && new_32);

		/* Keep the same fractional delay, and phase_acc below the new max_phase */
		dsrc->phase_acc = ((dsrc->phase_acc >> 32) * new_32 / old_32) << 32;

		/* previous_sample_norm is normalized by max_phase, renormalize it */
		for (size_t ch = 0; ch < dsrc->channels; ch++) {
			int64_t sample = (dsrc->previous_sample_norm[ch] * old_32) >> 32;

			dsrc->previous_sample_norm[ch] = sample * ((int64_t)1 << 32) / new_32;
		}
	} else {
		/* phase_acc == 0 gives previous samples zero weight */
		dsrc->phase_acc = 0;
	}

	dsrc->max_phase = max_phase;
}

/* phase_acc and max_phase use uint64_t with << 32 to achieve high fractional precision
 * without using floating-point arithmetic. The phase increment is 1 (i.e., 1 << 32).
 * Frequency precision is 1 Hz. Audio data is 32-bit.
 */
size_t dsrc_process(struct dsrc *dsrc, const struct cir_buf_ptr *in,
		  struct cir_buf_ptr *out, size_t frames)
{
	if (dsrc->max_phase == 0) {
		/* No resampling needed, just copy the data. */
		cir_buf_copy(in->ptr, in->buf_start, in->buf_end, out->ptr,
			     out->buf_start, out->buf_end,
			     sizeof(int32_t) * dsrc->channels * frames);

		return 0;
	}

	int32_t *in_ptr = in->ptr;
	int32_t *out_ptr = out->ptr;
	size_t added_frames = 0;

	/* Two different data types are needed: dsrc->max_phase must be 64 bits to maintain
	 * precision when calculating dsrc->phase_acc rollover, but max_phase_32 must be 32 bits
	 * to prevent overflow during multiplication.
	 */
	uint32_t max_phase_32 = dsrc->max_phase >> 32;

	while (frames--) {
		/* Same precision considerations as max_phase_32 above apply here. */
		uint32_t phase_acc_32 = dsrc->phase_acc >> 32;

		for (size_t ch = 0; ch < dsrc->channels; ch++) {
			in_ptr = cir_buf_wrap(in_ptr, in->buf_start, in->buf_end);
			out_ptr = cir_buf_wrap(out_ptr, out->buf_start, out->buf_end);

			int64_t sample_norm = *in_ptr * ((int64_t)1 << 32) / max_phase_32;

			int64_t previous_sample_norm = dsrc->previous_sample_norm[ch];
			dsrc->previous_sample_norm[ch] = sample_norm;

			*out_ptr = (sample_norm * (max_phase_32 - phase_acc_32) +
				    previous_sample_norm * phase_acc_32) >> 32;

			in_ptr++;
			out_ptr++;
		}

		dsrc->phase_acc += (uint64_t)1 << 32;

		if (dsrc->phase_acc >= dsrc->max_phase) {
			dsrc->phase_acc -= dsrc->max_phase;
			added_frames++;

			/* Insert new frame here */
			for (size_t ch = 0; ch < dsrc->channels; ch++) {
				out_ptr = cir_buf_wrap(out_ptr, out->buf_start, out->buf_end);

				*out_ptr = (dsrc->previous_sample_norm[ch] * max_phase_32) >> 32;

				out_ptr++;
			}
		}
	}

	return added_frames;
}
