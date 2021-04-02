// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Waves Audio Ltd. All rights reserved.
//
// Author: Oleksandr Strelchenko <oleksandr.strelchenko@waves.com>
//
#include <sof/audio/codec_adapter/codec/generic.h>
#include <sof/audio/codec_adapter/codec/waves.h>
#include <sof/debug/debug.h>
#include <sof/compiler_attributes.h>

#include "MaxxEffect/MaxxEffect.h"
#include "MaxxEffect/MaxxStream.h"
#include "MaxxEffect/MaxxStatus.h"
#include "MaxxEffect/Initialize/MaxxEffect_Initialize.h"
#include "MaxxEffect/Process/MaxxEffect_Process.h"
#include "MaxxEffect/Process/MaxxEffect_Reset.h"
#include "MaxxEffect/Control/RPC/MaxxEffect_RPC_Server.h"
#include "MaxxEffect/Control/Direct/MaxxEffect_Revision.h"

#define MAX_CONFIG_SIZE_BYTES (8192)
#define NUM_IO_STREAMS (1)

/* d944281a-afe9-4695-a043-d7f62b89538e*/
DECLARE_SOF_RT_UUID("waves_codec", waves_uuid, 0xd944281a, 0xafe9, 0x4695,
		    0xa0, 0x43, 0xd7, 0xf6, 0x2b, 0x89, 0x53, 0x8e);
DECLARE_TR_CTX(waves_tr, SOF_UUID(waves_uuid), LOG_LEVEL_INFO);

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
		COMPILER_FALLTHROUGH
	case MAXX_BUFFER_FORMAT_Q1_31:
		COMPILER_FALLTHROUGH
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
		COMPILER_FALLTHROUGH
	case SOF_IPC_FRAME_S24_4LE:
		COMPILER_FALLTHROUGH
	case SOF_IPC_FRAME_S32_LE:
		supported = true;
		break;
	case SOF_IPC_FRAME_FLOAT:
		COMPILER_FALLTHROUGH
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
		COMPILER_FALLTHROUGH
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
		COMPILER_FALLTHROUGH
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
static int waves_effect_allocate(struct comp_dev *dev)
{
	struct codec_data *codec = comp_get_codec(dev);
	struct waves_codec_data *waves_codec = codec->private;
	MaxxStatus_t status;

	comp_dbg(dev, "waves_effect_allocate() start");

	status = MaxxEffect_GetEffectSize(&waves_codec->effect_size);
	if (status) {
		comp_err(dev, "waves_effect_allocate() MaxxEffect_GetEffectSize returned %d",
			 status);
		return -EINVAL;
	}

	waves_codec->effect = (MaxxEffect_t *)codec_allocate_memory(dev,
		waves_codec->effect_size, 16);

	if (!waves_codec->effect) {
		comp_err(dev, "waves_effect_allocate() failed to allocate %d bytes for effect",
			 waves_codec->effect_size);
		return -ENOMEM;
	}

	comp_info(dev, "waves_codec_init() allocated %d bytes for effect",
		  waves_codec->effect_size);

	comp_dbg(dev, "waves_codec_init() done");
	return 0;
}

