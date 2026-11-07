/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2026 Intel Corporation.
 *
 * Real WebRTC Digital AGC backend implementation.
 */

#include "webrtc_agc.h"
#include <rtos/string.h>

static int webrtc_agc_real_init(struct processing_module *mod)
{
	return 0;
}

static int webrtc_agc_real_prepare(struct processing_module *mod)
{
	struct webrtc_agc_comp_data *cd = module_get_private_data(mod);
	int c;

	if (cd->rate != 8000 && cd->rate != 16000 &&
	    cd->rate != 32000 && cd->rate != 48000)
		return -EINVAL;

	cd->frame_samples = (cd->rate * 10) / 1000;
	if (cd->frame_samples > WEBRTC_AGC_FRAME_SAMPLES_MAX)
		return -EINVAL;

	cd->in_fifo_count = 0;
	cd->out_fifo_count = cd->frame_samples;
	cd->out_fifo_rd = 0;

	for (c = 0; c < cd->channels; c++) {
		struct webrtc_agc_channel *ch = &cd->ch_data[c];

		memset(ch->in_fifo, 0, sizeof(ch->in_fifo));
		memset(ch->out_fifo, 0, sizeof(ch->out_fifo));

		WebRtcAgc_InitDigital(&ch->inst, cd->mode);
		WebRtcAgc_CalculateGainTable(ch->inst.gainTable,
					     cd->compression_gain_db,
					     cd->target_dbfs,
					     cd->limiter_enable,
					     0);
	}

	return 0;
}

static int webrtc_agc_real_process(struct processing_module *mod,
				   const void *src, void *dst, int frames,
				   enum sof_ipc_frame fmt)
{
	struct webrtc_agc_comp_data *cd = module_get_private_data(mod);
	size_t ch_count = cd->channels;
	size_t c, i;

	if (frames <= 0)
		return 0;

	/* 1. Pull input audio into planar in_fifo */
	{
		size_t frames_to_copy = frames;
		if (cd->in_fifo_count + frames_to_copy > WEBRTC_AGC_FIFO_FRAMES)
			frames_to_copy = WEBRTC_AGC_FIFO_FRAMES - cd->in_fifo_count;

		if (fmt == SOF_IPC_FRAME_S16_LE) {
			const int16_t *s = src;
			for (i = 0; i < frames_to_copy; i++) {
				for (c = 0; c < ch_count; c++)
					cd->ch_data[c].in_fifo[cd->in_fifo_count + i] = s[i * ch_count + c];
			}
		} else {
			const int32_t *s = src;
			for (i = 0; i < frames_to_copy; i++) {
				for (c = 0; c < ch_count; c++)
					cd->ch_data[c].in_fifo[cd->in_fifo_count + i] = (int16_t)(s[i * ch_count + c] >> 16);
			}
		}
		cd->in_fifo_count += frames_to_copy;
	}

	/* 2. Process complete 10 ms blocks */
	while (cd->in_fifo_count >= cd->frame_samples &&
	       cd->out_fifo_count + cd->frame_samples <= WEBRTC_AGC_FIFO_FRAMES) {
		int16_t proc_out[WEBRTC_AGC_FRAME_SAMPLES_MAX];
		size_t num_bands = (cd->rate <= 16000) ? 1 : (size_t)(cd->rate / 16000);

		for (c = 0; c < ch_count; c++) {
			struct webrtc_agc_channel *ch = &cd->ch_data[c];

			if (num_bands == 1) {
				const int16_t *in_ptr = ch->in_fifo;
				int16_t *out_ptr = &ch->out_fifo[cd->out_fifo_count];

				WebRtcAgc_ProcessDigital(&ch->inst, &in_ptr, 1, &out_ptr,
							 cd->rate, 0);
			} else {
				const int16_t *in_ptrs[3];
				int16_t *out_ptrs[3];
				uint8_t max_band_idx[160];
				size_t b, m;

				/*
				 * 2a. Partition proc_out[] into num_bands contiguous 160-sample
				 *     sub-band buffers (2 bands @ 32 kHz, 3 bands @ 48 kHz).
				 *     Setting in_ptrs[b] == out_ptrs[b] enables in-place processing
				 *     inside WebRtcAgc_ProcessDigital() without an extra memcpy.
				 */
				for (b = 0; b < num_bands; b++) {
					in_ptrs[b] = &proc_out[b * 160];
					out_ptrs[b] = &proc_out[b * 160];
				}

				/*
				 * 2b. Polyphase-deinterleave the 10 ms frame into num_bands
				 *     160-sample phases and identify the phase b_max with the
				 *     largest magnitude in each num_bands-sample tuple.
				 */
				for (m = 0; m < 160; m++) {
					size_t b_max = 0;
					int32_t max_abs = -1;
					int16_t tmp;

					for (b = 0; b < num_bands; b++) {
						int16_t s = ch->in_fifo[m * num_bands + b];
						int32_t a = (s < 0) ? -(int32_t)s : (int32_t)s;

						proc_out[b * 160 + m] = s;
						if (a > max_abs) {
							max_abs = a;
							b_max = b;
						}
					}

					/*
					 * 2c. Swap the tuple's peak sample into band 0 (proc_out[m]).
					 *     WebRtcAgc_ProcessDigital() computes the 1 ms subframe
					 *     peak energy env[k] and overload limiter threshold from
					 *     band 0 only, then applies the same interpolated gain to
					 *     all bands at index m. Placing the peak in band 0 ensures
					 *     the limiter tracks the true 32/48 kHz peak.
					 */
					max_band_idx[m] = (uint8_t)b_max;
					tmp = proc_out[m];
					proc_out[m] = proc_out[b_max * 160 + m];
					proc_out[b_max * 160 + m] = tmp;
				}

				/*
				 * 2d. Run WebRtcAgc_ProcessDigital() across all num_bands phases.
				 */
				WebRtcAgc_ProcessDigital(&ch->inst, in_ptrs, num_bands,
							 out_ptrs, cd->rate, 0);

				/*
				 * 2e. Restore original sample order by swapping band 0 and b_max
				 *     back, then re-interleave all num_bands phases into out_fifo.
				 */
				for (m = 0; m < 160; m++) {
					size_t b_max = max_band_idx[m];
					int16_t tmp = proc_out[m];

					proc_out[m] = proc_out[b_max * 160 + m];
					proc_out[b_max * 160 + m] = tmp;

					for (b = 0; b < num_bands; b++)
						ch->out_fifo[cd->out_fifo_count + m * num_bands + b] =
							proc_out[b * 160 + m];
				}
			}

			/* Shift remaining in_fifo */
			memmove(ch->in_fifo, &ch->in_fifo[cd->frame_samples],
				(cd->in_fifo_count - cd->frame_samples) * sizeof(int16_t));
		}
		cd->in_fifo_count -= cd->frame_samples;
		cd->out_fifo_count += cd->frame_samples;
	}

