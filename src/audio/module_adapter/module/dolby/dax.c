// SPDX-License-Identifier: BSD-3-Clause
//
// NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE GRANTED BY THIS LICENSE
//
// Copyright(c) 2025 Dolby Laboratories. All rights reserved.
//
// Author: Jun Lai <jun.lai@dolby.com>
//

#include <stdio.h>

#include <rtos/init.h>
#include <sof/audio/data_blob.h>
#include <sof/audio/module_adapter/module/generic.h>
#include <sof/compiler_attributes.h>
#include <sof/debug/debug.h>

#include <dax_inf.h>

LOG_MODULE_REGISTER(dolby_dax_audio_processing, CONFIG_SOF_LOG_LEVEL);
SOF_DEFINE_REG_UUID(dolby_dax_audio_processing);
DECLARE_TR_CTX(dolby_dax_audio_processing_tr, SOF_UUID(dolby_dax_audio_processing_uuid),
	       LOG_LEVEL_INFO);

#define MAX_PARAMS_STR_BUFFER_SIZE 1536
#define DAX_ENABLE_MASK 0x1
#define DAX_PROFILE_MASK 0x2
#define DAX_DEVICE_MASK 0x4
#define DAX_CP_MASK 0x8
#define DAX_VOLUME_MASK 0x10
#define DAX_CTC_MASK 0x20

#define DAX_SWITCH_ENABLE_CONTROL_ID 0
#define DAX_SWITCH_CP_CONTROL_ID 1
#define DAX_SWITCH_CTC_CONTROL_ID 2
#define DAX_ENUM_PROFILE_CONTROL_ID 0
#define DAX_ENUM_DEVICE_CONTROL_ID 1

static const char *get_params_str(const void *val, uint32_t val_sz)
{
	static char params_str[MAX_PARAMS_STR_BUFFER_SIZE + 16];
	const int32_t *param_val = (const int32_t *)val;
	const uint32_t param_sz = val_sz >> 2;
	uint32_t offset = 0;

	for (uint32_t i = 0; i < param_sz && offset < MAX_PARAMS_STR_BUFFER_SIZE; i++)
		offset += sprintf(params_str + offset, "%d,", param_val[i]);
	return &params_str[0];
}

static int sof_to_dax_frame_fmt(enum sof_ipc_frame sof_frame_fmt)
{
	switch (sof_frame_fmt) {
	case SOF_IPC_FRAME_S16_LE:
		return DAX_FMT_SHORT_16;
	case SOF_IPC_FRAME_S32_LE:
		return DAX_FMT_INT;
	case SOF_IPC_FRAME_FLOAT:
		return DAX_FMT_FLOAT;
	default:
		return DAX_FMT_UNSUPPORTED;
	}
}

static int sof_to_dax_sample_rate(uint32_t rate)
{
	switch (rate) {
	case 48000:
		return rate;
	default:
		return DAX_RATE_UNSUPPORTED;
	}
}

static int sof_to_dax_channels(uint32_t channels)
{
	switch (channels) {
	case 2:
		return channels;
	default:
		return DAX_CHANNLES_UNSUPPORTED;
	}
}

static int sof_to_dax_buffer_layout(enum sof_ipc_buffer_format sof_buf_fmt)
{
	switch (sof_buf_fmt) {
	case SOF_IPC_BUFFER_INTERLEAVED:
		return DAX_BUFFER_LAYOUT_INTERLEAVED;
	case SOF_IPC_BUFFER_NONINTERLEAVED:
		return DAX_BUFFER_LAYOUT_NONINTERLEAVED;
	default:
		return DAX_BUFFER_LAYOUT_UNSUPPORTED;
	}
}

static void dax_buffer_release(struct dax_buffer *dax_buff)
{
	if (dax_buff->addr) {
		rfree(dax_buff->addr);
		dax_buff->addr = NULL;
	}
	dax_buff->size = 0;
	dax_buff->avail = 0;
	dax_buff->free = 0;
}

