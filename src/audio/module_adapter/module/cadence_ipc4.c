// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2025-2026 Intel Corporation. All rights reserved.
//

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/module_adapter/module/cadence.h>
#include <sof/audio/cadence/mp3_dec/xa_mp3_dec_api.h>
#include <sof/audio/cadence/mp3_enc/xa_mp3_enc_api.h>
#include <sof/audio/cadence/aac_dec/xa_aac_dec_api.h>
#include <sof/schedule/ll_schedule_domain.h>
#include <ipc/compress_params.h>
#include <rtos/init.h>

SOF_DEFINE_REG_UUID(cadence_codec);
LOG_MODULE_DECLARE(cadence_codec, CONFIG_SOF_LOG_LEVEL);
DECLARE_TR_CTX(cadence_codec_tr, SOF_UUID(cadence_codec_uuid), LOG_LEVEL_INFO);

int cadence_codec_resolve_api(struct processing_module *mod)
{
	struct cadence_codec_data *cd = module_get_private_data(mod);
	struct module_config *setup_cfg = &cd->setup_cfg;
	struct snd_codec *codec_params;
	uint32_t codec_id = DEFAULT_CODEC_ID;

	/* update codec_id if setup_cfg is available */
	if (setup_cfg->avail) {
		codec_params = (struct snd_codec *)cd->setup_cfg.data;
		codec_id = codec_params->id;
	}

	return cadence_codec_resolve_api_with_id(mod, codec_id, cd->direction);
}

static int cadence_configure_mp3_dec_params(struct processing_module *mod)
{
	struct cadence_codec_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	int word_size;
	int ret;

	/* Cadence module only supports 16bit or 24 bits for word size */
	switch (cd->base_cfg.audio_fmt.depth) {
	case IPC4_DEPTH_16BIT:
		word_size = 16;
		break;
	case IPC4_DEPTH_24BIT:
	case IPC4_DEPTH_32BIT:
		word_size = 24;
		break;
	default:
		comp_err(dev, "Unsupported bit depth: %d", cd->base_cfg.audio_fmt.depth);
		return -EINVAL;
	}

	API_CALL(cd, XA_API_CMD_SET_CONFIG_PARAM, XA_MP3DEC_CONFIG_PARAM_PCM_WDSZ,
		 (void *)&word_size, ret);
	if (ret != LIB_NO_ERROR) {
		if (LIB_IS_FATAL_ERROR(ret)) {
			comp_err(dev, "failed to apply config param word size: error: %#x", ret);
			return ret;
		}
		comp_warn(dev, "applied param word size return code: %#x", ret);
	}

	return 0;
}

static int cadence_configure_mp3_enc_params(struct processing_module *mod)
{
	struct cadence_codec_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	int word_size = 16;
	int ret;

	/*
	 * Cadence encoder only supports 16-bit word size. Make sure the topology is set up
	 * correctly
	 */
	switch (cd->base_cfg.audio_fmt.depth) {
	case IPC4_DEPTH_24BIT:
	case IPC4_DEPTH_32BIT:
		comp_err(dev, "Unsupported bit depth: %d for MP3 encoder",
			 cd->base_cfg.audio_fmt.depth);
		return -EINVAL;
	default:
		break;
	}

	API_CALL(cd, XA_API_CMD_SET_CONFIG_PARAM, XA_MP3ENC_CONFIG_PARAM_PCM_WDSZ,
		 (void *)&word_size, ret);
	if (ret != LIB_NO_ERROR) {
		if (LIB_IS_FATAL_ERROR(ret)) {
			comp_err(dev, "failed to apply config param word size: error: %#x", ret);
			return ret;
		}
		comp_warn(dev, "applied param word size return code: %#x", ret);
	}

	int num_channels = cd->base_cfg.audio_fmt.channels_count;

	API_CALL(cd, XA_API_CMD_SET_CONFIG_PARAM, XA_MP3ENC_CONFIG_PARAM_NUM_CHANNELS,
		 (void *)&num_channels, ret);
	if (ret != LIB_NO_ERROR) {
		if (LIB_IS_FATAL_ERROR(ret)) {
			comp_err(dev, "failed to apply config num_channels: error: %#x", ret);
			return ret;
		}
	}

	int sampling_freq = cd->base_cfg.audio_fmt.sampling_frequency;

	API_CALL(cd, XA_API_CMD_SET_CONFIG_PARAM, XA_MP3ENC_CONFIG_PARAM_SAMP_FREQ,
		 (void *)&sampling_freq, ret);
	if (ret != LIB_NO_ERROR) {
		if (LIB_IS_FATAL_ERROR(ret)) {
			comp_err(dev, "failed to apply config sampling_frequency: error: %#x", ret);
			return ret;
		}
	}

	int bitrate = CADENCE_MP3_ENCODER_DEFAULT_BITRATE;

	API_CALL(cd, XA_API_CMD_SET_CONFIG_PARAM, XA_MP3ENC_CONFIG_PARAM_BITRATE,
		 (void *)&bitrate, ret);
	if (ret != LIB_NO_ERROR) {
		if (LIB_IS_FATAL_ERROR(ret)) {
			comp_err(dev, "failed to apply config bitrate: error: %#x", ret);
			return ret;
		}
	}

	return 0;
}

