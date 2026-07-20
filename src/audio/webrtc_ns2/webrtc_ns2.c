// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.
//
// webrtc_ns2 — SOF module_interface glue for the RNNoise deep-learning
// noise suppressor.
//
// Pass-through PCM effect: audio flows source → sink after per-channel
// RNNoise denoising. Each 480-sample (10 ms at 48 kHz) frame is
// processed independently per channel. Partial pipeline periods are
// accumulated until a full frame is ready.
//
// As a bonus, the per-frame VAD probability returned by RNNoise is
// compared against a configurable threshold and a NOTIFIER_ID_VAD event
// is fired, making this a drop-in combined denoiser + voice detector.

#include <sof/audio/module_adapter/module/generic.h>
#include <module/audio/source_api.h>
#include <module/audio/sink_api.h>
#include <sof/lib/notifier.h>
#include <sof/math/numbers.h>
#include <rtos/init.h>
#include <rtos/string.h>
#include <errno.h>
#include <stdint.h>
#include "webrtc_ns2.h"

SOF_DEFINE_REG_UUID(webrtc_ns2);
LOG_MODULE_REGISTER(webrtc_ns2, CONFIG_SOF_LOG_LEVEL);

/* Normalisation constants for S16 ↔ float and S32 ↔ float. */
#define NS2_S16_SCALE	32768.0f
#define NS2_S32_SCALE	2147483648.0f

/* ----------------------------------------------------------------
 * Format conversion helpers  (interleaved PCM ↔ per-channel float)
 * ---------------------------------------------------------------- */

static inline void src_to_float(struct webrtc_ns2_comp_data *cd,
				const void *raw, int n_frames, int frame0)
{
	int i, c, ch = cd->channels;

	if (cd->is_s32) {
		const int32_t *p = raw;

		for (i = 0; i < n_frames; i++)
			for (c = 0; c < ch; c++)
				cd->in_buf[c][frame0 + i] =
					(float)p[i * ch + c] / NS2_S32_SCALE;
	} else {
		const int16_t *p = raw;

		for (i = 0; i < n_frames; i++)
			for (c = 0; c < ch; c++)
				cd->in_buf[c][frame0 + i] =
					(float)p[i * ch + c] / NS2_S16_SCALE;
	}
}

static inline void float_to_sink(struct webrtc_ns2_comp_data *cd,
				 void *raw, int n_frames)
{
	int i, c, ch = cd->channels;

	if (cd->is_s32) {
		int32_t *p = raw;

		for (i = 0; i < n_frames; i++) {
			for (c = 0; c < ch; c++) {
				float f = cd->out_buf[c][i] * NS2_S32_SCALE;

				f = f >  2147483647.0f ?  2147483647.0f :
				    f < -2147483648.0f ? -2147483648.0f : f;
				p[i * ch + c] = (int32_t)f;
			}
		}
	} else {
		int16_t *p = raw;

		for (i = 0; i < n_frames; i++) {
			for (c = 0; c < ch; c++) {
				float f = cd->out_buf[c][i] * NS2_S16_SCALE;

				f = f >  32767.0f ?  32767.0f :
				    f < -32768.0f ? -32768.0f : f;
				p[i * ch + c] = (int16_t)f;
			}
		}
	}
}

/* ----------------------------------------------------------------
 * module_interface operations
 * ---------------------------------------------------------------- */

__cold static int webrtc_ns2_init(struct processing_module *mod)
{
	struct module_data *md = &mod->priv;
	struct comp_dev *dev = mod->dev;
	struct webrtc_ns2_comp_data *cd;
	int ret;

	assert_can_be_cold();
	comp_info(dev, "webrtc_ns2: init");

	cd = mod_zalloc(mod, sizeof(*cd));
	if (!cd)
		return -ENOMEM;

	md->private = cd;
	cd->backend = &webrtc_ns2_backend;
	cd->vad_threshold = (float)CONFIG_WEBRTC_NS2_VAD_THRESHOLD_PCT / 100.0f;

	comp_info(dev, "webrtc_ns2: backend '%s' vad_threshold=%.2f",
		  cd->backend->name, (double)cd->vad_threshold);

	if (cd->backend->init) {
		ret = cd->backend->init(mod);
		if (ret) {
			comp_err(dev, "webrtc_ns2: backend init failed %d", ret);
			mod_free(mod, cd);
			return ret;
		}
	}

	return 0;
}

