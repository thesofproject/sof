// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.
//
// ALAC per-codec container parsing, extradata setup, and frame sizing for ffmpeg_dec.

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/sink_source_utils.h>
#include <sof/audio/source_api.h>
#include <ipc/compress_params.h>
#include <rtos/string.h>
#include <errno.h>
#include <limits.h>
#if !defined(CONFIG_COMP_FFMPEG_DEC_STUB)
#include <libavcodec/version.h>
#endif
#include "ffmpeg_dec.h"

LOG_MODULE_DECLARE(ffmpeg_dec, CONFIG_SOF_LOG_LEVEL);

#define ALAC_EXTRADATA_SIZE		36
#define ALAC_SPECIFIC_CONFIG_SIZE	24
#define ALAC_ATOM_HDR_SIZE		12
#define ALAC_ATOM_TAG_OFFSET		4
#define ALAC_COOKIE_SAMPLES_OFFSET	12
#define ALAC_COOKIE_COMPAT_OFFSET	16
#define ALAC_COOKIE_BITS_OFFSET		17
#define ALAC_COOKIE_PB_OFFSET		18
#define ALAC_COOKIE_MB_OFFSET		19
#define ALAC_COOKIE_KB_OFFSET		20
#define ALAC_COOKIE_CH_OFFSET		21
#define ALAC_COOKIE_MAX_RUN_OFFSET	22
#define ALAC_COOKIE_MAX_FRAME_OFFSET	24
#define ALAC_COOKIE_AVG_BITRATE_OFFSET	28
#define ALAC_COOKIE_SAMPLE_RATE_OFFSET	32
#define ALAC_COOKIE_MIN_SIZE		22

#define ALAC_HEADER_TAG_SIZE		4
#define ALAC_HEADER_PEEK_BYTES		8
#define ALAC_CAF_HDR_SIZE		8
#define ALAC_CAF_CHUNK_HDR_SIZE		12
#define ALAC_CAF_DATA_EDIT_COUNT_SIZE	4
#define ALAC_CAF_DATA_HDR_SIZE		(ALAC_CAF_CHUNK_HDR_SIZE + ALAC_CAF_DATA_EDIT_COUNT_SIZE)
#define ALAC_CAF_MAX_CHUNK_SIZE		0x10000000LL

#define ALAC_DEFAULT_FRAME_SAMPLES	4096
#define ALAC_MAX_FRAME_SAMPLES		16384
#define ALAC_MAX_BITS_PER_SAMPLE	32
#define ALAC_DEFAULT_COMPAT_VERSION	0
#define ALAC_DEFAULT_SAMPLE_SIZE	16
#define ALAC_DEFAULT_PB			40
#define ALAC_DEFAULT_MB			10
#define ALAC_DEFAULT_KB			14
#define ALAC_DEFAULT_CHANNELS		2
#define ALAC_DEFAULT_MAX_RUN		255
#define ALAC_DEFAULT_SAMPLE_RATE	48000

/*
 * Prefix layout of libavcodec's private ALACContext (libavcodec/alac.c):
 *   AVClass *class;
 *   AVCodecContext *avctx;
 *   GetBitContext gb;  -> { const uint8_t *buffer, *buffer_end; int index; ... }
 *
 * ALAC frames are self-delimiting via a 3-bit TYPE_END (7) syntax element and
 * padded to the next byte boundary by flush_put_bits(). Because libavcodec
 * does not ship an AVCodecParser for ALAC, reading gb.index after
 * avcodec_receive_frame() yields the exact bit count of the decoded frame.
 * The _Static_assert below pins the expected libavcodec version so any
 * submodule upgrade forces re-verification of ALACContext's prefix layout.
 */
#if !defined(CONFIG_COMP_FFMPEG_DEC_STUB)
_Static_assert(LIBAVCODEC_VERSION_MAJOR == 61 &&
	       LIBAVCODEC_VERSION_MINOR == 19 &&
	       LIBAVCODEC_VERSION_MICRO == 101,
	       "struct alac_priv_prefix must be verified against ALACContext in libavcodec/alac.c");
#endif

struct alac_priv_prefix {
	const void *av_class;
	void *avctx;
	const uint8_t *gb_buffer;
	const uint8_t *gb_buffer_end;
	int gb_index;
};

static inline uint32_t alac_read_be32(const uint8_t *p)
{
	return ((uint32_t)p[0] << 24) |
	       ((uint32_t)p[1] << 16) |
	       ((uint32_t)p[2] << 8)  |
	       (uint32_t)p[3];
}

