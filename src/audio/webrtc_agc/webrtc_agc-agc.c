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

	cd->frame_samples = (cd->rate * 10) / 1000;
	if (cd->frame_samples > WEBRTC_AGC_FRAME_SAMPLES_MAX)
		return -EINVAL;

	cd->in_fifo_count = 0;
	cd->out_fifo_count = 0;
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

		for (c = 0; c < ch_count; c++) {
			struct webrtc_agc_channel *ch = &cd->ch_data[c];
			const int16_t *in_ptr = ch->in_fifo;
			int16_t *out_ptr = proc_out;

			WebRtcAgc_ProcessDigital(&ch->inst, &in_ptr, 1, &out_ptr,
						 (cd->rate <= 16000) ? cd->rate : 16000, 0);

			/* Append to out_fifo */
			memcpy(&ch->out_fifo[cd->out_fifo_count], proc_out,
			       cd->frame_samples * sizeof(int16_t));

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
			}
		}

		/* If out_fifo had fewer frames than requested, zero-fill or pass-through remainder */
		if (frames_to_write < (size_t)frames) {
			size_t rem = (size_t)frames - frames_to_write;
			if (fmt == SOF_IPC_FRAME_S16_LE) {
				const int16_t *s = (const int16_t *)src + frames_to_write * ch_count;
				int16_t *d = (int16_t *)dst + frames_to_write * ch_count;
				memcpy(d, s, rem * ch_count * sizeof(int16_t));
			} else {
				const int32_t *s = (const int32_t *)src + frames_to_write * ch_count;
				int32_t *d = (int32_t *)dst + frames_to_write * ch_count;
				memcpy(d, s, rem * ch_count * sizeof(int32_t));
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
	cd->out_fifo_count = 0;
	cd->out_fifo_rd = 0;

	for (c = 0; c < cd->channels; c++) {
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
