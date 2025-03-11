// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Waves Audio Ltd. All rights reserved.
//
// Author: Oleksandr Strelchenko <oleksandr.strelchenko@waves.com>
//

#include <rtos/init.h>
#include <sof/audio/module_adapter/module/generic.h>
#include <sof/debug/debug.h>
#include <sof/compiler_attributes.h>
#include <sof/audio/MaxxEffect/Initialize/MaxxEffect_Initialize.h>
#include <sof/audio/MaxxEffect/Process/MaxxEffect_Process.h>
#include <sof/audio/MaxxEffect/Process/MaxxEffect_Reset.h>
#include <sof/audio/MaxxEffect/Control/RPC/MaxxEffect_RPC_Server.h>
#include <sof/audio/MaxxEffect/Control/Direct/MaxxEffect_Revision.h>

#define MAX_CONFIG_SIZE_BYTES (8192)
#define NUM_IO_STREAMS (1)

SOF_DEFINE_REG_UUID(waves);

DECLARE_TR_CTX(waves_tr, SOF_UUID(waves_uuid), LOG_LEVEL_INFO);
LOG_MODULE_REGISTER(waves, CONFIG_SOF_LOG_LEVEL);

struct waves_codec_data {
	uint32_t                sample_rate;
	uint32_t                buffer_bytes;
	uint32_t                buffer_samples;
	uint32_t                sample_size_in_bytes;
	uint64_t                reserved;

	MaxxEffect_t            *effect;
	uint32_t                effect_size;
	MaxxStreamFormat_t      i_format;
	MaxxStreamFormat_t      o_format;
	MaxxStream_t            i_stream;
	MaxxStream_t            o_stream;
	MaxxBuffer_t            i_buffer;
	MaxxBuffer_t            o_buffer;
	uint32_t                response_max_bytes;
	uint32_t                request_max_bytes;
	void                    *response;
	uint32_t                config_blob_size;
	void                    *config_blob;
	bool                    initialized;
};

enum waves_codec_params {
	PARAM_NOP = 0,
	PARAM_MESSAGE = 1,
	PARAM_REVISION = 2
};

/* convert MaxxBuffer_Format_t to number of bytes it requires */
static int32_t sample_format_convert_to_bytes(MaxxBuffer_Format_t format)
{
	int32_t res;

	switch (format) {
	case MAXX_BUFFER_FORMAT_Q1_15:
		res = sizeof(uint16_t);
		break;
	case MAXX_BUFFER_FORMAT_Q1_23:
		res = 3; /* 3 bytes */
		break;
	case MAXX_BUFFER_FORMAT_Q9_23:
		COMPILER_FALLTHROUGH;
	case MAXX_BUFFER_FORMAT_Q1_31:
		COMPILER_FALLTHROUGH;
	case MAXX_BUFFER_FORMAT_Q5_27:
		res = sizeof(uint32_t);
		break;
	case MAXX_BUFFER_FORMAT_FLOAT:
		res = sizeof(float);
		break;
	default:
		res = -EINVAL;
		break;
	}
	return res;
}

/* convert enum sof_ipc_frame to MaxxBuffer_Format_t */
static MaxxBuffer_Format_t format_convert_sof_to_me(enum sof_ipc_frame format)
{
	MaxxBuffer_Format_t res;

	switch (format) {
	case SOF_IPC_FRAME_S16_LE:
		res = MAXX_BUFFER_FORMAT_Q1_15;
		break;
	case SOF_IPC_FRAME_S24_4LE:
		res = MAXX_BUFFER_FORMAT_Q9_23;
		break;
	case SOF_IPC_FRAME_S32_LE:
		res = MAXX_BUFFER_FORMAT_Q1_31;
		break;
	case SOF_IPC_FRAME_FLOAT:
		res = MAXX_BUFFER_FORMAT_FLOAT;
		break;
	default:
		res = -EINVAL;
		break;
	}
	return res;
}