static inline uint64_t alac_read_be64(const uint8_t *p)
{
	return ((uint64_t)alac_read_be32(p) << 32) |
	       (uint64_t)alac_read_be32(p + 4);
}

static inline void alac_write_be16(uint8_t *p, uint16_t v)
{
	p[0] = (uint8_t)(v >> 8);
	p[1] = (uint8_t)v;
}

static inline void alac_write_be32(uint8_t *p, uint32_t v)
{
	p[0] = (uint8_t)(v >> 24);
	p[1] = (uint8_t)(v >> 16);
	p[2] = (uint8_t)(v >> 8);
	p[3] = (uint8_t)v;
}

static uint32_t ffmpeg_dec_alac_max_frame_samples(const struct ffmpeg_dec_comp_data *cd)
{
	if (cd->extradata_size >= ALAC_COOKIE_SAMPLES_OFFSET + 4 &&
	    memcmp(&cd->extradata[ALAC_ATOM_TAG_OFFSET], "alac", 4) == 0) {
		uint32_t max_samples =
			alac_read_be32(&cd->extradata[ALAC_COOKIE_SAMPLES_OFFSET]);

		if (max_samples > 0 && max_samples <= ALAC_MAX_FRAME_SAMPLES)
			return max_samples;
	}
	return ALAC_DEFAULT_FRAME_SAMPLES;
}

static size_t ffmpeg_dec_alac_frame_bytes_consumed(const void *priv_data)
{
	const struct alac_priv_prefix *priv = priv_data;
	size_t bytes;

	if (!priv || !priv->gb_buffer || !priv->gb_buffer_end ||
	    priv->gb_buffer_end < priv->gb_buffer || priv->gb_index <= 0)
		return 0;

	bytes = (size_t)((priv->gb_index + 7) >> 3);
	if (bytes > (size_t)(priv->gb_buffer_end - priv->gb_buffer))
		return 0;

	return bytes;
}

/*
 * Strip an optional leading ID3v2 tag, a Core Audio Format ("caff") container
 * header (extracting the 36-byte QuickTime 'alac' magic cookie from the "kuki"
 * chunk and stopping at the start of the "data" chunk payload), or a leading
 * 36-byte QuickTime 'alac' atom prefix emitted by the SOF ALAC encoder.
 */
static int ffmpeg_dec_alac_skip_container(struct processing_module *mod,
					  struct sof_source *src)
{
	struct ffmpeg_dec_comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	const void *sp, *sstart;
	size_t sbytes, avail, req;
	int ret;

	/* Step 1: Drain any pending skip bytes from a previous CAF chunk */
	if (cd->alac_parsing_caf && cd->skip_bytes > 0) {
		avail = source_get_data_available(src);
		if (!avail)
			return -ENODATA;
		size_t to_skip = MIN(avail, cd->skip_bytes);

		source_drop_data(src, to_skip);
		cd->skip_bytes -= to_skip;
		if (cd->skip_bytes > 0)
			return -ENODATA;
	}

