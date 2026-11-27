// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.
//
// WebRTC AEC module — SOF IPC4 module_interface core.
//
// Accepts dual IPC4 input pins (queue 0 = microphone capture, queue 1 = playback
// echo reference) and produces a single echo-cancelled capture stream.
// Handles multi-rate 10 ms frame accumulation, optional 48/32 <-> 16 kHz
// resampling, S16/S32 sample conversion, and IPC4 switch controls before
// delegating echo cancellation to the selected backend.

#include <sof/audio/module_adapter/module/generic.h>
#include <module/audio/source_api.h>
#include <module/audio/sink_api.h>
#include <sof/audio/audio_stream.h>
#include <ipc4/header.h>
#include <sof/math/numbers.h>
#include <rtos/init.h>
#include <rtos/string.h>
#include <limits.h>
#include <errno.h>
#include <stdint.h>
#include "webrtc_aec.h"

#if !CONFIG_COMP_WEBRTC_AEC_STUB
#include <signal_processing_library.h>
#else
static inline void WebRtcSpl_Resample48khzTo16khz(const int16_t *in, int16_t *out, void *state, int32_t *tmp)
{
	(void)in; (void)out; (void)state; (void)tmp;
}
static inline void WebRtcSpl_ResetResample48khzTo16khz(void *state)
{
	(void)state;
}
static inline void WebRtcSpl_Resample16khzTo48khz(const int16_t *in, int16_t *out, void *state, int32_t *tmp)
{
	(void)in; (void)out; (void)state; (void)tmp;
}
static inline void WebRtcSpl_ResetResample16khzTo48khz(void *state)
{
	(void)state;
}
#endif

SOF_DEFINE_REG_UUID(webrtc_aec);
LOG_MODULE_REGISTER(webrtc_aec, CONFIG_SOF_LOG_LEVEL);

/* ----------------------------------------------------------------
 * Helper: convert between S32/S16 interleaved ↔ per-channel S16
 * (matching the style of google_rtc_audio_processing.c)
 * ---------------------------------------------------------------- */

/**
 * src_to_s16() - Copy frames from a source into per-channel S16 scratch.
 * Handles both S16 and S32 pipeline formats.
 * @src:    SOF source (provides source_get_data / source_release_data)
 * @n:      number of frames to consume
 * @dst:    per-channel S16 destination (ch × frame_samples)
 * @frame0: offset within dst at which to start writing
 * @ch:     number of channels
 * @is_s32: true if the pipeline format is S32_LE
 */
static void src_to_s16(struct sof_source *src, int n,
		       int16_t dst[][WEBRTC_AEC_FIFO_FRAMES],
		       int frame0, int ch, bool is_s32)
{
	size_t sample_sz = is_s32 ? sizeof(int32_t) : sizeof(int16_t);
	size_t frame_bytes = (size_t)ch * sample_sz;
	size_t nbytes = (size_t)n * frame_bytes;
	struct cir_buf_source source_buf;
	size_t bufsz;
	int remaining = n;
	int cur_frame = frame0;
	int err;

	err = source_get_data(src, nbytes, &source_buf.ptr, &source_buf.buf_start, &bufsz);
	if (err)
		return; /* shouldn't happen if caller checked availability */
	source_buf.buf_end = (const char *)source_buf.buf_start + bufsz;

	while (remaining > 0) {
		size_t fmax = circ_buf_frames_without_wrap(source_buf.ptr, source_buf.buf_end,
							  sample_sz, (size_t)ch);
		size_t chunk = MIN((size_t)remaining, fmax);
		size_t i;
		int c;

		for (i = 0; i < chunk; i++) {
			for (c = 0; c < ch; c++) {
				if (is_s32) {
					int32_t s = *(const int32_t *)source_buf.ptr;

					dst[c][cur_frame + i] = (int16_t)(s >> 16);
					source_buf.ptr = (const char *)source_buf.ptr + sizeof(int32_t);
				} else {
					dst[c][cur_frame + i] = *(const int16_t *)source_buf.ptr;
					source_buf.ptr = (const char *)source_buf.ptr + sizeof(int16_t);
				}
			}
		}

		remaining -= (int)chunk;
		cur_frame += (int)chunk;
		source_buf.ptr = source_cir_buf_wrap(source_buf.ptr, source_buf.buf_start, source_buf.buf_end);
	}

	source_release_data(src, nbytes);
}

