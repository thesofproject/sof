// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.
//
// Opus per-codec extradata validation and frame sizing for ffmpeg_dec.

#include <sof/audio/module_adapter/module/generic.h>
#include <ipc/compress_params.h>
#include <rtos/string.h>
#include "ffmpeg_dec.h"

LOG_MODULE_DECLARE(ffmpeg_dec, CONFIG_SOF_LOG_LEVEL);

static uint32_t ffmpeg_dec_opus_max_frame_samples(const struct ffmpeg_dec_comp_data *cd)
{
	(void)cd;
	return 5760;	/* 120 ms @ 48 kHz */
}

static void ffmpeg_dec_opus_prepare_extradata(struct processing_module *mod)
{
	struct ffmpeg_dec_comp_data *cd = module_get_private_data(mod);

	/*
	 * Opus needs an "OpusHead" (RFC 7845) header or empty extradata
	 * (which falls back to the built-in stereo default). If the topology
	 * carries a non-Opus header, ignore it so Opus uses its default.
	 */
	if (cd->extradata_size &&
	    (cd->extradata_size < 8 || memcmp(cd->extradata, "OpusHead", 8) != 0)) {
		comp_info(mod->dev, "ignoring non-Opus extradata from topology");
		cd->extradata_size = 0;
	}
}

const struct ffmpeg_dec_codec_ops ffmpeg_dec_opus_ops = {
	.id = FFMPEG_DEC_CODEC_OPUS,
	.snd_codec_id = SND_AUDIOCODEC_OPUS_RAW,
	.name = "opus",
	.max_frame_samples = ffmpeg_dec_opus_max_frame_samples,
	.prepare_extradata = ffmpeg_dec_opus_prepare_extradata,
};