/* convert sof frame format to MaxxBuffer_Layout_t */
static MaxxBuffer_Layout_t layout_convert_sof_to_me(uint32_t layout)
{
	MaxxBuffer_Layout_t res;

	switch (layout) {
	case SOF_IPC_BUFFER_INTERLEAVED:
		res = MAXX_BUFFER_LAYOUT_INTERLEAVED;
		break;
	case SOF_IPC_BUFFER_NONINTERLEAVED:
		res = MAXX_BUFFER_LAYOUT_DEINTERLEAVED;
		break;
	default:
		res = -EINVAL;
		break;
	}
	return res;
}

/* check if sample format supported by codec */
static bool format_is_supported(enum sof_ipc_frame format)
{
	bool supported;

	switch (format) {
	case SOF_IPC_FRAME_S16_LE:
		COMPILER_FALLTHROUGH;
	case SOF_IPC_FRAME_S24_4LE:
		COMPILER_FALLTHROUGH;
	case SOF_IPC_FRAME_S32_LE:
		supported = true;
		break;
	case SOF_IPC_FRAME_FLOAT:
		COMPILER_FALLTHROUGH;
	default:
		supported = false;
		break;
	}
	return supported;
}

/* check if buffer layout supported by codec */
static bool layout_is_supported(uint32_t layout)
{
	bool supported;

	switch (layout) {
	case SOF_IPC_BUFFER_INTERLEAVED:
		supported = true;
		break;
	case SOF_IPC_BUFFER_NONINTERLEAVED:
		COMPILER_FALLTHROUGH;
	default:
		supported = false;
		break;
	}
	return supported;
}

/* check if sample rate supported by codec */
static bool rate_is_supported(uint32_t rate)
{
	bool supported;

	switch (rate) {
	case 44100:
		COMPILER_FALLTHROUGH;
	case 48000:
		supported = true;
		break;
	default:
		supported = false;
		break;
	}
	return supported;
}

/* allocate memory for MaxxEffect object */
static int waves_effect_allocate(struct processing_module *mod)
{
	struct comp_dev *dev = mod->dev;
	struct module_data *codec = &mod->priv;
	struct waves_codec_data *waves_codec = codec->private;
	MaxxStatus_t status;

	comp_dbg(dev, "waves_effect_allocate() start");

	status = MaxxEffect_GetEffectSize(&waves_codec->effect_size);
	if (status) {
		comp_err(dev, "waves_effect_allocate() MaxxEffect_GetEffectSize returned %d",
			 status);
		return -EINVAL;
	}

	waves_codec->effect = (MaxxEffect_t *)module_allocate_memory(mod,
		waves_codec->effect_size, 16);

	if (!waves_codec->effect) {
		comp_err(dev, "waves_effect_allocate() failed to allocate %d bytes for effect",
			 waves_codec->effect_size);
		return -ENOMEM;
	}

	comp_dbg(dev, "waves_effect_allocate() allocated %d bytes for effect",
		  waves_codec->effect_size);

	return 0;
}

