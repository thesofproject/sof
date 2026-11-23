// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.
//
// WebRTC Voice Activity Detection module — SOF module_interface core.
//
// This translation unit contains the SOF module_interface glue only; the
// actual VAD classification is delegated to the backend selected at build
// time (webrtc_vad-stub.c or webrtc_vad-fvad.c).
//
// The module is a pass-through PCM effect: audio flows from source to sink
// unmodified. On each complete VAD frame (10/20/30 ms at the configured
// rate), the module classifies channel-0 audio and broadcasts the binary
// speech/non-speech decision via NOTIFIER_ID_VAD so that downstream
// consumers (keyword detection gating, host-side VAD) can react.
//
// Because the WebRTC VAD requires a fixed 10 ms frame size which may differ
// from the SOF pipeline period, the module accumulates incoming S16 samples
// in a small ring buffer, running the classifier once per full frame.

#include <sof/audio/module_adapter/module/generic.h>
#include <module/audio/source_api.h>
#include <module/audio/sink_api.h>
#include <sof/audio/audio_stream.h>
#include <sof/lib/notifier.h>
#include <sof/math/numbers.h>
#include <rtos/init.h>
#include <rtos/string.h>
#include <errno.h>
#include <stdint.h>
#include "webrtc_vad.h"

/* UUID identifies the component. Registered in uuid-registry.txt. */
SOF_DEFINE_REG_UUID(webrtc_vad);

/* Logging context. */
LOG_MODULE_REGISTER(webrtc_vad, CONFIG_SOF_LOG_LEVEL);

/**
 * webrtc_vad_init() - Allocate private data and initialise the backend.
 * @mod: Pointer to the processing module.
 *
 * Return: 0 on success, negative errno on failure.
 */
__cold static int webrtc_vad_init(struct processing_module *mod)
{
	struct module_data *md = &mod->priv;
	struct comp_dev *dev = mod->dev;
	struct webrtc_vad_comp_data *cd;
	int ret;

	assert_can_be_cold();
	comp_info(dev, "webrtc_vad: init");

	cd = mod_zalloc(mod, sizeof(*cd));
	if (!cd)
		return -ENOMEM;

	md->private = cd;
	cd->backend = &webrtc_vad_backend;

	comp_info(dev, "webrtc_vad: backend '%s'", cd->backend->name);

	if (cd->backend->init) {
		ret = cd->backend->init(mod);
		if (ret) {
			comp_err(dev, "webrtc_vad: backend init failed %d", ret);
			mod_free(mod, cd);
			return ret;
		}
	}

	return 0;
}

/**
 * webrtc_vad_prepare() - Configure the VAD for the negotiated audio format.
 * @mod: Pointer to the processing module.
 * @sources: Array of input audio sources (exactly 1 expected).
 * @num_of_sources: Must be 1.
 * @sinks: Array of output sinks (exactly 1 expected).
 * @num_of_sinks: Must be 1.
 *
 * Reads the sample rate and format from the source, configures the backend,
 * and calculates the frame size for the configured frame_ms.
 *
 * Return: 0 on success, negative errno on failure.
 */
static int webrtc_vad_sample_bytes(enum sof_ipc_frame fmt)
{
	switch (fmt) {
	case SOF_IPC_FRAME_S16_LE:
		return sizeof(int16_t);
	case SOF_IPC_FRAME_S32_LE:
		return sizeof(int32_t);
	default:
		return -EINVAL;
	}
}

__cold static int webrtc_vad_prepare(struct processing_module *mod,
				     struct sof_source **sources, int num_of_sources,
				     struct sof_sink **sinks, int num_of_sinks)
{
	struct webrtc_vad_comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	enum sof_ipc_frame in_fmt, out_fmt;
	int rate, in_sb, out_sb, ret;
	int frame_ms = CONFIG_WEBRTC_VAD_FRAME_MS;

	assert_can_be_cold();