/* checks if sink/source parameters fit MaxxEffect */
static int waves_effect_check(struct comp_dev *dev)
{
	struct comp_data *component = comp_get_drvdata(dev);
	const struct audio_stream *src_fmt = &component->ca_source->stream;
	const struct audio_stream *snk_fmt = &component->ca_sink->stream;

	comp_dbg(dev, "waves_effect_check() start");

	/* todo use fallback to comp_verify_params when ready */

	/* resampling not supported */
	if (src_fmt->rate != snk_fmt->rate) {
		comp_err(dev, "waves_effect_check() source %d sink %d rate mismatch",
			 src_fmt->rate, snk_fmt->rate);
		return -EINVAL;
	}

	/* upmix/downmix not supported */
	if (src_fmt->channels != snk_fmt->channels) {
		comp_err(dev, "waves_effect_check() source %d sink %d channels mismatch",
			 src_fmt->channels, snk_fmt->channels);
		return -EINVAL;
	}

	/* different frame format not supported */
	if (src_fmt->frame_fmt != snk_fmt->frame_fmt) {
		comp_err(dev, "waves_effect_check() source %d sink %d sample format mismatch",
			 src_fmt->frame_fmt, snk_fmt->frame_fmt);
		return -EINVAL;
	}

	/* different interleaving is not supported */
	if (component->ca_source->buffer_fmt != component->ca_sink->buffer_fmt) {
		comp_err(dev, "waves_effect_check() source %d sink %d buffer format mismatch");
		return -EINVAL;
	}

	if (!format_is_supported(src_fmt->frame_fmt)) {
		comp_err(dev, "waves_effect_check() float samples not supported");
		return -EINVAL;
	}

	if (!layout_is_supported(component->ca_source->buffer_fmt)) {
		comp_err(dev, "waves_effect_check() non interleaved format not supported");
		return -EINVAL;
	}

	if (!rate_is_supported(src_fmt->rate)) {
		comp_err(dev, "waves_effect_check() rate %d not supported", src_fmt->rate);
		return -EINVAL;
	}

	if (src_fmt->channels != 2) {
		comp_err(dev, "waves_effect_check() channels %d not supported", src_fmt->channels);
		return -EINVAL;
	}

	comp_dbg(dev, "waves_effect_check() done");
	return 0;
}

/* initializes MaxxEffect based on stream parameters */
static int waves_effect_init(struct comp_dev *dev)
{
	struct codec_data *codec = comp_get_codec(dev);
	struct waves_codec_data *waves_codec = codec->private;
	struct comp_data *component = comp_get_drvdata(dev);

	const struct audio_stream *src_fmt = &component->ca_source->stream;

	MaxxStatus_t status;
	MaxxBuffer_Format_t sample_format;
	MaxxBuffer_Layout_t buffer_format;
	int32_t sample_bytes;
	MaxxStreamFormat_t *i_formats[NUM_IO_STREAMS] = { &waves_codec->i_format };
	MaxxStreamFormat_t *o_formats[NUM_IO_STREAMS] = { &waves_codec->o_format };

	comp_dbg(dev, "waves_effect_init() start");

	sample_format = format_convert_sof_to_me(src_fmt->frame_fmt);
	if (sample_format < 0) {
		comp_err(dev, "waves_effect_init() sof sample format %d not supported",
			 src_fmt->frame_fmt);
		return -EINVAL;
	}

	buffer_format = layout_convert_sof_to_me(component->ca_source->buffer_fmt);
	if (buffer_format < 0) {
		comp_err(dev, "waves_effect_init() sof buffer format %d not supported",
			 component->ca_source->buffer_fmt);
		return -EINVAL;
	}

	sample_bytes = sample_format_convert_to_bytes(sample_format);
	if (sample_bytes < 0) {
		comp_err(dev, "waves_effect_init() sample_format %d not supported",
			 sample_format);
		return -EINVAL;
	}

	waves_codec->request_max_bytes = 0;
	waves_codec->response_max_bytes = 0;
	waves_codec->response = 0;
	waves_codec->i_buffer = 0;
	waves_codec->o_buffer = 0;

	waves_codec->i_format.sampleRate = src_fmt->rate;
	waves_codec->i_format.numChannels = src_fmt->channels;
	waves_codec->i_format.samplesFormat = sample_format;
	waves_codec->i_format.samplesLayout = buffer_format;

	waves_codec->o_format = waves_codec->i_format;

	waves_codec->sample_size_in_bytes = sample_bytes;
	waves_codec->buffer_samples = (src_fmt->rate * 2) / 1000; /* 2 ms io buffers */
	waves_codec->buffer_bytes = waves_codec->buffer_samples * src_fmt->channels *
		waves_codec->sample_size_in_bytes;

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

	comp_dbg(dev, "waves_effect_init() done");
	return 0;
}