/* checks if sink/source parameters fit MaxxEffect */
static int waves_effect_check(struct comp_dev *dev)
{
	struct comp_buffer *sink = comp_dev_get_first_data_consumer(dev);
	struct comp_buffer *source = comp_dev_get_first_data_producer(dev);
	const struct audio_stream *src_fmt = &source->stream;
	const struct audio_stream *snk_fmt = &sink->stream;

	/* Init sink & source buffers */
	comp_dbg(dev, "waves_effect_check() start");

	/* todo use fallback to comp_verify_params when ready */

	/* resampling not supported */
	if (audio_stream_get_rate(src_fmt) != audio_stream_get_rate(snk_fmt)) {
		comp_err(dev, "waves_effect_check() source %d sink %d rate mismatch",
			 audio_stream_get_rate(src_fmt), audio_stream_get_rate(snk_fmt));
		return -EINVAL;
	}

	/* upmix/downmix not supported */
	if (audio_stream_get_channels(src_fmt) != audio_stream_get_channels(snk_fmt)) {
		comp_err(dev, "waves_effect_check() source %d sink %d channels mismatch",
			 audio_stream_get_channels(src_fmt), audio_stream_get_channels(snk_fmt));
		return -EINVAL;
	}

	/* different frame format not supported */
	if (audio_stream_get_frm_fmt(src_fmt) != audio_stream_get_frm_fmt(snk_fmt)) {
		comp_err(dev, "waves_effect_check() source %d sink %d sample format mismatch",
			 audio_stream_get_frm_fmt(src_fmt), audio_stream_get_frm_fmt(snk_fmt));
		return -EINVAL;
	}

	/* different interleaving is not supported */
	if (audio_stream_get_buffer_fmt(src_fmt) != audio_stream_get_buffer_fmt(snk_fmt)) {
		comp_err(dev, "waves_effect_check() source %d sink %d buffer format mismatch",
			 audio_stream_get_buffer_fmt(src_fmt), audio_stream_get_buffer_fmt(snk_fmt));
		return -EINVAL;
	}

	if (!format_is_supported(audio_stream_get_frm_fmt(src_fmt))) {
		comp_err(dev, "waves_effect_check() float samples not supported");
		return -EINVAL;
	}

	if (!layout_is_supported(audio_stream_get_buffer_fmt(src_fmt))) {
		comp_err(dev, "waves_effect_check() non interleaved format not supported");
		return -EINVAL;
	}

	if (!rate_is_supported(audio_stream_get_rate(src_fmt))) {
		comp_err(dev, "waves_effect_check() rate %d not supported",
			 audio_stream_get_rate(src_fmt));
		return -EINVAL;
	}

	if (audio_stream_get_channels(src_fmt) != 2) {
		comp_err(dev, "waves_effect_check() channels %d not supported",
			 audio_stream_get_channels(src_fmt));
		return -EINVAL;
	}

	comp_dbg(dev, "waves_effect_check() done");
	return 0;
}

/* initializes MaxxEffect based on stream parameters */
static int waves_effect_init(struct processing_module *mod)
{
	struct comp_dev *dev = mod->dev;
	struct comp_buffer *source = comp_dev_get_first_data_producer(dev);
	struct module_data *codec = &mod->priv;
	struct waves_codec_data *waves_codec = codec->private;
	const struct audio_stream *src_fmt = &source->stream;
	MaxxStatus_t status;
	MaxxBuffer_Format_t sample_format;
	MaxxBuffer_Layout_t buffer_format;
	int32_t sample_bytes;
	MaxxStreamFormat_t *i_formats[NUM_IO_STREAMS] = { &waves_codec->i_format };
	MaxxStreamFormat_t *o_formats[NUM_IO_STREAMS] = { &waves_codec->o_format };

	comp_dbg(dev, "waves_effect_init() start");

	sample_format = format_convert_sof_to_me(audio_stream_get_frm_fmt(src_fmt));
	if (sample_format < 0) {
		comp_err(dev, "waves_effect_init() sof sample format %d not supported",
			 audio_stream_get_frm_fmt(src_fmt));
		return -EINVAL;
	}

	buffer_format = layout_convert_sof_to_me(audio_stream_get_buffer_fmt(src_fmt));
	if (buffer_format < 0) {
		comp_err(dev, "waves_effect_init() sof buffer format %d not supported",
			 audio_stream_get_buffer_fmt(src_fmt));
		return -EINVAL;
	}

	sample_bytes = sample_format_convert_to_bytes(sample_format);
	if (sample_bytes < 0) {
		comp_err(dev, "waves_effect_init() sample_format %d not supported",
			 sample_format);
		return -EINVAL;
	}

	waves_codec->request_max_bytes = 0;
	waves_codec->i_buffer = 0;
	waves_codec->o_buffer = 0;

	waves_codec->i_format.sampleRate = audio_stream_get_rate(src_fmt);
	waves_codec->i_format.numChannels = audio_stream_get_channels(src_fmt);
	waves_codec->i_format.samplesFormat = sample_format;
	waves_codec->i_format.samplesLayout = buffer_format;
	waves_codec->o_format = waves_codec->i_format;

	waves_codec->sample_size_in_bytes = sample_bytes;
	/* Prepare a buffer for 1 period worth of data
	 * dev->pipeline->period stands for the scheduling period in us
	 */
	waves_codec->buffer_samples = audio_stream_get_rate(src_fmt) * dev->pipeline->period /
		1000000;
	waves_codec->buffer_bytes = waves_codec->buffer_samples *
		audio_stream_get_channels(src_fmt) * waves_codec->sample_size_in_bytes;

	// trace allows printing only up-to 4 words at a time
	// logging all the information in two calls
	comp_info(dev, "waves_effect_init() rate %d, channels %d", waves_codec->i_format.sampleRate,
		  waves_codec->i_format.numChannels);

	comp_info(dev, "waves_effect_init() format %d, layout %d, frame %d",
		  waves_codec->i_format.samplesFormat, waves_codec->i_format.samplesLayout,
		  waves_codec->buffer_samples);

	status = MaxxEffect_Initialize(waves_codec->effect, i_formats, 1, o_formats, 1);

	if (status) {
		comp_err(dev, "waves_effect_init() MaxxEffect_Initialize returned %d", status);
		return -EINVAL;
	}
	waves_codec->initialized = true;

	comp_dbg(dev, "waves_effect_init() done");
	return 0;
}

