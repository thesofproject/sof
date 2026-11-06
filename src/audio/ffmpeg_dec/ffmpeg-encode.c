// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.
//
// Generic encode framework for the ffmpeg_dec module (CONFIG_FFMPEG_ENC):
// PCM in, compressed elementary stream out via per-codec ffmpeg_enc_codec_ops.
//
// Like the decoder this is a sink/source (DP-domain) module: PCM arrives on the
// circular source buffer, compressed frames are written to the circular
// sink buffer. RAW_DATA processing is only prepared by module_adapter in the LL
// domain, so a DP-scheduled codec (needed to keep the heavy codec init and
// per-frame encode off the LL tick) must use the sink/source .process interface.

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/source_api.h>
#include <sof/audio/sink_api.h>
#include <ipc/compress_params.h>
#include <rtos/string.h>
#include <errno.h>
#include "ffmpeg_dec.h"

LOG_MODULE_DECLARE(ffmpeg_dec, CONFIG_SOF_LOG_LEVEL);

#define FFMPEG_ENC_DEFAULT_BITRATE	128000

/* Linear staging for compressed output, drained into the circular sink over as
 * many cycles as needed. An AAC frame at 128 kbps is ~340 bytes; MP3 is ~384 bytes.
 * 16 KiB holds dozens of frames comfortably.
 */
#define FFMPEG_ENC_STAGE_SIZE		16384

/* LL->DP input ring depth hint (bytes). bind allocates a ~3x ring; this must
 * hold one whole worst-case encode of real-time capture PCM so a blocking
 * frame encode does not overflow the input and overrun the DAI.
 */
#define FFMPEG_ENC_PCM_STAGE		16384

static const struct ffmpeg_enc_codec_ops * const ffmpeg_enc_codecs[] = {
#if defined(CONFIG_FFMPEG_ENC_MP3)
	&ffmpeg_enc_mp3_ops,
#endif
#if defined(CONFIG_FFMPEG_ENC_AAC)
	&ffmpeg_enc_aac_ops,
#endif
};

static const struct ffmpeg_enc_codec_ops *ffmpeg_enc_find_codec_ops_by_snd_id(uint32_t snd_id)
{
	for (size_t i = 0; i < ARRAY_SIZE(ffmpeg_enc_codecs); i++) {
		if (ffmpeg_enc_codecs[i]->snd_codec_id == snd_id)
			return ffmpeg_enc_codecs[i];
	}
	return NULL;
}

static inline size_t ffmpeg_enc_min(size_t a, size_t b)
{
	return a < b ? a : b;
}

static inline int16_t ffmpeg_enc_s24_to_s16(int32_t s)
{
	int32_t v = s >> 8;

	if (v > 32767)
		return 32767;
	if (v < -32768)
		return -32768;
	return (int16_t)v;
}

void ffmpeg_enc_pcm_to_s16_interleaved(const uint8_t *pcm_in, int16_t *dst,
				       size_t total_samples, int src_fmt)
{
	size_t i;

	if (src_fmt == SOF_IPC_FRAME_S16_LE) {
		memcpy_s(dst, total_samples * sizeof(int16_t),
			 pcm_in, total_samples * sizeof(int16_t));
		return;
	}

	if (src_fmt == SOF_IPC_FRAME_S24_4LE) {
		const int32_t *in = (const int32_t *)pcm_in;

		for (i = 0; i < total_samples; i++)
			dst[i] = ffmpeg_enc_s24_to_s16(in[i]);
		return;
	}

	{
		const int32_t *in = (const int32_t *)pcm_in;

		for (i = 0; i + 4 <= total_samples; i += 4) {
			dst[i + 0] = (int16_t)(in[i + 0] >> 16);
			dst[i + 1] = (int16_t)(in[i + 1] >> 16);
			dst[i + 2] = (int16_t)(in[i + 2] >> 16);
			dst[i + 3] = (int16_t)(in[i + 3] >> 16);
		}
		for (; i < total_samples; i++)
			dst[i] = (int16_t)(in[i] >> 16);
	}
}