static int dax_buffer_alloc(struct dax_buffer *dax_buff, uint32_t bytes)
{
	dax_buffer_release(dax_buff);
	dax_buff->addr = rballoc(SOF_MEM_CAPS_RAM, bytes);
	if (!dax_buff->addr)
		dax_buff->addr = rzalloc(SOF_MEM_CAPS_RAM, bytes);
	if (!dax_buff->addr)
		return -ENOMEM;

	dax_buff->size = bytes;
	dax_buff->avail = 0;
	dax_buff->free = bytes;
	return 0;
}

/* After reading from buffer */
static void dax_buffer_consume(struct dax_buffer *dax_buff, uint32_t bytes)
{
	bytes = MIN(bytes, dax_buff->avail);
	memmove(dax_buff->addr, (uint8_t *)dax_buff->addr + bytes, dax_buff->avail - bytes);
	dax_buff->avail = dax_buff->avail - bytes;
	dax_buff->free = dax_buff->size - dax_buff->avail;
}

/* After writing to buffer */
static void dax_buffer_produce(struct dax_buffer *dax_buff, uint32_t bytes)
{
	dax_buff->avail += bytes;
	dax_buff->avail = MIN(dax_buff->avail, dax_buff->size);
	dax_buff->free = dax_buff->size - dax_buff->avail;
}

static int set_tuning_file(struct processing_module *mod, void *value, uint32_t size)
{
	int ret = 0;
	struct comp_dev *dev = mod->dev;
	struct sof_dax *dax_ctx = module_get_private_data(mod);

	if (dax_buffer_alloc(&dax_ctx->tuning_file_buffer, size) != 0) {
		comp_err(dev, "allocate %u bytes failed for tuning file", size);
		ret = -ENOMEM;
	} else {
		memcpy_s(dax_ctx->tuning_file_buffer.addr,
			 dax_ctx->tuning_file_buffer.free,
			 value,
			 size);
	}

	comp_info(dev, "allocated: tuning %u, ret %d", dax_ctx->tuning_file_buffer.size, ret);
	return ret;
}

static int set_enable(struct processing_module *mod, int32_t enable)
{
	int ret = 0;
	struct sof_dax *dax_ctx = module_get_private_data(mod);

	if (enable) {
		ret = dax_set_enable(1, dax_ctx);
		dax_ctx->enable = (ret == 0 ? 1 : 0);
	} else {
		dax_ctx->enable = 0;
		dax_set_enable(0, dax_ctx);
	}

	comp_info(mod->dev, "set dax enable %d, ret %d", enable, ret);
	return ret;
}

static int set_volume(struct processing_module *mod, int32_t abs_volume)
{
	int ret;
	struct sof_dax *dax_ctx = module_get_private_data(mod);

	dax_ctx->volume = abs_volume;
	if (!dax_ctx->enable)
		return 0;

	ret = dax_set_volume(abs_volume, dax_ctx);
	comp_info(mod->dev, "set volume %d, ret %d", abs_volume, ret);
	return ret;
}

static int set_device(struct processing_module *mod, int32_t out_device)
{
	int ret;
	struct sof_dax *dax_ctx = module_get_private_data(mod);

	dax_ctx->out_device = out_device;
	ret = dax_set_device(out_device, dax_ctx);

	comp_info(mod->dev, "set device %d, ret %d", out_device, ret);
	return ret;
}

static int set_crosstalk_cancellation_enable(struct processing_module *mod, int32_t enable)
{
	int ret;
	struct sof_dax *dax_ctx = module_get_private_data(mod);

	dax_ctx->ctc_enable = enable;
	ret = dax_set_ctc_enable(enable, dax_ctx);

	comp_info(mod->dev, "set ctc enable %d, ret %d", enable, ret);
	return ret;
}

static int update_params_from_buffer(struct processing_module *mod, void *params, uint32_t size);

static int set_profile(struct processing_module *mod, int32_t profile_id)
{
	int ret = -EINVAL;
	struct comp_dev *dev = mod->dev;
	struct sof_dax *dax_ctx = module_get_private_data(mod);
	uint32_t params_sz = 0;
	void *params;

	dax_ctx->profile = profile_id;
	if (!dax_ctx->enable)
		return 0;

	params = dax_find_params(DAX_PARAM_ID_PROFILE, profile_id, &params_sz, dax_ctx);
	if (params)
		ret = update_params_from_buffer(mod, params, params_sz);

	comp_info(dev, "switched to profile %d, ret %d", profile_id, ret);
	return ret;
}