static int cadence_configure_aac_dec_params(struct processing_module *mod)
{
	struct cadence_codec_data *cd = module_get_private_data(mod);
	struct module_config *setup_cfg = &cd->setup_cfg;
	struct comp_dev *dev = mod->dev;
	struct snd_codec *codec_params;
	int bitstream_format = XA_AACDEC_EBITSTREAM_TYPE_AAC_ADTS;
	int word_size;
	int ret;

	/* check bitstream format. Only MPEG-4 ADTS supported for now */
	if (setup_cfg->avail) {
		codec_params = (struct snd_codec *)cd->setup_cfg.data;
		if (codec_params->format != SND_AUDIOSTREAMFORMAT_MP4ADTS) {
			comp_err(dev, "Unsupported AAC format: %d", codec_params->format);
			return -EINVAL;
		}
	} else {
		comp_err(dev, "No setup config available for AAC decoder");
		return -EINVAL;
	}

	/* AAC decoder module only supports 16bit or 24 bits for word size */
	switch (cd->base_cfg.audio_fmt.depth) {
	case IPC4_DEPTH_16BIT:
		word_size = 16;
		break;
	case IPC4_DEPTH_24BIT:
	case IPC4_DEPTH_32BIT:
		word_size = 24;
		break;
	default:
		comp_err(dev, "Unsupported bit depth: %d", cd->base_cfg.audio_fmt.depth);
		return -EINVAL;
	}

	API_CALL(cd, XA_API_CMD_SET_CONFIG_PARAM, XA_AACDEC_CONFIG_PARAM_PCM_WDSZ,
		 (void *)&word_size, ret);
	if (ret != LIB_NO_ERROR) {
		if (LIB_IS_FATAL_ERROR(ret)) {
			comp_err(dev, "failed to apply config param word size: error: %#x", ret);
			return ret;
		}
		comp_warn(dev, "applied param word size return code: %#x", ret);
	}

	API_CALL(cd, XA_API_CMD_SET_CONFIG_PARAM, XA_AACDEC_CONFIG_PARAM_EXTERNALBSFORMAT,
		 (void *)&bitstream_format, ret);
	if (ret != LIB_NO_ERROR) {
		if (LIB_IS_FATAL_ERROR(ret)) {
			comp_err(dev, "failed to apply config param bitstream format: error: %#x",
				 ret);
			return ret;
		}
		comp_warn(dev, "applied param bitstream format return code: %#x", ret);
	}

	return 0;
}

static int cadence_configure_codec_params(struct processing_module *mod)
{
	struct cadence_codec_data *cd = module_get_private_data(mod);

	switch (cd->api_id) {
	case CADENCE_CODEC_MP3_DEC_ID:
		return cadence_configure_mp3_dec_params(mod);
	case CADENCE_CODEC_MP3_ENC_ID:
		return cadence_configure_mp3_enc_params(mod);
	case CADENCE_CODEC_AAC_DEC_ID:
		return cadence_configure_aac_dec_params(mod);
	default:
		break;
	}

	comp_err(mod->dev, "Unsupported codec API ID: %u", cd->api_id);
	return -EINVAL;
}

