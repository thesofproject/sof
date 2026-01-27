// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Intel Corporation. All rights reserved.
//
// Author: Marcin Rajwa <marcin.rajwa@linux.intel.com>

/*
 * \file cadence.c
 * \brief Cadence Codec API
 * \author Marcin Rajwa <marcin.rajwa@linux.intel.com>
 *
 */

#include <ipc/compress_params.h>
#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/module_adapter/module/cadence.h>
#include <rtos/init.h>

SOF_DEFINE_REG_UUID(cadence_codec);
LOG_MODULE_DECLARE(cadence_codec, CONFIG_SOF_LOG_LEVEL);
DECLARE_TR_CTX(cadence_codec_tr, SOF_UUID(cadence_codec_uuid), LOG_LEVEL_INFO);

int cadence_codec_resolve_api(struct processing_module *mod)
{
	int ret;
	struct snd_codec codec_params;
	uint32_t codec_id = DEFAULT_CODEC_ID;

	if (mod->stream_params->ext_data_length) {
		ret = memcpy_s(&codec_params, mod->stream_params->ext_data_length,
			       (uint8_t *)mod->stream_params + sizeof(*mod->stream_params),
			       mod->stream_params->ext_data_length);
		if (ret < 0)
			return ret;

		codec_id = codec_params.id;
	}

	/* IPC3 only supports playback */
	return cadence_codec_resolve_api_with_id(mod, codec_id, mod->stream_params->direction);
}

static int cadence_codec_init(struct processing_module *mod)
{
	struct module_data *codec = &mod->priv;
	struct cadence_codec_data *cd;
	struct comp_dev *dev = mod->dev;
	struct module_config *setup_cfg;
	int ret;

	comp_dbg(dev, "cadence_codec_init() start");

	cd = mod_zalloc(mod, sizeof(struct cadence_codec_data));
	if (!cd) {
		comp_err(dev, "failed to allocate memory for cadence codec data");
		return -ENOMEM;
	}

	codec->private = cd;
	codec->mpd.init_done = 0;

	/* copy the setup config only for the first init */
	if (codec->state == MODULE_DISABLED && codec->cfg.avail) {
		setup_cfg = &cd->setup_cfg;

		/* allocate memory for set up config */
		setup_cfg->data = mod_alloc(mod, codec->cfg.size);
		if (!setup_cfg->data) {
			comp_err(dev, "failed to alloc setup config");
			ret = -ENOMEM;
			goto free;
		}

		/* copy the setup config */
		setup_cfg->size = codec->cfg.size;
		ret = memcpy_s(setup_cfg->data, setup_cfg->size,
			       codec->cfg.init_data, setup_cfg->size);
		if (ret) {
			comp_err(dev, "failed to copy setup config %d", ret);
			goto free_cfg;
		}
		setup_cfg->avail = true;
	}

	comp_dbg(dev, "cadence_codec_init() done");

	return 0;

free_cfg:
	mod_free(mod, setup_cfg->data);
free:
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
	struct cadence_codec_data *cd = codec->private;

	comp_dbg(dev, "cadence_codec_apply_config() start");

	cfg = &codec->cfg;

	/* use setup config if no runtime config available. This will be true during reset */
	if (!cfg->avail)
		cfg = &cd->setup_cfg;

	data = cfg->data;
	size = cfg->size;

	if (!cfg->avail || !size) {
		comp_err(dev, "cadence_codec_apply_config() error: no config available");
		return -EIO;
	}

	return cadence_codec_apply_params(mod, size, data);
}

static int cadence_codec_deep_buff_allowed(struct processing_module *mod)
{
	struct cadence_codec_data *cd = module_get_private_data(mod);

	switch (cd->api_id) {
	case CADENCE_CODEC_MP3_ENC_ID:
		return 0;
	default:
		return 1;
	}
}

static int cadence_codec_prepare(struct processing_module *mod,
				 struct sof_source **sources, int num_of_sources,
				 struct sof_sink **sinks, int num_of_sinks)
{
	int ret = 0, mem_tabs_size;
	struct comp_dev *dev = mod->dev;
	struct module_data *codec = &mod->priv;
	struct cadence_codec_data *cd = codec->private;

	comp_dbg(dev, "cadence_codec_prepare() start");

	ret = cadence_init_codec_object(mod);
	if (ret)
		return ret;

	ret = cadence_codec_apply_config(mod);
	if (ret) {
		comp_err(dev, "cadence_codec_prepare() error %x: failed to apply config",
			 ret);
		return ret;
	}

	/* Allocate memory for the codec */
	API_CALL(cd, XA_API_CMD_GET_MEMTABS_SIZE, 0, &mem_tabs_size, ret);
	if (ret != LIB_NO_ERROR) {
		comp_err(dev, "cadence_codec_prepare() error %x: failed to get memtabs size",
			 ret);
		return ret;
	}