/**
 * s16_to_sink() - Write per-channel S16 output to a SOF sink.
 * Upshifts to S32 if the pipeline format requires it, handling buffer wrapping.
 * @dst:    SOF sink
 * @src:    per-channel S16 source (ch × frame_samples, from index 0)
 * @n:      number of frames to write
 * @ch:     number of channels
 * @is_s32: true if the pipeline format is S32_LE
 */
static void s16_to_sink(struct sof_sink *dst, int16_t src[][WEBRTC_AEC_FIFO_FRAMES],
			int n, int ch, bool is_s32)
{
	size_t sample_sz = is_s32 ? sizeof(int32_t) : sizeof(int16_t);
	size_t frame_bytes = (size_t)ch * sample_sz;
	size_t nbytes = (size_t)n * frame_bytes;
	struct cir_buf_sink sink_buf;
	size_t bufsz;
	int remaining = n;
	int cur_frame = 0;
	int err;

	err = sink_get_buffer(dst, nbytes, &sink_buf.ptr, &sink_buf.buf_start, &bufsz);
	if (err)
		return;
	sink_buf.buf_end = (char *)sink_buf.buf_start + bufsz;

	while (remaining > 0) {
		size_t fmax = circ_buf_frames_without_wrap(sink_buf.ptr, sink_buf.buf_end,
							  sample_sz, (size_t)ch);
		size_t chunk = MIN((size_t)remaining, fmax);
		size_t i;
		int c;

		for (i = 0; i < chunk; i++) {
			for (c = 0; c < ch; c++) {
				if (is_s32) {
					*(int32_t *)sink_buf.ptr = (int32_t)src[c][cur_frame + i] << 16;
					sink_buf.ptr = (char *)sink_buf.ptr + sizeof(int32_t);
				} else {
					*(int16_t *)sink_buf.ptr = src[c][cur_frame + i];
					sink_buf.ptr = (char *)sink_buf.ptr + sizeof(int16_t);
				}
			}
		}

		remaining -= (int)chunk;
		cur_frame += (int)chunk;
		sink_buf.ptr = cir_buf_wrap(sink_buf.ptr, sink_buf.buf_start, sink_buf.buf_end);
	}

	sink_commit_buffer(dst, nbytes);
}

static int src_drop_frames(struct sof_source *src, int frames)
{
	size_t frame_bytes = source_get_frame_bytes(src);
	const void *data_ptr, *buf_start;
	size_t buf_size, nbytes;
	int err;

	if (frames <= 0 || !frame_bytes)
		return 0;

	nbytes = (size_t)frames * frame_bytes;
	err = source_get_data(src, nbytes, &data_ptr, &buf_start, &buf_size);
	if (err)
		return err;

	source_release_data(src, nbytes);
	return 0;
}

static void webrtc_aec_resolve_sources(struct sof_source **sources, int num_of_sources,
				       int *mic_idx, int *ref_idx)
{
	uint32_t q0, q1;

	*mic_idx = 0;
	*ref_idx = -1;

	if (num_of_sources <= 1)
		return;

	/*
	 * IPC4 binds topology Pin 0 (queue 0) to the capture microphone stream and
	 * Pin 1 (queue 1) to the playback echo reference. Resolve via queue IDs
	 * because sources[] order depends on pipeline attachment order.
	 */
	q0 = IPC4_SINK_QUEUE_ID(source_get_id(sources[0]));
	q1 = IPC4_SINK_QUEUE_ID(source_get_id(sources[1]));
	if (q0 != q1 && q1 < q0) {
		*mic_idx = 1;
		*ref_idx = 0;
	} else {
		*mic_idx = 0;
		*ref_idx = 1;
	}
}

/* ----------------------------------------------------------------
 * module_interface operations
 * ---------------------------------------------------------------- */