static int cadence_codec_init(struct processing_module *mod)
{
	struct module_data *codec = &mod->priv;
	struct module_config *cfg = &codec->cfg;
	struct module_ext_init_data *ext_data = cfg->ext_data;
	struct cadence_codec_data *cd;
	struct module_config *setup_cfg;
	struct comp_dev *dev = mod->dev;
	int mem_tabs_size;
	int ret;

	comp_dbg(dev, "cadence_codec_init() start");

	cd = mod_zalloc(mod, sizeof(struct cadence_codec_data));
	if (!cd) {
		comp_err(dev, "failed to allocate memory for cadence codec data");
		return -ENOMEM;
	}

	codec->private = cd;
	memcpy_s(&cd->base_cfg, sizeof(cd->base_cfg), &cfg->base_cfg, sizeof(cd->base_cfg));

	codec->mpd.init_done = 0;

	/* copy the setup config only for the first init */
	if (codec->state == MODULE_DISABLED && ext_data->module_data_size > 0) {
		int size = ext_data->module_data_size;
		uint8_t *init_bytes;

		setup_cfg = &cd->setup_cfg;

		/* allocate memory for set up config (codec params) */
		setup_cfg->data = mod_alloc(mod, size);
		if (!setup_cfg->data) {
			comp_err(dev, "failed to alloc setup config");
			ret = -ENOMEM;
			goto free_cd;
		}

		setup_cfg->size = size;
		ret = memcpy_s(setup_cfg->data, size, ext_data->module_data, size);
		if (ret) {
			comp_err(dev, "failed to copy setup config %d", ret);
			goto free_cfg;
		}
		setup_cfg->avail = true;
		codec->cfg.avail = false;

		/* direction follows the codec params in init data */
		init_bytes = (uint8_t *)ext_data->module_data;
		cd->direction = *(uint32_t *)(init_bytes + sizeof(struct snd_codec));

		comp_info(dev, "codec direction set to %u", cd->direction);
	}

	ret = cadence_init_codec_object(mod);
	if (ret)
		goto free_cfg;

	ret = cadence_configure_codec_params(mod);
	if (ret)
		goto free_cfg;

	/* Allocate memory for the codec */
	API_CALL(cd, XA_API_CMD_GET_MEMTABS_SIZE, 0, &mem_tabs_size, ret);
	if (ret != LIB_NO_ERROR) {
		comp_err(dev, "error %x: failed to get memtabs size", ret);
		goto free_cfg;
	}

	cd->mem_tabs = mod_alloc(mod, mem_tabs_size);
	if (!cd->mem_tabs) {
		comp_err(dev, "failed to allocate space for memtabs");
		goto free_cfg;
	}

	comp_dbg(dev, "allocated %d bytes for memtabs", mem_tabs_size);

	API_CALL(cd, XA_API_CMD_SET_MEMTABS_PTR, 0, cd->mem_tabs, ret);
	if (ret != LIB_NO_ERROR) {
		comp_err(dev, "error %x: failed to set memtabs", ret);
		goto free;
	}

	ret = cadence_codec_init_memory_tables(mod);
	if (ret != LIB_NO_ERROR) {
		comp_err(dev, "error %x: failed to init memory tables", ret);
		goto free;
	}

	comp_dbg(dev, "cadence_codec_init() done");

	return 0;
free:
	mod_free(mod, cd->mem_tabs);
free_cfg:
	mod_free(mod, setup_cfg->data);
free_cd:
	mod_free(mod, cd);

	return ret;
}

int cadence_codec_apply_config(struct processing_module *mod)
{
	int size;
	struct module_config *cfg;
	void *data;
	struct comp_dev *dev = mod->dev;
	struct module_data *codec = &mod->priv;

	cfg = &codec->cfg;

	/* this will be true during prepare if there's no config available after init */
	if (!cfg->avail)
		return 0;

	data = cfg->data;
	size = cfg->size;

	if (!size) {
		comp_err(dev, "error: no data available in config to apply");
		return -EIO;
	}

	return cadence_codec_apply_params(mod, size, data);
}