	if (num_of_sources != 1 || num_of_sinks != 1) {
		comp_err(dev, "webrtc_vad: need exactly 1 source and 1 sink");
		return -EINVAL;
	}

	if (frame_ms != 10 && frame_ms != 20 && frame_ms != 30) {
		comp_err(dev, "webrtc_vad: invalid frame_ms %d (must be 10, 20, or 30)", frame_ms);
		return -EINVAL;
	}

	rate = source_get_rate(sources[0]);
	in_fmt = source_get_frm_fmt(sources[0]);
	out_fmt = sink_get_frm_fmt(sinks[0]);
	cd->channels = source_get_channels(sources[0]);
	cd->sample_rate = rate;

	if (cd->channels != sink_get_channels(sinks[0]) ||
	    rate != sink_get_rate(sinks[0])) {
		comp_err(dev, "webrtc_vad: source/sink mismatch");
		return -EINVAL;
	}

	/* libfvad accepts 8/16/32/48 kHz. */
	if (rate != 8000 && rate != 16000 && rate != 32000 && rate != 48000) {
		comp_err(dev, "webrtc_vad: unsupported rate %d", rate);
		return -EINVAL;
	}

	in_sb = webrtc_vad_sample_bytes(in_fmt);
	out_sb = webrtc_vad_sample_bytes(out_fmt);
	if (in_sb < 0 || out_sb < 0) {
		comp_err(dev, "webrtc_vad: unsupported format in=%d out=%d",
			 in_fmt, out_fmt);
		return -EINVAL;
	}
	cd->in_fmt = in_fmt;
	cd->out_fmt = out_fmt;
	cd->in_sample_bytes = in_sb;
	cd->out_sample_bytes = out_sb;

	/* Number of samples per VAD frame (mono). */
	cd->frame_samples = (rate * frame_ms) / 1000;
	if (cd->frame_samples > WEBRTC_VAD_MAX_FRAME_SAMPLES) {
		comp_err(dev, "webrtc_vad: frame too large (%d samples)", cd->frame_samples);
		return -EINVAL;
	}

	comp_info(dev, "webrtc_vad: rate=%d ch=%d in_fmt=%d out_fmt=%d frame_ms=%d frame_samples=%d",
		  rate, cd->channels, in_fmt, out_fmt, frame_ms, cd->frame_samples);

	if (cd->backend->configure) {
		ret = cd->backend->configure(mod, rate, CONFIG_WEBRTC_VAD_MODE);
		if (ret) {
			comp_err(dev, "webrtc_vad: backend configure failed %d", ret);
			return ret;
		}
	}

	cd->accum_samples = 0;
	cd->last_decision = WEBRTC_VAD_SILENCE;
	cd->configured = true;
	return 0;
}

/**
 * webrtc_vad_accumulate() - Extract channel-0 samples into the VAD ring buffer.
 * @cd:       Module private data.
 * @raw:      Pointer to interleaved raw audio (S16 or S32).
 * @n_frames: Number of frames in @raw.
 *
 * Downmixes to mono by picking channel 0. For S32, shifts right 16 bits to
 * produce S16 for libfvad (which always classifies at 16-bit depth).
 */
static void webrtc_vad_accumulate(struct webrtc_vad_comp_data *cd,
				  const void *raw, int n_frames)
{
	int ch = cd->channels;
	int i;

	for (i = 0; i < n_frames && cd->accum_samples < WEBRTC_VAD_MAX_FRAME_SAMPLES; i++) {
		int16_t s;

		if (cd->in_sample_bytes == sizeof(int16_t)) {
			const int16_t *p = raw;

			s = p[i * ch]; /* channel 0 */
		} else {
			const int32_t *p = raw;

			s = (int16_t)(p[i * ch] >> 16);
		}
		cd->accumulator[cd->accum_samples++] = s;
	}
}

/**
 * webrtc_vad_classify_pending() - Drain the accumulator, classifying full frames.
 * @mod: Pointer to the processing module.
 *
 * For each complete VAD frame accumulated, calls the backend and fires a
 * NOTIFIER_ID_VAD event with the binary decision.
 */
