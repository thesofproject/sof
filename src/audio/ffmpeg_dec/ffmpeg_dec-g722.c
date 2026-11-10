// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.
//
// ITU-T G.722 ADPCM wideband speech per-codec packet sizing for ffmpeg_dec.

#include <sof/audio/module_adapter/module/generic.h>
#include <ipc/compress_params.h>
#include "ffmpeg_dec.h"

#define G722_DEC_MAX_SAMPLES	1024
#define G722_DEC_MAX_PKT_BYTES	(G722_DEC_MAX_SAMPLES / 2)

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

const struct ffmpeg_dec_codec_ops ffmpeg_dec_g722_ops = {
	.id = FFMPEG_DEC_CODEC_G722,
	.snd_codec_id = SND_AUDIOCODEC_G722,
	.name = "g722",
	.decoder_name = "g722",
	.max_frame_samples = ffmpeg_dec_g722_max_frame_samples,
	.limit_packet_size = ffmpeg_dec_g722_limit_packet_size,
};