__cold static int webrtc_ns2_prepare(struct processing_module *mod,
				     struct sof_source **sources, int num_of_sources,
				     struct sof_sink **sinks, int num_of_sinks)
{
	struct webrtc_ns2_comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	int rate, fmt, ret;

	assert_can_be_cold();

	if (num_of_sources != 1 || num_of_sinks != 1) {
		comp_err(dev, "webrtc_ns2: need exactly 1 source and 1 sink");
		return -EINVAL;
	}

	rate = source_get_rate(sources[0]);
	fmt  = source_get_frm_fmt(sources[0]);
	cd->channels = source_get_channels(sources[0]);

	/* RNNoise is hard-wired to 48 kHz. */
	if (rate != WEBRTC_NS2_SAMPLE_RATE) {
		comp_err(dev, "webrtc_ns2: rate %d != required %d Hz; "
			 "add an ASRC upstream", rate, WEBRTC_NS2_SAMPLE_RATE);
		return -EINVAL;
	}

	if (cd->channels > WEBRTC_NS2_CHANNELS_MAX) {
		comp_err(dev, "webrtc_ns2: %d channels exceeds max %d",
			 cd->channels, WEBRTC_NS2_CHANNELS_MAX);
		return -EINVAL;
	}

	switch (fmt) {
	case SOF_IPC_FRAME_S16_LE:
		cd->is_s32 = false;
		break;
	case SOF_IPC_FRAME_S32_LE:
		cd->is_s32 = true;
		break;
	default:
		comp_err(dev, "webrtc_ns2: unsupported format %d", fmt);
		return -EINVAL;
	}

	comp_info(dev, "webrtc_ns2: rate=%d ch=%d %s frame=%d",
		  rate, cd->channels, cd->is_s32 ? "S32" : "S16",
		  WEBRTC_NS2_FRAME_SAMPLES);

	if (cd->backend->configure) {
		ret = cd->backend->configure(mod, cd->channels);
		if (ret) {
			comp_err(dev, "webrtc_ns2: backend configure failed %d", ret);
			return ret;
		}
	}

	cd->buffered_frames = 0;
	cd->last_vad = -1; /* unknown */
	cd->configured = true;
	return 0;
}

static int webrtc_ns2_process(struct processing_module *mod,
			      struct sof_source **sources, int num_of_sources,
			      struct sof_sink **sinks, int num_of_sinks)
{
	struct webrtc_ns2_comp_data *cd = module_get_private_data(mod);
	struct sof_source *src = sources[0];
	struct sof_sink   *snk = sinks[0];
	int frame_bytes = source_get_frame_bytes(src);
	int avail = (int)source_get_data_frames_available(src);
	int free_f = (int)sink_get_free_frames(snk);
	int n = MIN(avail, free_f);
	size_t nbytes;
	const void *rd_ptr, *rd_start;
	void *wr_ptr, *wr_start;
	size_t buf_sz;
	int ret, frames_rem, chunk;

	if (n <= 0)
		return 0;

	nbytes = (size_t)n * frame_bytes;

	ret = source_get_data(src, nbytes, &rd_ptr, &rd_start, &buf_sz);
	if (ret)
		return ret;

	ret = sink_get_buffer(snk, nbytes, &wr_ptr, &wr_start, &buf_sz);
	if (ret) {
		source_release_data(src, 0);
		return ret;
	}