/* allocate additional buffers for MaxxEffect */
static int waves_effect_buffers(struct processing_module *mod)
{
	struct comp_dev *dev = mod->dev;
	struct module_data *codec = &mod->priv;
	struct waves_codec_data *waves_codec = codec->private;
	int ret;
	void *i_buffer = NULL, *o_buffer = NULL;

	comp_dbg(dev, "waves_effect_buffers() start");

	i_buffer = module_allocate_memory(mod, waves_codec->buffer_bytes, 16);
	if (!i_buffer) {
		comp_err(dev, "waves_effect_buffers() failed to allocate %d bytes for i_buffer",
			 waves_codec->buffer_bytes);
		ret = -ENOMEM;
		goto err;
	}

	o_buffer = module_allocate_memory(mod, waves_codec->buffer_bytes, 16);
	if (!o_buffer) {
		comp_err(dev, "waves_effect_buffers() failed to allocate %d bytes for o_buffer",
			 waves_codec->buffer_bytes);
		ret = -ENOMEM;
		goto err;
	}

	waves_codec->i_buffer = i_buffer;
	waves_codec->o_buffer = o_buffer;
	codec->mpd.in_buff = waves_codec->i_buffer;
	codec->mpd.in_buff_size = waves_codec->buffer_bytes;
	codec->mpd.out_buff = waves_codec->o_buffer;
	codec->mpd.out_buff_size = waves_codec->buffer_bytes;

	comp_dbg(dev, "waves_effect_buffers() in_buff_size %d, out_buff_size %d",
		  codec->mpd.in_buff_size, codec->mpd.out_buff_size);

	comp_dbg(dev, "waves_effect_buffers() done");
	return 0;

err:
	if (i_buffer)
		module_free_memory(mod, i_buffer);
	if (o_buffer)
		module_free_memory(mod, o_buffer);
	return ret;
}