static void webrtc_vad_classify_pending(struct processing_module *mod)
{
	struct webrtc_vad_comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	int decision, remaining;

	while (cd->accum_samples >= cd->frame_samples) {
		decision = cd->backend->classify(mod, cd->accumulator,
						 cd->frame_samples);
		if (decision < 0) {
			comp_warn(dev, "webrtc_vad: classify error %d", decision);
			decision = cd->last_decision;
		} else {
			cd->last_decision = decision;
		}

		/* Shift consumed samples out of the accumulator. */
		remaining = cd->accum_samples - cd->frame_samples;
		if (remaining > 0)
			memmove(cd->accumulator,
				cd->accumulator + cd->frame_samples,
				(size_t)remaining * sizeof(int16_t));
		cd->accum_samples = remaining;

		/* Broadcast VAD decision to registered listeners. */
		notifier_event(dev, NOTIFIER_ID_VAD,
			       NOTIFIER_TARGET_CORE_LOCAL, &decision,
			       sizeof(decision));
	}
}

/**
 * webrtc_vad_process() - Pass audio through and classify accumulated frames.
 * @mod: Pointer to the processing module.
 * @sources: Input sources (1 element).
 * @num_of_sources: 1.
 * @sinks: Output sinks (1 element).
 * @num_of_sinks: 1.
 *
 * Audio is passed from source to sink byte-for-byte. Simultaneously, channel-0
 * samples are extracted from the source data buffer (before it is released)
 * and appended to the accumulator for VAD classification.
 *
 * Return: 0 on success, negative errno on error.
 */
static int webrtc_vad_process(struct processing_module *mod,
			      struct sof_source **sources, int num_of_sources,
			      struct sof_sink **sinks, int num_of_sinks)
{
	struct webrtc_vad_comp_data *cd = module_get_private_data(mod);
	struct sof_source *src = sources[0];
	struct sof_sink *snk = sinks[0];
	size_t in_frame_bytes = source_get_frame_bytes(src);
	size_t out_frame_bytes = sink_get_frame_bytes(snk);
	int avail = (int)source_get_data_frames_available(src);
	int free  = (int)sink_get_free_frames(snk);
	int n = MIN(avail, free);
	size_t in_nbytes, out_nbytes;
	struct cir_buf_source source_buf;
	struct cir_buf_sink sink_buf;
	size_t src_bytes, snk_bytes;
	size_t remaining_frames;
	int ret;

	if (n <= 0)
		return 0;

	in_nbytes = (size_t)n * in_frame_bytes;
	out_nbytes = (size_t)n * out_frame_bytes;

	/* ---- Obtain source data pointer (read-only, zero-copy) ---- */
	ret = source_get_data(src, in_nbytes, &source_buf.ptr, &source_buf.buf_start, &src_bytes);
	if (ret)
		return ret;
	source_buf.buf_end = (const char *)source_buf.buf_start + src_bytes;

	/* ---- Obtain sink write pointer ---- */
	ret = sink_get_buffer(snk, out_nbytes, &sink_buf.ptr, &sink_buf.buf_start, &snk_bytes);
	if (ret) {
		source_release_data(src, 0); /* release without consuming */
		return ret;
	}
	sink_buf.buf_end = (char *)sink_buf.buf_start + snk_bytes;

