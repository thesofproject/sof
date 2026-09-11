// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.
//
// WebRTC AECm module — SOF module_interface core.
//
// This is the SOF glue layer for the AECm fixed-point echo canceller.
// The dual-source topology and source-routing heuristic are copied
// directly from google_rtc_audio_processing.c which is the canonical
// reference for this pattern in SOF.
//
// Key differences from google_rtc_audio_processing:
//  - Uses fixed-point S16 throughout (no float intermediate buffers)
//  - AECm runs per-channel independently, not as a multi-channel block
//  - Operates at 8 or 16 kHz maximum (pipeline downsampling is out-of-scope
//    for this first revision — see CONFIG_WEBRTC_AEC_SAMPLE_RATE_HZ)

#include <sof/audio/module_adapter/module/generic.h>
#include <module/audio/source_api.h>
#include <module/audio/sink_api.h>
#include <ipc4/aec.h>
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
	size_t nbytes = (size_t)n * ch * sample_sz;
	const char *buf, *bufstart;
	size_t bufsz;
	int i, c, err;

	err = source_get_data(src, nbytes, (void *)&buf, (void *)&bufstart, &bufsz);
	if (err)
		return; /* shouldn't happen if caller checked availability */

	for (i = 0; i < n; i++) {
		for (c = 0; c < ch; c++) {
			if (is_s32) {
				int32_t s = *(const int32_t *)buf;

				dst[c][frame0 + i] = (int16_t)(s >> 16);
				buf += sizeof(int32_t);
			} else {
				dst[c][frame0 + i] = *(const int16_t *)buf;
				buf += sizeof(int16_t);
			}
		}
		/* Wrap circular buffer. */
		if (buf >= bufstart + bufsz)
			buf = bufstart;
	}

	source_release_data(src, nbytes);
}

/**
 * s16_to_sink() - Write per-channel S16 output to a SOF sink.
 * Upshifts to S32 if the pipeline format requires it.
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
	size_t nbytes = (size_t)n * ch * sample_sz;
	char *buf, *bufstart;
	size_t bufsz;
	int i, c, err;

	err = sink_get_buffer(dst, nbytes, (void *)&buf, (void *)&bufstart, &bufsz);
	if (err)
		return;

	for (i = 0; i < n; i++) {
		for (c = 0; c < ch; c++) {
			if (is_s32) {
				*(int32_t *)buf = (int32_t)src[c][i] << 16;
				buf += sizeof(int32_t);
			} else {
				*(int16_t *)buf = src[c][i];
				buf += sizeof(int16_t);
			}
		}
		if (buf >= bufstart + bufsz)
			buf = bufstart;
	}

	sink_commit_buffer(dst, nbytes);
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

__cold static int webrtc_aec_prepare(struct processing_module *mod,
				     struct sof_source **sources, int num_of_sources,
				     struct sof_sink **sinks, int num_of_sinks)
{
	struct webrtc_aec_comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	int mic_fmt, ref_fmt, mic_rate, ref_rate, ret;

	assert_can_be_cold();

	if ((num_of_sources != 1 && num_of_sources != 2) || num_of_sinks != 1) {
		comp_err(dev, "webrtc_aec: need 1 or 2 sources and 1 sink (got %d/%d)",
			 num_of_sources, num_of_sinks);
		return -EINVAL;
	}

	if (num_of_sources == 1) {
		cd->mic_src = 0;
		cd->ref_src = -1;
	} else if (source_get_pipeline_id(sources[0]) == sink_get_pipeline_id(sinks[0]) &&
	    source_get_pipeline_id(sources[1]) != sink_get_pipeline_id(sinks[0])) {
		cd->mic_src = 0;
		cd->ref_src = 1;
	} else if (source_get_pipeline_id(sources[1]) == sink_get_pipeline_id(sinks[0]) &&
		   source_get_pipeline_id(sources[0]) != sink_get_pipeline_id(sinks[0])) {
		cd->mic_src = 1;
		cd->ref_src = 0;
	} else {
		cd->mic_src = 0;
		cd->ref_src = 1;
	}

	mic_fmt  = source_get_frm_fmt(sources[cd->mic_src]);
	mic_rate = source_get_rate(sources[cd->mic_src]);
	cd->channels = source_get_channels(sources[cd->mic_src]);

	if (cd->ref_src >= 0) {
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
		cd->frame_samples = WEBRTC_AEC_FRAME_SAMPLES_48K; /* 480 samples per 10 ms */
		for (ret = 0; ret < cd->channels; ret++) {
			WebRtcSpl_ResetResample48khzTo16khz((void *)&cd->mic_resamp[ret]);
			WebRtcSpl_ResetResample48khzTo16khz((void *)&cd->ref_resamp[ret]);
			WebRtcSpl_ResetResample16khzTo48khz((void *)&cd->out_resamp[ret]);
		}
	} else if (cd->rate == 16000 || cd->rate == 8000) {
		cd->proc_rate = cd->rate;
		cd->needs_resample = false;
		cd->frame_samples = (cd->proc_rate * 10) / 1000;
	} else {
		comp_err(dev, "webrtc_aec: unsupported pipeline rate %d (must be 48000, 16000, or 8000)",
			 cd->rate);
		return -EINVAL;
	}

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