static int set_tuning_device(struct processing_module *mod, int32_t tuning_device)
{
	int ret = -EINVAL;
	struct comp_dev *dev = mod->dev;
	struct sof_dax *dax_ctx = module_get_private_data(mod);
	uint32_t params_sz = 0;
	void *params;

	dax_ctx->tuning_device = tuning_device;
	if (!dax_ctx->enable)
		return 0;

	params = dax_find_params(DAX_PARAM_ID_TUNING_DEVICE, tuning_device, &params_sz, dax_ctx);
	if (params)
		ret = update_params_from_buffer(mod, params, params_sz);

	comp_info(dev, "switched to tuning device %d, ret %d", tuning_device, ret);
	return ret;
}

static int set_content_processing_enable(struct processing_module *mod, int32_t enable)
{
	int ret = -EINVAL;
	struct comp_dev *dev = mod->dev;
	struct sof_dax *dax_ctx = module_get_private_data(mod);
	uint32_t params_sz = 0;
	void *params;

	dax_ctx->content_processing_enable = enable;
	if (!dax_ctx->enable)
		return 0;

	params = dax_find_params(DAX_PARAM_ID_CP_ENABLE, enable, &params_sz, dax_ctx);
	if (params)
		ret = update_params_from_buffer(mod, params, params_sz);

	comp_info(dev, "set content processing enable %d, ret %d", enable, ret);
	return ret;
}

static int dax_set_param_wrapper(struct processing_module *mod,
				 uint32_t id, void *value, uint32_t size)
{
	int ret = 0;
	struct comp_dev *dev = mod->dev;
	struct sof_dax *dax_ctx = module_get_private_data(mod);
	int32_t tmp_val;

	switch (id) {
	case DAX_PARAM_ID_TUNING_FILE:
		set_tuning_file(mod, value, size);
		break;
	case DAX_PARAM_ID_ENABLE:
		tmp_val = *((int32_t *)value);
		tmp_val = !!tmp_val;
		if (dax_ctx->enable != tmp_val) {
			dax_ctx->enable = tmp_val;
			dax_ctx->update_flags |= DAX_ENABLE_MASK;
		}
		break;
	case DAX_PARAM_ID_ABSOLUTE_VOLUME:
		dax_ctx->volume = *((int32_t *)value);
		dax_ctx->update_flags |= DAX_VOLUME_MASK;
		break;
	case DAX_PARAM_ID_OUT_DEVICE:
		tmp_val = *((int32_t *)value);
		if (dax_ctx->out_device != tmp_val) {
			dax_ctx->out_device = tmp_val;
			dax_ctx->update_flags |= DAX_DEVICE_MASK;
		}
		break;
	case DAX_PARAM_ID_PROFILE:
		tmp_val = *((int32_t *)value);
		if (dax_ctx->profile != tmp_val) {
			dax_ctx->profile = tmp_val;
			dax_ctx->update_flags |= DAX_PROFILE_MASK;
		}
		break;
	case DAX_PARAM_ID_CP_ENABLE:
		tmp_val = *((int32_t *)value);
		tmp_val = !!tmp_val;
		if (dax_ctx->content_processing_enable != tmp_val) {
			dax_ctx->content_processing_enable = tmp_val;
			dax_ctx->update_flags |= DAX_CP_MASK;
		}
		break;
	case DAX_PARAM_ID_CTC_ENABLE:
		tmp_val = *((int32_t *)value);
		tmp_val = !!tmp_val;
		if (dax_ctx->ctc_enable != tmp_val) {
			dax_ctx->ctc_enable = tmp_val;
			dax_ctx->update_flags |= DAX_CTC_MASK;
		}
		break;
	case DAX_PARAM_ID_ENDPOINT:
		if (dax_ctx->endpoint == *((int32_t *)value)) {
			ret = update_params_from_buffer(mod, (uint8_t *)value + 4, size - 4);
			comp_info(dev, "switched to endpoint %d, ret %d", dax_ctx->endpoint, ret);
		}
		break;
	default:
		ret = dax_set_param(id, (void *)(value), size, dax_ctx);
		comp_info(dev, "dax_set_param: ret %d, id %#x, size %u, value %s",
			  ret, id, size >> 2, get_params_str(value, size));
		break;
	}

	return ret;
}

