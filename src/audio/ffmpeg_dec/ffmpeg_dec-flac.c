// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.
//
// FLAC per-codec container parsing and frame sizing for ffmpeg_dec.

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/sink_source_utils.h>
#include <sof/audio/source_api.h>
#include <ipc/compress_params.h>
#include <rtos/string.h>
#include <errno.h>
#include "ffmpeg_dec.h"

LOG_MODULE_DECLARE(ffmpeg_dec, CONFIG_SOF_LOG_LEVEL);

static uint32_t ffmpeg_dec_flac_max_frame_samples(const struct ffmpeg_dec_comp_data *cd)
{
	if (cd->extradata_size >= 4) {
		const uint8_t *ed = cd->extradata;
		uint32_t max_bs = ((uint32_t)ed[2] << 8) | (uint32_t)ed[3];

		if (max_bs > 0)
			return max_bs;
	}
	return 8192;	/* standard max FLAC block (subset) */
}

/*
 * Strip an optional leading ID3v2 tag, the 4-byte "fLaC" container marker, and
 * all FLAC metadata blocks so the libavcodec parser receives raw FLAC frames.
 * When the 34-byte STREAMINFO block (type 0) is encountered, dynamically store
 * it as codec extradata if none was provided via set_configuration().
 */
static int ffmpeg_dec_flac_skip_container(struct processing_module *mod,
					  struct sof_source *src)
{
	struct ffmpeg_dec_comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	const void *sp, *sstart;
	size_t sbytes, avail, req;
	int ret;

	/* Step 1: Drain any pending skip bytes from a previous FLAC metadata block */
	if (cd->flac_parsing_meta && cd->skip_bytes > 0) {
		avail = source_get_data_available(src);
		if (!avail)
			return -ENODATA;
		size_t to_skip = MIN(avail, cd->skip_bytes);

		source_drop_data(src, to_skip);
		cd->skip_bytes -= to_skip;
		if (cd->skip_bytes > 0)
			return -ENODATA;

		if (cd->flac_last_block) {
			cd->flac_parsing_meta = false;
			return 0;
		}
	}

	/* Step 2: Parse ongoing FLAC metadata blocks across chunks */
	while (cd->flac_parsing_meta) {
		uint8_t hdr_bytes[4];
		uint8_t hdr;
		uint32_t len;
		bool is_last;

		avail = source_get_data_available(src);
		if (avail < 4)
			return -ENODATA;

		ret = source_get_data(src, 4, &sp, &sstart, &sbytes);
		if (ret)
			return ret;
		ffmpeg_dec_copy_from_circular(hdr_bytes, sp, sstart, sbytes, 4);
		source_release_data(src, 0);

		hdr = hdr_bytes[0];
		len = ((uint32_t)hdr_bytes[1] << 16) |
		      ((uint32_t)hdr_bytes[2] << 8) |
		      (uint32_t)hdr_bytes[3];
		is_last = (hdr & 0x80) != 0;

		/* STREAMINFO block (type 0, 34 bytes) */
		if ((hdr & 0x7F) == 0 && len == 34) {
			if (avail < 38)
				return -ENODATA;

			req = 38;
			ret = source_get_data(src, req, &sp, &sstart, &sbytes);
			if (ret)
				return ret;
			ffmpeg_dec_copy_from_circular(cd->in_buf, sp, sstart, sbytes, req);
			source_release_data(src, 0);

			if (!cd->extradata_size) {
				ffmpeg_dec_store_extradata(mod, &cd->in_buf[4], 34);
				comp_info(dev, "dynamically extracted 34-byte FLAC STREAMINFO");
			}
			source_drop_data(src, 38);
			if (is_last) {
				cd->flac_parsing_meta = false;
				return 0;
			}
			continue;
		}

		/* Other metadata block (PADDING, PICTURE, VORBIS_COMMENT, etc.): skip 4 + len */
		uint32_t block_total = 4 + len;

		if (avail >= block_total) {
			source_drop_data(src, block_total);
			if (is_last) {
				cd->flac_parsing_meta = false;
				return 0;
			}
			continue;
		}

		/* Block exceeds available data in this period: skip available and track remainder */
		source_drop_data(src, avail);
		cd->skip_bytes = block_total - avail;
		cd->flac_last_block = is_last;
		return -ENODATA;
	}

	/* Step 3: Skip any leading ID3v2 tag before checking for the "fLaC" marker */
	do {
		ret = ffmpeg_dec_skip_id3v2(mod, src);
		if (ret < 0)
			return ret;
	} while (ret > 0);

	/* Step 4: Check for the 4-byte "fLaC" container marker */
	if (memcmp(cd->in_buf, "fLaC", 4) == 0) {
		comp_info(dev, "FLAC marker detected, consuming container header");
		source_drop_data(src, 4);
		cd->flac_parsing_meta = true;
		return ffmpeg_dec_flac_skip_container(mod, src);
	}

	/* No container marker: raw FLAC elementary stream */
	return 0;
}

const struct ffmpeg_dec_codec_ops ffmpeg_dec_flac_ops = {
	.id = FFMPEG_DEC_CODEC_FLAC,
	.snd_codec_id = SND_AUDIOCODEC_FLAC,
	.name = "flac",
	.max_frame_samples = ffmpeg_dec_flac_max_frame_samples,
	.skip_container = ffmpeg_dec_flac_skip_container,
};
