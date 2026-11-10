// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.
//
// Bluetooth SBC / mSBC per-codec frame sizing for ffmpeg_dec.

#include <sof/audio/module_adapter/module/generic.h>
#include <ipc/compress_params.h>
#include "ffmpeg_dec.h"

/*
 * Standard A2DP SBC frames hold at most 8 subbands x 16 blocks = 128 samples;
 * Bluetooth HFP Wideband Speech mSBC frames hold 8 subbands x 15 blocks = 120
 * samples. Size the output staging buffer to 1024 samples so several frames
 * fit comfortably per DP cycle.
 */
static uint32_t ffmpeg_dec_sbc_max_frame_samples(const struct ffmpeg_dec_comp_data *cd)
{
	(void)cd;
	return 1024;
}

const struct ffmpeg_dec_codec_ops ffmpeg_dec_sbc_ops = {
	.id = FFMPEG_DEC_CODEC_SBC,
	.snd_codec_id = SND_AUDIOCODEC_SBC,
	.name = "sbc",
	.decoder_name = "sbc",
	.max_frame_samples = ffmpeg_dec_sbc_max_frame_samples,
};