static int update_params_from_buffer(struct processing_module *mod, void *data, uint32_t data_size)
{
	struct comp_dev *dev = mod->dev;
	struct module_param *param;
	void *pos = data;
	const uint32_t param_header_size = 8;
	uint32_t param_data_size;

	for (uint32_t i = 0; i < data_size;) {
		param = (struct module_param *)(pos);
		if (param->size < param_header_size ||
		    param->size > data_size - i ||
		    (param->size & 0x03) != 0) {
			comp_err(dev, "invalid param %#x, param size %u, pos %u",
				 param->id, param->size, i);
			return -EINVAL;
		}

		if (param->size > param_header_size) {
			param_data_size = param->size - param_header_size;
			dax_set_param_wrapper(mod, param->id, (void *)(param->data),
					      param_data_size);
		}

		pos = (void *)((uint8_t *)(pos) + param->size);
		i += param->size;
	}

	return 0;
}

static void check_and_update_settings(struct processing_module *mod)
{
	struct sof_dax *dax_ctx = module_get_private_data(mod);

	if (dax_ctx->update_flags & DAX_ENABLE_MASK) {
		set_enable(mod, dax_ctx->enable);
		if (dax_ctx->enable) {
			dax_ctx->update_flags |= DAX_DEVICE_MASK;
			dax_ctx->update_flags |= DAX_VOLUME_MASK;
		}
		dax_ctx->update_flags &= ~DAX_ENABLE_MASK;
		return;
	}
	if (dax_ctx->update_flags & DAX_DEVICE_MASK) {
		set_device(mod, dax_ctx->out_device);
		set_tuning_device(mod, dax_ctx->tuning_device);
		dax_ctx->update_flags |= DAX_PROFILE_MASK;
		dax_ctx->update_flags &= ~DAX_DEVICE_MASK;
		return;
	}
	if (dax_ctx->update_flags & DAX_CTC_MASK) {
		set_crosstalk_cancellation_enable(mod, dax_ctx->ctc_enable);
		dax_ctx->update_flags |= DAX_PROFILE_MASK;
		dax_ctx->update_flags &= ~DAX_CTC_MASK;
		return;
	}
	if (dax_ctx->update_flags & DAX_PROFILE_MASK) {
		set_profile(mod, dax_ctx->profile);
		if (!dax_ctx->content_processing_enable)
			dax_ctx->update_flags |= DAX_CP_MASK;
		dax_ctx->update_flags &= ~DAX_PROFILE_MASK;
		return;
	}
	if (dax_ctx->update_flags & DAX_CP_MASK) {
		set_content_processing_enable(mod, dax_ctx->content_processing_enable);
		dax_ctx->update_flags &= ~DAX_CP_MASK;
		return;
	}
	if (dax_ctx->update_flags & DAX_VOLUME_MASK) {
		set_volume(mod, dax_ctx->volume);
		dax_ctx->update_flags &= ~DAX_VOLUME_MASK;
	}
}

static int sof_dax_free(struct processing_module *mod)
{
	struct sof_dax *dax_ctx = module_get_private_data(mod);

	if (dax_ctx) {
		dax_free(dax_ctx);
		dax_buffer_release(&dax_ctx->persist_buffer);
		dax_buffer_release(&dax_ctx->scratch_buffer);
		dax_buffer_release(&dax_ctx->tuning_file_buffer);
		dax_buffer_release(&dax_ctx->input_buffer);
		dax_buffer_release(&dax_ctx->output_buffer);
		comp_data_blob_handler_free(dax_ctx->blob_handler);
		dax_ctx->blob_handler = NULL;
		rfree(dax_ctx);
		module_set_private_data(mod, NULL);
	}
	return 0;
}