#ifdef CONFIG_IPC_MAJOR_4
	/* Apply reference format override from topology pin descriptor. */
	if (cd->ref_src >= 0)
		ipc4_update_source_format(sources[cd->ref_src],
					  &mod->priv.cfg.input_pins[1].audio_fmt);
#endif

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
	cd->configured = true;
	return 0;
}

static int webrtc_aec_process(struct processing_module *mod,
			      struct sof_source **sources, int num_of_sources,
			      struct sof_sink **sinks, int num_of_sinks)
{
	struct webrtc_aec_comp_data *cd = module_get_private_data(mod);
	struct sof_source *mic = sources[cd->mic_src];
	struct sof_source *ref = cd->ref_src >= 0 ? sources[cd->ref_src] : NULL;
	struct sof_sink   *out = sinks[0];
	int fmic = (int)source_get_data_frames_available(mic);
	int fref = ref ? (int)source_get_data_frames_available(ref) : 0;
	int n, drain, c;

	if (fmic <= 0)
		return 0;

	/* Cap to available space in input accumulation buffers */
	n = MIN(fmic, WEBRTC_AEC_FIFO_FRAMES - cd->buffered_in_frames);
	if (n <= 0)
		return 0;

	/* Convert mic input to per-channel S16 */
	src_to_s16(mic, n, cd->mic_buf, cd->buffered_in_frames, cd->channels, cd->is_s32);

