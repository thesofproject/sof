// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.
//
// WebRTC Noise Suppression module — SOF module_interface core.
//
// Single-input, single-output PCM effect (mic → denoised mic). The module
// converts interleaved S16/S32 → planar float, accumulates a full 10 ms frame,
// runs the backend NS for each channel, and converts float → S16/S32 back to
// the sink. Latency is at most 10 ms (one NS frame period).

#include <sof/audio/module_adapter/module/generic.h>
#include <module/audio/source_api.h>
#include <module/audio/sink_api.h>
#include <sof/math/numbers.h>
#include <rtos/init.h>
#include <rtos/string.h>
#include <errno.h>
#include <stdint.h>
#include <math.h>
#include "webrtc_ns.h"

/* UUID registered in uuid-registry.txt. */
SOF_DEFINE_REG_UUID(webrtc_ns);

LOG_MODULE_REGISTER(webrtc_ns, CONFIG_SOF_LOG_LEVEL);

/* S32 normalisation scale factor (2^31). */
#define WEBRTC_NS_S32_SCALE	2147483648.0f

/* S16 normalisation scale factor (2^15). */
#define WEBRTC_NS_S16_SCALE	32768.0f

/**
 * webrtc_ns_init() - Allocate private data and initialise the backend.
 */
__cold static int webrtc_ns_init(struct processing_module *mod)
{
	struct module_data *md = &mod->priv;
	struct comp_dev *dev = mod->dev;
	struct webrtc_ns_comp_data *cd;
	int ret, i;

	assert_can_be_cold();
	comp_info(dev, "webrtc_ns: init");

	cd = mod_zalloc(mod, sizeof(*cd));
	if (!cd)
		return -ENOMEM;

	md->private = cd;
	cd->backend = &webrtc_ns_backend;

	/* Wire up permanent planar buffer pointer arrays. */
	for (i = 0; i < WEBRTC_NS_CHANNELS_MAX; i++) {
		cd->in_ptrs[i]  = cd->in_buf[i];
		cd->out_ptrs[i] = cd->out_buf[i];
	}

	comp_info(dev, "webrtc_ns: backend '%s'", cd->backend->name);

	if (cd->backend->init) {
		ret = cd->backend->init(mod);
		if (ret) {
			comp_err(dev, "webrtc_ns: backend init failed %d", ret);
			mod_free(mod, cd);
			return ret;
		}
	}

	return 0;
}

/**
 * webrtc_ns_prepare() - Configure NS for the negotiated pipeline format.
 */
__cold static int webrtc_ns_prepare(struct processing_module *mod,
				    struct sof_source **sources, int num_of_sources,
				    struct sof_sink **sinks, int num_of_sinks)
{
	struct webrtc_ns_comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	int rate, fmt, ret;

	assert_can_be_cold();

	if (num_of_sources != 1 || num_of_sinks != 1) {
		comp_err(dev, "webrtc_ns: need exactly 1 source and 1 sink");
		return -EINVAL;
	}

	rate = source_get_rate(sources[0]);
	fmt  = source_get_frm_fmt(sources[0]);
	cd->channels = source_get_channels(sources[0]);

	if (cd->channels > WEBRTC_NS_CHANNELS_MAX) {
		comp_err(dev, "webrtc_ns: too many channels %d (max %d)",
			 cd->channels, WEBRTC_NS_CHANNELS_MAX);
		return -EINVAL;
	}

	/* NS only accepts 8/16/32/48 kHz. */
	if (rate != 8000 && rate != 16000 && rate != 32000 && rate != 48000) {
		comp_err(dev, "webrtc_ns: unsupported rate %d", rate);
		return -EINVAL;
	}

	switch (fmt) {
	case SOF_IPC_FRAME_S16_LE:
	case SOF_IPC_FRAME_S32_LE:
		break;
	default:
		comp_err(dev, "webrtc_ns: unsupported frame format %d", fmt);
		return -EINVAL;
	}

	cd->in_rate = rate;
	cd->proc_rate = CONFIG_WEBRTC_NS_SAMPLE_RATE_HZ;
	if (cd->proc_rate != rate) {
		/* Proc rate must be <= in_rate for simple downsampling. */
		if (cd->proc_rate > rate || rate % cd->proc_rate != 0) {
			comp_err(dev, "webrtc_ns: proc_rate %d incompatible with in_rate %d",
				 cd->proc_rate, rate);
			return -EINVAL;
		}
	}