static int sof_dax_init(struct processing_module *mod)
{
	int ret;
	struct comp_dev *dev = mod->dev;
	struct module_data *md = &mod->priv;
	struct sof_dax *dax_ctx;
	uint32_t persist_sz;
	uint32_t scratch_sz;

	md->private = rzalloc(SOF_MEM_CAPS_RAM, sizeof(struct sof_dax));
	if (!md->private) {
		comp_err(dev, "failed to allocate %u bytes for initialization",
			 sizeof(struct sof_dax));
		return -ENOMEM;
	}
	dax_ctx = module_get_private_data(mod);
	dax_ctx->enable = 0;
	dax_ctx->profile = 0;
	dax_ctx->out_device = 0;
	dax_ctx->ctc_enable = 1;
	dax_ctx->content_processing_enable = 1;
	dax_ctx->volume = 1 << 23;
	dax_ctx->update_flags = 0;

	dax_ctx->blob_handler = comp_data_blob_handler_new(dev);
	if (!dax_ctx->blob_handler) {
		comp_err(dev, "create blob handler failed");
		ret = -ENOMEM;
		goto err;
	}

	persist_sz = dax_query_persist_memory(dax_ctx);
	if (dax_buffer_alloc(&dax_ctx->persist_buffer, persist_sz) != 0) {
		comp_err(dev, "allocate %u bytes failed for persist", persist_sz);
		ret = -ENOMEM;
		goto err;
	}
	scratch_sz = dax_query_scratch_memory(dax_ctx);
	if (dax_buffer_alloc(&dax_ctx->scratch_buffer, scratch_sz) != 0) {
		comp_err(dev, "allocate %u bytes failed for scratch", scratch_sz);
		ret = -ENOMEM;
		goto err;
	}
	ret = dax_init(dax_ctx);
	if (ret != 0) {
		comp_err(dev, "dax instance initialization failed, ret %d", ret);
		goto err;
	}

	comp_info(dev, "allocated: persist %u, scratch %u. version: %s",
		  persist_sz, scratch_sz, dax_get_version());
	return 0;

err:
	sof_dax_free(mod);
	return ret;
}

static int check_media_format(struct processing_module *mod)
{
	int ret = 0;
	struct comp_dev *dev = mod->dev;
	struct comp_buffer *source = comp_dev_get_first_data_producer(dev);
	struct comp_buffer *sink = comp_dev_get_first_data_consumer(dev);
	const struct audio_stream *src_stream = &source->stream;
	const struct audio_stream *sink_stream = &sink->stream;
	struct sof_dax *dax_ctx = module_get_private_data(mod);

	if (audio_stream_get_frm_fmt(src_stream) != audio_stream_get_frm_fmt(sink_stream) ||
	    sof_to_dax_frame_fmt(audio_stream_get_frm_fmt(src_stream)) == DAX_FMT_UNSUPPORTED) {
		comp_err(dev, "unsupported format, source %d, sink %d",
			 audio_stream_get_frm_fmt(src_stream),
			 audio_stream_get_frm_fmt(sink_stream));
		ret = -EINVAL;
	}

	if (audio_stream_get_rate(src_stream) != audio_stream_get_rate(sink_stream) ||
	    sof_to_dax_sample_rate(audio_stream_get_rate(src_stream)) == DAX_RATE_UNSUPPORTED) {
		comp_err(dev, "unsupported sample rate, source %d, sink %d",
			 audio_stream_get_rate(src_stream), audio_stream_get_rate(sink_stream));
		ret = -EINVAL;
	}

	if (audio_stream_get_channels(sink_stream) != 2 ||
	    sof_to_dax_channels(audio_stream_get_channels(src_stream)) ==
				DAX_CHANNLES_UNSUPPORTED) {
		comp_err(dev, "unsupported number of channels, source %d, sink %d",
			 audio_stream_get_channels(src_stream),
			 audio_stream_get_channels(sink_stream));
		ret = -EINVAL;
	}

	if (audio_stream_get_buffer_fmt(src_stream) != audio_stream_get_buffer_fmt(sink_stream) ||
	    sof_to_dax_buffer_layout(audio_stream_get_buffer_fmt(src_stream)) ==
				     DAX_BUFFER_LAYOUT_UNSUPPORTED) {
		comp_err(dev, "unsupported buffer layout %d",
			 audio_stream_get_buffer_fmt(src_stream));
		ret = -EINVAL;
	}

	if (ret != 0)
		return ret;

	dax_ctx->input_media_format.data_format =
		sof_to_dax_frame_fmt(audio_stream_get_frm_fmt(src_stream));
	dax_ctx->input_media_format.sampling_rate =
		sof_to_dax_sample_rate(audio_stream_get_rate(src_stream));
	dax_ctx->input_media_format.num_channels =
		sof_to_dax_channels(audio_stream_get_channels(src_stream));
	dax_ctx->input_media_format.layout =
		sof_to_dax_buffer_layout(audio_stream_get_buffer_fmt(src_stream));
	dax_ctx->input_media_format.bytes_per_sample = audio_stream_sample_bytes(src_stream);

	dax_ctx->output_media_format.data_format =
		sof_to_dax_frame_fmt(audio_stream_get_frm_fmt(sink_stream));
	dax_ctx->output_media_format.sampling_rate =
		sof_to_dax_sample_rate(audio_stream_get_rate(sink_stream));
	dax_ctx->output_media_format.num_channels =
		sof_to_dax_channels(audio_stream_get_channels(sink_stream));
	dax_ctx->output_media_format.layout =
		sof_to_dax_buffer_layout(audio_stream_get_buffer_fmt(sink_stream));
	dax_ctx->output_media_format.bytes_per_sample = audio_stream_sample_bytes(sink_stream);

	comp_info(dev, "format %d, sample rate %d, channels %d, data format %d",
		  dax_ctx->input_media_format.data_format,
		  dax_ctx->input_media_format.sampling_rate,
		  dax_ctx->input_media_format.num_channels,
		  dax_ctx->input_media_format.data_format);
	return 0;
}