static int cadence_codec_prepare(struct processing_module *mod,
				 struct sof_source **sources, int num_of_sources,
				 struct sof_sink **sinks, int num_of_sinks)
{
	int ret = 0;
	struct comp_dev *dev = mod->dev;
	struct module_data *codec = &mod->priv;

	comp_dbg(dev, "cadence_codec_prepare() start");

	ret = cadence_codec_apply_config(mod);
	if (ret) {
		comp_err(dev, "failed to apply config error %x:", ret);
		return ret;
	}

	/* Check init done status. Note, it may happen that init_done flag will return
	 * false value, this is normal since some codec variants needs input in order to
	 * fully finish initialization. That's why at codec_adapter_copy() we call
	 * codec_init_process() base on result obtained below.
	 */
#ifdef CONFIG_CADENCE_CODEC_WRAPPER
	/* TODO: remove the "#ifdef CONFIG_CADENCE_CODEC_WRAPPER" once cadence fixes the bug
	 * in the init/prepare sequence. Basically below API_CALL shall return 1 for
	 * PCM streams and 0 for compress ones. As it turns out currently it returns 1
	 * in both cases so in turn compress stream won't finish its prepare during first copy
	 * in codec_adapter_copy().
	 */
	API_CALL(cd, XA_API_CMD_INIT, XA_CMD_TYPE_INIT_DONE_QUERY,
		 &codec->mpd.init_done, ret);
	if (ret != LIB_NO_ERROR) {
		comp_err(dev, "failed to get lib init status error %x:", ret);
		return ret;
	}
#endif

	/* set the period based on the minimum required input data size */
	dev->period = 1000000ULL * codec->mpd.in_buff_size /
			      (source_get_frame_bytes(sources[0]) * source_get_rate(sources[0]));
	comp_dbg(dev, "period set to %u usec", dev->period);

	/* align down period to LL cycle time */
	dev->period /= LL_TIMER_PERIOD_US;
	dev->period *= LL_TIMER_PERIOD_US;

	comp_dbg(dev, "cadence_codec_prepare() done");
	return 0;
}

static void cadence_copy_data_from_buffer(void *dest, const void *buffer_ptr, size_t bytes_to_copy,
					  size_t buffer_size, uint8_t const *buffer_start)
{
	size_t bytes_to_end = (size_t)((uint8_t *)buffer_start +
				       buffer_size - (uint8_t *)buffer_ptr);

	if (bytes_to_end >= bytes_to_copy) {
		/* No wrap, copy directly */
		memcpy_s(dest, bytes_to_copy, buffer_ptr, bytes_to_copy);
		return;
	}

	/* Wrap occurs, copy in two parts */
	memcpy_s(dest, bytes_to_end, buffer_ptr, bytes_to_end);
	memcpy_s((uint8_t *)dest + bytes_to_end, bytes_to_copy - bytes_to_end,
		 buffer_start, bytes_to_copy - bytes_to_end);
}