__cold static int webrtc_aec_init(struct processing_module *mod)
{
	struct module_data *md = &mod->priv;
	struct comp_dev *dev = mod->dev;
	struct webrtc_aec_comp_data *cd;
	int ret;

	webrtc_aec_set_active_mod(mod);
	assert_can_be_cold();
	comp_info(dev, "webrtc_aec: init");

	cd = mod_zalloc(mod, sizeof(*cd));
	if (!cd)
		return -ENOMEM;

	md->private = cd;
	cd->backend = &webrtc_aec_backend;
	cd->enabled = false;
	cd->high_suppression = false;

	/* Two input pins: mic + echo reference. */
	mod->max_sources = 2;

	comp_info(dev, "webrtc_aec: backend '%s'", cd->backend->name);

	if (cd->backend->init) {
		ret = cd->backend->init(mod);
		if (ret) {
			comp_err(dev, "webrtc_aec: backend init failed %d", ret);
			mod_free(mod, cd);
			return ret;
		}
	}

	return 0;
}

static void webrtc_aec_reset_resamplers(struct webrtc_aec_comp_data *cd)
{
	int c;

	if (cd->needs_resample) {
		for (c = 0; c < cd->channels; c++) {
			WebRtcSpl_ResetResample48khzTo16khz((void *)&cd->mic_resamp[c]);
			WebRtcSpl_ResetResample48khzTo16khz((void *)&cd->ref_resamp[c]);
			WebRtcSpl_ResetResample16khzTo48khz((void *)&cd->out_resamp[c]);
		}
	}
}

__cold static int webrtc_aec_prepare(struct processing_module *mod,
				     struct sof_source **sources, int num_of_sources,
				     struct sof_sink **sinks, int num_of_sinks)
{
	struct webrtc_aec_comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	int mic_fmt, ref_fmt, mic_rate, ref_rate, ret;

	webrtc_aec_set_active_mod(mod);
	assert_can_be_cold();

	if ((num_of_sources != 1 && num_of_sources != 2) || num_of_sinks != 1) {
		comp_err(dev, "webrtc_aec: need 1 or 2 sources and 1 sink (got %d/%d)",
			 num_of_sources, num_of_sinks);
		return -EINVAL;
	}

	webrtc_aec_resolve_sources(sources, num_of_sources, &cd->mic_src, &cd->ref_src);

	mic_fmt  = source_get_frm_fmt(sources[cd->mic_src]);
	mic_rate = source_get_rate(sources[cd->mic_src]);
	cd->channels = source_get_channels(sources[cd->mic_src]);

	if (cd->ref_src >= 0) {
		/* Pin 1 may come from a separate render pipeline; apply its IPC4 pin format. */
		if (mod->priv.cfg.nb_input_pins > 1 && mod->priv.cfg.input_pins)
			ipc4_update_source_format(sources[cd->ref_src],
						  &mod->priv.cfg.input_pins[1].audio_fmt);
		ref_fmt  = source_get_frm_fmt(sources[cd->ref_src]);
		ref_rate = source_get_rate(sources[cd->ref_src]);
		if (mic_rate != ref_rate) {
			comp_err(dev, "webrtc_aec: mic_rate %d != ref_rate %d", mic_rate, ref_rate);
			return -EINVAL;
		}
	} else {
		ref_fmt  = mic_fmt;
		ref_rate = mic_rate;
	}

	if (cd->channels > WEBRTC_AEC_CHANNELS_MAX) {
		comp_err(dev, "webrtc_aec: too many channels %d (max %d)",
			 cd->channels, WEBRTC_AEC_CHANNELS_MAX);
		return -EINVAL;
	}
	cd->rate = mic_rate;

	/* AECm operates at 16 kHz or 8 kHz natively.
	 * When the pipeline operates at 48 kHz, we use WebRTC APM's fixed-point
	 * resampler to downsample 48kHz -> 16kHz for AECm and upsample 16kHz -> 48kHz.
	 */
	if (cd->rate == 48000) {
		cd->proc_rate = 16000;
		cd->needs_resample = true;
		cd->frame_samples = WEBRTC_AEC_FRAME_SAMPLES_48K;
	} else if (cd->rate == 16000 || cd->rate == 8000) {
		cd->proc_rate = cd->rate;
		cd->needs_resample = false;
		cd->frame_samples = (cd->proc_rate * 10) / 1000;
	} else {
		comp_err(dev, "webrtc_aec: unsupported pipeline rate %d (must be 48000, 16000, or 8000)",
			 cd->rate);
		return -EINVAL;
	}
	webrtc_aec_reset_resamplers(cd);