static int sof_dax_prepare(struct processing_module *mod, struct sof_source **sources,
			   int num_of_sources, struct sof_sink **sinks, int num_of_sinks)
{
	int ret;
	struct comp_dev *dev = mod->dev;
	struct sof_dax *dax_ctx = module_get_private_data(mod);
	uint32_t ibs, obs;

	if (num_of_sources != 1 || num_of_sinks != 1) {
		comp_err(dev, "unsupported number of buffers, in %d, out %d",
			 num_of_sources, num_of_sinks);
		return -EINVAL;
	}

	ret = check_media_format(mod);
	if (ret != 0)
		return ret;

	dax_ctx->sof_period_bytes = dev->frames *
		dax_ctx->output_media_format.num_channels *
		dax_ctx->output_media_format.bytes_per_sample;
	dax_ctx->period_bytes = dax_query_period_frames(dax_ctx) *
		dax_ctx->output_media_format.num_channels *
		dax_ctx->output_media_format.bytes_per_sample;
	dax_ctx->period_us = 1000000 * dax_ctx->period_bytes /
		(dax_ctx->output_media_format.bytes_per_sample *
		 dax_ctx->output_media_format.num_channels *
		 dax_ctx->output_media_format.sampling_rate);

	ibs = (dax_query_period_frames(dax_ctx) + dev->frames) *
		dax_ctx->input_media_format.num_channels *
		dax_ctx->input_media_format.bytes_per_sample;
	obs = dax_ctx->period_bytes + dax_ctx->sof_period_bytes;
	if (dax_buffer_alloc(&dax_ctx->input_buffer, ibs) != 0) {
		comp_err(dev, "allocate %u bytes failed for input", ibs);
		ret = -ENOMEM;
		goto err;
	}
	if (dax_buffer_alloc(&dax_ctx->output_buffer, obs) != 0) {
		comp_err(dev, "allocate %u bytes failed for output", obs);
		ret = -ENOMEM;
		goto err;
	}
	memset(dax_ctx->output_buffer.addr, 0, dax_ctx->output_buffer.size);
	dax_buffer_produce(&dax_ctx->output_buffer, dax_ctx->output_buffer.size);
	comp_info(dev, "allocated: ibs %u, obs %u", ibs, obs);

	return 0;

err:
	dax_buffer_release(&dax_ctx->input_buffer);
	dax_buffer_release(&dax_ctx->output_buffer);
	return ret;
}

