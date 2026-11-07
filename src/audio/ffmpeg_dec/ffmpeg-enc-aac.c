// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.
//
// AAC-LC per-codec encoder implementation (vo-aacenc) for ffmpeg_dec.

#include <sof/audio/module_adapter/module/generic.h>
#include <ipc/compress_params.h>
#include <errno.h>
#include <vo-aacenc/voAAC.h>
#include <vo-aacenc/cmnMemory.h>
#include "ffmpeg_dec.h"

LOG_MODULE_DECLARE(ffmpeg_dec, CONFIG_SOF_LOG_LEVEL);

struct ffmpeg_enc_aac_data {
	VO_AUDIO_CODECAPI aac_api;
	VO_HANDLE aac_handle;
	VO_MEM_OPERATOR aac_memop;
	int16_t *pcm_s16;
};

static int ffmpeg_enc_aac_validate_bitrate(struct comp_dev *dev, uint32_t bit_rate,
					   uint32_t sample_rate, uint32_t channels)
{
	if (channels < 1 || channels > 2) {
		comp_err(dev, "unsupported AAC channel count %u", channels);
		return -EINVAL;
	}
	if (bit_rate == 0 ||
	    bit_rate / channels < 4000 ||
	    bit_rate / channels > 160000 ||
	    (sample_rate && (uint64_t)bit_rate > (uint64_t)sample_rate * 6 * channels)) {
		comp_err(dev, "unsupported AAC bit_rate %u for rate %u ch %u",
			 bit_rate, sample_rate, channels);
		return -EINVAL;
	}

	return 0;
}

static int ffmpeg_enc_aac_validate_params(struct processing_module *mod,
					  const struct snd_codec *codec_params)
{
	struct comp_dev *dev = mod->dev;
	uint32_t ch = codec_params->ch_in ? codec_params->ch_in : codec_params->ch_out;

	/* vo-aacenc writes the ADTS MPEG ID bit as 1 (MPEG-2 ADTS) unconditionally,
	 * so only UNDEFINED and MP2ADTS can be honored accurately.
	 */
	if (codec_params->format != SND_AUDIOSTREAMFORMAT_UNDEFINED &&
	    codec_params->format != SND_AUDIOSTREAMFORMAT_MP2ADTS) {
		comp_err(dev, "unsupported AAC stream format 0x%x",
			 codec_params->format);
		return -ENOTSUP;
	}
	if (codec_params->profile != 0 &&
	    codec_params->profile != SND_AUDIOPROFILE_AAC) {
		comp_err(dev, "unsupported AAC profile 0x%x",
			 codec_params->profile);
		return -ENOTSUP;
	}
	if (codec_params->ch_mode != 0 &&
	    codec_params->ch_mode != SND_AUDIOMODE_AAC_LC) {
		comp_err(dev, "unsupported AAC mode 0x%x",
			 codec_params->ch_mode);
		return -ENOTSUP;
	}
	if (codec_params->bit_rate && ch) {
		int ret = ffmpeg_enc_aac_validate_bitrate(dev, codec_params->bit_rate,
							  codec_params->sample_rate, ch);
		if (ret)
			return ret;
	}

	return 0;
}

static int ffmpeg_enc_aac_init(struct processing_module *mod, struct ffmpeg_enc_data *e)
{
	struct comp_dev *dev = mod->dev;
	struct ffmpeg_enc_aac_data *aac;
	VO_CODEC_INIT_USERDATA user_data;
	int ret;

	aac = mod_zalloc(mod, sizeof(*aac));
	if (!aac)
		return -ENOMEM;

	voGetAACEncAPI(&aac->aac_api);
	aac->aac_memop.Alloc = cmnMemAlloc;
	aac->aac_memop.Copy = cmnMemCopy;
	aac->aac_memop.Free = cmnMemFree;
	aac->aac_memop.Set = cmnMemSet;
	aac->aac_memop.Check = cmnMemCheck;
	user_data.memflag = VO_IMF_USERMEMOPERATOR;
	user_data.memData = &aac->aac_memop;

	ret = aac->aac_api.Init(&aac->aac_handle, VO_AUDIO_CodingAAC, &user_data);
	if (ret != VO_ERR_NONE) {
		comp_err(dev, "vo-aacenc Init failed %d", ret);
		mod_free(mod, aac);
		return -EIO;
	}

	e->codec_priv = aac;
	return 0;
}

