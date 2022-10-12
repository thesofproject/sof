// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Xperi. All rights reserved.
//
// Author: Mark Barton <mark.barton@xperi.com>

#include "sof/audio/module_adapter/module/generic.h"

#include "DtsSofInterface.h"

/* d95fc34f-370f-4ac7-bc86-bfdc5be241e6 */
DECLARE_SOF_RT_UUID("dts_codec", dts_uuid, 0xd95fc34f, 0x370f, 0x4ac7,
			0xbc, 0x86, 0xbf, 0xdc, 0x5b, 0xe2, 0x41, 0xe6);
DECLARE_TR_CTX(dts_tr, SOF_UUID(dts_uuid), LOG_LEVEL_INFO);

#define MAX_EXPECTED_DTS_CONFIG_DATA_SIZE 8192

/* The enumeration should be aligned as topology side. */
enum dts_config_mode_id {
	DTS_CONFIG_MODE_BYPASS    = 0,
	DTS_CONFIG_MODE_SPEAKERS  = 1,
	DTS_CONFIG_MODE_HEADPHONE = 2,
	DTS_CONFIG_MODE_LINEOUT   = DTS_CONFIG_MODE_BYPASS,
	DTS_CONFIG_MODE_MAX       = 3, /* for defining the number of config entries */
};

struct dts_module_private_data {
	DtsSofInterfaceInst *inst;            /* DTS SDK instance */
	struct module_param *config[DTS_CONFIG_MODE_MAX]; /* table of config entries */
};

static int dts_codec_apply_config(struct processing_module *mod);

static void *dts_effect_allocate_codec_memory(void *mod_void, unsigned int length,
					      unsigned int alignment)
{
	struct processing_module *mod = mod_void;
	struct comp_dev *dev = mod->dev;
	void *pMem;

	comp_dbg(dev, "dts_effect_allocate_codec_memory() start");

	pMem = module_allocate_memory(mod, (uint32_t)length, (uint32_t)alignment);

	if (pMem == NULL)
		comp_err(dev,
			"dts_effect_allocate_codec_memory() failed to allocate %d bytes", length);

	comp_dbg(dev, "dts_effect_allocate_codec_memory() done");
	return pMem;
}