	if ((mic_fmt != SOF_IPC_FRAME_S16_LE && mic_fmt != SOF_IPC_FRAME_S32_LE) ||
	    (ref_fmt != SOF_IPC_FRAME_S16_LE && ref_fmt != SOF_IPC_FRAME_S32_LE)) {
		comp_err(dev, "webrtc_aec: unsupported format mic=%d ref=%d",
			 mic_fmt, ref_fmt);
		return -EINVAL;
	}

	cd->is_s32 = (mic_fmt == SOF_IPC_FRAME_S32_LE);
	cd->mic_frame_bytes = source_get_frame_bytes(sources[cd->mic_src]);
	cd->ref_frame_bytes = cd->ref_src >= 0 ?
		source_get_frame_bytes(sources[cd->ref_src]) : cd->mic_frame_bytes;
	cd->out_frame_bytes = sink_get_frame_bytes(sinks[0]);

	if (cd->frame_samples > WEBRTC_AEC_FRAME_SAMPLES_MAX) {
		comp_err(dev, "webrtc_aec: frame_samples %d exceeds max %d",
			 cd->frame_samples, WEBRTC_AEC_FRAME_SAMPLES_MAX);
		return -EINVAL;
	}

	comp_info(dev, "webrtc_aec: mic_src=%d ref_src=%d rate=%d/%d ch=%d frame=%d %s resamp=%d",
		  cd->mic_src, cd->ref_src, cd->rate, cd->proc_rate, cd->channels,
		  cd->frame_samples, cd->is_s32 ? "S32" : "S16", cd->needs_resample);

	if (cd->backend->configure) {
		ret = cd->backend->configure(mod, cd->proc_rate,
					     CONFIG_WEBRTC_AEC_FILTER_LEN_MS,
					     CONFIG_WEBRTC_AEC_SUPPRESSION_LEVEL,
					     cd->channels);
		if (ret) {
			comp_err(dev, "webrtc_aec: backend configure failed %d", ret);
			return ret;
		}
	}

	cd->buffered_in_frames = 0;
	for (ret = 0; ret < cd->channels; ret++)
		memset(cd->out_fifo[ret], 0, sizeof(int16_t) * cd->frame_samples);
	cd->buffered_out_frames = cd->frame_samples;
	cd->last_ref_ok = true;
	cd->ref_mismatch_warned = false;
	cd->configured = true;
	return 0;
}

static int webrtc_aec_ref_surplus(struct sof_source *mic, struct sof_source *ref,
				  int *ref_min_out)
{
	int fmic, fref, ref_min;
	size_t ref_fb;

	if (!ref) {
		if (ref_min_out)
			*ref_min_out = 0;
		return 0;
	}

	fmic = mic ? (int)source_get_data_frames_available(mic) : 0;
	fref = (int)source_get_data_frames_available(ref);
	ref_fb = source_get_frame_bytes(ref);
	ref_min = ref_fb ? (int)(source_get_min_available(ref) / ref_fb) : 0;

	if (ref_min_out)
		*ref_min_out = ref_min;

	/*
	 * Preserve reference frames matching queued mic backlog (and at least
	 * one ref period so freshly starting capture remains aligned); only
	 * frames beyond MAX(fmic, ref_min) are true surplus.
	 */
	return fref - MAX(fmic, ref_min);
}

static int webrtc_aec_process(struct processing_module *mod,
			      struct sof_source **sources, int num_of_sources,
			      struct sof_sink **sinks, int num_of_sinks)
{
	struct webrtc_aec_comp_data *cd = module_get_private_data(mod);
	struct sof_source *mic;
	struct sof_source *ref;
	struct sof_sink   *out;
	int mic_idx, ref_idx;
	int fmic, fref, n, drain, c, ref_min, surplus;
	bool ref_s32;

	if (num_of_sources < 1 || num_of_sinks < 1)
		return 0;

	webrtc_aec_resolve_sources(sources, num_of_sources, &mic_idx, &ref_idx);
	cd->mic_src = mic_idx;
	cd->ref_src = ref_idx;

	mic = sources[mic_idx];
	ref = ref_idx >= 0 ? sources[ref_idx] : NULL;
	out = sinks[0];
	fmic = (int)source_get_data_frames_available(mic);
	fref = ref ? (int)source_get_data_frames_available(ref) : 0;