	cd->in_frame_bytes    = source_get_frame_bytes(sources[0]);
	cd->proc_frame_samples = (cd->proc_rate * 10) / 1000; /* 10 ms */
	cd->in_frame_samples   = (cd->in_rate  * 10) / 1000;

	if (cd->proc_frame_samples > WEBRTC_NS_FRAME_SAMPLES_MAX) {
		comp_err(dev, "webrtc_ns: proc_frame_samples %d exceeds max %d",
			 cd->proc_frame_samples, WEBRTC_NS_FRAME_SAMPLES_MAX);
		return -EINVAL;
	}

	comp_info(dev, "webrtc_ns: in_rate=%d proc_rate=%d ch=%d proc_frame=%d",
		  cd->in_rate, cd->proc_rate, cd->channels, cd->proc_frame_samples);

	if (cd->backend->configure) {
		ret = cd->backend->configure(mod, cd->proc_rate,
					     CONFIG_WEBRTC_NS_LEVEL,
					     cd->channels);
		if (ret) {
			comp_err(dev, "webrtc_ns: backend configure failed %d", ret);
			return ret;
		}
	}

	cd->buffered_frames = 0;
	cd->configured = true;
	return 0;
}

/**
 * s32_to_float() - Convert interleaved S32 samples to planar float.
 * @cd:       Module private data (channels, proc_frame_samples).
 * @src:      Source pointer (interleaved S32, raw[ch]).
 * @n_frames: Number of frames (each frame has cd->channels samples).
 * @frame0:   Starting offset in the planar float buffer (for accumulation).
 */
static void s32_to_float(struct webrtc_ns_comp_data *cd,
			 const int32_t *src, int n_frames, int frame0)
{
	int i, c;

	for (i = 0; i < n_frames; i++)
		for (c = 0; c < cd->channels; c++)
			cd->in_buf[c][frame0 + i] =
				(float)src[i * cd->channels + c] / WEBRTC_NS_S32_SCALE;
}

/**
 * s16_to_float() - Convert interleaved S16 samples to planar float.
 */
static void s16_to_float(struct webrtc_ns_comp_data *cd,
			 const int16_t *src, int n_frames, int frame0)
{
	int i, c;

	for (i = 0; i < n_frames; i++)
		for (c = 0; c < cd->channels; c++)
			cd->in_buf[c][frame0 + i] =
				(float)src[i * cd->channels + c] / WEBRTC_NS_S16_SCALE;
}

/**
 * float_to_s32() - Write planar float NS output back as interleaved S32.
 */
static void float_to_s32(struct webrtc_ns_comp_data *cd,
			 int32_t *dst, int n_frames)
{
	int i, c;

	for (i = 0; i < n_frames; i++) {
		for (c = 0; c < cd->channels; c++) {
			float f = cd->out_buf[c][i] * WEBRTC_NS_S32_SCALE;

			f = f >  2147483647.0f ?  2147483647.0f :
			    f < -2147483648.0f ? -2147483648.0f : f;
			dst[i * cd->channels + c] = (int32_t)f;
		}
	}
}

/**
 * float_to_s16() - Write planar float NS output back as interleaved S16.
 */
static void float_to_s16(struct webrtc_ns_comp_data *cd,
			 int16_t *dst, int n_frames)
{
	int i, c;

	for (i = 0; i < n_frames; i++) {
		for (c = 0; c < cd->channels; c++) {
			float f = cd->out_buf[c][i] * WEBRTC_NS_S16_SCALE;

			f = f >  32767.0f ?  32767.0f :
			    f < -32768.0f ? -32768.0f : f;
			dst[i * cd->channels + c] = (int16_t)f;
		}
	}
}

/**
 * webrtc_ns_process() - Run NS on accumulated 10 ms frames.
 *
 * The pipeline period may be shorter than 10 ms, so we accumulate interleaved
 * PCM → planar float until we have a full NS frame, then process and drain.
 */
static int webrtc_ns_process(struct processing_module *mod,
			     struct sof_source **sources, int num_of_sources,
			     struct sof_sink **sinks, int num_of_sinks)
{
	struct webrtc_ns_comp_data *cd = module_get_private_data(mod);
	struct sof_source *src = sources[0];
	struct sof_sink *snk = sinks[0];
	enum sof_ipc_frame fmt = source_get_frm_fmt(src);
	int avail = (int)source_get_data_frames_available(src);
	int free  = (int)sink_get_free_frames(snk);
	int n = MIN(avail, free);
	size_t nbytes, buf_size;
	void const *rd_ptr, *buf_start;
	void *wr_ptr, *wr_buf_start;
	int ret;