	/* Step 2: Parse CAF chunks until the "data" chunk payload is reached */
	while (cd->alac_parsing_caf) {
		uint8_t chunk_hdr[ALAC_CAF_CHUNK_HDR_SIZE];
		int64_t chunk_size;

		avail = source_get_data_available(src);
		if (avail < ALAC_CAF_CHUNK_HDR_SIZE)
			return -ENODATA;

		ret = source_get_data(src, ALAC_CAF_CHUNK_HDR_SIZE, &sp, &sstart, &sbytes);
		if (ret)
			return ret;
		ffmpeg_dec_copy_from_circular(chunk_hdr, sp, sstart, sbytes,
					      ALAC_CAF_CHUNK_HDR_SIZE);
		source_release_data(src, 0);

		chunk_size = (int64_t)alac_read_be64(&chunk_hdr[4]);

		/* "data" chunk: 12-byte header + 4-byte mEditCount, followed by ALAC frames */
		if (memcmp(chunk_hdr, "data", 4) == 0) {
			if (avail < ALAC_CAF_DATA_HDR_SIZE)
				return -ENODATA;

			source_drop_data(src, ALAC_CAF_DATA_HDR_SIZE);
			cd->alac_parsing_caf = false;
			comp_info(dev, "ALAC CAF data chunk reached (size %lld)",
				  (long long)chunk_size);
			return 0;
		}

		if (chunk_size < 0 || chunk_size > ALAC_CAF_MAX_CHUNK_SIZE) {
			comp_err(dev, "invalid CAF chunk %.4s size %lld",
				 (const char *)chunk_hdr, (long long)chunk_size);
			return -EINVAL;
		}

		/* "kuki" (magic cookie) chunk: extract 36-byte QuickTime 'alac' atom */
		if (memcmp(chunk_hdr, "kuki", 4) == 0 &&
		    chunk_size >= ALAC_EXTRADATA_SIZE &&
		    ALAC_CAF_CHUNK_HDR_SIZE + (size_t)chunk_size <= cd->in_buf_size) {
			req = ALAC_CAF_CHUNK_HDR_SIZE + (size_t)chunk_size;
			if (avail < req)
				return -ENODATA;

			ret = source_get_data(src, req, &sp, &sstart, &sbytes);
			if (ret)
				return ret;
			ffmpeg_dec_copy_from_circular(cd->in_buf, sp, sstart, sbytes, req);
			source_release_data(src, 0);

			if (!cd->extradata_size) {
				const uint8_t *cookie = &cd->in_buf[ALAC_CAF_CHUNK_HDR_SIZE];
				size_t cookie_len = (size_t)chunk_size;
				const uint8_t *atom = NULL;

				/* Locate the 36-byte 'alac' atom inside the cookie */
				for (size_t off = 0; off + ALAC_EXTRADATA_SIZE <= cookie_len;
				     off++) {
					if (alac_read_be32(&cookie[off]) == ALAC_EXTRADATA_SIZE &&
					    memcmp(&cookie[off + ALAC_ATOM_TAG_OFFSET],
						   "alac", 4) == 0) {
						atom = &cookie[off];
						break;
					}
				}
				if (!atom)
					atom = cookie + cookie_len - ALAC_EXTRADATA_SIZE;

				ffmpeg_dec_store_extradata(mod, atom, ALAC_EXTRADATA_SIZE);
				comp_info(dev, "extracted 36-byte ALAC cookie from CAF kuki");
			}
			source_drop_data(src, req);
			continue;
		}

		/* Skip other CAF metadata chunks ("desc", "chan", "info", "free", etc.) */
		size_t chunk_total = ALAC_CAF_CHUNK_HDR_SIZE + (size_t)chunk_size;

		if (avail >= chunk_total) {
			source_drop_data(src, chunk_total);
			continue;
		}

		source_drop_data(src, avail);
		cd->skip_bytes = chunk_total - avail;
		return -ENODATA;
	}

	/* Step 3: Skip any leading ID3v2 tag before checking for "caff" or 'alac' atom */
	do {
		ret = ffmpeg_dec_skip_id3v2(mod, src);
		if (ret < 0)
			return ret;
	} while (ret > 0);

	avail = source_get_data_available(src);
	if (avail < ALAC_HEADER_TAG_SIZE)
		return -ENODATA;

	ret = source_get_data(src, ALAC_HEADER_TAG_SIZE, &sp, &sstart, &sbytes);
	if (ret)
		return ret;
	ffmpeg_dec_copy_from_circular(cd->in_buf, sp, sstart, sbytes,
				      ALAC_HEADER_TAG_SIZE);
	source_release_data(src, 0);

	/* Step 4: Check for the 8-byte Core Audio Format ("caff\x00\x01\x00\x00") header */
	if (memcmp(cd->in_buf, "caff", 4) == 0) {
		if (avail < ALAC_CAF_HDR_SIZE)
			return -ENODATA;
		comp_info(dev, "CAF header detected, parsing ALAC container chunks");
		source_drop_data(src, ALAC_CAF_HDR_SIZE);
		cd->alac_parsing_caf = true;
		return ffmpeg_dec_alac_skip_container(mod, src);
	}

	/* Step 5: Check for an inline 36-byte QuickTime 'alac' atom prefix */
	if (alac_read_be32(cd->in_buf) == ALAC_EXTRADATA_SIZE) {
		if (avail < ALAC_HEADER_PEEK_BYTES)
			return -ENODATA;

		ret = source_get_data(src, ALAC_HEADER_PEEK_BYTES, &sp, &sstart, &sbytes);
		if (ret)
			return ret;
		ffmpeg_dec_copy_from_circular(cd->in_buf, sp, sstart, sbytes,
					      ALAC_HEADER_PEEK_BYTES);
		source_release_data(src, 0);

		if (memcmp(&cd->in_buf[ALAC_ATOM_TAG_OFFSET], "alac", 4) == 0) {
			if (avail < ALAC_EXTRADATA_SIZE)
				return -ENODATA;

			ret = source_get_data(src, ALAC_EXTRADATA_SIZE, &sp, &sstart, &sbytes);
			if (ret)
				return ret;
			ffmpeg_dec_copy_from_circular(cd->in_buf, sp, sstart, sbytes,
						      ALAC_EXTRADATA_SIZE);
			source_release_data(src, 0);

			if (!cd->extradata_size) {
				ffmpeg_dec_store_extradata(mod, cd->in_buf, ALAC_EXTRADATA_SIZE);
				comp_info(dev, "extracted inline 36-byte ALAC atom header");
			}
			source_drop_data(src, ALAC_EXTRADATA_SIZE);
			return 0;
		}
	}