/* allocate additional buffers for MaxxEffect */
static int waves_effect_buffers(struct comp_dev *dev)
{
	struct codec_data *codec = comp_get_codec(dev);
	struct waves_codec_data *waves_codec = codec->private;
	MaxxStatus_t status;
	int ret;
	void *i_buffer = NULL, *o_buffer = NULL, *response = NULL;

	comp_dbg(dev, "waves_effect_buffers() start");

	status = MaxxEffect_GetMessageMaxSize(waves_codec->effect, &waves_codec->request_max_bytes,
					      &waves_codec->response_max_bytes);

	if (status) {
		comp_err(dev, "waves_effect_buffers() MaxxEffect_GetMessageMaxSize returned %d",
			 status);
		ret = -EINVAL;
		goto err;
	}

	response = codec_allocate_memory(dev, waves_codec->response_max_bytes, 16);
	if (!response) {
		comp_err(dev, "waves_effect_buffers() failed to allocate %d bytes for response",
			 waves_codec->response_max_bytes);
		ret = -ENOMEM;
		goto err;
	}

	i_buffer = codec_allocate_memory(dev, waves_codec->buffer_bytes, 16);
	if (!i_buffer) {
		comp_err(dev, "waves_effect_buffers() failed to allocate %d bytes for i_buffer",
			 waves_codec->buffer_bytes);
		ret = -ENOMEM;
		goto err;
	}

	o_buffer = codec_allocate_memory(dev, waves_codec->buffer_bytes, 16);
	if (!o_buffer) {
		comp_err(dev, "waves_effect_buffers() failed to allocate %d bytes for o_buffer",
			 waves_codec->buffer_bytes);
		ret = -ENOMEM;
		goto err;
	}

	waves_codec->i_buffer = i_buffer;
	waves_codec->o_buffer = o_buffer;
	waves_codec->response = response;
	codec->cpd.in_buff = waves_codec->i_buffer;
	codec->cpd.in_buff_size = waves_codec->buffer_bytes;
	codec->cpd.out_buff = waves_codec->o_buffer;
	codec->cpd.out_buff_size = waves_codec->buffer_bytes;

	comp_info(dev, "waves_effect_buffers() size response %d, i_buffer %d, o_buffer %d",
		  waves_codec->response_max_bytes, waves_codec->buffer_bytes,
		  waves_codec->buffer_bytes);

	comp_dbg(dev, "waves_effect_buffers() done");
	return 0;

err:
	if (i_buffer)
		codec_free_memory(dev, i_buffer);
	if (o_buffer)
		codec_free_memory(dev, o_buffer);
	if (response)
		codec_free_memory(dev, response);
	return ret;
}