	/* 3. Drain processed samples to dst buffer */
	{
		size_t frames_avail = cd->out_fifo_count - cd->out_fifo_rd;
		size_t frames_to_write = (frames_avail < (size_t)frames) ? frames_avail : (size_t)frames;

		if (frames_to_write > 0) {
			if (fmt == SOF_IPC_FRAME_S16_LE) {
				int16_t *d = dst;
				for (i = 0; i < frames_to_write; i++) {
					for (c = 0; c < ch_count; c++)
						d[i * ch_count + c] = cd->ch_data[c].out_fifo[cd->out_fifo_rd + i];
				}
			} else {
				int32_t *d = dst;
				for (i = 0; i < frames_to_write; i++) {
					for (c = 0; c < ch_count; c++)
						d[i * ch_count + c] = ((int32_t)cd->ch_data[c].out_fifo[cd->out_fifo_rd + i]) << 16;
				}
			}

			cd->out_fifo_rd += frames_to_write;
			if (cd->out_fifo_rd == cd->out_fifo_count) {
				cd->out_fifo_rd = 0;
				cd->out_fifo_count = 0;
			} else if (cd->out_fifo_rd > 0) {
				size_t rem_fifo = cd->out_fifo_count - cd->out_fifo_rd;

				for (c = 0; c < ch_count; c++) {
					memmove(cd->ch_data[c].out_fifo,
						&cd->ch_data[c].out_fifo[cd->out_fifo_rd],
						rem_fifo * sizeof(int16_t));
				}
				cd->out_fifo_count = rem_fifo;
				cd->out_fifo_rd = 0;
			}
		}

		/* If out_fifo had fewer frames than requested, zero-fill remainder */
		if (frames_to_write < (size_t)frames) {
			size_t rem = (size_t)frames - frames_to_write;

			if (fmt == SOF_IPC_FRAME_S16_LE) {
				int16_t *d = (int16_t *)dst + frames_to_write * ch_count;

				memset(d, 0, rem * ch_count * sizeof(int16_t));
			} else {
				int32_t *d = (int32_t *)dst + frames_to_write * ch_count;

				memset(d, 0, rem * ch_count * sizeof(int32_t));
			}
		}
	}

	return 0;
}

static int webrtc_agc_real_reset(struct processing_module *mod)
{
	struct webrtc_agc_comp_data *cd = module_get_private_data(mod);
	int c;

	cd->in_fifo_count = 0;
	cd->out_fifo_count = cd->frame_samples;
	cd->out_fifo_rd = 0;

	for (c = 0; c < cd->channels; c++) {
		memset(cd->ch_data[c].in_fifo, 0, sizeof(cd->ch_data[c].in_fifo));
		memset(cd->ch_data[c].out_fifo, 0, sizeof(cd->ch_data[c].out_fifo));
		WebRtcAgc_InitDigital(&cd->ch_data[c].inst, cd->mode);
		WebRtcAgc_CalculateGainTable(cd->ch_data[c].inst.gainTable,
					     cd->compression_gain_db,
					     cd->target_dbfs,
					     cd->limiter_enable,
					     0);
	}

	return 0;
}

static int webrtc_agc_real_free(struct processing_module *mod)
{
	return 0;
}

const struct webrtc_agc_backend webrtc_agc_real_backend = {
	.name    = "webrtc_digital_agc",
	.init    = webrtc_agc_real_init,
	.prepare = webrtc_agc_real_prepare,
	.process = webrtc_agc_real_process,
	.reset   = webrtc_agc_real_reset,
	.free    = webrtc_agc_real_free,
};