void ffmpeg_enc_pcm_to_s16_planar(const uint8_t *pcm_in, uint8_t * const *planes,
				  int nb_samples, int ch, int src_fmt)
{
	int i, c;

	if (src_fmt == SOF_IPC_FRAME_S16_LE) {
		const int16_t *in = (const int16_t *)pcm_in;

		if (ch == 2) {
			int16_t *out0 = (int16_t *)planes[0];
			int16_t *out1 = (int16_t *)planes[1];

			for (i = 0; i < nb_samples; i++) {
				out0[i] = in[2 * i + 0];
				out1[i] = in[2 * i + 1];
			}
			return;
		}
		for (c = 0; c < ch; c++) {
			int16_t *out = (int16_t *)planes[c];

			for (i = 0; i < nb_samples; i++)
				out[i] = in[i * ch + c];
		}
		return;
	}

	if (src_fmt == SOF_IPC_FRAME_S24_4LE) {
		const int32_t *in = (const int32_t *)pcm_in;

		if (ch == 2) {
			int16_t *out0 = (int16_t *)planes[0];
			int16_t *out1 = (int16_t *)planes[1];

			for (i = 0; i < nb_samples; i++) {
				out0[i] = ffmpeg_enc_s24_to_s16(in[2 * i + 0]);
				out1[i] = ffmpeg_enc_s24_to_s16(in[2 * i + 1]);
			}
			return;
		}
		for (c = 0; c < ch; c++) {
			int16_t *out = (int16_t *)planes[c];

			for (i = 0; i < nb_samples; i++)
				out[i] = ffmpeg_enc_s24_to_s16(in[i * ch + c]);
		}
		return;
	}

	{
		const int32_t *in = (const int32_t *)pcm_in;

		if (ch == 2) {
			int16_t *out0 = (int16_t *)planes[0];
			int16_t *out1 = (int16_t *)planes[1];

			for (i = 0; i + 4 <= nb_samples; i += 4) {
				out0[i + 0] = (int16_t)(in[0] >> 16);
				out1[i + 0] = (int16_t)(in[1] >> 16);
				out0[i + 1] = (int16_t)(in[2] >> 16);
				out1[i + 1] = (int16_t)(in[3] >> 16);
				out0[i + 2] = (int16_t)(in[4] >> 16);
				out1[i + 2] = (int16_t)(in[5] >> 16);
				out0[i + 3] = (int16_t)(in[6] >> 16);
				out1[i + 3] = (int16_t)(in[7] >> 16);
				in += 8;
			}
			for (; i < nb_samples; i++) {
				out0[i] = (int16_t)(in[0] >> 16);
				out1[i] = (int16_t)(in[1] >> 16);
				in += 2;
			}
			return;
		}
		for (c = 0; c < ch; c++) {
			int16_t *out = (int16_t *)planes[c];

			for (i = 0; i < nb_samples; i++)
				out[i] = (int16_t)(in[i * ch + c] >> 16);
		}
	}
}

void ffmpeg_enc_stage_packet(struct processing_module *mod, struct ffmpeg_enc_data *e,
			     const uint8_t *data, size_t len)
{
	if (e->out_avail + len <= e->out_cap) {
		memcpy_s(e->out_stage + e->out_avail,
			 e->out_cap - e->out_avail, data, len);
		e->out_avail += len;
	} else {
		comp_warn(mod->dev, "%s stage full, packet dropped",
			  e->codec_ops ? e->codec_ops->name : "enc");
	}
}

static void ffmpeg_enc_free_staging(struct processing_module *mod, struct ffmpeg_enc_data *e)
{
	if (!e)
		return;

	if (e->pcm_in) {
		mod_free(mod, e->pcm_in);
		e->pcm_in = NULL;
	}
	if (e->out_stage) {
		mod_free(mod, e->out_stage);
		e->out_stage = NULL;
	}
	e->out_cap = 0;
	e->out_avail = 0;
	e->out_rd = 0;
	e->pcm_fill = 0;
}

/* Drain as much staged compressed output as the sink can currently accept. */
static void ffmpeg_enc_drain(struct ffmpeg_enc_data *e, struct sof_sink *sink)
{
	void *dst, *buf_start;
	size_t buf_size, n;
	int ret;

	n = ffmpeg_enc_min(e->out_avail, sink_get_free_size(sink));
	if (!n)
		return;

	ret = sink_get_buffer(sink, n, &dst, &buf_start, &buf_size);
	if (ret)
		return;

	ffmpeg_dec_copy_to_circular(dst, buf_start, buf_size,
				    e->out_stage + e->out_rd, n);
	sink_commit_buffer(sink, n);
	e->out_rd += n;
	e->out_avail -= n;
	if (!e->out_avail)
		e->out_rd = 0;
}

