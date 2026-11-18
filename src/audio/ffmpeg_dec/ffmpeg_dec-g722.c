// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.
//
// ITU-T G.722 ADPCM wideband speech per-codec packet sizing for ffmpeg_dec.

#include <sof/audio/module_adapter/module/generic.h>
#include <ipc/compress_params.h>
#include <errno.h>
#if !defined(CONFIG_COMP_FFMPEG_DEC_STUB)
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#endif
#include "ffmpeg_dec.h"

LOG_MODULE_DECLARE(ffmpeg_dec, CONFIG_SOF_LOG_LEVEL);

#define G722_DEC_MAX_SAMPLES	1024
#define G722_DEC_MAX_PKT_BYTES	(G722_DEC_MAX_SAMPLES / 2)
#define G722_SAMPLE_RATE	16000
#define G722_BPC_48K		6
#define G722_BPC_56K		7
#define G722_BPC_64K		8

static uint32_t ffmpeg_dec_g722_max_frame_samples(const struct ffmpeg_dec_comp_data *cd)
{
	(void)cd;
	return G722_DEC_MAX_SAMPLES;
}

/*
 * Each G.722 input byte decodes to 2 mono PCM samples. Clamp each unparsed
 * packet chunk to 512 bytes so g722_decode_frame produces at most 1024 samples
 * per decode step.
 */
static size_t ffmpeg_dec_g722_limit_packet_size(const struct ffmpeg_dec_comp_data *cd,
						size_t pkt_len)
{
	(void)cd;
	return pkt_len > G722_DEC_MAX_PKT_BYTES ? G722_DEC_MAX_PKT_BYTES : pkt_len;
}

#if !defined(CONFIG_COMP_FFMPEG_DEC_STUB)
static int ffmpeg_dec_g722_configure_avctx(struct processing_module *mod, void *ctx)
{
	struct ffmpeg_dec_comp_data *cd = module_get_private_data(mod);
	AVCodecContext *avctx = ctx;
	int bpc = G722_BPC_64K;

	if (cd->req_bit_rate == 48000) {
		bpc = G722_BPC_48K;
	} else if (cd->req_bit_rate == 56000) {
		bpc = G722_BPC_56K;
	} else if (cd->req_bit_rate != 0 && cd->req_bit_rate != 64000) {
		comp_err(mod->dev, "unsupported G.722 bitrate %u", cd->req_bit_rate);
		return -EINVAL;
	}

	avctx->sample_rate = G722_SAMPLE_RATE;
	av_opt_set_int(avctx->priv_data, "bits_per_codeword", bpc, 0);
	return 0;
}
#endif

const struct ffmpeg_dec_codec_ops ffmpeg_dec_g722_ops = {
	.id = FFMPEG_DEC_CODEC_G722,
	.snd_codec_id = SND_AUDIOCODEC_G722,
	.name = "g722",
	.decoder_name = "g722",
	.max_frame_samples = ffmpeg_dec_g722_max_frame_samples,
#if !defined(CONFIG_COMP_FFMPEG_DEC_STUB)
	.configure_avctx = ffmpeg_dec_g722_configure_avctx,
#endif
	.limit_packet_size = ffmpeg_dec_g722_limit_packet_size,
};
