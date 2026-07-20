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
#include <sof/math/numbers.h>
#include <rtos/init.h>
#include <rtos/string.h>
#include <limits.h>
#include <errno.h>
#include <stdint.h>
#include "webrtc_aec.h"

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
		       int16_t dst[][WEBRTC_AEC_FRAME_SAMPLES_MAX],
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
static void s16_to_sink(struct sof_sink *dst, int16_t src[][WEBRTC_AEC_FRAME_SAMPLES_MAX],
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

	if (num_of_sources != 2 || num_of_sinks != 1) {
		comp_err(dev, "webrtc_aec: need 2 sources and 1 sink (got %d/%d)",
			 num_of_sources, num_of_sinks);
		return -EINVAL;
	}

	/*
	 * Resolve which source is mic and which is echo reference.
	 * The mic is the one on the same pipeline as the output sink.
	 * This matches google_rtc_audio_processing.c exactly.
	 */
	cd->ref_src = (source_get_pipeline_id(sources[0]) == sink_get_pipeline_id(sinks[0]));
	cd->mic_src = cd->ref_src ? 0 : 1;

	mic_fmt  = source_get_frm_fmt(sources[cd->mic_src]);
	ref_fmt  = source_get_frm_fmt(sources[cd->ref_src]);
	mic_rate = source_get_rate(sources[cd->mic_src]);
	ref_rate = source_get_rate(sources[cd->ref_src]);
	cd->channels = source_get_channels(sources[cd->mic_src]);

	if (cd->channels > WEBRTC_AEC_CHANNELS_MAX) {
		comp_err(dev, "webrtc_aec: too many channels %d (max %d)",
			 cd->channels, WEBRTC_AEC_CHANNELS_MAX);
		return -EINVAL;
	}

	if (mic_rate != ref_rate) {
		comp_err(dev, "webrtc_aec: mic_rate %d != ref_rate %d", mic_rate, ref_rate);
		return -EINVAL;
	}
	cd->rate = mic_rate;

	/* AECm supports only 8 and 16 kHz natively. */
	cd->proc_rate = CONFIG_WEBRTC_AEC_SAMPLE_RATE_HZ;
	if (cd->proc_rate != 8000 && cd->proc_rate != 16000) {
		comp_err(dev, "webrtc_aec: invalid proc_rate %d (must be 8000 or 16000)",
			 cd->proc_rate);
		return -EINVAL;
	}

	if (cd->rate != cd->proc_rate) {
		comp_warn(dev, "webrtc_aec: pipeline rate %d != AECm rate %d; "
			  "pipeline resampling required upstream", cd->rate, cd->proc_rate);
	}

	if ((mic_fmt != SOF_IPC_FRAME_S16_LE && mic_fmt != SOF_IPC_FRAME_S32_LE) ||
	    (ref_fmt != SOF_IPC_FRAME_S16_LE && ref_fmt != SOF_IPC_FRAME_S32_LE)) {
		comp_err(dev, "webrtc_aec: unsupported format mic=%d ref=%d",
			 mic_fmt, ref_fmt);
		return -EINVAL;
	}

	cd->is_s32 = (mic_fmt == SOF_IPC_FRAME_S32_LE);
	cd->mic_frame_bytes = source_get_frame_bytes(sources[cd->mic_src]);
	cd->ref_frame_bytes = source_get_frame_bytes(sources[cd->ref_src]);
	cd->out_frame_bytes = sink_get_frame_bytes(sinks[0]);
	cd->frame_samples   = (cd->proc_rate * 10) / 1000; /* 10 ms */

	if (cd->frame_samples > WEBRTC_AEC_FRAME_SAMPLES_MAX) {
		comp_err(dev, "webrtc_aec: frame_samples %d exceeds max %d",
			 cd->frame_samples, WEBRTC_AEC_FRAME_SAMPLES_MAX);
		return -EINVAL;
	}

	comp_info(dev, "webrtc_aec: mic_src=%d ref_src=%d rate=%d/%d ch=%d frame=%d %s",
		  cd->mic_src, cd->ref_src, cd->rate, cd->proc_rate, cd->channels,
		  cd->frame_samples, cd->is_s32 ? "S32" : "S16");

#ifdef CONFIG_IPC_MAJOR_4
	/* Apply reference format override from topology pin descriptor. */
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

	cd->buffered_frames = 0;
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
	struct sof_source *ref = sources[cd->ref_src];
	struct sof_sink   *out = sinks[0];
	int fmic = (int)source_get_data_frames_available(mic);
	int fref = (int)source_get_data_frames_available(ref);
	int frames = MIN(fmic, fref);
	int n, frames_rem;

	for (frames_rem = frames; frames_rem > 0; frames_rem -= n) {
		/* Consume at most what fills one complete AECm frame. */
		n = MIN(frames_rem, cd->frame_samples - cd->buffered_frames);

		/* Convert source data → per-channel S16 accumulators. */
		src_to_s16(mic, n, cd->mic_buf, cd->buffered_frames, cd->channels, cd->is_s32);
		src_to_s16(ref, n, cd->ref_buf, cd->buffered_frames, cd->channels, cd->is_s32);

		cd->buffered_frames += n;

		/* Once we have a full 10 ms block, run AECm per channel. */
		if (cd->buffered_frames >= cd->frame_samples) {
			int fs = cd->frame_samples;

			/* Check output headroom. */
			if (sink_get_free_size(out) < (size_t)(fs * cd->out_frame_bytes)) {
				comp_warn(mod->dev, "webrtc_aec: sink backed up!");
				break;
			}

			int c, ret;

			for (c = 0; c < cd->channels; c++) {
				ret = cd->backend->process_ch(mod,
							      cd->mic_buf[c],
							      cd->ref_buf[c],
							      cd->out_buf[c],
							      fs, c);
				if (ret) {
					/* Fall back to mic pass-through for this channel. */
					memcpy(cd->out_buf[c], cd->mic_buf[c],
					       (size_t)fs * sizeof(int16_t));
				}
			}

			/* Write denoised output to sink. */
			s16_to_sink(out, cd->out_buf, fs, cd->channels, cd->is_s32);
			cd->buffered_frames = 0;
		}
	}

	cd->last_ref_ok = true;
	return 0;
}

static int webrtc_aec_reset(struct processing_module *mod)
{
	struct webrtc_aec_comp_data *cd = module_get_private_data(mod);

	comp_dbg(mod->dev, "webrtc_aec: reset");
	cd->buffered_frames = 0;

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

static const struct module_interface webrtc_aec_interface = {
	.init    = webrtc_aec_init,
	.prepare = webrtc_aec_prepare,
	.process = webrtc_aec_process,
	.reset   = webrtc_aec_reset,
	.free    = webrtc_aec_free,
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