static int sof_dax_process(struct processing_module *mod, struct sof_source **sources,
			   int num_of_sources, struct sof_sink **sinks, int num_of_sinks)
{
	struct sof_dax *dax_ctx = module_get_private_data(mod);
	struct sof_source *source = sources[0];
	struct sof_sink *sink = sinks[0];
	uint8_t *buf, *bufstart, *bufend, *dax_buf;
	size_t bufsz;
	struct dax_buffer *dax_input_buffer = &dax_ctx->input_buffer;
	struct dax_buffer *dax_output_buffer = &dax_ctx->output_buffer;
	uint32_t consumed_bytes, processed_bytes, produced_bytes;

	/* source stream -> internal input buffer */
	consumed_bytes = MIN(source_get_data_available(source), dax_input_buffer->free);
	source_get_data(source, consumed_bytes, (void *)&buf, (void *)&bufstart, &bufsz);
	bufend = &bufstart[bufsz];
	dax_buf = (uint8_t *)(dax_input_buffer->addr);
	cir_buf_copy(buf, bufstart, bufend,
		     dax_buf + dax_input_buffer->avail,
		     dax_buf,
		     dax_buf + dax_input_buffer->size,
		     consumed_bytes);
	dax_buffer_produce(dax_input_buffer, consumed_bytes);
	source_release_data(source, consumed_bytes);

	check_and_update_settings(mod);

	/* internal input buffer -> internal output buffer */
	processed_bytes = dax_process(dax_ctx);
	dax_buffer_consume(dax_input_buffer, processed_bytes);
	dax_buffer_produce(dax_output_buffer, processed_bytes);

	/* internal output buffer -> sink stream */
	produced_bytes = MIN(dax_output_buffer->avail, sink_get_free_size(sink));
	if (produced_bytes > 0) {
		sink_get_buffer(sink, produced_bytes, (void *)&buf, (void *)&bufstart, &bufsz);
		bufend = &bufstart[bufsz];
		dax_buf = (uint8_t *)(dax_output_buffer->addr);
		cir_buf_copy(dax_buf, dax_buf, dax_buf + dax_output_buffer->size,
			     buf, bufstart, bufend, produced_bytes);
		dax_buffer_consume(dax_output_buffer, produced_bytes);
		sink_commit_buffer(sink, produced_bytes);
	}

	return 0;
}