int ffmpeg_enc_mod_init(struct processing_module *mod)
{
	struct module_data *md = &mod->priv;
	struct comp_dev *dev = mod->dev;
	struct ffmpeg_dec_comp_data *cd;
	struct ffmpeg_enc_data *e;
	int ret;

	ffmpeg_dec_libc_bind(mod);

	/* Allocate the module private data and publish it (module_adapter does
	 * not allocate it for us; the decoder path does the same in its init).
	 */
	cd = mod_zalloc(mod, sizeof(*cd));
	if (!cd)
		return -ENOMEM;
	cd->is_encoder = true;
	md->private = cd;

	/* Size the DP output ring so it holds several compressed frames
	 * (an AAC frame at 128 kbps is ~340 bytes; MP3 is ~384 bytes).
	 * bind sizes the ring between this DP module and downstream copier as 3x this.
	 */
	md->mpd.out_buff_size = FFMPEG_ENC_STAGE_SIZE;

	/* Size the LL->DP input ring deep enough to absorb one whole worst-case
	 * encode of real-time capture PCM.
	 */
	md->mpd.in_buff_size = FFMPEG_ENC_PCM_STAGE;

	e = mod_zalloc(mod, sizeof(*e));
	if (!e) {
		mod_free(mod, cd);
		md->private = NULL;
		return -ENOMEM;
	}

	cd->codec = FFMPEG_DEC_CODEC_NONE;
	e->bit_rate = FFMPEG_ENC_DEFAULT_BITRATE;

#if CONFIG_IPC_MAJOR_4
	if (!mod->priv.cfg.ext_data ||
	    !mod->priv.cfg.ext_data->module_data ||
	    mod->priv.cfg.ext_data->module_data_size < sizeof(struct snd_codec)) {
		comp_err(dev, "missing IPC codec parameters for encoder");
		mod_free(mod, e);
		mod_free(mod, cd);
		md->private = NULL;
		return -EINVAL;
	}

	{
		const struct snd_codec *codec_params = mod->priv.cfg.ext_data->module_data;
		const struct ffmpeg_enc_codec_ops *ops;

		if (codec_params->ch_in && codec_params->ch_out &&
		    codec_params->ch_in != codec_params->ch_out) {
			comp_err(dev, "channel conversion ch_in %u -> ch_out %u not supported",
				 codec_params->ch_in, codec_params->ch_out);
			mod_free(mod, e);
			mod_free(mod, cd);
			md->private = NULL;
			return -ENOTSUP;
		}
		if (codec_params->rate_control != 0 &&
		    codec_params->rate_control != SND_RATECONTROLMODE_CONSTANTBITRATE) {
			comp_err(dev, "unsupported rate_control mode 0x%x",
				 codec_params->rate_control);
			mod_free(mod, e);
			mod_free(mod, cd);
			md->private = NULL;
			return -ENOTSUP;
		}

		ops = ffmpeg_enc_find_codec_ops_by_snd_id(codec_params->id);
		if (!ops) {
			comp_err(dev, "unsupported or disabled encoder snd_codec id 0x%x",
				 codec_params->id);
			mod_free(mod, e);
			mod_free(mod, cd);
			md->private = NULL;
			return -ENOTSUP;
		}
		if (ops->validate_params) {
			ret = ops->validate_params(mod, codec_params);
			if (ret) {
				mod_free(mod, e);
				mod_free(mod, cd);
				md->private = NULL;
				return ret;
			}
		}
		e->codec_ops = ops;
		cd->codec = ops->id;
		e->req_rate = codec_params->sample_rate;
		e->req_channels = codec_params->ch_in ?
				  codec_params->ch_in : codec_params->ch_out;
		e->req_ch_mode = codec_params->ch_mode;
		comp_info(dev, "encoder codec selected from IPC: %s (snd_codec id 0x%x)",
			  ops->name, codec_params->id);
		if (codec_params->bit_rate)
			e->bit_rate = codec_params->bit_rate;
	}
#endif

	if (!e->codec_ops || !e->codec_ops->init) {
		mod_free(mod, e);
		mod_free(mod, cd);
		md->private = NULL;
		return -ENOTSUP;
	}

	ret = e->codec_ops->init(mod, e);
	if (ret) {
		mod_free(mod, e);
		mod_free(mod, cd);
		md->private = NULL;
		return ret;
	}

	cd->backend_data = e;
	return 0;
}

