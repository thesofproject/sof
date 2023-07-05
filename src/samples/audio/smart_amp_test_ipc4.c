// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Intel Corporation. All rights reserved.
//
// Author: Bartosz Kokoszko <bartoszx.kokoszko@linux.intel.com>
#include <sof/compiler_attributes.h>
#include <sof/samples/audio/smart_amp_test.h>
#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/ipc-config.h>
#include <sof/trace/trace.h>
#include <sof/ipc/msg.h>
#include <rtos/init.h>
#include <sof/ut.h>

#ifndef __ZEPHYR__
#include <rtos/mutex.h>
#endif
#include <ipc4/module.h>

LOG_MODULE_REGISTER(smart_amp_test, CONFIG_SOF_LOG_LEVEL);

/* 167a961e-8ae4-11ea-89f1-000c29ce1635 */
DECLARE_SOF_RT_UUID("smart_amp-test", smart_amp_comp_uuid, 0x167a961e, 0x8ae4,
		    0x11ea, 0x89, 0xf1, 0x00, 0x0c, 0x29, 0xce, 0x16, 0x35);

DECLARE_TR_CTX(smart_amp_comp_tr, SOF_UUID(smart_amp_comp_uuid),
	       LOG_LEVEL_INFO);
typedef int(*smart_amp_proc)(struct processing_module *mod,
			     struct input_stream_buffer *bsource,
			     struct output_stream_buffer *bsink, uint32_t frames,
			     int8_t *chan_map);
struct smart_amp_data {
	struct sof_smart_amp_ipc4_config ipc4_cfg;
	struct sof_smart_amp_config config;
	struct comp_data_blob_handler *model_handler;
	void *data_blob;
	size_t data_blob_size;
	smart_amp_proc process;
	uint32_t out_channels;
};

static int smart_amp_init(struct processing_module *mod)
{
	struct smart_amp_data *sad;
	struct comp_dev *dev = mod->dev;
	struct module_data *mod_data = &mod->priv;
	const size_t in_size = sizeof(struct ipc4_input_pin_format) * SMART_AMP_NUM_IN_PINS;
	const size_t out_size = sizeof(struct ipc4_output_pin_format) * SMART_AMP_NUM_OUT_PINS;
	int ret;
	const struct ipc4_base_module_extended_cfg *base_cfg = mod_data->cfg.init_data;

	comp_dbg(dev, "smart_amp_init()");
	sad = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(*sad));
	if (!sad)
		return -ENOMEM;

	mod_data->private = sad;

	/* component model data handler */
	sad->model_handler = comp_data_blob_handler_new(dev);
	if (!sad->model_handler) {
		ret = -ENOMEM;
		goto sad_fail;
	}

	if (base_cfg->base_cfg_ext.nb_input_pins != SMART_AMP_NUM_IN_PINS ||
	    base_cfg->base_cfg_ext.nb_output_pins != SMART_AMP_NUM_OUT_PINS) {
		comp_err(dev, "smart_amp_init(): Invalid pin configuration");
		ret = -EINVAL;
		goto sad_fail;
	}

	/* Copy the pin formats */
	memcpy_s(sad->ipc4_cfg.input_pins, in_size,
		 base_cfg->base_cfg_ext.pin_formats, in_size);
	memcpy_s(&sad->ipc4_cfg.output_pin, out_size,
		 &base_cfg->base_cfg_ext.pin_formats[in_size], out_size);

	mod->max_sources = SMART_AMP_NUM_IN_PINS;

	return 0;

sad_fail:
	comp_data_blob_handler_free(sad->model_handler);
	rfree(sad);
	return ret;
}