static int dts_effect_convert_sof_interface_result(struct comp_dev *dev,
	DtsSofInterfaceResult dts_result)
{
	int ret;

	switch (dts_result) {
	case DTS_SOF_INTERFACE_RESULT_SUCCESS:
		ret = 0;
		break;
	case DTS_SOF_INTERFACE_RESULT_ERROR_NO_MEMORY:
		ret = -ENOMEM;
		break;
	case DTS_SOF_INTERFACE_RESULT_ERROR_DTS_INTERNAL_MODULE_ERROR:
		ret = -EIO;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int dts_effect_populate_buffer_configuration(struct comp_dev *dev,
	DtsSofInterfaceBufferConfiguration *buffer_config)
{
	struct comp_buffer *source = list_first_item(&dev->bsource_list, struct comp_buffer,
						     sink_list);
	const struct audio_stream *stream;
	DtsSofInterfaceBufferLayout buffer_layout;
	DtsSofInterfaceBufferFormat buffer_format;
	comp_dbg(dev, "dts_effect_populate_buffer_configuration() start");

	if (!source)
		return -EINVAL;

	stream = &source->stream;

	switch (source->buffer_fmt) {
	case SOF_IPC_BUFFER_INTERLEAVED:
		buffer_layout = DTS_SOF_INTERFACE_BUFFER_LAYOUT_INTERLEAVED;
		break;
	case SOF_IPC_BUFFER_NONINTERLEAVED:
		buffer_layout = DTS_SOF_INTERFACE_BUFFER_LAYOUT_NONINTERLEAVED;
		break;
	default:
		return -EINVAL;
	}

	switch (stream->frame_fmt) {
	case SOF_IPC_FRAME_S16_LE:
		buffer_format = DTS_SOF_INTERFACE_BUFFER_FORMAT_SINT16LE;
		break;
	case SOF_IPC_FRAME_S24_4LE:
		buffer_format = DTS_SOF_INTERFACE_BUFFER_FORMAT_SINT24LE;
		break;
	case SOF_IPC_FRAME_S32_LE:
		buffer_format = DTS_SOF_INTERFACE_BUFFER_FORMAT_SINT32LE;
		break;
	case SOF_IPC_FRAME_FLOAT:
		buffer_format = DTS_SOF_INTERFACE_BUFFER_FORMAT_FLOAT32;
		break;
	default:
		return -EINVAL;
	}

	buffer_config->bufferLayout = buffer_layout;
	buffer_config->bufferFormat = buffer_format;
	buffer_config->sampleRate = stream->rate;
	buffer_config->numChannels = stream->channels;
	buffer_config->periodInFrames = dev->frames;
	/* totalBufferLengthInBytes will be populated in dtsSofInterfacePrepare */
	buffer_config->totalBufferLengthInBytes = 0;

	comp_dbg(dev, "dts_effect_populate_buffer_configuration() done");

	return 0;
}

static int dts_codec_init(struct processing_module *mod)
{
	int ret;
	struct comp_dev *dev = mod->dev;
	struct module_data *codec = &mod->priv;
	struct dts_module_private_data *dts_private = NULL;
	DtsSofInterfaceResult dts_result;
	DtsSofInterfaceVersionInfo interface_version;
	DtsSofInterfaceVersionInfo sdk_version;

	comp_dbg(dev, "dts_codec_init() start");

	dts_private = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM,
			      sizeof(struct dts_module_private_data));
	if (!dts_private) {
		comp_err(dev, "dts_codec_init(): failed to allocate dts_module_private_data");
		return -ENOMEM;
	}

	dts_result = dtsSofInterfaceInit(&dts_private->inst, dts_effect_allocate_codec_memory, mod);
	ret = dts_effect_convert_sof_interface_result(dev, dts_result);

	if (ret)
		comp_err(dev, "dts_codec_init() dtsSofInterfaceInit failed %d %d", ret, dts_result);

	/* Obtain the current versions of DTS interface and SDK */
	dts_result = dtsSofInterfaceGetVersion(&interface_version, &sdk_version);

	/* Not necessary to fail initialisation if only get version failed */
	if (dts_result == DTS_SOF_INTERFACE_RESULT_SUCCESS) {
		comp_info(dev,
			"dts_codec_init() DTS SOF Interface version %d.%d.%d.%d",
			interface_version.major,
			interface_version.minor,
			interface_version.patch,
			interface_version.build);
		comp_info(dev,
			"dts_codec_init() DTS SDK version %d.%d.%d.%d",
			sdk_version.major,
			sdk_version.minor,
			sdk_version.patch,
			sdk_version.build);
	}

	if (ret)
		comp_err(dev, "dts_codec_init() failed %d %d", ret, dts_result);

	codec->private = dts_private;

	comp_dbg(dev, "dts_codec_init() done");

	return ret;
}

static int dts_codec_prepare(struct processing_module *mod)
{
	int ret;
	struct comp_dev *dev = mod->dev;
	struct module_data *codec = &mod->priv;
	struct dts_module_private_data *dts_private = codec->private;
	DtsSofInterfaceBufferConfiguration buffer_configuration;
	DtsSofInterfaceResult dts_result;

	comp_dbg(dev, "dts_codec_prepare() start");

	/*
	 * mod->config_mode should have been assigned the index as wanted via mixer control so
	 * apply the stored config to DTS SDK now.
	 */
	ret = dts_codec_apply_config(mod);
	if (ret) {
		comp_err(dev, "dts_codec_prepare() dts_codec_apply_config failed %d", ret);
		return ret;
	}

	ret = dts_effect_populate_buffer_configuration(dev, &buffer_configuration);
	if (ret) {
		comp_err(dev,
			"dts_codec_prepare() dts_effect_populate_buffer_configuration failed %d",
			ret);
		return ret;
	}

	dts_result = dtsSofInterfacePrepare(
		dts_private->inst,
		&buffer_configuration,
		&codec->mpd.in_buff,
		&codec->mpd.in_buff_size,
		&codec->mpd.out_buff,
		&codec->mpd.out_buff_size);
	ret = dts_effect_convert_sof_interface_result(dev, dts_result);

	if (ret)
		comp_err(dev, "dts_codec_prepare() failed %d", ret);

	comp_dbg(dev, "dts_codec_prepare() done");

	return ret;
}

static int dts_codec_init_process(struct processing_module *mod)
{
	int ret;
	struct comp_dev *dev = mod->dev;
	struct module_data *codec = &mod->priv;
	struct dts_module_private_data *dts_private = codec->private;
	DtsSofInterfaceResult dts_result;

	comp_dbg(dev, "dts_codec_init_process() start");

	dts_result = dtsSofInterfaceInitProcess(dts_private->inst);
	ret = dts_effect_convert_sof_interface_result(dev, dts_result);

	codec->mpd.produced = 0;
	codec->mpd.consumed = 0;
	codec->mpd.init_done = 1;

	if (ret)
		comp_err(dev, "dts_codec_init_process() failed %d %d", ret, dts_result);

	comp_dbg(dev, "dts_codec_init_process() done");

	return ret;
}

static int
dts_codec_process(struct processing_module *mod,
		  struct input_stream_buffer *input_buffers, int num_input_buffers,
		  struct output_stream_buffer *output_buffers, int num_output_buffers)
{
	int ret;
	struct comp_dev *dev = mod->dev;
	struct module_data *codec = &mod->priv;
	struct dts_module_private_data *dts_private = codec->private;
	DtsSofInterfaceResult dts_result;
	unsigned int bytes_processed = 0;

	/* Proceed only if we have enough data to fill the module buffer completely */
	if (input_buffers[0].size < codec->mpd.in_buff_size) {
		comp_dbg(dev, "dts_codec_process(): not enough data to process");
		return -ENODATA;
	}

	if (!codec->mpd.init_done) {
		ret = dts_codec_init_process(mod);
		if (ret < 0)
			return ret;
	}

	memcpy_s(codec->mpd.in_buff, codec->mpd.in_buff_size,
		 input_buffers[0].data, codec->mpd.in_buff_size);
	codec->mpd.avail = codec->mpd.in_buff_size;

	comp_dbg(dev, "dts_codec_process() start");

	dts_result = dtsSofInterfaceProcess(dts_private->inst, &bytes_processed);
	ret = dts_effect_convert_sof_interface_result(dev, dts_result);

	codec->mpd.consumed = !ret ? bytes_processed : 0;
	codec->mpd.produced = !ret ? bytes_processed : 0;
	input_buffers[0].consumed = codec->mpd.consumed;

	if (ret) {
		comp_err(dev, "dts_codec_process() failed %d %d", ret, dts_result);
		return ret;
	}

	/* copy the produced samples into the output buffer */
	memcpy_s(output_buffers[0].data, codec->mpd.produced, codec->mpd.out_buff,
		 codec->mpd.produced);
	output_buffers[0].size = codec->mpd.produced;

	comp_dbg(dev, "dts_codec_process() done");

	return ret;
}

/*
 * For read-only byte control, all configs (speaker, headphone, and etc.) will be loaded after
 * module initiated. Just store them to internal buffers without applying to SDK. "apply_config"
 * will be done on prepare() according to the config index assigned via enum control.
 *
 * Internal buffers for storage are required because mod->priv.cfg.data will be freed once it is
 * transferred down to module.
 */
static int dts_codec_store_config(struct processing_module *mod)
{
	int ret = 0;
	struct comp_dev *dev = mod->dev;
	struct module_data *codec = &mod->priv;
	struct dts_module_private_data *dts_private = codec->private;
	struct module_config *config;
	struct module_param *param;
	struct module_param *dst_param;
	uint32_t config_header_size;
	uint32_t config_data_size;
	uint32_t param_header_size;
	uint32_t i;
	uint32_t param_number = 0;

	comp_dbg(dev, "dts_codec_store_config() start");

	config = &codec->cfg;

	/* Check that config->data isn't invalid and has size greater than 0 */
	config_header_size = sizeof(config->size) + sizeof(config->avail);
	if (config->size < config_header_size) {
		comp_err(dev, "dts_codec_store_config() config->data is invalid");
		return -EINVAL;
	} else if (config->size == config_header_size) {
		comp_err(dev, "dts_codec_store_config() size of config->data is 0");
		return -EINVAL;
	}

	/* Calculate size of config->data */
	config_data_size = config->size - config_header_size;

	/* Check that config->data is not greater than the max expected for DTS data */
	if (config_data_size > MAX_EXPECTED_DTS_CONFIG_DATA_SIZE) {
		comp_err(dev,
			"dts_codec_store_config() size of config->data is larger than max for DTS data");
		return -EINVAL;
	}

	/* Allow for multiple module_params to be packed into the data pointed to by config
	 */
	for (i = 0; i < config_data_size; param_number++) {
		param = (struct module_param *)((char *)config->data + i);
		param_header_size = sizeof(param->id) + sizeof(param->size);

		/* If param->size is less than param_header_size, then this param is not valid */
		if (param->size <= param_header_size) {
			comp_err(dev, "dts_codec_store_config() param is invalid");
			return 0;
		}

		/* If param->id is larger than DTS_CONFIG_MODE_MAX, it is not valid */
		if (param->id >= DTS_CONFIG_MODE_MAX) {
			comp_err(dev, "dts_codec_store_config() param->id %u is invalid",
				 param->id);
			return 0;
		}

		if (!dts_private->config[param->id]) {
			/* No space for config available yet, allocate now */
			dst_param = rballoc(0, SOF_MEM_CAPS_RAM, param->size);
		} else if (dts_private->config[param->id]->size != param->size) {
			/* The size allocated for previous config doesn't match the new one.
			 * Free old container and allocate new one.
			 */
			rfree(dts_private->config[param->id]);
			dst_param = rballoc(0, SOF_MEM_CAPS_RAM, param->size);
		} else {
			dst_param = dts_private->config[param->id];
		}

		if (!dst_param) {
			comp_err(dev, "dts_codec_store_config() failed to allocate dst_param");
			return -ENOMEM;
		}

		ret = memcpy_s(dst_param, param->size, param, param->size);
		assert(!ret);

		comp_dbg(dev, "dts_codec_store_config() stored config id %u size %u", param->id,
			 param->size);
		dts_private->config[param->id] = dst_param;

		/* For backward compatibility, set to be inclined to apply the last config blob
		 * written to module.
		 */
		mod->config_mode = param->id;

		/* Advance to the next module_param */
		i += param->size;
	}

	comp_dbg(dev, "dts_codec_store_config() done");

	return ret;
}

static int dts_codec_apply_config(struct processing_module *mod)
{
	int ret = 0;
	struct comp_dev *dev = mod->dev;
	struct module_data *codec = &mod->priv;
	struct dts_module_private_data *dts_private = codec->private;
	struct module_param *param;
	uint32_t param_header_size;
	uint32_t param_data_size;
	DtsSofInterfaceResult dts_result;

	comp_dbg(dev, "dts_codec_apply_config() start");

	if (mod->config_mode >= DTS_CONFIG_MODE_MAX || !dts_private->config[mod->config_mode]) {
		comp_err(dev, "dts_codec_apply_config() config_mode %u is invalid",
			 mod->config_mode);
		return -EINVAL;
	}

	param = (struct module_param *)dts_private->config[mod->config_mode];
	param_header_size = sizeof(param->id) + sizeof(param->size);
	param_data_size = param->size - param_header_size;

	/*
	 * Pass constant 1 to arg parameterId instead of param->id. parameterId is re-defined by
	 * SDK for internal multi-config support usage in the future version.
	 */
	dts_result = dtsSofInterfaceApplyConfig(dts_private->inst, 1, param->data,
						param_data_size);
	ret = dts_effect_convert_sof_interface_result(dev, dts_result);
	if (ret) {
		comp_err(dev, "dts_codec_apply_config() dtsSofInterfaceApplyConfig failed %d",
			 dts_result);
		return ret;
	}

	comp_dbg(dev, "dts_codec_apply_config() done");

	return ret;
}

static int dts_codec_reset(struct processing_module *mod)
{
	int ret;
	struct comp_dev *dev = mod->dev;
	struct module_data *codec = &mod->priv;
	struct dts_module_private_data *dts_private = codec->private;
	DtsSofInterfaceResult dts_result;

	comp_dbg(dev, "dts_codec_reset() start");

	dts_result = dtsSofInterfaceReset(dts_private->inst);
	ret = dts_effect_convert_sof_interface_result(dev, dts_result);

	if (ret)
		comp_err(dev, "dts_codec_reset() failed %d %d", ret, dts_result);

	comp_dbg(dev, "dts_codec_reset() done");

	return ret;
}

static int dts_codec_free(struct processing_module *mod)
{
	int ret;
	struct comp_dev *dev = mod->dev;
	struct module_data *codec = &mod->priv;
	struct dts_module_private_data *dts_private = codec->private;
	int i;
	DtsSofInterfaceResult dts_result;

	comp_dbg(dev, "dts_codec_free() start");

	dts_result = dtsSofInterfaceFree(dts_private->inst);
	ret = dts_effect_convert_sof_interface_result(dev, dts_result);

	if (ret)
		comp_err(dev, "dts_codec_free() failed %d %d", ret, dts_result);

	module_free_all_memory(mod);

	/* Free dts_module_private_data */
	for (i = 0; i < DTS_CONFIG_MODE_MAX; i++)
		rfree(dts_private->config[i]);
	rfree(dts_private);

	comp_dbg(dev, "dts_codec_free() done");

	return ret;
}

static int
dts_codec_set_configuration(struct processing_module *mod, uint32_t config_id,
			    enum module_cfg_fragment_position pos, uint32_t data_offset_size,
			    const uint8_t *fragment, size_t fragment_size, uint8_t *response,
			    size_t response_size)
{
	struct module_data *md = &mod->priv;
	struct comp_dev *dev = mod->dev;
	int ret;

	ret = module_set_configuration(mod, config_id, pos, data_offset_size, fragment,
				       fragment_size, response, response_size);
	if (ret < 0)
		return ret;

	/* return if more fragments are expected or if the module is not prepared */
	if ((pos != MODULE_CFG_FRAGMENT_LAST && pos != MODULE_CFG_FRAGMENT_SINGLE) ||
	    md->state < MODULE_INITIALIZED)
		return 0;

	/* whole configuration received, store it now */
	ret = dts_codec_store_config(mod);
	if (ret) {
		comp_err(dev, "dts_codec_set_configuration(): error %x: runtime config store failed",
			 ret);
		return ret;
	}

	comp_dbg(dev, "dts_codec_set_configuration(): config stored");

	return 0;
}

static struct module_interface dts_interface = {
	.init  = dts_codec_init,
	.prepare = dts_codec_prepare,
	.process = dts_codec_process,
	.set_configuration = dts_codec_set_configuration,
	.reset = dts_codec_reset,
	.free = dts_codec_free
};

DECLARE_MODULE_ADAPTER(dts_interface, dts_uuid, dts_tr);