static int ffmpeg_enc_aac_configure(struct processing_module *mod, struct ffmpeg_enc_data *e)
{
	struct ffmpeg_dec_comp_data *cd = module_get_private_data(mod);
	struct ffmpeg_enc_aac_data *aac = e->codec_priv;
	struct comp_dev *dev = mod->dev;
	VO_CODEC_INIT_USERDATA user_data;
	AACENC_PARAM params = { 0 };
	int ret;

	if (aac->pcm_s16) {
		mod_free(mod, aac->pcm_s16);
		aac->pcm_s16 = NULL;
	}

	ret = ffmpeg_enc_aac_validate_bitrate(dev, e->bit_rate, cd->out_rate,
					      cd->out_channels);
	if (ret)
		return ret;

	/* Recreate the vo-aacenc handle before each new stream so that
	 * transform delay buffers and block-switching history from any
	 * previous stream do not leak across reset().
	 */
	if (aac->aac_handle) {
		aac->aac_api.Uninit(aac->aac_handle);
		aac->aac_handle = NULL;
	}
	user_data.memflag = VO_IMF_USERMEMOPERATOR;
	user_data.memData = &aac->aac_memop;
	ret = aac->aac_api.Init(&aac->aac_handle, VO_AUDIO_CodingAAC, &user_data);
	if (ret != VO_ERR_NONE) {
		comp_err(dev, "vo-aacenc Init failed %d", ret);
		return -EIO;
	}

	params.sampleRate = cd->out_rate;
	params.bitRate = e->bit_rate;
	params.nChannels = cd->out_channels;
	params.adtsUsed = 1;
	ret = aac->aac_api.SetParam(aac->aac_handle, VO_PID_AAC_ENCPARAM, &params);
	if (ret != VO_ERR_NONE) {
		comp_err(dev, "vo-aacenc SetParam failed %d", ret);
		return -EINVAL;
	}

	e->nb = 1024;
	aac->pcm_s16 = mod_alloc(mod, (size_t)e->nb * cd->out_channels * sizeof(int16_t));
	if (!aac->pcm_s16) {
		comp_err(dev, "ffmpeg_enc: AAC pcm_s16 alloc failed");
		return -ENOMEM;
	}

	return 0;
}

static int ffmpeg_enc_aac_encode_frame(struct processing_module *mod, struct ffmpeg_enc_data *e)
{
	struct ffmpeg_dec_comp_data *cd = module_get_private_data(mod);
	struct ffmpeg_enc_aac_data *aac = e->codec_priv;
	struct comp_dev *dev = mod->dev;
	VO_CODECBUFFER input = { 0 }, output = { 0 };
	VO_AUDIO_OUTPUTINFO output_info = { 0 };
	uint8_t outbuf[2048];
	size_t total_samples = (size_t)e->nb * cd->out_channels;
	int ret;

	ffmpeg_enc_pcm_to_s16_interleaved(e->pcm_in, aac->pcm_s16,
					  total_samples, e->src_fmt);

	input.Buffer = (uint8_t *)aac->pcm_s16;
	input.Length = total_samples * sizeof(int16_t);
	aac->aac_api.SetInputData(aac->aac_handle, &input);

	output.Buffer = outbuf;
	output.Length = sizeof(outbuf);
	ret = aac->aac_api.GetOutputData(aac->aac_handle, &output, &output_info);
	if (ret != VO_ERR_NONE) {
		comp_err(dev, "vo-aacenc GetOutputData failed %d", ret);
		return -EIO;
	}

	if (output.Length > 0)
		ffmpeg_enc_stage_packet(mod, e, outbuf, output.Length);

	return 0;
}

static void ffmpeg_enc_aac_reset(struct processing_module *mod, struct ffmpeg_enc_data *e)
{
	struct ffmpeg_enc_aac_data *aac = e->codec_priv;

	if (!aac)
		return;

	if (aac->pcm_s16) {
		mod_free(mod, aac->pcm_s16);
		aac->pcm_s16 = NULL;
	}
	if (aac->aac_handle) {
		aac->aac_api.Uninit(aac->aac_handle);
		aac->aac_handle = NULL;
	}
}

static void ffmpeg_enc_aac_free(struct processing_module *mod, struct ffmpeg_enc_data *e)
{
	struct ffmpeg_enc_aac_data *aac = e->codec_priv;

	if (!aac)
		return;

	ffmpeg_enc_aac_reset(mod, e);
	mod_free(mod, aac);
	e->codec_priv = NULL;
}

const struct ffmpeg_enc_codec_ops ffmpeg_enc_aac_ops = {
	.id = FFMPEG_DEC_CODEC_AAC,
	.snd_codec_id = SND_AUDIOCODEC_AAC,
	.name = "aac",
	.validate_params = ffmpeg_enc_aac_validate_params,
	.init = ffmpeg_enc_aac_init,
	.configure = ffmpeg_enc_aac_configure,
	.encode_frame = ffmpeg_enc_aac_encode_frame,
	.reset = ffmpeg_enc_aac_reset,
	.free = ffmpeg_enc_aac_free,
};