	/* Cap copy size to mic availability, input FIFO capacity, and free sink frames. */
	n = MIN(fmic, WEBRTC_AEC_FIFO_FRAMES - cd->buffered_in_frames);
	n = MIN(n, (int)sink_get_free_frames(out));
	if (n <= 0) {
		/* Drain surplus reference data even when mic/sink stalls so render never blocks. */
		surplus = webrtc_aec_ref_surplus(mic, ref, &ref_min);
		if (surplus > 0 && surplus >= ref_min)
			src_drop_frames(ref, surplus);
		return 0;
	}

	/* Convert mic input to per-channel S16 */
	src_to_s16(mic, n, cd->mic_buf, cd->buffered_in_frames, cd->channels, cd->is_s32);

	/*
	 * Validate dynamically attached reference source format/channels and trim
	 * only genuine surplus beyond queued mic frames so alignment stays intact.
	 */
	if (ref) {
		int ref_ch = source_get_channels(ref);
		int ref_fmt = source_get_frm_fmt(ref);

		if (ref_ch != cd->channels ||
		    (ref_fmt != SOF_IPC_FRAME_S16_LE && ref_fmt != SOF_IPC_FRAME_S32_LE)) {
			if (!cd->ref_mismatch_warned) {
				comp_warn(mod->dev,
					  "webrtc_aec: ref mismatch ch=%d (exp %d) fmt=%d, dropping",
					  ref_ch, cd->channels, ref_fmt);
				cd->ref_mismatch_warned = true;
			}
			src_drop_frames(ref, fref);
			fref = 0;
			ref_s32 = cd->is_s32;
		} else {
			ref_s32 = (ref_fmt == SOF_IPC_FRAME_S32_LE);
		}
	} else {
		ref_s32 = cd->is_s32;
	}

	if (fref > fmic && src_drop_frames(ref, fref - fmic) == 0)
		fref = fmic;
	if (fref >= n) {
		src_to_s16(ref, n, cd->ref_buf, cd->buffered_in_frames, cd->channels, ref_s32);
	} else if (fref > 0) {
		src_to_s16(ref, fref, cd->ref_buf, cd->buffered_in_frames, cd->channels, ref_s32);
		for (c = 0; c < cd->channels; c++)
			memset(&cd->ref_buf[c][cd->buffered_in_frames + fref], 0,
			       (size_t)(n - fref) * sizeof(int16_t));
	} else {
		for (c = 0; c < cd->channels; c++)
			memset(&cd->ref_buf[c][cd->buffered_in_frames], 0,
			       (size_t)n * sizeof(int16_t));
	}

	cd->buffered_in_frames += n;

