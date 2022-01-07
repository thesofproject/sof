// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Xperi. All rights reserved.
//
// Author: Mark Barton <mark.barton@xperi.com>

#include "sof/audio/codec_adapter/codec/generic.h"
#include "sof/audio/codec_adapter/codec/dts.h"

#include "DtsSofInterface.h"

/* d95fc34f-370f-4ac7-bc86-bfdc5be241e6 */
DECLARE_SOF_RT_UUID("dts_codec", dts_uuid, 0xd95fc34f, 0x370f, 0x4ac7,
			0xbc, 0x86, 0xbf, 0xdc, 0x5b, 0xe2, 0x41, 0xe6);
DECLARE_TR_CTX(dts_tr, SOF_UUID(dts_uuid), LOG_LEVEL_INFO);

#define MAX_EXPECTED_DTS_CONFIG_DATA_SIZE 8192

static void *dts_effect_allocate_codec_memory(void *dev_void, unsigned int length,
	unsigned int alignment)
{
	struct comp_dev *dev = dev_void;
	void *pMem;

	comp_dbg(dev, "dts_effect_allocate_codec_memory() start");

	pMem = codec_allocate_memory(dev, (uint32_t)length, (uint32_t)alignment);

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
	struct processing_module *mod = comp_get_drvdata(dev);
	const struct audio_stream *stream;
	DtsSofInterfaceBufferLayout buffer_layout;
	DtsSofInterfaceBufferFormat buffer_format;

	comp_dbg(dev, "dts_effect_populate_buffer_configuration() start");

	if (!mod->ca_source)
		return -EINVAL;

	stream = &mod->ca_source->stream;

	switch (mod->ca_source->buffer_fmt) {
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

int dts_codec_init(struct comp_dev *dev)
{
	int ret;
	struct module_data *codec = comp_get_codec(dev);
	DtsSofInterfaceResult dts_result;
	DtsSofInterfaceVersionInfo interface_version;
	DtsSofInterfaceVersionInfo sdk_version;

	comp_dbg(dev, "dts_codec_init() start");

	dts_result = dtsSofInterfaceInit((DtsSofInterfaceInst **)&(codec->private),
		dts_effect_allocate_codec_memory, dev);
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

	comp_dbg(dev, "dts_codec_init() done");

	return ret;
}

int dts_codec_prepare(struct comp_dev *dev)
{
	int ret;
	struct module_data *codec = comp_get_codec(dev);
	DtsSofInterfaceBufferConfiguration buffer_configuration;
	DtsSofInterfaceResult dts_result;

	comp_dbg(dev, "dts_codec_prepare() start");

	ret = dts_effect_populate_buffer_configuration(dev, &buffer_configuration);
	if (ret) {
		comp_err(dev,
			"dts_codec_prepare() dts_effect_populate_buffer_configuration failed %d",
			ret);
		return ret;
	}

	dts_result = dtsSofInterfacePrepare(
		(DtsSofInterfaceInst *)codec->private,
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

static int dts_codec_init_process(struct comp_dev *dev)
{
	int ret;
	struct module_data *codec = comp_get_codec(dev);
	DtsSofInterfaceResult dts_result;

	comp_dbg(dev, "dts_codec_init_process() start");

	dts_result = dtsSofInterfaceInitProcess(codec->private);
	ret = dts_effect_convert_sof_interface_result(dev, dts_result);

	codec->mpd.produced = 0;
	codec->mpd.consumed = 0;
	codec->mpd.init_done = 1;

	if (ret)
		comp_err(dev, "dts_codec_init_process() failed %d %d", ret, dts_result);

	comp_dbg(dev, "dts_codec_init_process() done");

	return ret;
}

int dts_codec_process(struct comp_dev *dev)
{
	int ret;
	struct module_data *codec = comp_get_codec(dev);
	DtsSofInterfaceResult dts_result;
	unsigned int bytes_processed = 0;

	if (!codec->mpd.init_done)
		return dts_codec_init_process(dev);

	comp_dbg(dev, "dts_codec_process() start");

	dts_result = dtsSofInterfaceProcess(codec->private, &bytes_processed);
	ret = dts_effect_convert_sof_interface_result(dev, dts_result);

	codec->mpd.consumed = !ret ? bytes_processed : 0;
	codec->mpd.produced = !ret ? bytes_processed : 0;

	if (ret)
		comp_err(dev, "dts_codec_process() failed %d %d", ret, dts_result);

	comp_dbg(dev, "dts_codec_process() done");

	return ret;
}

int dts_codec_apply_config(struct comp_dev *dev)
{
	int ret = 0;
	struct module_data *codec = comp_get_codec(dev);
	struct module_config *config;
	struct module_param *param;
	uint32_t config_header_size;
	uint32_t config_data_size;
	uint32_t param_header_size;
	uint32_t param_data_size;
	uint32_t i;
	uint32_t param_number = 0;
	DtsSofInterfaceResult dts_result;

	comp_dbg(dev, "dts_codec_apply_config() start");

	config = &codec->r_cfg;

	/* Check that config->data isn't invalid and has size greater than 0 */
	config_header_size = sizeof(config->size) + sizeof(config->avail);
	if (config->size < config_header_size) {
		comp_err(dev, "dts_codec_apply_config() config->data is invalid");
		return -EINVAL;
	} else if (config->size == config_header_size) {
		comp_err(dev, "dts_codec_apply_config() size of config->data is 0");
		return -EINVAL;
	}

	/* Calculate size of config->data */
	config_data_size = config->size - config_header_size;

	/* Check that config->data is not greater than the max expected for DTS data */
	if (config_data_size > MAX_EXPECTED_DTS_CONFIG_DATA_SIZE) {
		comp_err(dev,
			"dts_codec_apply_config() size of config->data is larger than max for DTS data");
		return -EINVAL;
	}

	/* Allow for multiple module_params to be packed into the data pointed to by config
	 */
	for (i = 0; i < config_data_size; param_number++) {
		param = (struct module_param *)((char *)config->data + i);
		param_header_size = sizeof(param->id) + sizeof(param->size);

		/* If param->size is less than param_header_size, then this param is not valid */
		if (param->size < param_header_size) {
			comp_err(dev, "dts_codec_apply_config() param is invalid");
			return -EINVAL;
		}

		/* Only process param->data if it has size greater than 0 */
		if (param->size > param_header_size) {
			/* Calculate size of param->data */
			param_data_size = param->size - param_header_size;

			if (param_data_size) {
				dts_result = dtsSofInterfaceApplyConfig(codec->private, param->id,
					param->data, param_data_size);
				ret = dts_effect_convert_sof_interface_result(dev, dts_result);
				if (ret) {
					comp_err(dev,
						"dts_codec_apply_config() dtsSofInterfaceApplyConfig failed %d",
						dts_result);
					return ret;
				}
			}
		}

		/* Advance to the next module_param */
		i += param->size;
	}

	comp_dbg(dev, "dts_codec_apply_config() done");

	return ret;
}

int dts_codec_reset(struct comp_dev *dev)
{
	int ret;
	struct module_data *codec = comp_get_codec(dev);
	DtsSofInterfaceResult dts_result;

	comp_dbg(dev, "dts_codec_reset() start");

	dts_result = dtsSofInterfaceReset(codec->private);
	ret = dts_effect_convert_sof_interface_result(dev, dts_result);

	if (ret)
		comp_err(dev, "dts_codec_reset() failed %d %d", ret, dts_result);

	comp_dbg(dev, "dts_codec_reset() done");

	return ret;
}

int dts_codec_free(struct comp_dev *dev)
{
	int ret;
	struct module_data *codec = comp_get_codec(dev);
	DtsSofInterfaceResult dts_result;

	comp_dbg(dev, "dts_codec_free() start");

	dts_result = dtsSofInterfaceFree(codec->private);
	ret = dts_effect_convert_sof_interface_result(dev, dts_result);

	if (ret)
		comp_err(dev, "dts_codec_free() failed %d %d", ret, dts_result);

	comp_dbg(dev, "dts_codec_free() done");

	return ret;
}

static struct module_interface dts_interface = {
	.init  = dts_codec_init,
	.prepare = dts_codec_prepare,
	.process = dts_codec_process,
	.apply_config = dts_codec_apply_config,
	.reset = dts_codec_reset,
	.free = dts_codec_free
};

DECLARE_CODEC_ADAPTER(dts_interface, dts_uuid, dts_tr);