static void smart_amp_set_params(struct processing_module *mod)
{
	const struct ipc4_audio_format *audio_fmt = &mod->priv.cfg.base_cfg.audio_fmt;
	struct sof_ipc_stream_params *params = mod->stream_params;
	struct comp_dev *dev = mod->dev;
	struct smart_amp_data *sad = module_get_private_data(mod);
	struct comp_buffer *sink;
	struct comp_buffer __sparse_cache *sink_c;
	enum sof_ipc_frame frame_fmt, valid_fmt;
	int i;

	memset(params, 0, sizeof(*params));
	params->channels = audio_fmt->channels_count;
	params->rate = audio_fmt->sampling_frequency;
	params->sample_container_bytes = audio_fmt->depth / 8;
	params->sample_valid_bytes = audio_fmt->valid_bit_depth / 8;
	params->buffer_fmt = audio_fmt->interleaving_style;
	params->buffer.size = mod->priv.cfg.base_cfg.ibs;

	for (i = 0; i < SOF_IPC_MAX_CHANNELS; i++)
		params->chmap[i] = (audio_fmt->ch_map >> i * 4) & 0xf;

	/* update sink format */
	if (!list_is_empty(&dev->bsink_list)) {
		struct ipc4_output_pin_format *sink_fmt = &sad->ipc4_cfg.output_pin;
		struct ipc4_audio_format out_fmt = sink_fmt->audio_fmt;

		sink = list_first_item(&dev->bsink_list, struct comp_buffer, source_list);
		sink_c = buffer_acquire(sink);

		audio_stream_fmt_conversion(out_fmt.depth,
					    out_fmt.valid_bit_depth,
					    &frame_fmt, &valid_fmt,
					    out_fmt.s_type);

		audio_stream_set_frm_fmt(&sink_c->stream, frame_fmt);
		audio_stream_set_valid_fmt(&sink_c->stream, valid_fmt);
		audio_stream_set_channels(&sink_c->stream, out_fmt.channels_count);
		audio_stream_set_rate(&sink_c->stream, out_fmt.sampling_frequency);
		audio_stream_set_buffer_fmt(&sink_c->stream, out_fmt.interleaving_style);
		params->frame_fmt = audio_stream_get_frm_fmt(&sink_c->stream);
		sink_c->hw_params_configured = true;
		buffer_release(sink_c);
	}
}

static int smart_amp_set_config(struct processing_module *mod, uint32_t config_id,
				enum module_cfg_fragment_position pos, uint32_t data_offset_size,
				const uint8_t *fragment, size_t fragment_size, uint8_t *response,
				size_t response_size)
{
	struct comp_dev *dev = mod->dev;
	struct smart_amp_data *sad = module_get_private_data(mod);

	comp_dbg(dev, "smart_amp_set_config()");

	switch (config_id) {
	case SMART_AMP_SET_MODEL:
		return comp_data_blob_set(sad->model_handler, pos,
					 data_offset_size, fragment, fragment_size);
	case SMART_AMP_SET_CONFIG:
		if (fragment_size != sizeof(sad->config)) {
			comp_err(dev, "smart_amp_set_config(): invalid config size %u, expect %u",
				 fragment_size, sizeof(struct sof_smart_amp_config));
			return -EINVAL;
		}
		comp_dbg(dev, "smart_amp_set_config(): config size = %u", fragment_size);
		memcpy_s(&sad->config, sizeof(sad->config), fragment, fragment_size);
		return 0;
	default:
		return -EINVAL;
	}
}

static inline int smart_amp_get_config(struct processing_module *mod,
				       uint32_t config_id, uint32_t *data_offset_size,
				       uint8_t *fragment, size_t fragment_size)
{
	struct smart_amp_data *sad = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	int ret;

	comp_dbg(dev, "smart_amp_get_config()");

	switch (config_id) {
	case SMART_AMP_GET_CONFIG:
		ret = memcpy_s(fragment, fragment_size, &sad->config, sizeof(sad->config));
		if (ret) {
			comp_err(dev, "smart_amp_get_config(): wrong config size %d",
				 fragment_size);
			return ret;
		}
		*data_offset_size = sizeof(sad->config);
		return 0;
	default:
		return -EINVAL;
	}
}

static int smart_amp_free(struct processing_module *mod)
{
	struct smart_amp_data *sad = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;

	comp_dbg(dev, "smart_amp_free()");
	comp_data_blob_handler_free(sad->model_handler);
	rfree(sad);
	return 0;
}