/* get MaxxEffect revision */
static int waves_effect_revision(struct processing_module *mod)
{
	struct module_data *codec = &mod->priv;
	struct waves_codec_data *waves_codec = codec->private;
	struct comp_dev *dev = mod->dev;
	const char *revision = NULL;
	uint32_t revision_len;
	MaxxStatus_t status;

	comp_info(dev, "waves_effect_revision() start");

	status = MaxxEffect_Revision_Get(waves_codec->effect, &revision, &revision_len);

	if (status) {
		comp_err(dev, "waves_effect_revision() MaxxEffect_Revision_Get returned %d",
			 status);
		return -EINVAL;
	}

#if CONFIG_TRACEV
	if (revision_len) {
		const uint32_t *ptr = (uint32_t *)revision;
		uint32_t len = revision_len / sizeof(uint32_t);
		uint32_t idx = 0;

		/* get requests from codec_adapter are not supported
		 * printing strings is not supported
		 * so dumping revision string to trace log as ascii values
		 * if simply write a for loop here then depending on trace filtering settings
		 * some parts of revision might not be printed - this is highly unwanted
		 */
		dump_hex(ptr, idx, len);
		dump_hex(ptr, idx, len);
		dump_hex(ptr, idx, len);
		dump_hex(ptr, idx, len);
		dump_hex(ptr, idx, len);
		dump_hex(ptr, idx, len);
		dump_hex(ptr, idx, len);
		dump_hex(ptr, idx, len);
		dump_hex(ptr, idx, len);
		dump_hex(ptr, idx, len);
	}
#endif

	comp_info(dev, "waves_effect_revision() done");
	return 0;
}

/* cache config blob*/
static int waves_effect_save_config_blob_to_cache(struct processing_module *mod,
	void *data, uint32_t size)
{
	struct comp_dev *dev = mod->dev;
	struct module_data *codec = &mod->priv;
	struct waves_codec_data *waves_codec = codec->private;

	comp_info(dev, "waves_effect_save_config_blob_to_cache() start");

	/* release old cached config blob*/
	if (waves_codec->config_blob && size != waves_codec->config_blob_size) {
		comp_info(dev, "waves_effect_save_config_blob_to_cache() release blob");
		module_free_memory(mod, waves_codec->config_blob);
		waves_codec->config_blob = NULL;
		waves_codec->config_blob_size = 0;
	}

	if (!waves_codec->config_blob) {
		waves_codec->config_blob = module_allocate_memory(mod, size, 16);
		if (!waves_codec->config_blob) {
			comp_err(dev,
				"waves_effect_save_config_blob_to_cache() failed to allocate %d bytes for config blob",
				size);
			return -ENOMEM;
		}
		waves_codec->config_blob_size = size;
	}

	int ret = memcpy_s(waves_codec->config_blob, waves_codec->config_blob_size,
			data, size);
	if (ret) {
		comp_err(dev,
			"waves_effect_save_config_blob_to_cache(): failed to copy config blob %d",
			ret);
		module_free_memory(mod, waves_codec->config_blob);
		waves_codec->config_blob = NULL;
		waves_codec->config_blob_size = 0;
		return ret;
	}

	comp_dbg(dev, "waves_effect_save_config_blob_to_cache() done");
	return 0;
}

/* apply MaxxEffect message */
static int waves_effect_message(struct processing_module *mod, void *data, uint32_t size)
{
	struct module_data *codec = &mod->priv;
	struct waves_codec_data *waves_codec = codec->private;
	struct comp_dev *dev = mod->dev;
	MaxxStatus_t status;
	uint32_t response_size = 0;

	if (waves_codec->initialized) {
		comp_info(dev, "waves_effect_message() start data %p size %d", data, size);

		status = MaxxEffect_Message(waves_codec->effect, data, size,
									waves_codec->response, &response_size);

		if (status) {
			comp_err(dev, "waves_effect_message() MaxxEffect_Message returned %d",
					status);
			return -EINVAL;
		}
	}

#if CONFIG_TRACEV
	/* at time of writing codec adapter does not support getting something from codec
	 * so response is stored to internal structure and dumped into trace messages
	 */
	if (response_size) {
		uint32_t idx;
		uint32_t len = response_size / sizeof(uint32_t);
		const uint32_t *ptr = (uint32_t *)waves_codec->response;

		for (idx = 0; idx < len; )
			dump_hex(ptr, idx, len);
	}
#endif

	return 0;
}

/* apply config blob */
static int waves_effect_apply_config_blob_from_cache(struct processing_module *mod)
{
	struct comp_dev *dev = mod->dev;
	struct module_data *codec = &mod->priv;
	struct waves_codec_data *waves_codec = codec->private;

	comp_info(dev, "waves_effect_apply_config_blob_from_cache()");

	if (waves_codec->config_blob) {
		return waves_effect_message(mod, waves_codec->config_blob,
				waves_codec->config_blob_size);
	}
	return 0;
}

