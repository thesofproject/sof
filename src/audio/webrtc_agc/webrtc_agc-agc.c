/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2026 Intel Corporation.
 *
 * Real WebRTC Digital AGC backend implementation.
 */

#include "webrtc_agc.h"
#include <sof/audio/format.h>
#include <rtos/string.h>
#include <stdint.h>

/* Overload limiter constants (matching digital_agc.c) using SOF Q-format macros */
#define WEBRTC_AGC_PROBE_MAX_Q			13
#define WEBRTC_AGC_PROBE_HEADROOM_Q15		Q_CONVERT_FLOAT(0.5, 15)
#define WEBRTC_AGC_LIMITER_SHIFT_DEFAULT	10
#define WEBRTC_AGC_GAIN_NORM_TLD_Q16		Q_CONVERT_FLOAT(724.0773439, 16)
#define WEBRTC_AGC_LIMITER_DECAY_Q8		253 /* 253/256 (~-0.1 dB) in Q8 */

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

	cd->frame_samples = (cd->rate * WEBRTC_AGC_FRAME_MS) / 1000;
	if (cd->frame_samples > WEBRTC_AGC_FRAME_SAMPLES_MAX)
		return -EINVAL;

	cd->in_fifo_count = 0;
	cd->out_fifo_count = cd->frame_samples;
	cd->out_fifo_rd = 0;

	for (c = 0; c < cd->channels; c++) {
		struct webrtc_agc_channel *ch = &cd->ch_data[c];

		memset(ch->in_fifo, 0, sizeof(ch->in_fifo));
		memset(ch->out_fifo, 0, sizeof(ch->out_fifo));
		memset(ch->down_state_32_to_16, 0, sizeof(ch->down_state_32_to_16));
		WebRtcSpl_ResetResample48khzTo16khz(&ch->state_48_to_16);

		WebRtcAgc_InitDigital(&ch->inst, cd->mode);
		WebRtcAgc_CalculateGainTable(ch->inst.gainTable,
					     cd->compression_gain_db,
					     cd->target_dbfs,
					     cd->limiter_enable,
					     0);
	}

	return 0;
}

