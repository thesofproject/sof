// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2026 Intel Corporation.
 *
 * ITU-T G.726 ADPCM per-codec packet sizing and configuration for ffmpeg_dec.
 */

#include <sof/audio/module_adapter/module/generic.h>
#include <ipc/compress_params.h>
#include <errno.h>
#if !defined(CONFIG_COMP_FFMPEG_DEC_STUB)
#include <libavcodec/avcodec.h>
#include <libavutil/channel_layout.h>
#endif
#include "ffmpeg_dec.h"

LOG_MODULE_DECLARE(ffmpeg_dec, CONFIG_SOF_LOG_LEVEL);

#define G726_DEFAULT_RATE	8000
#define G726_DEFAULT_CODE_SIZE	4
#define G726_DEC_MAX_SAMPLES	1024

static int ffmpeg_dec_g726_code_size(const struct ffmpeg_dec_comp_data *cd, uint32_t rate)
{
	uint32_t cs;

	if (!cd->req_bit_rate) {
		return G726_DEFAULT_CODE_SIZE;
	}

	cs = (cd->req_bit_rate + rate / 2) / rate;
	if (cs < 2 || cs > 5 || cs * rate != cd->req_bit_rate) {
		return -EINVAL;
	}

	return (int)cs;
}

static uint32_t ffmpeg_dec_g726_max_frame_samples(const struct ffmpeg_dec_comp_data *cd)
{
	(void)cd;
	return G726_DEC_MAX_SAMPLES;
}

/*
 * Align unparsed G.726 chunks to whole byte-aligned code blocks (1 byte for
 * 2/4-bit codes, 3 bytes for 3-bit codes, 5 bytes for 5-bit codes) and cap
 * packet length so decoded output stays within G726_DEC_MAX_SAMPLES.
 */
static size_t ffmpeg_dec_g726_limit_packet_size(const struct ffmpeg_dec_comp_data *cd,
						size_t pkt_len)
{
	uint32_t rate = cd->req_rate ? cd->req_rate :
			(cd->out_rate ? cd->out_rate : G726_DEFAULT_RATE);
	int cs = ffmpeg_dec_g726_code_size(cd, rate);
	size_t align;
	size_t max_bytes;
	size_t clamped;

	if (cs < 2) {
		cs = G726_DEFAULT_CODE_SIZE;
	}

	align = (cs == 3 || cs == 5) ? (size_t)cs : 1;
	max_bytes = (G726_DEC_MAX_SAMPLES * (size_t)cs) / 8;
	clamped = pkt_len > max_bytes ? max_bytes : pkt_len;

	return clamped - (clamped % align);
}

#if !defined(CONFIG_COMP_FFMPEG_DEC_STUB)
static int ffmpeg_dec_g726_configure_avctx(struct processing_module *mod, void *ctx)
{
	struct ffmpeg_dec_comp_data *cd = module_get_private_data(mod);
	AVCodecContext *avctx = ctx;
	uint32_t rate = cd->req_rate ? cd->req_rate :
			(cd->out_rate ? cd->out_rate : G726_DEFAULT_RATE);
	int cs;

	if (cd->req_profile > 1) {
		comp_err(mod->dev, "unsupported G.726 profile %u", cd->req_profile);
		return -EINVAL;
	}

	cs = ffmpeg_dec_g726_code_size(cd, rate);
	if (cs < 0) {
		comp_err(mod->dev, "unsupported G.726 bitrate %u at %u Hz",
			 cd->req_bit_rate, rate);
		return cs;
	}

	avctx->sample_rate = rate;
	av_channel_layout_default(&avctx->ch_layout, 1);
	avctx->bits_per_coded_sample = cs;
	return 0;
}
#endif

const struct ffmpeg_dec_codec_ops ffmpeg_dec_g726_ops = {
	.id = FFMPEG_DEC_CODEC_G726,
	.snd_codec_id = SND_AUDIOCODEC_G726,
	.name = "g726",
	.max_frame_samples = ffmpeg_dec_g726_max_frame_samples,
#if !defined(CONFIG_COMP_FFMPEG_DEC_STUB)
	.configure_avctx = ffmpeg_dec_g726_configure_avctx,
#endif
	.limit_packet_size = ffmpeg_dec_g726_limit_packet_size,
};