int ffmpeg_enc_mod_prepare(struct processing_module *mod,
			   struct sof_source **sources, int num_of_sources,
			   struct sof_sink **sinks, int num_of_sinks)
{
	struct ffmpeg_dec_comp_data *cd = module_get_private_data(mod);
	struct ffmpeg_enc_data *e = cd->backend_data;
	struct comp_dev *dev = mod->dev;

	if (num_of_sources != 1 || num_of_sinks != 1) {
		comp_err(dev, "ffmpeg_enc: need 1 source/1 sink, got %d/%d",
			 num_of_sources, num_of_sinks);
		return -EINVAL;
	}

	ffmpeg_dec_libc_bind(mod);

	if (e) {
		e->out_avail = 0;
		e->out_rd = 0;
		e->pcm_fill = 0;
	}

	dev->period = 96000; /* us */

	comp_info(dev, "ffmpeg_enc prepare: %d src / %d sink (open deferred to DP)",
		  num_of_sources, num_of_sinks);

	return 0;
}

/* Open encoder once the source PCM format is known. Runs on the DP thread. */
static int ffmpeg_enc_configure(struct processing_module *mod, struct sof_source *src)
{
	struct ffmpeg_dec_comp_data *cd = module_get_private_data(mod);
	struct ffmpeg_enc_data *e = cd->backend_data;
	struct comp_dev *dev = mod->dev;
	int container_fmt;
	int expected_bps;
	int ret;

	cd->out_channels = source_get_channels(src);
	cd->out_rate = source_get_rate(src);
	container_fmt = source_get_frm_fmt(src);
	e->src_fmt = source_get_valid_fmt(src);
	e->frame_bytes = source_get_frame_bytes(src);

	switch (container_fmt) {
	case SOF_IPC_FRAME_S16_LE:
		expected_bps = 2;
		break;
	case SOF_IPC_FRAME_S24_4LE:
	case SOF_IPC_FRAME_S32_LE:
		expected_bps = 4;
		break;
	default:
		comp_err(dev, "ffmpeg_enc: unsupported input container format %d",
			 container_fmt);
		return -EINVAL;
	}

	switch (e->src_fmt) {
	case SOF_IPC_FRAME_S16_LE:
		if (expected_bps != 2) {
			comp_err(dev, "ffmpeg_enc: unsupported S16 valid format in %d-byte container",
				 expected_bps);
			return -EINVAL;
		}
		break;
	case SOF_IPC_FRAME_S24_4LE:
	case SOF_IPC_FRAME_S32_LE:
		if (expected_bps != 4) {
			comp_err(dev, "ffmpeg_enc: unsupported valid format %d in %d-byte container",
				 e->src_fmt, expected_bps);
			return -EINVAL;
		}
		break;
	default:
		comp_err(dev, "ffmpeg_enc: unsupported input valid sample format %d",
			 e->src_fmt);
		return -EINVAL;
	}

	if (cd->out_channels == 0 || e->frame_bytes != (int)cd->out_channels * expected_bps) {
		comp_err(dev, "ffmpeg_enc: invalid channels %u / frame_bytes %d for fmt %d",
			 cd->out_channels, e->frame_bytes, e->src_fmt);
		return -EINVAL;
	}

	if ((e->req_rate && e->req_rate != cd->out_rate) ||
	    (e->req_channels && e->req_channels != cd->out_channels)) {
		comp_err(dev, "ffmpeg_enc: requested %u Hz/%u ch does not match source %u Hz/%u ch",
			 e->req_rate, e->req_channels, cd->out_rate, cd->out_channels);
		return -EINVAL;
	}

	ffmpeg_enc_free_staging(mod, e);

	ret = e->codec_ops->configure(mod, e);
	if (ret)
		return ret;

	e->need = (size_t)e->nb * e->frame_bytes;
	e->pcm_in = mod_alloc(mod, e->need);
	e->out_stage = mod_alloc(mod, FFMPEG_ENC_STAGE_SIZE);
	if (!e->pcm_in || !e->out_stage) {
		comp_err(dev, "ffmpeg_enc: staging alloc failed");
		if (e->codec_ops->reset)
			e->codec_ops->reset(mod, e);
		ffmpeg_enc_free_staging(mod, e);
		return -ENOMEM;
	}
	e->out_cap = FFMPEG_ENC_STAGE_SIZE;

	comp_info(dev, "ffmpeg_enc: %s open OK, rate %u ch %u frame_size %d need %zu",
		  e->codec_ops->name, cd->out_rate, cd->out_channels, e->nb, e->need);
	return 0;
}