	return 0;
}

static void ffmpeg_dec_alac_resize_pcm_buf(struct processing_module *mod)
{
	struct ffmpeg_dec_comp_data *cd = module_get_private_data(mod);
	uint32_t max_samples = ffmpeg_dec_alac_max_frame_samples(cd);
	size_t frame_bytes = cd->out_frame_bytes ?
			     cd->out_frame_bytes :
			     ALAC_DEFAULT_CHANNELS * sizeof(int32_t);
	size_t need = (size_t)max_samples * frame_bytes;

	if (need > cd->pcm_buf_size) {
		uint8_t *new_buf = mod_alloc(mod, need);

		if (new_buf) {
			mod_free(mod, cd->pcm_buf);
			cd->pcm_buf = new_buf;
			cd->pcm_buf_size = need;
		}
	}
}

static void ffmpeg_dec_alac_prepare_extradata(struct processing_module *mod)
{
	struct ffmpeg_dec_comp_data *cd = module_get_private_data(mod);
	uint8_t atom[ALAC_EXTRADATA_SIZE];
	uint32_t frame_length = ALAC_DEFAULT_FRAME_SAMPLES;
	uint8_t compatible_version = ALAC_DEFAULT_COMPAT_VERSION;
	uint8_t sample_size = ALAC_DEFAULT_SAMPLE_SIZE;
	uint8_t pb = ALAC_DEFAULT_PB;
	uint8_t mb = ALAC_DEFAULT_MB;
	uint8_t kb = ALAC_DEFAULT_KB;
	uint8_t channels = cd->out_channels ? (uint8_t)cd->out_channels : ALAC_DEFAULT_CHANNELS;
	uint16_t max_run = ALAC_DEFAULT_MAX_RUN;
	uint32_t max_frame_bytes = 0;
	uint32_t avg_bitrate = 0;
	uint32_t sample_rate = cd->out_rate ? cd->out_rate : ALAC_DEFAULT_SAMPLE_RATE;

	/* Valid 36-byte QuickTime 'alac' atom already present */
	if (cd->extradata_size == ALAC_EXTRADATA_SIZE &&
	    memcmp(&cd->extradata[ALAC_ATOM_TAG_OFFSET], "alac", 4) == 0) {
		ffmpeg_dec_alac_resize_pcm_buf(mod);
		return;
	}

	/* Wrap a raw 24-byte ALACSpecificConfig into the 36-byte QuickTime 'alac' atom */
	if (cd->extradata_size == ALAC_SPECIFIC_CONFIG_SIZE) {
		memset(atom, 0, ALAC_ATOM_HDR_SIZE);
		alac_write_be32(&atom[0], ALAC_EXTRADATA_SIZE);
		memcpy_s(&atom[ALAC_ATOM_TAG_OFFSET], 4, "alac", 4);
		memcpy_s(&atom[ALAC_ATOM_HDR_SIZE], ALAC_SPECIFIC_CONFIG_SIZE,
			 cd->extradata, ALAC_SPECIFIC_CONFIG_SIZE);
		ffmpeg_dec_store_extradata(mod, atom, ALAC_EXTRADATA_SIZE);
		ffmpeg_dec_alac_resize_pcm_buf(mod);
		return;
	}

#if CONFIG_IPC_MAJOR_4
	if (mod->priv.cfg.ext_data &&
	    mod->priv.cfg.ext_data->module_data &&
	    mod->priv.cfg.ext_data->module_data_size >= sizeof(struct snd_codec)) {
		const struct snd_codec *codec_params = mod->priv.cfg.ext_data->module_data;
		const struct snd_dec_alac *alac_opt = &codec_params->options.alac_d;

		if (alac_opt->frame_length > 0 &&
		    alac_opt->frame_length <= ALAC_MAX_FRAME_SAMPLES)
			frame_length = alac_opt->frame_length;
		if (alac_opt->pb > 0)
			pb = alac_opt->pb;
		if (alac_opt->mb > 0)
			mb = alac_opt->mb;
		if (alac_opt->kb > 0)
			kb = alac_opt->kb;
		if (alac_opt->max_run > 0 && alac_opt->max_run <= UINT16_MAX)
			max_run = (uint16_t)alac_opt->max_run;
		if (alac_opt->max_frame_bytes > 0)
			max_frame_bytes = alac_opt->max_frame_bytes;
	}
#endif

	memset(atom, 0, sizeof(atom));
	alac_write_be32(&atom[0], ALAC_EXTRADATA_SIZE);
	memcpy_s(&atom[ALAC_ATOM_TAG_OFFSET], 4, "alac", 4);
	alac_write_be32(&atom[ALAC_COOKIE_SAMPLES_OFFSET], frame_length);
	atom[ALAC_COOKIE_COMPAT_OFFSET] = compatible_version;
	atom[ALAC_COOKIE_BITS_OFFSET] = sample_size;
	atom[ALAC_COOKIE_PB_OFFSET] = pb;
	atom[ALAC_COOKIE_MB_OFFSET] = mb;
	atom[ALAC_COOKIE_KB_OFFSET] = kb;
	atom[ALAC_COOKIE_CH_OFFSET] = channels;
	alac_write_be16(&atom[ALAC_COOKIE_MAX_RUN_OFFSET], max_run);
	alac_write_be32(&atom[ALAC_COOKIE_MAX_FRAME_OFFSET], max_frame_bytes);
	alac_write_be32(&atom[ALAC_COOKIE_AVG_BITRATE_OFFSET], avg_bitrate);
	alac_write_be32(&atom[ALAC_COOKIE_SAMPLE_RATE_OFFSET], sample_rate);

	ffmpeg_dec_store_extradata(mod, atom, ALAC_EXTRADATA_SIZE);
	ffmpeg_dec_alac_resize_pcm_buf(mod);
	comp_info(mod->dev, "synthesized 36-byte ALAC extradata (frame_len=%u rate=%u ch=%u)",
		  frame_length, sample_rate, channels);
}