	cd->mem_tabs = mod_alloc(mod, mem_tabs_size);
	if (!cd->mem_tabs) {
		comp_err(dev, "cadence_codec_prepare() error: failed to allocate space for memtabs");
		return -ENOMEM;
	}

	comp_dbg(dev, "allocated %d bytes for memtabs", mem_tabs_size);

	API_CALL(cd, XA_API_CMD_SET_MEMTABS_PTR, 0, cd->mem_tabs, ret);
	if (ret != LIB_NO_ERROR) {
		comp_err(dev, "cadence_codec_prepare() error %x: failed to set memtabs",
			 ret);
		goto free;
	}

	ret = cadence_codec_init_memory_tables(mod);
	if (ret != LIB_NO_ERROR) {
		comp_err(dev, "cadence_codec_prepare() error %x: failed to init memory tables",
			 ret);
		goto free;
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
		comp_err(dev, "cadence_codec_init_process() error %x: failed to get lib init status",
			 ret);
		return ret;
	}
#endif
	comp_dbg(dev, "cadence_codec_prepare() done");
	return 0;
free:
	mod_free(mod, cd->mem_tabs);
	return ret;
}

static int
cadence_codec_process(struct processing_module *mod,
		      struct input_stream_buffer *input_buffers, int num_input_buffers,
		      struct output_stream_buffer *output_buffers, int num_output_buffers)
{
	struct comp_buffer *local_buff;
	struct comp_dev *dev = mod->dev;
	struct module_data *codec = &mod->priv;
	int free_bytes, output_bytes = cadence_codec_get_samples(mod) *
				mod->stream_params->sample_container_bytes *
				mod->stream_params->channels;
	uint32_t remaining = input_buffers[0].size;
	int ret;

	if (!cadence_codec_deep_buff_allowed(mod))
		mod->deep_buff_bytes = 0;

	/* Proceed only if we have enough data to fill the module buffer completely */
	if (input_buffers[0].size < codec->mpd.in_buff_size) {
		comp_dbg(dev, "not enough data to process");
		return -ENODATA;
	}

	if (!codec->mpd.init_done) {
		memcpy_s(codec->mpd.in_buff, codec->mpd.in_buff_size, input_buffers[0].data,
			 codec->mpd.in_buff_size);
		codec->mpd.avail = codec->mpd.in_buff_size;

		ret = cadence_codec_init_process(mod);
		if (ret)
			return ret;

		remaining -= codec->mpd.consumed;
		input_buffers[0].consumed = codec->mpd.consumed;
	}

	/* do not proceed with processing if not enough free space left in the local buffer */
	local_buff = list_first_item(&mod->raw_data_buffers_list, struct comp_buffer, buffers_list);
	free_bytes = audio_stream_get_free(&local_buff->stream);
	if (free_bytes < output_bytes)
		return -ENOSPC;

	/* Proceed only if we have enough data to fill the module buffer completely */
	if (remaining < codec->mpd.in_buff_size)
		return -ENODATA;

	memcpy_s(codec->mpd.in_buff, codec->mpd.in_buff_size,
		 (uint8_t *)input_buffers[0].data + input_buffers[0].consumed,
		 codec->mpd.in_buff_size);
	codec->mpd.avail = codec->mpd.in_buff_size;

	comp_dbg(dev, "cadence_codec_process() start");

	ret = cadence_codec_process_data(mod);
	if (ret)
		return ret;

	/* update consumed with the number of samples consumed during init */
	input_buffers[0].consumed += codec->mpd.consumed;
	codec->mpd.consumed = input_buffers[0].consumed;

	/* copy the produced samples into the output buffer */
	memcpy_s(output_buffers[0].data, codec->mpd.produced, codec->mpd.out_buff,
		 codec->mpd.produced);
	output_buffers[0].size = codec->mpd.produced;

	comp_dbg(dev, "cadence_codec_process() done");

	return 0;
}

static int cadence_codec_reset(struct processing_module *mod)
{
	struct module_data *codec = &mod->priv;
	struct cadence_codec_data *cd = codec->private;
	int ret;

	cadence_codec_free_memory_tables(mod);
	mod_free(mod, cd->mem_tabs);

	/* reset to default params */
	API_CALL(cd, XA_API_CMD_INIT, XA_CMD_TYPE_INIT_API_PRE_CONFIG_PARAMS, NULL, ret);
	if (ret != LIB_NO_ERROR)
		return ret;

	codec->mpd.init_done = 0;

	mod_free(mod, cd->self);
	cd->self = NULL;

	return ret;
}

static const struct module_interface cadence_codec_interface = {
	.init = cadence_codec_init,
	.prepare = cadence_codec_prepare,
	.process_raw_data = cadence_codec_process,
	.set_configuration = cadence_codec_set_configuration,
	.reset = cadence_codec_reset,
	.free = cadence_codec_free
};

DECLARE_MODULE_ADAPTER(cadence_codec_interface, cadence_codec_uuid, cadence_codec_tr);
SOF_MODULE_INIT(cadence_codec, sys_comp_module_cadence_codec_interface_init);