	/* Process complete 10 ms blocks into out_fifo */
	while (cd->buffered_in_frames >= cd->frame_samples &&
	       cd->buffered_out_frames + cd->frame_samples <= WEBRTC_AEC_FIFO_FRAMES) {
		int fs = cd->frame_samples;
		int ret;

		if (!cd->enabled) {
			/* Bypassed: pass mic straight through */
			for (c = 0; c < cd->channels; c++) {
				memcpy(&cd->out_fifo[c][cd->buffered_out_frames],
				       cd->mic_buf[c],
				       (size_t)fs * sizeof(int16_t));
			}
		} else if (cd->needs_resample) {
			/* 48 kHz -> 16 kHz -> AEC -> 48 kHz */
			if (cd->backend->process_frame) {
				const int16_t *mic_ptrs[WEBRTC_AEC_CHANNELS_MAX];
				const int16_t *ref_ptrs[WEBRTC_AEC_CHANNELS_MAX];
				int16_t *out_ptrs[WEBRTC_AEC_CHANNELS_MAX];

				for (c = 0; c < cd->channels; c++) {
					WebRtcSpl_Resample48khzTo16khz(cd->mic_buf[c], cd->mic16[c],
								      (void *)&cd->mic_resamp[c],
								      cd->resamp_tmpmem);
					WebRtcSpl_Resample48khzTo16khz(cd->ref_buf[c], cd->ref16[c],
								      (void *)&cd->ref_resamp[c],
								      cd->resamp_tmpmem);
					mic_ptrs[c] = cd->mic16[c];
					ref_ptrs[c] = cd->ref16[c];
					out_ptrs[c] = cd->out16[c];
				}

				ret = cd->backend->process_frame(mod, mic_ptrs, ref_ptrs, out_ptrs,
								 WEBRTC_AEC_FRAME_SAMPLES_16K,
								 cd->channels);
				if (ret) {
					for (c = 0; c < cd->channels; c++)
						memcpy(cd->out16[c], cd->mic16[c], sizeof(cd->out16[c]));
				}

				for (c = 0; c < cd->channels; c++) {
					WebRtcSpl_Resample16khzTo48khz(cd->out16[c],
								      &cd->out_fifo[c][cd->buffered_out_frames],
								      (void *)&cd->out_resamp[c],
								      cd->resamp_tmpmem);
				}
			} else {
				for (c = 0; c < cd->channels; c++) {
					if (c > 0 && cd->channels > 1) {
						/* Replicate processed ch0 to secondary mic channels */
						memcpy(&cd->out_fifo[c][cd->buffered_out_frames],
						       &cd->out_fifo[0][cd->buffered_out_frames],
						       (size_t)fs * sizeof(int16_t));
						continue;
					}

					WebRtcSpl_Resample48khzTo16khz(cd->mic_buf[c], cd->mic16[c],
								      (void *)&cd->mic_resamp[c],
								      cd->resamp_tmpmem);
					WebRtcSpl_Resample48khzTo16khz(cd->ref_buf[c], cd->ref16[c],
								      (void *)&cd->ref_resamp[c],
								      cd->resamp_tmpmem);

					ret = cd->backend->process_ch(mod,
								      cd->mic16[c],
								      cd->ref16[c],
								      cd->out16[c],
								      WEBRTC_AEC_FRAME_SAMPLES_16K,
								      c);
					if (ret)
						memcpy(cd->out16[c], cd->mic16[c], sizeof(cd->out16[c]));

					WebRtcSpl_Resample16khzTo48khz(cd->out16[c],
								      &cd->out_fifo[c][cd->buffered_out_frames],
								      (void *)&cd->out_resamp[c],
								      cd->resamp_tmpmem);
				}
			}
		} else {
			/* Native 16 kHz or 8 kHz */
			if (cd->backend->process_frame) {
				const int16_t *mic_ptrs[WEBRTC_AEC_CHANNELS_MAX];
				const int16_t *ref_ptrs[WEBRTC_AEC_CHANNELS_MAX];
				int16_t *out_ptrs[WEBRTC_AEC_CHANNELS_MAX];

				for (c = 0; c < cd->channels; c++) {
					mic_ptrs[c] = cd->mic_buf[c];
					ref_ptrs[c] = cd->ref_buf[c];
					out_ptrs[c] = &cd->out_fifo[c][cd->buffered_out_frames];
				}

				ret = cd->backend->process_frame(mod, mic_ptrs, ref_ptrs, out_ptrs,
								 fs, cd->channels);
				if (ret) {
					for (c = 0; c < cd->channels; c++) {
						memcpy(&cd->out_fifo[c][cd->buffered_out_frames],
						       cd->mic_buf[c],
						       (size_t)fs * sizeof(int16_t));
					}
				}
			} else {
				for (c = 0; c < cd->channels; c++) {
					if (c > 0 && cd->channels > 1) {
						memcpy(&cd->out_fifo[c][cd->buffered_out_frames],
						       &cd->out_fifo[0][cd->buffered_out_frames],
						       (size_t)fs * sizeof(int16_t));
						continue;
					}

					ret = cd->backend->process_ch(mod,
								      cd->mic_buf[c],
								      cd->ref_buf[c],
								      &cd->out_fifo[c][cd->buffered_out_frames],
								      fs, c);
					if (ret) {
						memcpy(&cd->out_fifo[c][cd->buffered_out_frames],
						       cd->mic_buf[c],
						       (size_t)fs * sizeof(int16_t));
					}
				}
			}
		}

		cd->buffered_out_frames += fs;
		cd->buffered_in_frames -= fs;

		if (cd->buffered_in_frames > 0) {
			for (c = 0; c < cd->channels; c++) {
				memmove(cd->mic_buf[c], cd->mic_buf[c] + fs,
					(size_t)cd->buffered_in_frames * sizeof(int16_t));
				memmove(cd->ref_buf[c], cd->ref_buf[c] + fs,
					(size_t)cd->buffered_in_frames * sizeof(int16_t));
			}
		}
	}

