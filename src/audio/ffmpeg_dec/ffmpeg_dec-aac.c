// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.
//
// AAC per-codec container parsing and frame sizing for ffmpeg_dec.

#include <sof/audio/module_adapter/module/generic.h>
#include <ipc/compress_params.h>
#include "ffmpeg_dec.h"

static uint32_t ffmpeg_dec_aac_max_frame_samples(const struct ffmpeg_dec_comp_data *cd)
{
	(void)cd;
	return 2048;	/* HE-AAC (SBR) */
}

static int ffmpeg_dec_aac_skip_container(struct processing_module *mod,
					 struct sof_source *src)
{
	int ret;

	do {
		ret = ffmpeg_dec_skip_id3v2(mod, src);
	} while (ret > 0);

	return ret;
}

const struct ffmpeg_dec_codec_ops ffmpeg_dec_aac_ops = {
	.id = FFMPEG_DEC_CODEC_AAC,
	.snd_codec_id = SND_AUDIOCODEC_AAC,
	.name = "aac",
	.decoder_name = "aac_fixed",
	.max_frame_samples = ffmpeg_dec_aac_max_frame_samples,
	.skip_container = ffmpeg_dec_aac_skip_container,
};