static int waves_effect_handle_param_message(struct processing_module *mod,
	void *data, uint32_t size)
{
	int ret = waves_effect_save_config_blob_to_cache(mod, data, size);

	if (!ret)
		ret = waves_effect_apply_config_blob_from_cache(mod);

	return ret;
}

/* apply codec config */
static int waves_effect_apply_config(struct processing_module *mod)
{
	struct comp_dev *dev = mod->dev;
	struct module_data *codec = &mod->priv;
	struct module_param *param;
	struct module_config *cfg;
	uint32_t index;
	uint32_t param_number = 0;
	int ret = 0;

	comp_info(dev, "waves_effect_apply_config() start");

	cfg = &codec->cfg;

	comp_info(dev, "waves_effect_apply_config() config %p, size %d, avail %d",
		  cfg->data, cfg->size, cfg->avail);

	if (!cfg->data) {
		ret = waves_effect_apply_config_blob_from_cache(mod);
		if (ret) {
			comp_err(dev, "waves_effect_apply_config() error %x: apply cache fail",
				ret);
			return ret;
		}
	}

	if (cfg->size > MAX_CONFIG_SIZE_BYTES) {
		comp_err(dev, "waves_effect_apply_config() provided config is too big, size %d",
			 cfg->size);
		return -EINVAL;
	}

	/* incoming data in cfg->data is arranged according to struct module_param
	 * there migh be more than one struct module_param inside cfg->data, glued back to back
	 */
	const uint32_t header_size = sizeof(param->size) + sizeof(param->id);
	for (index = 0; index < cfg->size && (!ret); param_number++) {
		uint32_t param_data_size;

		param = (struct module_param *)((char *)cfg->data + index);
		param_data_size = param->size - sizeof(param->size) - sizeof(param->id);

		comp_info(dev, "waves_effect_apply_config() param num %d id %d size %d",
			  param_number, param->id, param->size);

		if ((param->size <= header_size) || (param->size > MAX_CONFIG_SIZE_BYTES)) {
			comp_err(dev, "waves_effect_apply_config() invalid module_param size: %d",
				param->size);
			return -EINVAL;
		}

		if ((index + param->size) > cfg->size) {
			comp_err(dev, "waves_effect_apply_config() module_param size: %d exceeds cfg buffer size: %d",
				param->size, cfg->size);
			return -EINVAL;
		}

		switch (param->id) {
		case PARAM_NOP:
			comp_info(dev, "waves_effect_apply_config() NOP");
			break;
		case PARAM_MESSAGE:
			ret = waves_effect_handle_param_message(mod, param->data, param_data_size);
			break;
		case PARAM_REVISION:
			ret = waves_effect_revision(mod);
			break;
		default:
			ret = -EINVAL;
			break;
		}

		index += param->size;
	}

	if (ret) {
		comp_err(dev, "waves_effect_apply_config() failed %d", ret);
		return ret;
	}

	comp_dbg(dev, "waves_effect_apply_config() done");
	return 0;
}

static int waves_codec_init(struct processing_module *mod)
{
	struct comp_dev *dev = mod->dev;
	struct module_data *codec = &mod->priv;
	struct waves_codec_data *waves_codec;
	int ret = 0;
	void *response = NULL;

	comp_dbg(dev, "waves_codec_init() start");

	waves_codec = module_allocate_memory(mod, sizeof(struct waves_codec_data), 16);
	if (!waves_codec) {
		comp_err(dev, "waves_codec_init() failed to allocate %d bytes for waves_codec_data",
			 sizeof(struct waves_codec_data));
		ret = -ENOMEM;
	} else {
		memset(waves_codec, 0, sizeof(struct waves_codec_data));
		codec->private = waves_codec;
		ret = waves_effect_allocate(mod);
		if (ret) {
			module_free_memory(mod, waves_codec);
			codec->private = NULL;
		}
	}

	if (ret) {
		comp_err(dev, "waves_codec_init() failed %d", ret);
		return ret;
	}

	ret = MaxxEffect_GetMessageMaxSize(waves_codec->effect, &waves_codec->request_max_bytes,
					      &waves_codec->response_max_bytes);

	if (ret) {
		comp_err(dev, "waves_codec_init() MaxxEffect_GetMessageMaxSize returned %d", ret);
		return -EINVAL;
	}

	response = module_allocate_memory(mod, waves_codec->response_max_bytes, 16);
	if (!response) {
		comp_err(dev, "waves_codec_init() failed to allocate %d bytes for response",
			 waves_codec->response_max_bytes);
		return -ENOMEM;
	}
	waves_codec->response = response;
	waves_codec->initialized = false;

	comp_dbg(dev, "waves_codec_init() done");
	return ret;
}