	/* Drain exactly n frames from out_fifo to sink to maintain symmetric period I/O */
	drain = MIN(n, cd->buffered_out_frames);
	if (drain > 0) {
		s16_to_sink(out, cd->out_fifo, drain, cd->channels, cd->is_s32);
		cd->buffered_out_frames -= drain;
		if (cd->buffered_out_frames > 0) {
			for (c = 0; c < cd->channels; c++)
				memmove(cd->out_fifo[c], cd->out_fifo[c] + drain,
					(size_t)cd->buffered_out_frames * sizeof(int16_t));
		}
	}

	cd->last_ref_ok = (fref > 0);
	return 0;
}

static int webrtc_aec_reset(struct processing_module *mod)
{
	struct webrtc_aec_comp_data *cd = module_get_private_data(mod);
	int c;

	webrtc_aec_set_active_mod(mod);
	comp_dbg(mod->dev, "webrtc_aec: reset");
	cd->buffered_in_frames = 0;
	cd->ref_mismatch_warned = false;
	for (c = 0; c < cd->channels; c++)
		memset(cd->out_fifo[c], 0, sizeof(int16_t) * cd->frame_samples);
	cd->buffered_out_frames = cd->frame_samples;
	webrtc_aec_reset_resamplers(cd);

	if (cd->backend->reset)
		return cd->backend->reset(mod);

	return 0;
}

__cold static int webrtc_aec_free(struct processing_module *mod)
{
	struct webrtc_aec_comp_data *cd = module_get_private_data(mod);

	webrtc_aec_set_active_mod(mod);
	assert_can_be_cold();
	comp_dbg(mod->dev, "webrtc_aec: free");

	if (cd && cd->backend && cd->backend->free)
		cd->backend->free(mod);

	if (webrtc_aec_get_active_mod() == mod)
		webrtc_aec_set_active_mod(NULL);

	mod_free(mod, cd);
	return 0;
}

static int webrtc_aec_apply_ctl(struct processing_module *mod,
				struct webrtc_aec_comp_data *cd,
				uint32_t id, uint32_t value)
{
	struct comp_dev *dev = mod->dev;

	switch (id) {
	case WEBRTC_AEC_CTL_ID_ENABLE:
		cd->enabled = (value != 0);
		comp_info(dev, "webrtc_aec: switch enable = %d", cd->enabled);
		return 0;
	case WEBRTC_AEC_CTL_ID_HIGH_SUPPRESSION:
		cd->high_suppression = (value != 0);
		comp_info(dev, "webrtc_aec: high suppression = %d", cd->high_suppression);
		if (cd->backend && cd->backend->set_suppression)
			return cd->backend->set_suppression(mod, cd->high_suppression);
		return 0;
	default:
		comp_err(dev, "webrtc_aec: unknown control id %u", id);
		return -EINVAL;
	}
}

static int webrtc_aec_set_config(struct processing_module *mod, uint32_t param_id,
				 enum module_cfg_fragment_position pos, uint32_t data_offset_size,
				 const uint8_t *fragment, size_t fragment_size, uint8_t *response,
				 size_t response_size)
{
	struct webrtc_aec_comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;

	webrtc_aec_set_active_mod(mod);
	if (param_id == SOF_IPC4_SWITCH_CONTROL_PARAM_ID) {
		const struct sof_ipc4_control_msg_payload *ctl =
			(const struct sof_ipc4_control_msg_payload *)fragment;

		if (pos != MODULE_CFG_FRAGMENT_SINGLE ||
		    fragment_size < sizeof(struct sof_ipc4_control_msg_payload) +
		    sizeof(struct sof_ipc4_ctrl_value_chan)) {
			comp_err(dev, "webrtc_aec: invalid payload size %zu", fragment_size);
			return -EINVAL;
		}

		if (ctl->num_elems != WEBRTC_AEC_CTL_NUM_ELEMS) {
			comp_err(dev, "webrtc_aec: invalid num_elems %d", ctl->num_elems);
			return -EINVAL;
		}

		return webrtc_aec_apply_ctl(mod, cd, ctl->id, ctl->chanv[0].value);
	}

	comp_err(dev, "webrtc_aec: unsupported param_id 0x%x", param_id);
	return -EINVAL;
}