static int ffmpeg_dec_alac_configure_avctx(struct processing_module *mod, void *avctx)
{
	struct ffmpeg_dec_comp_data *cd = module_get_private_data(mod);
	uint32_t max_ch = cd->out_channels ? cd->out_channels : ALAC_DEFAULT_CHANNELS;
	uint32_t cookie_samples = 0;
	uint8_t cookie_bits = 0;
	uint8_t cookie_ch = 0;

	(void)avctx;

	if (cd->extradata_size >= ALAC_COOKIE_MIN_SIZE &&
	    memcmp(&cd->extradata[ALAC_ATOM_TAG_OFFSET], "alac", 4) == 0) {
		cookie_samples = alac_read_be32(&cd->extradata[ALAC_COOKIE_SAMPLES_OFFSET]);
		cookie_bits = cd->extradata[ALAC_COOKIE_BITS_OFFSET];
		cookie_ch = cd->extradata[ALAC_COOKIE_CH_OFFSET];
	}
	if (cookie_samples == 0 || cookie_samples > ALAC_MAX_FRAME_SAMPLES ||
	    cookie_bits == 0 || cookie_bits > ALAC_MAX_BITS_PER_SAMPLE ||
	    cookie_ch == 0 || cookie_ch > max_ch) {
		comp_err(mod->dev, "unsupported ALAC cookie (samples=%u bits=%u ch=%u)",
			 cookie_samples, cookie_bits, cookie_ch);
		return -EINVAL;
	}

	return 0;
}

static bool ffmpeg_dec_alac_is_trailing_eof(const uint8_t *pkt_data, size_t pkt_len)
{
	return pkt_len >= 4 &&
	       (memcmp(pkt_data, "pakt", 4) == 0 ||
		memcmp(pkt_data, "free", 4) == 0);
}

const struct ffmpeg_dec_codec_ops ffmpeg_dec_alac_ops = {
	.id = FFMPEG_DEC_CODEC_ALAC,
	.snd_codec_id = SND_AUDIOCODEC_ALAC,
	.name = "alac",
	.max_frame_samples = ffmpeg_dec_alac_max_frame_samples,
	.skip_container = ffmpeg_dec_alac_skip_container,
	.prepare_extradata = ffmpeg_dec_alac_prepare_extradata,
	.configure_avctx = ffmpeg_dec_alac_configure_avctx,
	.is_trailing_eof = ffmpeg_dec_alac_is_trailing_eof,
	.frame_bytes_consumed = ffmpeg_dec_alac_frame_bytes_consumed,
};