bool ffmpeg_enc_is_ready_to_process(struct processing_module *mod,
				    struct sof_source **sources, int num_of_sources,
				    struct sof_sink **sinks, int num_of_sinks)
{
	struct ffmpeg_dec_comp_data *cd = module_get_private_data(mod);
	struct ffmpeg_enc_data *e = cd->backend_data;

	if (num_of_sources < 1 || num_of_sinks < 1)
		return false;

	if (!cd->configured)
		return true;

	if (e->out_avail)
		return sink_get_free_size(sinks[0]) > 0;

	return source_get_data_available(sources[0]) > 0;
}

/* Accumulate one encoder frame of input PCM, then invoke codec_ops->encode_frame(). */
int ffmpeg_enc_mod_process(struct processing_module *mod,
			   struct sof_source **sources, int num_of_sources,
			   struct sof_sink **sinks, int num_of_sinks)
{
	struct ffmpeg_dec_comp_data *cd = module_get_private_data(mod);
	struct ffmpeg_enc_data *e = cd->backend_data;
	struct sof_source *src;
	struct sof_sink *sink;
	const void *sp, *sstart;
	size_t sbytes;
	int ret;

	if (num_of_sources < 1 || num_of_sinks < 1)
		return -EINVAL;

	src = sources[0];
	sink = sinks[0];

	ffmpeg_dec_libc_bind(mod);

	/* One-time lazy open on the DP thread; encode on a later cycle. */
	if (!cd->configured) {
		ret = ffmpeg_enc_configure(mod, src);
		if (ret)
			return ret;
		cd->configured = true;
		return 0;
	}

	/* Drain leftover compressed output before consuming more input. */
	if (e->out_avail) {
		ffmpeg_enc_drain(e, sink);
		return 0;
	}

	/* Accumulate source PCM into pcm_in until a full encoder frame is
	 * buffered. The LL->DP input ring holds only a few LL periods (far less
	 * than one encoder frame), so a frame must be gathered over
	 * several DP cycles rather than read in a single source_get_data() call.
	 */
	{
		size_t avail = source_get_data_available(src);
		size_t want = e->need - e->pcm_fill;
		size_t n = ffmpeg_enc_min(want, avail);

		if (n) {
			ret = source_get_data(src, n, &sp, &sstart, &sbytes);
			if (ret)
				return 0;
			ffmpeg_dec_copy_from_circular(e->pcm_in + e->pcm_fill, sp,
						      sstart, sbytes, n);
			source_release_data(src, n);
			e->pcm_fill += n;
		}
	}

	/* Not a full encoder frame yet - wait for more input next cycle. */
	if (e->pcm_fill < e->need)
		return 0;
	e->pcm_fill = 0;

	ret = e->codec_ops->encode_frame(mod, e);
	if (ret)
		return ret;

	/* Drain what fits now; the rest is carried to the next cycle. */
	ffmpeg_enc_drain(e, sink);
	return 0;
}

int ffmpeg_enc_mod_reset(struct processing_module *mod)
{
	struct ffmpeg_dec_comp_data *cd = module_get_private_data(mod);
	struct ffmpeg_enc_data *e = cd->backend_data;

	ffmpeg_dec_libc_bind(mod);
	if (e) {
		if (e->codec_ops && e->codec_ops->reset)
			e->codec_ops->reset(mod, e);
		ffmpeg_enc_free_staging(mod, e);
	}
	cd->configured = false;
	return 0;
}

int ffmpeg_enc_mod_free(struct processing_module *mod)
{
	struct ffmpeg_dec_comp_data *cd = module_get_private_data(mod);
	struct ffmpeg_enc_data *e = cd->backend_data;

	ffmpeg_dec_libc_bind(mod);
	if (!e) {
		mod_free(mod, cd);
		mod->priv.private = NULL;
		return 0;
	}

	if (e->codec_ops && e->codec_ops->free)
		e->codec_ops->free(mod, e);

	ffmpeg_enc_free_staging(mod, e);
	mod_free(mod, e);
	cd->backend_data = NULL;
	mod_free(mod, cd);
	mod->priv.private = NULL;
	return 0;
}