	/* Convert ref input to per-channel S16; if starved/silent, fill with silence */
	if (fref >= n) {
		src_to_s16(ref, n, cd->ref_buf, cd->buffered_in_frames, cd->channels, cd->is_s32);
	} else if (fref > 0) {
		src_to_s16(ref, fref, cd->ref_buf, cd->buffered_in_frames, cd->channels, cd->is_s32);
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
				int16_t mic16[WEBRTC_AEC_CHANNELS_MAX][WEBRTC_AEC_FRAME_SAMPLES_16K];
				int16_t ref16[WEBRTC_AEC_CHANNELS_MAX][WEBRTC_AEC_FRAME_SAMPLES_16K];
				int16_t out16[WEBRTC_AEC_CHANNELS_MAX][WEBRTC_AEC_FRAME_SAMPLES_16K];
				const int16_t *mic_ptrs[WEBRTC_AEC_CHANNELS_MAX];
				const int16_t *ref_ptrs[WEBRTC_AEC_CHANNELS_MAX];
				int16_t *out_ptrs[WEBRTC_AEC_CHANNELS_MAX];

				for (c = 0; c < cd->channels; c++) {
					WebRtcSpl_Resample48khzTo16khz(cd->mic_buf[c], mic16[c],
								      (void *)&cd->mic_resamp[c],
								      cd->resamp_tmpmem);
					WebRtcSpl_Resample48khzTo16khz(cd->ref_buf[c], ref16[c],
								      (void *)&cd->ref_resamp[c],
								      cd->resamp_tmpmem);
					mic_ptrs[c] = mic16[c];
					ref_ptrs[c] = ref16[c];
					out_ptrs[c] = out16[c];
				}

				ret = cd->backend->process_frame(mod, mic_ptrs, ref_ptrs, out_ptrs,
								 WEBRTC_AEC_FRAME_SAMPLES_16K,
								 cd->channels);
				if (ret) {
					for (c = 0; c < cd->channels; c++)
						memcpy(out16[c], mic16[c], sizeof(out16[c]));
				}

				for (c = 0; c < cd->channels; c++) {
					WebRtcSpl_Resample16khzTo48khz(out16[c],
								      &cd->out_fifo[c][cd->buffered_out_frames],
								      (void *)&cd->out_resamp[c],
								      cd->resamp_tmpmem);
				}
			} else {
				for (c = 0; c < cd->channels; c++) {
					int16_t mic16[WEBRTC_AEC_FRAME_SAMPLES_16K];
					int16_t ref16[WEBRTC_AEC_FRAME_SAMPLES_16K];
					int16_t out16[WEBRTC_AEC_FRAME_SAMPLES_16K];

					if (c > 0 && cd->channels > 1) {
						/* Replicate processed ch0 to secondary mic channels */
						memcpy(&cd->out_fifo[c][cd->buffered_out_frames],
						       &cd->out_fifo[0][cd->buffered_out_frames],
						       (size_t)fs * sizeof(int16_t));
						continue;
					}

					WebRtcSpl_Resample48khzTo16khz(cd->mic_buf[c], mic16,
								      (void *)&cd->mic_resamp[c],
								      cd->resamp_tmpmem);
					WebRtcSpl_Resample48khzTo16khz(cd->ref_buf[c], ref16,
								      (void *)&cd->ref_resamp[c],
								      cd->resamp_tmpmem);

					ret = cd->backend->process_ch(mod,
								      mic16,
								      ref16,
								      out16,
								      WEBRTC_AEC_FRAME_SAMPLES_16K,
								      c);
					if (ret)
						memcpy(out16, mic16, sizeof(out16));

					WebRtcSpl_Resample16khzTo48khz(out16,
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

	comp_dbg(mod->dev, "webrtc_aec: reset");
	cd->buffered_in_frames = 0;
	for (c = 0; c < cd->channels; c++)
		memset(cd->out_fifo[c], 0, sizeof(int16_t) * cd->frame_samples);
	cd->buffered_out_frames = cd->frame_samples;

	if (cd->backend->reset)
		return cd->backend->reset(mod);

	return 0;
}

__cold static int webrtc_aec_free(struct processing_module *mod)
{
	struct webrtc_aec_comp_data *cd = module_get_private_data(mod);

	assert_can_be_cold();
	comp_dbg(mod->dev, "webrtc_aec: free");

	if (cd->backend->free)
		cd->backend->free(mod);

	mod_free(mod, cd);
	return 0;
}

static int webrtc_aec_set_config(struct processing_module *mod, uint32_t param_id,
				 enum module_cfg_fragment_position pos, uint32_t data_offset_size,
				 const uint8_t *fragment, size_t fragment_size, uint8_t *response,
				 size_t response_size)
{
	struct webrtc_aec_comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;

	if (param_id == SOF_IPC4_SWITCH_CONTROL_PARAM_ID) {
		const struct sof_ipc4_control_msg_payload *ctl =
			(const struct sof_ipc4_control_msg_payload *)fragment;

		if (ctl->num_elems != 1) {
			comp_err(dev, "webrtc_aec: invalid num_elems %d", ctl->num_elems);
			return -EINVAL;
		}

		if (ctl->id == 0) {
			cd->enabled = (ctl->chanv[0].value != 0);
			comp_info(dev, "webrtc_aec: switch enable = %d", cd->enabled);
			return 0;
		}
		if (ctl->id == 1) {
			cd->high_suppression = (ctl->chanv[0].value != 0);
			comp_info(dev, "webrtc_aec: high suppression = %d", cd->high_suppression);
			if (cd->backend && cd->backend->set_suppression)
				return cd->backend->set_suppression(mod, cd->high_suppression);
			return 0;
		}

		comp_err(dev, "webrtc_aec: unknown control id %d", ctl->id);
		return -EINVAL;
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
		ctl->num_elems = 1;
		ctl->chanv[0].channel = 0;
		if (ctl->id == 0) {
			ctl->chanv[0].value = cd->enabled ? 1 : 0;
		} else if (ctl->id == 1) {
			ctl->chanv[0].value = cd->high_suppression ? 1 : 0;
		} else {
			return -EINVAL;
		}
		*data_offset_size = sizeof(struct sof_ipc4_control_msg_payload) +
				    sizeof(struct sof_ipc4_ctrl_value_chan);
		return 0;
	}

	return -EINVAL;
}

static const struct module_interface webrtc_aec_interface = {
	.init              = webrtc_aec_init,
	.prepare           = webrtc_aec_prepare,
	.process           = webrtc_aec_process,
	.set_configuration = webrtc_aec_set_config,
	.get_configuration = webrtc_aec_get_config,
	.reset             = webrtc_aec_reset,
	.free              = webrtc_aec_free,
};

#if CONFIG_COMP_WEBRTC_AEC_MODULE

#include <module/module/api_ver.h>
#include <module/module/llext.h>
#include <rimage/sof/user/manifest.h>

static const struct sof_man_module_manifest mod_manifest __section(".module") __used =
	SOF_LLEXT_MODULE_MANIFEST("WRTCAEC", &webrtc_aec_interface, 2,
				  SOF_REG_UUID(webrtc_aec), 40);

SOF_LLEXT_BUILDINFO;

#else

DECLARE_TR_CTX(webrtc_aec_tr, SOF_UUID(webrtc_aec_uuid), LOG_LEVEL_INFO);
DECLARE_MODULE_ADAPTER(webrtc_aec_interface, webrtc_aec_uuid, webrtc_aec_tr);
SOF_MODULE_INIT(webrtc_aec, sys_comp_module_webrtc_aec_interface_init);

#endif