static int cadence_codec_process(struct processing_module *mod, struct sof_source **sources,
				 int num_of_sources, struct sof_sink **sinks, int num_of_sinks)
{
	struct comp_dev *dev = mod->dev;
	struct module_data *codec = &mod->priv;
	size_t in_size = source_get_data_available(sources[0]);
	size_t out_space = sink_get_free_size(sinks[0]);
	uint8_t const *source_buffer_start;
	int consumed_during_init = 0;
	uint32_t remaining = in_size;
	const void *src_ptr;
	size_t src_bytes;
	int ret;

	if (!codec->mpd.init_done) {
		/* Acquire data from the source buffer */
		ret = source_get_data(sources[0], codec->mpd.in_buff_size, &src_ptr,
				      (const void **)&source_buffer_start, &src_bytes);
		if (ret) {
			comp_err(dev, "cannot get data from source buffer");
			return ret;
		}

		cadence_copy_data_from_buffer(codec->mpd.in_buff, src_ptr, codec->mpd.in_buff_size,
					      src_bytes, source_buffer_start);

		codec->mpd.avail = codec->mpd.in_buff_size;
		ret = cadence_codec_init_process(mod);
		if (ret)
			return ret;

		remaining -= codec->mpd.consumed;
		source_release_data(sources[0], codec->mpd.consumed);
		consumed_during_init = codec->mpd.consumed;
	}

	codec->mpd.consumed = 0;

	/* Proceed only if we have enough data to fill the module buffer completely */
	if (remaining < codec->mpd.in_buff_size)
		return -ENODATA;

	/* Acquire data from the source buffer */
	ret = source_get_data(sources[0], codec->mpd.in_buff_size, &src_ptr,
			      (const void **)&source_buffer_start, &src_bytes);

	cadence_copy_data_from_buffer(codec->mpd.in_buff, src_ptr, codec->mpd.in_buff_size,
				      src_bytes, source_buffer_start);
	codec->mpd.avail = codec->mpd.in_buff_size;

	comp_dbg(dev, "cadence_codec_process() start");

	ret = cadence_codec_process_data(mod);
	if (ret) {
		source_release_data(sources[0], 0);
		return ret;
	}

	/* do not proceed if not enough free space left */
	if (out_space < codec->mpd.produced) {
		source_release_data(sources[0], 0);
		return -ENOSPC;
	}

	void *sink_ptr;
	size_t sink_bytes;
	uint8_t const *sink_buffer_start;

	ret = sink_get_buffer(sinks[0], codec->mpd.produced, &sink_ptr,
			      (void **)&sink_buffer_start, &sink_bytes);
	if (ret) {
		comp_err(dev, "cannot get sink buffer");
		return ret;
	}

	/* Copy the produced samples into the output buffer */
	size_t bytes_to_end = (size_t)((uint8_t *)sink_buffer_start +
				       sink_bytes - (uint8_t *)sink_ptr);

	if (bytes_to_end >= codec->mpd.produced) {
		/* No wrap, copy directly */
		memcpy_s(sink_ptr, codec->mpd.produced, codec->mpd.out_buff,
			 codec->mpd.produced);
	} else {
		/* Wrap occurs, copy in two parts */
		memcpy_s(sink_ptr, bytes_to_end, codec->mpd.out_buff, bytes_to_end);
		memcpy_s((uint8_t *)sink_buffer_start, codec->mpd.produced - bytes_to_end,
			 (uint8_t *)codec->mpd.out_buff + bytes_to_end,
			 codec->mpd.produced - bytes_to_end);
	}

	source_release_data(sources[0], codec->mpd.consumed);
	sink_commit_buffer(sinks[0], codec->mpd.produced);

	/* reset produced and consumed */
	codec->mpd.consumed = 0;
	codec->mpd.produced = 0;

	comp_dbg(dev, "cadence_codec_process() done");

	return 0;
}

static int cadence_codec_reset(struct processing_module *mod)
{
	struct module_data *codec = &mod->priv;

	codec->mpd.init_done = 0;

	return 0;
}

static bool cadence_is_ready_to_process(struct processing_module *mod,
					struct sof_source **sources, int num_of_sources,
					struct sof_sink **sinks, int num_of_sinks)
{
	struct module_data *codec = &mod->priv;

	if (source_get_data_available(sources[0]) < codec->mpd.in_buff_size ||
	    sink_get_free_size(sinks[0]) < codec->mpd.out_buff_size)
		return false;

	return true;
}

static const struct module_interface cadence_codec_interface = {
	.init = cadence_codec_init,
	.prepare = cadence_codec_prepare,
	.process = cadence_codec_process,
	.set_configuration = cadence_codec_set_configuration,
	.reset = cadence_codec_reset,
	.free = cadence_codec_free,
	.is_ready_to_process = cadence_is_ready_to_process,
};

DECLARE_MODULE_ADAPTER(cadence_codec_interface, cadence_codec_uuid, cadence_codec_tr);
SOF_MODULE_INIT(cadence_codec, sys_comp_module_cadence_codec_interface_init);