static int smart_amp_params(struct processing_module *mod)
{
	struct sof_ipc_stream_params *params = mod->stream_params;
	struct comp_dev *dev = mod->dev;
	int ret;

	comp_dbg(dev, "smart_amp_params()");
	smart_amp_set_params(mod);
	ret = comp_verify_params(dev, BUFF_PARAMS_CHANNELS, params);
	if (ret < 0) {
		comp_err(dev, "smart_amp_params(): pcm params verification failed.");
		return -EINVAL;
	}
	return 0;
}

static int smart_amp_process_s16(struct processing_module *mod,
				 struct input_stream_buffer *bsource,
				 struct output_stream_buffer *bsink,
				 uint32_t frames, int8_t *chan_map)
{
	struct smart_amp_data *sad = module_get_private_data(mod);
	struct audio_stream __sparse_cache *source = bsource->data;
	struct audio_stream __sparse_cache *sink = bsink->data;
	int16_t *src;
	int16_t *dest;
	uint32_t in_frag = 0;
	uint32_t out_frag = 0;
	int i;
	int j;

	bsource->consumed += frames * audio_stream_get_channels(source) * sizeof(int16_t);
	for (i = 0; i < frames; i++) {
		for (j = 0 ; j < sad->out_channels; j++) {
			if (chan_map[j] != -1) {
				src = audio_stream_read_frag_s16(source,
								 in_frag +
								 chan_map[j]);
				dest = audio_stream_write_frag_s16(sink,
								   out_frag);
				*dest = *src;
			}
			out_frag++;
		}
		in_frag += audio_stream_get_channels(source);
	}
	return 0;
}

static int smart_amp_process_s32(struct processing_module *mod,
				 struct input_stream_buffer *bsource,
				 struct output_stream_buffer *bsink,
				 uint32_t frames, int8_t *chan_map)
{
	struct smart_amp_data *sad = module_get_private_data(mod);
	struct audio_stream __sparse_cache *source = bsource->data;
	struct audio_stream __sparse_cache *sink = bsink->data;
	int32_t *src;
	int32_t *dest;
	uint32_t in_frag = 0;
	uint32_t out_frag = 0;
	int i;
	int j;

	bsource->consumed += frames * audio_stream_get_channels(source) * sizeof(int32_t);
	for (i = 0; i < frames; i++) {
		for (j = 0 ; j < sad->out_channels; j++) {
			if (chan_map[j] != -1) {
				src = audio_stream_read_frag_s32(source,
								 in_frag +
								 chan_map[j]);
				dest = audio_stream_write_frag_s32(sink,
								   out_frag);
				*dest = *src;
			}
			out_frag++;
		}
		in_frag += audio_stream_get_channels(source);
	}

	return 0;
}

static smart_amp_proc get_smart_amp_process(struct comp_dev *dev,
					    struct comp_buffer __sparse_cache *buf)
{
	switch (audio_stream_get_frm_fmt(&buf->stream)) {
	case SOF_IPC_FRAME_S16_LE:
		return smart_amp_process_s16;
	case SOF_IPC_FRAME_S24_4LE:
	case SOF_IPC_FRAME_S32_LE:
		return smart_amp_process_s32;
	default:
		comp_err(dev, "smart_amp_process() error: not supported frame format");
		return NULL;
	}
}

static int smart_amp_process(struct processing_module *mod,
			     struct input_stream_buffer *input_buffers, int num_input_buffers,
			     struct output_stream_buffer *output_buffers, int num_output_buffers)
{
	struct smart_amp_data *sad = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	struct comp_buffer __sparse_cache *fb_buf_c;
	struct comp_buffer __sparse_cache *buf;
	struct module_source_info __sparse_cache *mod_source_info;
	struct input_stream_buffer *fb_input = NULL;
	/* if there is only one input stream, it should be the source input */
	struct input_stream_buffer *src_input = &input_buffers[0];
	uint32_t avail_passthrough_frames;
	uint32_t avail_frames = 0;
	uint32_t sink_bytes;
	uint32_t i;

	mod_source_info = module_source_info_acquire(mod->source_info);