static int sof_dax_set_configuration(struct processing_module *mod, uint32_t config_id,
				     enum module_cfg_fragment_position pos,
				     uint32_t data_offset_size, const uint8_t *fragment,
				     size_t fragment_size,
				     uint8_t *response, size_t response_size)
{
	int ret;
	struct comp_dev *dev = mod->dev;
	struct sof_dax *dax_ctx = module_get_private_data(mod);
	int32_t dax_param_id = 0;
	int32_t val;

	if (fragment_size == 0)
		return 0;

#if CONFIG_IPC_MAJOR_4
	const struct sof_ipc4_control_msg_payload *ctl = NULL;

	switch (config_id) {
	case 0: /* IPC4_VOLUME */
		/* ipc4_peak_volume_config::target_volume */
		val = ((const int32_t *)fragment)[1];
		val = sat_int32(Q_SHIFT_RND((int64_t)val, 31, 23));
		dax_param_id = DAX_PARAM_ID_ABSOLUTE_VOLUME;
		break;
	case SOF_IPC4_SWITCH_CONTROL_PARAM_ID:
		ctl = (const struct sof_ipc4_control_msg_payload *)fragment;
		if (ctl->num_elems != 1)
			return -EINVAL;

		val = ctl->chanv[0].value;
		switch (ctl->id) {
		case DAX_SWITCH_ENABLE_CONTROL_ID:
			dax_param_id = DAX_PARAM_ID_ENABLE;
			break;
		case DAX_SWITCH_CP_CONTROL_ID:
			dax_param_id = DAX_PARAM_ID_CP_ENABLE;
			break;
		case DAX_SWITCH_CTC_CONTROL_ID:
			dax_param_id = DAX_PARAM_ID_CTC_ENABLE;
			break;
		default:
			comp_err(dev, "unknown switch control %d", ctl->id);
			return -EINVAL;
		}
		break;
	case SOF_IPC4_ENUM_CONTROL_PARAM_ID:
		ctl = (const struct sof_ipc4_control_msg_payload *)fragment;
		if (ctl->num_elems != 1)
			return -EINVAL;

		val = ctl->chanv[0].value;
		switch (ctl->id) {
		case DAX_ENUM_PROFILE_CONTROL_ID:
			dax_param_id = DAX_PARAM_ID_PROFILE;
			break;
		case DAX_ENUM_DEVICE_CONTROL_ID:
			dax_param_id = DAX_PARAM_ID_OUT_DEVICE;
			break;
		default:
			comp_err(dev, "unknown enum control %d", ctl->id);
			return -EINVAL;
		}
		break;
	default:
		break;
	}
#else
	struct sof_ipc_ctrl_data *ctl = (struct sof_ipc_ctrl_data *)fragment;

	switch (ctl->cmd) {
	case SOF_CTRL_CMD_VOLUME:
		val = ctl->chanv[0].value;
		dax_param_id = DAX_PARAM_ID_ABSOLUTE_VOLUME;
		break;
	case SOF_CTRL_CMD_SWITCH:
		if (ctl->num_elems != 1)
			return -EINVAL;

		val = ctl->chanv[0].value;
		switch (ctl->index) {
		case DAX_SWITCH_ENABLE_CONTROL_ID:
			dax_param_id = DAX_PARAM_ID_ENABLE;
			break;
		case DAX_SWITCH_CP_CONTROL_ID:
			dax_param_id = DAX_PARAM_ID_CP_ENABLE;
			break;
		case DAX_SWITCH_CTC_CONTROL_ID:
			dax_param_id = DAX_PARAM_ID_CTC_ENABLE;
			break;
		default:
			comp_err(dev, "unknown switch control %d", ctl->index);
			return -EINVAL;
		}
		break;
	case SOF_CTRL_CMD_ENUM:
		if (ctl->num_elems != 1)
			return -EINVAL;

		val = ctl->chanv[0].value;
		switch (ctl->index) {
		case DAX_ENUM_PROFILE_CONTROL_ID:
			dax_param_id = DAX_PARAM_ID_PROFILE;
			break;
		case DAX_ENUM_DEVICE_CONTROL_ID:
			dax_param_id = DAX_PARAM_ID_OUT_DEVICE;
			break;
		default:
			comp_err(dev, "unknown enum control %d", ctl->index);
			return -EINVAL;
		}
		break;
	default:
		break;
	}
#endif

	if (dax_param_id == 0) {
		ret = comp_data_blob_set(dax_ctx->blob_handler, pos,
					 data_offset_size, fragment, fragment_size);
		if (ret == 0 && (pos == MODULE_CFG_FRAGMENT_LAST ||
				 pos == MODULE_CFG_FRAGMENT_SINGLE)) {
			void *data = NULL;
			size_t data_size = 0;

			data = comp_get_data_blob(dax_ctx->blob_handler, &data_size, NULL);
			if (data && data_size > 0)
				update_params_from_buffer(mod, data, data_size);
		}
	} else {
		ret = dax_set_param_wrapper(mod, dax_param_id, &val, sizeof(val));
	}
	return ret;
}

static const struct module_interface dolby_dax_audio_processing_interface = {
	.init = sof_dax_init,
	.prepare = sof_dax_prepare,
	.process = sof_dax_process,
	.set_configuration = sof_dax_set_configuration,
	.free = sof_dax_free,
};

DECLARE_MODULE_ADAPTER(dolby_dax_audio_processing_interface, dolby_dax_audio_processing_uuid,
		       dolby_dax_audio_processing_tr);
SOF_MODULE_INIT(dolby_dax_audio_processing,
		sys_comp_module_dolby_dax_audio_processing_interface_init);