static void webrtc_agc_process_multiband(struct webrtc_agc_comp_data *cd,
					 struct webrtc_agc_channel *ch,
					 size_t num_bands, int16_t *dst)
{
	int16_t *vad_buf = cd->vad_buf;
	int16_t *probe = cd->probe;
	int32_t *gains = cd->gains;
	const int16_t *in_ptrs[2];
	int16_t *out_ptrs[2];
	int32_t max_gain_q16;
	int probe_shift;
	int16_t probe_val;
	size_t k, n, b;

	/*
	 * 2a. Anti-aliased 16 kHz speech-analysis downsampling for Band 0.
	 *     WebRtcAgc_ProcessDigital() feeds Band 0 into WebRtcAgc_ProcessVad()
	 *     to control adaptive gain decay and noise gating. Downsample the
	 *     incoming 32 kHz or 48 kHz 10 ms frame to 16 kHz with WebRTC's
	 *     low-pass filters so high-frequency content (> 8 kHz) does not alias
	 *     into the VAD band.
	 */
	switch (cd->rate) {
	case 32000:
		WebRtcSpl_DownsampleBy2(ch->in_fifo, cd->frame_samples, vad_buf,
					ch->down_state_32_to_16);
		break;
	case 48000:
	default:
		WebRtcSpl_Resample48khzTo16khz(ch->in_fifo, vad_buf,
					       &ch->state_48_to_16,
					       cd->resamp_tmpmem);
		break;
	}

	/*
	 * 2b. Run WebRtcAgc_ProcessDigital() at WEBRTC_AGC_BAND_RATE_HZ (16 kHz)
	 *     on two synthetic 16 kHz sub-band vectors: the filtered vad_buf
	 *     (Band 0, used for VAD and envelope tracking) and a constant probe
	 *     vector (Band 1, used to extract the internal 10-subframe gain
	 *     trajectory across WEBRTC_AGC_SUBFRAMES = 10 x 1 ms subframes).
	 *     Bound the probe amplitude using the maximum of all gainTable
	 *     entries and the current gain so the scaled int16_t probe output
	 *     cannot overflow.
	 */
	max_gain_q16 = ch->inst.gain;
	for (n = 0; n < ARRAY_SIZE(ch->inst.gainTable); n++) {
		if (ch->inst.gainTable[n] > max_gain_q16)
			max_gain_q16 = ch->inst.gainTable[n];
	}

	probe_shift = WEBRTC_AGC_PROBE_MAX_Q;
	while (probe_shift > 0 &&
	       Q_SHIFT(max_gain_q16, 16, probe_shift) >
	       WEBRTC_AGC_PROBE_HEADROOM_Q15)
		probe_shift--;
	probe_val = (int16_t)(1 << probe_shift);

	for (n = 0; n < WEBRTC_AGC_BAND_SAMPLES; n++)
		probe[n] = probe_val;

	/*
	 * cd->gains has WEBRTC_AGC_GAIN_ENVELOPE_LEN (WEBRTC_AGC_SUBFRAMES + 1 = 11)
	 * elements: gains[0] holds the initial frame gain and gains[1..10] hold
	 * the end-of-subframe gains for subframes k = 0..9.
	 */
	gains[0] = ch->inst.gain;
	in_ptrs[0] = vad_buf;
	in_ptrs[1] = probe;
	out_ptrs[0] = vad_buf;
	out_ptrs[1] = probe;

	WebRtcAgc_ProcessDigital(&ch->inst, in_ptrs, ARRAY_SIZE(in_ptrs),
				 out_ptrs, WEBRTC_AGC_BAND_RATE_HZ, 0);

	/*
	 * Recover the Q16 gain at the start of each 1 ms subframe (10 subframes
	 * per 10 ms frame, each WEBRTC_AGC_SUBFRAME_BAND_SAMPLES = 16 samples
	 * long at the 16 kHz band rate). Index WEBRTC_AGC_SUBFRAMES (10) is the
	 * final element of gains[WEBRTC_AGC_GAIN_ENVELOPE_LEN] (size 11).
	 */
	for (k = 1; k < WEBRTC_AGC_SUBFRAMES; k++)
		gains[k] = Q_SHIFT_LEFT((int32_t)probe[k * WEBRTC_AGC_SUBFRAME_BAND_SAMPLES],
					probe_shift, 16);
	gains[WEBRTC_AGC_SUBFRAMES] = ch->inst.gain;

	/*
	 * 2c. Separate full-rate overload limiter peak tracking.
	 *     Measure the true 32/48 kHz peak energy across all samples of each
	 *     1 ms subframe (k = 0..9, where gains[k + 1] indexes gains[1..10]
	 *     within gains[WEBRTC_AGC_GAIN_ENVELOPE_LEN]) and apply the WebRTC
	 *     overload limiter (-0.1 dB step-down factor 253/256 and 1 ms
	 *     lookahead) so high-frequency peaks attenuated in vad_buf cannot
	 *     clip the full-rate output.
	 */
	for (k = 0; k < WEBRTC_AGC_SUBFRAMES; k++) {
		size_t subfr_samples = WEBRTC_AGC_SUBFRAME_BAND_SAMPLES * num_bands;
		const int16_t *subfr = &ch->in_fifo[k * subfr_samples];
		int32_t max_nrg = 0;
		int16_t zeros = WEBRTC_AGC_LIMITER_SHIFT_DEFAULT;
		int32_t gain32;

		for (n = 0; n < subfr_samples; n++) {
			int32_t s = subfr[n];
			int32_t nrg = s * s;

			if (nrg > max_nrg)
				max_nrg = nrg;
		}

		/* k < WEBRTC_AGC_SUBFRAMES (10), so k + 1 <= 10 < WEBRTC_AGC_GAIN_ENVELOPE_LEN */
		if (gains[k + 1] > WEBRTC_AGC_GAIN_NORM_TLD_Q16)
			zeros = 16 - WebRtcSpl_NormW32(gains[k + 1]);

		gain32 = (gains[k + 1] >> zeros) + 1;
		gain32 *= gain32;

		while (AGC_MUL32(Q_SHIFT(max_nrg, 30, 18) + 1, gain32) >
		       WEBRTC_SPL_SHIFT_W32((int32_t)INT16_MAX,
					    2 * (1 - zeros + WEBRTC_AGC_LIMITER_SHIFT_DEFAULT))) {
			gains[k + 1] = (int32_t)Q_MULTS_32X32((int64_t)gains[k + 1],
							      WEBRTC_AGC_LIMITER_DECAY_Q8,
							      16, 8, 16);
			gain32 = (gains[k + 1] >> zeros) + 1;
			gain32 *= gain32;
		}
	}

	for (k = 1; k < WEBRTC_AGC_SUBFRAMES; k++) {
		if (gains[k] > gains[k + 1])
			gains[k] = gains[k + 1];
	}
	ch->inst.gain = gains[WEBRTC_AGC_SUBFRAMES];

	/*
	 * 2d. Apply the 10-subframe interpolated gain trajectory across all
	 *     full-rate 32/48 kHz samples into dst using 64-bit Q-format
	 *     multiplication before saturating to int16_t.
	 */
	for (k = 0; k < WEBRTC_AGC_SUBFRAMES; k++) {
		int32_t delta = gains[k + 1] - gains[k];
		int32_t gain32 = Q_SHIFT_LEFT(gains[k], 16, 20);

		for (n = 0; n < WEBRTC_AGC_SUBFRAME_BAND_SAMPLES; n++) {
			int32_t g = Q_SHIFT(gain32, 20, 16);
			size_t base = (k * WEBRTC_AGC_SUBFRAME_BAND_SAMPLES + n) * num_bands;

			for (b = 0; b < num_bands; b++) {
				int64_t tmp64 = Q_MULTS_32X32((int64_t)ch->in_fifo[base + b],
							      (int64_t)g, 15, 16, 15);

				dst[base + b] = sat_int16(sat_int32(tmp64));
			}
			gain32 += delta;
		}
	}
}