	if (num_input_buffers == SMART_AMP_NUM_IN_PINS)
		for (i = 0; i < num_input_buffers; i++) {
			buf = attr_container_of(input_buffers[i].data,
						struct comp_buffer __sparse_cache,
						stream, __sparse_cache);

			if (IPC4_SINK_QUEUE_ID(buf->id) == SOF_SMART_AMP_FEEDBACK_QUEUE_ID) {
				fb_input = &input_buffers[i];
				fb_buf_c = buf;
			} else {
				src_input = &input_buffers[i];
			}
		}

	avail_passthrough_frames = src_input->size;

	if (fb_input) {
		if (fb_buf_c->source && comp_get_state(dev, fb_buf_c->source) == dev->state) {
			/* feedback */
			avail_frames = MIN(avail_passthrough_frames,
					   fb_input->size);

			sad->process(mod, fb_input, &output_buffers[0],
				     avail_frames, sad->config.feedback_ch_map);
		}
	}

	if (!avail_frames)
		avail_frames = avail_passthrough_frames;

	/* bytes calculation */
	sink_bytes = avail_frames *
		audio_stream_frame_bytes(output_buffers[0].data);

	/* process data */
	sad->process(mod, src_input, &output_buffers[0],
		     avail_frames, sad->config.source_ch_map);

	output_buffers[0].size = sink_bytes;

	module_source_info_release(mod_source_info);
	return 0;
}

static int smart_amp_reset(struct processing_module *mod)
{
	struct comp_dev *dev = mod->dev;

	comp_dbg(dev, "smart_amp_reset()");

	return 0;
}

static int smart_amp_prepare(struct processing_module *mod,
			     struct sof_source __sparse_cache **sources, int num_of_sources,
			     struct sof_sink __sparse_cache **sinks, int num_of_sinks)
{
	struct smart_amp_data *sad = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	struct comp_buffer *source_buffer;
	struct comp_buffer *sink_buffer;
	struct comp_buffer __sparse_cache *buffer_c;
	struct list_item *blist;
	int ret;

	ret = smart_amp_params(mod);
	if (ret < 0)
		return ret;

	comp_dbg(dev, "smart_amp_prepare()");
	/* searching for stream and feedback source buffers */
	list_for_item(blist, &dev->bsource_list) {
		source_buffer = container_of(blist, struct comp_buffer,
					     sink_list);
		buffer_c = buffer_acquire(source_buffer);
		audio_stream_init_alignment_constants(1, 1, &buffer_c->stream);
		if (IPC4_SINK_QUEUE_ID(buffer_c->id) == SOF_SMART_AMP_FEEDBACK_QUEUE_ID) {
			audio_stream_set_channels(&buffer_c->stream, sad->config.feedback_channels);
			audio_stream_set_rate(&buffer_c->stream,
					      mod->priv.cfg.base_cfg.audio_fmt.sampling_frequency);
		}
		buffer_release(buffer_c);
	}

	sink_buffer = list_first_item(&dev->bsink_list, struct comp_buffer, source_list);
	buffer_c = buffer_acquire(sink_buffer);
	sad->out_channels = audio_stream_get_channels(&buffer_c->stream);
	audio_stream_init_alignment_constants(1, 1, &buffer_c->stream);
	sad->process = get_smart_amp_process(dev, buffer_c);
	buffer_release(buffer_c);

	if (!sad->process) {
		comp_err(dev, "smart_amp_prepare(): get_smart_amp_process failed");
		ret = -EINVAL;
	}
	return ret;
}

static struct module_interface smart_amp_interface = {
	.init  = smart_amp_init,
	.prepare = smart_amp_prepare,
	.process_audio_stream = smart_amp_process,
	.set_configuration = smart_amp_set_config,
	.get_configuration = smart_amp_get_config,
	.reset = smart_amp_reset,
	.free = smart_amp_free
};
DECLARE_MODULE_ADAPTER(smart_amp_interface, smart_amp_comp_uuid, smart_amp_comp_tr);
SOF_MODULE_INIT(smart_amp_test, sys_comp_module_smart_amp_interface_init);