	remaining_frames = n;
	while (remaining_frames) {
		size_t fmax_src = circ_buf_frames_without_wrap(source_buf.ptr, source_buf.buf_end,
							      cd->in_sample_bytes, cd->channels);
		size_t fmax_snk = circ_buf_frames_without_wrap(sink_buf.ptr, sink_buf.buf_end,
							      cd->out_sample_bytes, cd->channels);
		size_t f = MIN(remaining_frames, MIN(fmax_src, fmax_snk));

		/* ---- Pass-through copy / format conversion ---- */
		if (cd->in_fmt == cd->out_fmt) {
			memcpy_s(sink_buf.ptr, f * out_frame_bytes,
				 source_buf.ptr, f * in_frame_bytes);
		} else {
			size_t samples = f * cd->channels;
			size_t i;

			if (cd->in_fmt == SOF_IPC_FRAME_S32_LE) {
				const int32_t *s = source_buf.ptr;
				int16_t *d = sink_buf.ptr;

				for (i = 0; i < samples; i++)
					d[i] = (int16_t)(s[i] >> 16);
			} else {
				const int16_t *s = source_buf.ptr;
				int32_t *d = sink_buf.ptr;

				for (i = 0; i < samples; i++)
					d[i] = ((int32_t)s[i]) << 16;
			}
		}

		/* ---- Accumulate channel-0 samples for VAD ---- */
		webrtc_vad_accumulate(cd, source_buf.ptr, (int)f);

		remaining_frames -= f;
		source_buf.ptr = source_cir_buf_wrap((const char *)source_buf.ptr + f * in_frame_bytes,
						     source_buf.buf_start, source_buf.buf_end);
		sink_buf.ptr = cir_buf_wrap((char *)sink_buf.ptr + f * out_frame_bytes,
					    sink_buf.buf_start, sink_buf.buf_end);
	}

	/* ---- Commit / release ---- */
	source_release_data(src, in_nbytes);
	sink_commit_buffer(snk, out_nbytes);

	/* ---- Classify any complete frames ---- */
	webrtc_vad_classify_pending(mod);

	return 0;
}

/**
 * webrtc_vad_reset() - Flush accumulator and reset backend state.
 * @mod: Pointer to the processing module.
 *
 * Return: 0 on success.
 */
static int webrtc_vad_reset(struct processing_module *mod)
{
	struct webrtc_vad_comp_data *cd = module_get_private_data(mod);

	comp_dbg(mod->dev, "webrtc_vad: reset");
	cd->accum_samples = 0;
	cd->last_decision = WEBRTC_VAD_SILENCE;

	if (cd->backend->reset)
		return cd->backend->reset(mod);

	return 0;
}

/**
 * webrtc_vad_free() - Free all resources.
 * @mod: Pointer to the processing module.
 *
 * Return: 0 on success.
 */
__cold static int webrtc_vad_free(struct processing_module *mod)
{
	struct webrtc_vad_comp_data *cd = module_get_private_data(mod);

	assert_can_be_cold();
	comp_dbg(mod->dev, "webrtc_vad: free");

	if (cd->backend->free)
		cd->backend->free(mod);

	mod_free(mod, cd);
	return 0;
}

/* Module operations vtable. */
static const struct module_interface webrtc_vad_interface = {
	.init    = webrtc_vad_init,
	.prepare = webrtc_vad_prepare,
	.process = webrtc_vad_process,
	.reset   = webrtc_vad_reset,
	.free    = webrtc_vad_free,
};

/* If COMP_WEBRTC_VAD is =m in Kconfig this is built as a loadable LLEXT. */
#if CONFIG_COMP_WEBRTC_VAD_MODULE

#include <module/module/api_ver.h>
#include <module/module/llext.h>
#include <rimage/sof/user/manifest.h>

static const struct sof_man_module_manifest mod_manifest __section(".module") __used =
	SOF_LLEXT_MODULE_MANIFEST("WRTCVAD", &webrtc_vad_interface, 1,
				  SOF_REG_UUID(webrtc_vad), 40);

SOF_LLEXT_BUILDINFO;

#else

DECLARE_TR_CTX(webrtc_vad_tr, SOF_UUID(webrtc_vad_uuid), LOG_LEVEL_INFO);
DECLARE_MODULE_ADAPTER(webrtc_vad_interface, webrtc_vad_uuid, webrtc_vad_tr);
SOF_MODULE_INIT(webrtc_vad, sys_comp_module_webrtc_vad_interface_init);

#endif