static int webrtc_agc_real_process(struct processing_module *mod,
				   const void *src, void *dst, int frames,
				   enum sof_ipc_frame fmt)
{
	struct webrtc_agc_comp_data *cd = module_get_private_data(mod);
	size_t ch_count = cd->channels;
	size_t num_bands = (cd->rate <= WEBRTC_AGC_BAND_RATE_HZ) ?
			   1 : (size_t)(cd->rate / WEBRTC_AGC_BAND_RATE_HZ);
	size_t offset = 0;
	size_t c, i;

	if (frames <= 0)
		return 0;

	/*
	 * Process in chunks of at most (frame_samples - in_fifo_count).
	 * Because out_fifo is pre-seeded with one 10 ms frame (frame_samples)
	 * of zeros in prepare()/reset(), the invariant
	 * (out_fifo_count - out_fifo_rd) == (frame_samples - in_fifo_count)
	 * holds at the start of every iteration:
	 *   1. Drain chunk frames from previously processed out_fifo data.
	 *   2. Enqueue chunk frames of new input into in_fifo.
	 *   3. When a full 10 ms frame is accumulated (in_fifo_count ==
	 *      frame_samples, which simultaneously exhausts out_fifo), process
	 *      the 10 ms block into out_fifo[0..frame_samples - 1] and reset
	 *      both FIFO cursors.
	 */
	while (offset < (size_t)frames) {
		size_t rem = (size_t)frames - offset;
		size_t space = (size_t)cd->frame_samples - cd->in_fifo_count;
		size_t chunk = (rem < space) ? rem : space;

		/* 1. Drain chunk of processed frames from out_fifo to dst */
		if (fmt == SOF_IPC_FRAME_S16_LE) {
			int16_t *d = (int16_t *)dst + offset * ch_count;

			for (i = 0; i < chunk; i++) {
				for (c = 0; c < ch_count; c++)
					d[i * ch_count + c] =
						cd->ch_data[c].out_fifo[cd->out_fifo_rd + i];
			}
		} else {
			int32_t *d = (int32_t *)dst + offset * ch_count;

			for (i = 0; i < chunk; i++) {
				for (c = 0; c < ch_count; c++)
					d[i * ch_count + c] =
						Q_SHIFT_LEFT((int32_t)cd->ch_data[c].out_fifo[cd->out_fifo_rd + i],
							     15, 31);
			}
		}
		cd->out_fifo_rd += chunk;

		/* 2. Enqueue chunk of input frames into planar in_fifo */
		if (fmt == SOF_IPC_FRAME_S16_LE) {
			const int16_t *s = (const int16_t *)src + offset * ch_count;

			for (i = 0; i < chunk; i++) {
				for (c = 0; c < ch_count; c++)
					cd->ch_data[c].in_fifo[cd->in_fifo_count + i] =
						s[i * ch_count + c];
			}
		} else {
			const int32_t *s = (const int32_t *)src + offset * ch_count;

			for (i = 0; i < chunk; i++) {
				for (c = 0; c < ch_count; c++)
					cd->ch_data[c].in_fifo[cd->in_fifo_count + i] =
						sat_int16(Q_SHIFT_RND(s[i * ch_count + c],
								      31, 15));
			}
		}
		cd->in_fifo_count += chunk;

		/* 3. Process a complete 10 ms block when in_fifo is full */
		if (cd->in_fifo_count == (size_t)cd->frame_samples) {
			for (c = 0; c < ch_count; c++) {
				struct webrtc_agc_channel *ch = &cd->ch_data[c];
				int16_t *out_ptr = ch->out_fifo;

				if (num_bands == 1) {
					const int16_t *in_ptr = ch->in_fifo;

					WebRtcAgc_ProcessDigital(&ch->inst, &in_ptr, 1,
								 &out_ptr, cd->rate, 0);
				} else {
					webrtc_agc_process_multiband(cd, ch,
								     num_bands, out_ptr);
				}
			}
			cd->in_fifo_count = 0;
			cd->out_fifo_rd = 0;
			cd->out_fifo_count = cd->frame_samples;
		}

		offset += chunk;
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
		memset(cd->ch_data[c].down_state_32_to_16, 0,
		       sizeof(cd->ch_data[c].down_state_32_to_16));
		WebRtcSpl_ResetResample48khzTo16khz(&cd->ch_data[c].state_48_to_16);
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