static int webrtc_aec_get_config(struct processing_module *mod, uint32_t config_id,
				 uint32_t *data_offset_size, uint8_t *fragment,
				 size_t fragment_size)
{
	struct webrtc_aec_comp_data *cd = module_get_private_data(mod);

	if (config_id == SOF_IPC4_SWITCH_CONTROL_PARAM_ID) {
		struct sof_ipc4_control_msg_payload *ctl =
			(struct sof_ipc4_control_msg_payload *)fragment;

		if (fragment_size < sizeof(struct sof_ipc4_control_msg_payload) +
		    sizeof(struct sof_ipc4_ctrl_value_chan))
			return -EINVAL;

		switch (ctl->id) {
		case WEBRTC_AEC_CTL_ID_ENABLE:
			ctl->chanv[0].value = cd->enabled ? 1 : 0;
			break;
		case WEBRTC_AEC_CTL_ID_HIGH_SUPPRESSION:
			ctl->chanv[0].value = cd->high_suppression ? 1 : 0;
			break;
		default:
			return -EINVAL;
		}

		ctl->num_elems = WEBRTC_AEC_CTL_NUM_ELEMS;
		ctl->chanv[0].channel = 0;
		*data_offset_size = sizeof(struct sof_ipc4_control_msg_payload) +
				    sizeof(struct sof_ipc4_ctrl_value_chan);
		return 0;
	}

	return -EINVAL;
}

static bool webrtc_aec_is_ready_to_process(struct processing_module *mod,
					   struct sof_source **sources,
					   int num_of_sources,
					   struct sof_sink **sinks,
					   int num_of_sinks)
{
	struct webrtc_aec_comp_data *cd = module_get_private_data(mod);
	struct sof_source *mic, *ref;
	struct sof_sink *out;
	int mic_idx, ref_idx;

	if (!cd || !cd->configured || num_of_sources < 1 || num_of_sinks < 1)
		return false;

	webrtc_aec_resolve_sources(sources, num_of_sources, &mic_idx, &ref_idx);
	mic = sources[mic_idx];
	ref = ref_idx >= 0 ? sources[ref_idx] : NULL;
	out = sinks[0];

	if (source_get_data_available(mic) >= source_get_min_available(mic) &&
	    sink_get_free_size(out) >= sink_get_min_free_space(out))
		return true;

	{
		int ref_min;
		int surplus = webrtc_aec_ref_surplus(mic, ref, &ref_min);

		if (ref_min > 0 && surplus >= ref_min)
			return true;
	}

	return false;
}

static const struct module_interface webrtc_aec_interface = {
	.init                = webrtc_aec_init,
	.prepare             = webrtc_aec_prepare,
	.is_ready_to_process = webrtc_aec_is_ready_to_process,
	.process             = webrtc_aec_process,
	.set_configuration   = webrtc_aec_set_config,
	.get_configuration   = webrtc_aec_get_config,
	.reset               = webrtc_aec_reset,
	.free                = webrtc_aec_free,
};

#if CONFIG_COMP_WEBRTC_AEC_MODULE

#include <module/module/api_ver.h>
#include <module/module/llext.h>
#include <rimage/sof/user/manifest.h>

static const struct sof_man_module_manifest mod_manifest __section(".module") __used = {
	.module = {
		.name = "WRTCAEC",
		.uuid = SOF_REG_UUID(webrtc_aec),
		.entry_point = (uint32_t)(&webrtc_aec_interface),
		.instance_max_count = 40,
		.type = {
			.load_type = SOF_MAN_MOD_TYPE_LLEXT,
			.domain_dp = 1,
		},
		.affinity_mask = 2,
	}
};

SOF_LLEXT_BUILDINFO;

#else

DECLARE_TR_CTX(webrtc_aec_tr, SOF_UUID(webrtc_aec_uuid), LOG_LEVEL_INFO);
DECLARE_MODULE_ADAPTER(webrtc_aec_interface, webrtc_aec_uuid, webrtc_aec_tr);
SOF_MODULE_INIT(webrtc_aec, sys_comp_module_webrtc_aec_interface_init);

#endif