	if (n <= 0)
		return 0;

	nbytes = (size_t)n * source_get_frame_bytes(src);

	ret = source_get_data(src, nbytes, &rd_ptr, &buf_start, &buf_size);
	if (ret)
		return ret;

	ret = sink_get_buffer(snk, nbytes, &wr_ptr, &wr_buf_start, &buf_size);
	if (ret) {
		source_release_data(src, 0);
		return ret;
	}

	/* Convert input to planar float, accumulating into in_buf[]. */
	if (fmt == SOF_IPC_FRAME_S32_LE)
		s32_to_float(cd, rd_ptr, n, cd->buffered_frames);
	else
		s16_to_float(cd, rd_ptr, n, cd->buffered_frames);

	source_release_data(src, nbytes);

	cd->buffered_frames += n;

	/*
	 * Process complete NS frames. Each call consumes proc_frame_samples.
	 * Any partial frame remains in in_buf[] for the next cycle.
	 */
	while (cd->buffered_frames >= cd->proc_frame_samples) {
		int fs = cd->proc_frame_samples;

		ret = cd->backend->process(mod,
					   (const float *const *)cd->in_ptrs,
					   cd->out_ptrs, fs);
		if (ret) {
			comp_err(mod->dev, "webrtc_ns: backend process error %d", ret);
			/* On error: fall back to pass-through for this frame. */
			int c;

			for (c = 0; c < cd->channels; c++)
				memcpy(cd->out_buf[c], cd->in_buf[c],
				       (size_t)fs * sizeof(float));
		}

		/* Write processed frame to sink. */
		if (fmt == SOF_IPC_FRAME_S32_LE)
			float_to_s32(cd, wr_ptr, fs);
		else
			float_to_s16(cd, wr_ptr, fs);

		/* Slide remaining samples to front of in_buf[]. */
		cd->buffered_frames -= fs;
		if (cd->buffered_frames > 0) {
			int c;

			for (c = 0; c < cd->channels; c++)
				memmove(cd->in_buf[c], cd->in_buf[c] + fs,
					(size_t)cd->buffered_frames * sizeof(float));
		}
	}

	sink_commit_buffer(snk, nbytes);
	return 0;
}

/**
 * webrtc_ns_reset() - Flush accumulator and reset backend state.
 */
static int webrtc_ns_reset(struct processing_module *mod)
{
	struct webrtc_ns_comp_data *cd = module_get_private_data(mod);

	comp_dbg(mod->dev, "webrtc_ns: reset");
	cd->buffered_frames = 0;

	if (cd->backend->reset)
		return cd->backend->reset(mod);

	return 0;
}

/**
 * webrtc_ns_free() - Free all resources.
 */
__cold static int webrtc_ns_free(struct processing_module *mod)
{
	struct webrtc_ns_comp_data *cd = module_get_private_data(mod);

	assert_can_be_cold();
	comp_dbg(mod->dev, "webrtc_ns: free");

	if (cd->backend->free)
		cd->backend->free(mod);

	mod_free(mod, cd);
	return 0;
}

static const struct module_interface webrtc_ns_interface = {
	.init    = webrtc_ns_init,
	.prepare = webrtc_ns_prepare,
	.process = webrtc_ns_process,
	.reset   = webrtc_ns_reset,
	.free    = webrtc_ns_free,
};

#if CONFIG_COMP_WEBRTC_NS_MODULE

#include <module/module/api_ver.h>
#include <module/module/llext.h>
#include <rimage/sof/user/manifest.h>

static const struct sof_man_module_manifest mod_manifest __section(".module") __used =
	SOF_LLEXT_MODULE_MANIFEST("WRTCNS", &webrtc_ns_interface, 1,
				  SOF_REG_UUID(webrtc_ns), 40);

SOF_LLEXT_BUILDINFO;

#else

DECLARE_TR_CTX(webrtc_ns_tr, SOF_UUID(webrtc_ns_uuid), LOG_LEVEL_INFO);
DECLARE_MODULE_ADAPTER(webrtc_ns_interface, webrtc_ns_uuid, webrtc_ns_tr);
SOF_MODULE_INIT(webrtc_ns, sys_comp_module_webrtc_ns_interface_init);

#endif