static int waves_codec_prepare(struct processing_module *mod,
			       struct sof_source **sources, int num_of_sources,
			       struct sof_sink **sinks, int num_of_sinks)
{
	struct comp_dev *dev = mod->dev;
	int ret;

	comp_dbg(dev, "waves_codec_prepare() start");

	ret = waves_effect_check(dev);
	if (ret)
		goto error;

	ret = waves_effect_init(mod);
	if (ret)
		goto error;

	ret = waves_effect_buffers(mod);
	if (ret)
		goto error;

	ret = waves_effect_apply_config(mod);
	if (ret)
		goto error;

	comp_dbg(dev, "waves_codec_prepare() done");
	return 0;

error:
	comp_err(dev, "waves_codec_prepare() failed %d", ret);
	return ret;
}

static int waves_codec_init_process(struct processing_module *mod)
{
	struct module_data *codec = &mod->priv;
	struct comp_dev *dev = mod->dev;

	comp_dbg(dev, "waves_codec_init_process()");

	codec->mpd.produced = 0;
	codec->mpd.consumed = 0;
	codec->mpd.init_done = 1;

	return 0;
}

static int
waves_codec_process(struct processing_module *mod,
		    struct input_stream_buffer *input_buffers, int num_input_buffers,
		    struct output_stream_buffer *output_buffers, int num_output_buffers)
{
	int ret;
	struct comp_dev *dev = mod->dev;
	struct module_data *codec = &mod->priv;
	struct waves_codec_data *waves_codec = codec->private;

	/* Proceed only if we have enough data to fill the module buffer completely */
	if (input_buffers[0].size < codec->mpd.in_buff_size) {
		comp_dbg(dev, "waves_codec_process(): not enough data to process");
		return -ENODATA;
	}

	if (!codec->mpd.init_done)
		waves_codec_init_process(mod);

	memcpy_s(codec->mpd.in_buff, codec->mpd.in_buff_size,
		 input_buffers[0].data, codec->mpd.in_buff_size);
	codec->mpd.avail = codec->mpd.in_buff_size;

	comp_dbg(dev, "waves_codec_process() start");

	MaxxStream_t *i_streams[NUM_IO_STREAMS] = { &waves_codec->i_stream };
	MaxxStream_t *o_streams[NUM_IO_STREAMS] = { &waves_codec->o_stream };
	MaxxStatus_t status;
	uint32_t num_input_samples = waves_codec->buffer_samples;

	/* here input buffer should always be filled up as requested
	 * since no one updates it`s size except code in prepare.
	 * on the other hand there is available/produced counters in mpd, check them anyways
	 */
	if (codec->mpd.avail != waves_codec->buffer_bytes) {
		comp_warn(dev, "waves_codec_process() input buffer %d is not full %d",
			  codec->mpd.avail, waves_codec->buffer_bytes);
		num_input_samples = codec->mpd.avail /
			(waves_codec->sample_size_in_bytes * waves_codec->i_format.numChannels);
	}

	waves_codec->i_stream.buffersArray = &waves_codec->i_buffer;
	waves_codec->i_stream.numAvailableSamples = num_input_samples;
	waves_codec->i_stream.numProcessedSamples = 0;
	waves_codec->i_stream.maxNumSamples = waves_codec->buffer_samples;