/* get MaxxEffect revision */
static int waves_effect_revision(struct comp_dev *dev)
{
	struct codec_data *codec = comp_get_codec(dev);
	struct waves_codec_data *waves_codec = codec->private;
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

/* apply MaxxEffect message */
static int waves_effect_message(struct comp_dev *dev, void *data, uint32_t size)
{
	struct codec_data *codec = comp_get_codec(dev);
	struct waves_codec_data *waves_codec = codec->private;
	MaxxStatus_t status;
	uint32_t response_size = 0;

	comp_info(dev, "waves_effect_message() start data %p size %d", data, size);

	status = MaxxEffect_Message(waves_codec->effect, data, size,
				    waves_codec->response, &response_size);

	if (status) {
		comp_err(dev, "waves_effect_message() MaxxEffect_Message returned %d", status);
		return -EINVAL;
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

/* apply codec config */
static int waves_effect_config(struct comp_dev *dev, enum codec_cfg_type type)
{
	struct codec_config *cfg;
	struct codec_data *codec = comp_get_codec(dev);
	struct codec_param *param;
	uint32_t index;
	uint32_t param_number = 0;
	int ret = 0;

	comp_info(dev, "waves_codec_configure() start type %d", type);

	cfg = (type == CODEC_CFG_SETUP) ? &codec->s_cfg : &codec->r_cfg;

	comp_info(dev, "waves_codec_configure() config %p, size %d, avail %d",
		  cfg->data, cfg->size, cfg->avail);

	if (!cfg->avail || !cfg->size) {
		comp_err(dev, "waves_codec_configure() no config for type %d, avail %d, size %d",
			 type, cfg->avail, cfg->size);
		return -EINVAL;
	}

	if (cfg->size > MAX_CONFIG_SIZE_BYTES) {
		comp_err(dev, "waves_codec_configure() provided config is too big, size %d",
			 cfg->size);
		return -EINVAL;
	}

	/* incoming data in cfg->data is arranged according to struct codec_param
	 * there migh be more than one struct codec_param inside cfg->data, glued back to back
	 */
	for (index = 0; index < cfg->size && (!ret); param_number++) {
		uint32_t param_data_size;

		param = (struct codec_param *)((char *)cfg->data + index);
		param_data_size = param->size - sizeof(param->size) - sizeof(param->id);

		comp_info(dev, "waves_codec_configure() param num %d id %d size %d",
			  param_number, param->id, param->size);

		switch (param->id) {
		case PARAM_NOP:
			comp_info(dev, "waves_codec_configure() NOP");
			break;
		case PARAM_MESSAGE:
			ret = waves_effect_message(dev, param->data, param_data_size);
			break;
		case PARAM_REVISION:
			ret = waves_effect_revision(dev);
			break;
		default:
			ret = -EINVAL;
			break;
		}

		index += param->size;
	}

	if (ret)
		comp_err(dev, "waves_codec_configure() failed %d", ret);

	comp_dbg(dev, "waves_codec_configure() done");
	return ret;
}

/* apply setup config */
static int waves_effect_setup_config(struct comp_dev *dev)
{
	struct codec_data *codec = comp_get_codec(dev);
	int ret;

	comp_dbg(dev, "waves_effect_setup_config() start");

	if (!codec->s_cfg.avail && !codec->s_cfg.size) {
		comp_err(dev, "waves_effect_startup_config() setup config is not provided");
		return -EINVAL;
	}

	if (!codec->s_cfg.avail) {
		comp_warn(dev, "waves_effect_startup_config() using old setup config");
		codec->s_cfg.avail = true;
	}

	ret = waves_effect_config(dev, CODEC_CFG_SETUP);
	codec->s_cfg.avail = false;

	comp_dbg(dev, "waves_effect_setup_config() done");
	return ret;
}

int waves_codec_init(struct comp_dev *dev)
{
	struct codec_data *codec = comp_get_codec(dev);
	struct waves_codec_data *waves_codec;
	int ret = 0;

	comp_dbg(dev, "waves_codec_init() start");

	waves_codec = codec_allocate_memory(dev, sizeof(struct waves_codec_data), 16);
	if (!waves_codec) {
		comp_err(dev, "waves_codec_init() failed to allocate %d bytes for waves_codec_data",
			 sizeof(struct waves_codec_data));
		ret = -ENOMEM;
	} else {
		memset(waves_codec, 0, sizeof(struct waves_codec_data));
		codec->private = waves_codec;

		ret = waves_effect_allocate(dev);
		if (ret) {
			codec_free_memory(dev, waves_codec);
			codec->private = NULL;
		}
	}

	if (ret)
		comp_err(dev, "waves_codec_init() failed %d", ret);

	comp_dbg(dev, "waves_codec_init() done");
	return ret;
}

int waves_codec_prepare(struct comp_dev *dev)
{
	int ret;

	comp_dbg(dev, "waves_codec_prepare() start");

	ret = waves_effect_check(dev);

	if (!ret)
		ret = waves_effect_init(dev);

	if (!ret)
		ret = waves_effect_buffers(dev);

	if (!ret)
		ret = waves_effect_setup_config(dev);

	if (ret)
		comp_err(dev, "waves_codec_prepare() failed %d", ret);

	comp_dbg(dev, "waves_codec_prepare() done");
	return ret;
}

int waves_codec_init_process(struct comp_dev *dev)
{
	struct codec_data *codec = comp_get_codec(dev);

	comp_dbg(dev, "waves_codec_init_process()");

	codec->cpd.produced = 0;
	codec->cpd.consumed = 0;
	codec->cpd.init_done = 1;

	return 0;
}

int waves_codec_process(struct comp_dev *dev)
{
	int ret;
	struct codec_data *codec = comp_get_codec(dev);
	struct waves_codec_data *waves_codec = codec->private;

	comp_dbg(dev, "waves_codec_process() start");

	MaxxStream_t *i_streams[NUM_IO_STREAMS] = { &waves_codec->i_stream };
	MaxxStream_t *o_streams[NUM_IO_STREAMS] = { &waves_codec->o_stream };
	MaxxStatus_t status;
	uint32_t num_input_samples = waves_codec->buffer_samples;

	/* here input buffer should always be filled up as requested
	 * since no one updates it`s size except code in prepare.
	 * on the other hand there is available/produced counters in cpd, check them anyways
	 */
	if (codec->cpd.avail != waves_codec->buffer_bytes) {
		comp_warn(dev, "waves_codec_process() input buffer %d is not full %d",
			  codec->cpd.avail, waves_codec->buffer_bytes);
		num_input_samples = codec->cpd.avail /
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
		codec->cpd.produced = waves_codec->o_stream.numAvailableSamples *
			waves_codec->o_format.numChannels * waves_codec->sample_size_in_bytes;
		codec->cpd.consumed = codec->cpd.produced;
		ret = 0;
	}

	if (ret)
		comp_err(dev, "waves_codec_process() failed %d", ret);

	comp_dbg(dev, "waves_codec_process() done");
	return ret;
}

int waves_codec_apply_config(struct comp_dev *dev)
{
	int ret;

	comp_dbg(dev, "waves_codec_apply_config() start");
	ret =  waves_effect_config(dev, CODEC_CFG_RUNTIME);

	if (ret)
		comp_err(dev, "waves_codec_apply_config() failed %d", ret);

	comp_dbg(dev, "waves_codec_apply_config() done");
	return ret;
}

int waves_codec_reset(struct comp_dev *dev)
{
	MaxxStatus_t status;
	int ret = 0;
	struct codec_data *codec = comp_get_codec(dev);
	struct waves_codec_data *waves_codec = codec->private;

	comp_dbg(dev, "waves_codec_reset() start");

	status = MaxxEffect_Reset(waves_codec->effect);
	if (status) {
		comp_err(dev, "waves_codec_reset() MaxxEffect_Reset returned %d", status);
		ret = -EINVAL;
	}

	if (ret)
		comp_err(dev, "waves_codec_reset() failed %d", ret);

	comp_dbg(dev, "waves_codec_reset() done");
	return ret;
}

int waves_codec_free(struct comp_dev *dev)
{
	/* codec is using codec_adapter method codec_allocate_memory for all allocations
	 * codec_adapter will free it all on component free call
	 * nothing to do here
	 */
	comp_dbg(dev, "waves_codec_free()");
	return 0;
}

static struct codec_interface waves_interface = {
	.init  = waves_codec_init,
	.prepare = waves_codec_prepare,
	.init_process = waves_codec_init_process,
	.process = waves_codec_process,
	.apply_config = waves_codec_apply_config,
	.reset = waves_codec_reset,
	.free = waves_codec_free
};

DECLARE_CODEC_ADAPTER(waves_interface, waves_uuid, waves_tr);