	/*
	 * Accumulate into in_buf[], process full 480-sample frames,
	 * drain denoised samples from out_buf[] into the sink write pointer.
	 * Both rd_ptr and wr_ptr advance by frame_bytes * n total.
	 */
	const char *rp = rd_ptr;
	char *wp = wr_ptr;

	for (frames_rem = n; frames_rem > 0; frames_rem -= chunk) {
		chunk = MIN(frames_rem,
			    WEBRTC_NS2_FRAME_SAMPLES - cd->buffered_frames);

		src_to_float(cd, rp, chunk, cd->buffered_frames);
		rp += chunk * frame_bytes;
		cd->buffered_frames += chunk;

		if (cd->buffered_frames >= WEBRTC_NS2_FRAME_SAMPLES) {
			float vad_prob = 0.0f, ch_prob;
			int c;

			for (c = 0; c < cd->channels; c++) {
				ch_prob = cd->backend->process_ch(
						mod,
						cd->in_buf[c],
						cd->out_buf[c], c);
				/* Average VAD probability across channels. */
				if (ch_prob >= 0.0f)
					vad_prob += ch_prob;
			}
			vad_prob /= (float)cd->channels;

			/* Write denoised samples to sink. */
			float_to_sink(cd, wp, WEBRTC_NS2_FRAME_SAMPLES);
			wp += WEBRTC_NS2_FRAME_SAMPLES * frame_bytes;

			cd->buffered_frames = 0;

#if CONFIG_WEBRTC_NS2_VAD_NOTIFY
			{
				int vad = (vad_prob >= cd->vad_threshold) ? 1 : 0;

				if (vad != cd->last_vad) {
					notifier_event(mod->dev, NOTIFIER_ID_VAD,
						       NOTIFIER_TARGET_CORE_LOCAL,
						       &vad, sizeof(vad));
					cd->last_vad = vad;
				}
			}
#endif
		}
	}

	source_release_data(src, nbytes);
	sink_commit_buffer(snk, nbytes);
	return 0;
}

static int webrtc_ns2_reset(struct processing_module *mod)
{
	struct webrtc_ns2_comp_data *cd = module_get_private_data(mod);

	comp_dbg(mod->dev, "webrtc_ns2: reset");
	cd->buffered_frames = 0;
	cd->last_vad = -1;

	if (cd->backend->reset)
		return cd->backend->reset(mod);

	return 0;
}

__cold static int webrtc_ns2_free(struct processing_module *mod)
{
	struct webrtc_ns2_comp_data *cd = module_get_private_data(mod);

	assert_can_be_cold();
	comp_dbg(mod->dev, "webrtc_ns2: free");

	if (cd->backend->free)
		cd->backend->free(mod);

	mod_free(mod, cd);
	return 0;
}

static const struct module_interface webrtc_ns2_interface = {
	.init    = webrtc_ns2_init,
	.prepare = webrtc_ns2_prepare,
	.process = webrtc_ns2_process,
	.reset   = webrtc_ns2_reset,
	.free    = webrtc_ns2_free,
};

#if CONFIG_COMP_WEBRTC_NS2_MODULE

#include <module/module/api_ver.h>
#include <module/module/llext.h>
#include <rimage/sof/user/manifest.h>

static const struct sof_man_module_manifest mod_manifest __section(".module") __used =
	SOF_LLEXT_MODULE_MANIFEST("WRTCNS2", &webrtc_ns2_interface, 1,
				  SOF_REG_UUID(webrtc_ns2), 40);

SOF_LLEXT_BUILDINFO;

#else

DECLARE_TR_CTX(webrtc_ns2_tr, SOF_UUID(webrtc_ns2_uuid), LOG_LEVEL_INFO);
DECLARE_MODULE_ADAPTER(webrtc_ns2_interface, webrtc_ns2_uuid, webrtc_ns2_tr);
SOF_MODULE_INIT(webrtc_ns2, sys_comp_module_webrtc_ns2_interface_init);

#endif