	waves_codec->o_stream.buffersArray = &waves_codec->o_buffer;
	waves_codec->o_stream.numAvailableSamples = 0;
	waves_codec->o_stream.numProcessedSamples = 0;
	waves_codec->o_stream.maxNumSamples = waves_codec->buffer_samples;

	status = MaxxEffect_Process(waves_codec->effect, i_streams, o_streams);
	if (status) {
		comp_err(dev, "waves_codec_process() MaxxEffect_Process returned %d", status);
		ret = -EINVAL;
	} else {
		codec->mpd.produced = waves_codec->o_stream.numAvailableSamples *
			waves_codec->o_format.numChannels * waves_codec->sample_size_in_bytes;
		codec->mpd.consumed = codec->mpd.produced;
		input_buffers[0].consumed = codec->mpd.consumed;
		ret = 0;
		/* copy the produced samples into the output buffer */
		memcpy_s(output_buffers[0].data, codec->mpd.produced, codec->mpd.out_buff,
			 codec->mpd.produced);
		output_buffers[0].size = codec->mpd.produced;
	}

	if (ret)
		comp_err(dev, "waves_codec_process() failed %d", ret);

	comp_dbg(dev, "waves_codec_process() done");
	return ret;
}

static int waves_codec_reset(struct processing_module *mod)
{
	MaxxStatus_t status;
	int ret = 0;
	struct comp_dev *dev = mod->dev;
	struct module_data *codec = &mod->priv;
	struct waves_codec_data *waves_codec = codec->private;

	comp_info(dev, "waves_codec_reset() start");

	status = MaxxEffect_Reset(waves_codec->effect);
	if (status) {
		comp_err(dev, "waves_codec_reset() MaxxEffect_Reset returned %d", status);
		ret = -EINVAL;
	}

	if (ret)
		comp_err(dev, "waves_codec_reset() failed %d", ret);

	if (codec->mpd.in_buff)
		module_free_memory(mod, codec->mpd.in_buff);

	if (codec->mpd.out_buff)
		module_free_memory(mod, codec->mpd.out_buff);

	waves_codec->initialized = false;
	comp_dbg(dev, "waves_codec_reset() done");
	return ret;
}

static int waves_codec_free(struct processing_module *mod)
{
	module_free_all_memory(mod);
	comp_dbg(mod->dev, "waves_codec_free()");
	return 0;
}

static int
waves_codec_set_configuration(struct processing_module *mod, uint32_t config_id,
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

	/* whole configuration received, apply it now */
	ret = waves_effect_apply_config(mod);
	if (ret) {
		comp_err(dev, "waves_codec_set_configuration(): error %x: runtime config apply failed",
			 ret);
		return ret;
	}

	comp_dbg(dev, "waves_codec_set_configuration(): config applied");

	return 0;
}

static const struct module_interface waves_interface = {
	.init = waves_codec_init,
	.prepare = waves_codec_prepare,
	.process_raw_data = waves_codec_process,
	.set_configuration = waves_codec_set_configuration,
	.reset = waves_codec_reset,
	.free = waves_codec_free
};

DECLARE_MODULE_ADAPTER(waves_interface, waves_uuid, waves_tr);
SOF_MODULE_INIT(waves, sys_comp_module_waves_interface_init);

#if CONFIG_WAVES_CODEC_MODULE && CONFIG_WAVES_CODEC_STUB
/* modular: llext dynamic link */

#include <module/module/api_ver.h>
#include <module/module/llext.h>
#include <rimage/sof/user/manifest.h>

SOF_LLEXT_MOD_ENTRY(waves, &waves_interface);

static const struct sof_man_module_manifest mod_manifest __section(".module") __used =
	SOF_LLEXT_MODULE_MANIFEST("WAVES", waves_llext_entry, 7, SOF_REG_UUID(waves), 8);

SOF_LLEXT_BUILDINFO;

#endif
