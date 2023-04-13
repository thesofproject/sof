// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2023 Intel Corporation. All rights reserved.
//
// Author: Bartosz Kokoszko <bartoszx.kokoszko@linux.intel.com>
#if CONFIG_IPC_MAJOR_4
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
	struct input_stream_buffer *src_input; /* source input stream buffer*/
	struct input_stream_buffer *fb_input; /* feedback input stream buffer*/
	smart_amp_proc process;
	uint32_t out_channels;
	uint32_t input_stream_num;
};

static int smart_amp_init(struct processing_module *mod)
{
	struct smart_amp_data *sad;
	struct comp_dev *dev = mod->dev;
	struct module_data *mod_data = &mod->priv;
	int bs;
	const struct ipc4_base_module_extended_cfg *base_cfg = mod_data->cfg.init_data;

	comp_dbg(dev, "smart_amp_init()");
	sad = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(*sad));
	if (!sad)
		return -ENOMEM;

	mod_data->private = sad;

	/* component model data handler */
	sad->model_handler = comp_data_blob_handler_new(dev);
	if (!sad->model_handler)
		goto sad_fail;

	if (base_cfg->base_cfg_ext.nb_input_pins != SMART_AMP_NUM_IN_PINS ||
	    base_cfg->base_cfg_ext.nb_output_pins != SMART_AMP_NUM_OUT_PINS) {
		comp_err(dev, "smart_amp_init(): Invalid pin configuration");
		goto sad_fail;
	}

	/* Copy the pin formats */
	bs = sizeof(sad->ipc4_cfg.input_pins) + sizeof(sad->ipc4_cfg.output_pin);
	memcpy_s(sad->ipc4_cfg.input_pins, bs,
		 base_cfg->base_cfg_ext.pin_formats, bs);

	mod->simple_copy = true;
	mod->skip_src_buffer_invalidate = true;
	return 0;

sad_fail:
	comp_data_blob_handler_free(sad->model_handler);
	rfree(sad);
	return -ENOMEM;
}

static void smart_amp_set_params(struct processing_module *mod)
{
	const struct ipc4_audio_format *audio_fmt = &mod->priv.cfg.base_cfg.audio_fmt;
	struct sof_ipc_stream_params *params = mod->stream_params;
	struct comp_dev *dev = mod->dev;
	struct smart_amp_data *sad = module_get_private_data(mod);
	struct comp_buffer *sink;
	struct comp_buffer __sparse_cache *sink_c;
	int i;

	memset(params, 0, sizeof(*params));
	params->channels = audio_fmt->channels_count;
	params->rate = audio_fmt->sampling_frequency;
	params->sample_container_bytes = audio_fmt->depth / 8;
	params->sample_valid_bytes = audio_fmt->valid_bit_depth / 8;
	params->buffer_fmt = audio_fmt->interleaving_style;

	for (i = 0; i < SOF_IPC_MAX_CHANNELS; i++)
		params->chmap[i] = (audio_fmt->ch_map >> i * 4) & 0xf;

	/* update sink format */
	if (!list_is_empty(&dev->bsink_list)) {
		struct ipc4_output_pin_format *sink_fmt = &sad->ipc4_cfg.output_pin;
		struct ipc4_audio_format out_fmt = sink_fmt->audio_fmt;

		sink = list_first_item(&dev->bsink_list, struct comp_buffer, source_list);
		sink_c = buffer_acquire(sink);
		sink_c->stream.channels = out_fmt.channels_count;
		sink_c->stream.rate = out_fmt.sampling_frequency;

		audio_stream_fmt_conversion(out_fmt.depth,
					    out_fmt.valid_bit_depth,
					    &sink_c->stream.frame_fmt,
					    &sink_c->stream.valid_sample_fmt,
					    out_fmt.s_type);

		sink_c->buffer_fmt = out_fmt.interleaving_style;
		params->frame_fmt = sink_c->stream.frame_fmt;
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

	comp_info(dev, "smart_amp_set_config()");

	switch (config_id) {
	case SMART_AMP_SET_MODEL:
		return comp_data_blob_set(sad->model_handler, pos,
					 data_offset_size, fragment, fragment_size);
	case SMART_AMP_SET_CONFIG:
		if (fragment_size != sizeof(struct sof_smart_amp_config)) {
			comp_err(dev, "smart_amp_set_config(): invalid config size %u, expect %u",
				 fragment_size, sizeof(struct sof_smart_amp_config));
			return -EINVAL;
		}
		comp_dbg(dev, "smart_amp_set_config(): config size = %u", fragment_size);
		memcpy_s(&sad->config, fragment_size, fragment, fragment_size);
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
	uint32_t cfg_size = sizeof(struct sof_smart_amp_config);

	comp_info(dev, "smart_amp_get_config()");

	switch (config_id) {
	case SMART_AMP_GET_CONFIG:
		if (cfg_size > fragment_size) {
			comp_err(dev, "smart_amp_get_config(): wrong config size %d",
				 fragment_size);
			return -EINVAL;
		}
		*data_offset_size = cfg_size;
		return memcpy_s(fragment, cfg_size, &sad->config, cfg_size);
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
		in_frag += source->channels;
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
		in_frag += source->channels;
	}
	return 0;
}

static smart_amp_proc get_smart_amp_process(struct comp_dev *dev,
					    struct comp_buffer __sparse_cache *buf)
{
	switch (buf->stream.frame_fmt) {
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

/* Only reset the src_input and fb_input if the num_input_buffers
 * (get from module adapter)has changed.
 */
static void smart_amp_input_setup(struct processing_module *mod,
				  struct input_stream_buffer *inputs,
				  int32_t num_input_buffers)
{
	struct smart_amp_data *sad = module_get_private_data(mod);
	int i;

	if (sad->input_stream_num != num_input_buffers) {
		if (num_input_buffers == 1) {
			sad->src_input = &inputs[0];
			sad->fb_input = NULL;
		} else {
			for (i = 0; i < num_input_buffers; i++) {
				if (IPC4_SINK_QUEUE_ID(inputs[i].comp_buf_id) ==
				   SOF_SMART_AMP_FEEDBACK_QUEUE_ID)
					sad->fb_input = &inputs[i];
				else
					sad->src_input = &inputs[i];
			}
		}

		sad->input_stream_num = num_input_buffers;
	}
}

static int smart_amp_process(struct processing_module *mod,
			     struct input_stream_buffer *input_buffers, int num_input_buffers,
			     struct output_stream_buffer *output_buffers, int num_output_buffers)
{
	struct smart_amp_data *sad = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	struct comp_buffer __sparse_cache *fb_buf;
	struct comp_buffer __sparse_cache *src_buf;
	uint32_t avail_passthrough_frames;
	uint32_t avail_frames = 0;
	uint32_t sink_bytes;
	uint32_t fb_bytes;
	uint32_t source_bytes;

	smart_amp_input_setup(mod, input_buffers, num_input_buffers);
	avail_passthrough_frames = sad->src_input->size;
	if (sad->fb_input) {
		fb_buf = attr_container_of(sad->fb_input->data,
					   struct comp_buffer __sparse_cache,
					   stream, __sparse_cache);

		if (fb_buf->source && comp_get_state(dev, fb_buf->source) == dev->state) {
			/* feedback */
			avail_frames =
				audio_stream_get_avail_frames(sad->fb_input->data);
			avail_frames = MIN(avail_passthrough_frames,
					   avail_frames);
			fb_bytes = avail_frames *
				audio_stream_frame_bytes(sad->fb_input->data);
			buffer_stream_invalidate(fb_buf, fb_bytes);

			sad->process(mod, sad->fb_input, &output_buffers[0],
				     avail_frames, sad->config.feedback_ch_map);
			sad->fb_input->consumed = fb_bytes;
		}
	}

	if (!avail_frames)
		avail_frames = avail_passthrough_frames;

	src_buf = attr_container_of(sad->src_input->data,
				    struct comp_buffer __sparse_cache,
				    stream, __sparse_cache);
	/* bytes calculation */
	source_bytes = avail_frames *
		audio_stream_frame_bytes(sad->src_input->data);
	/* bytes calculation */
	sink_bytes = avail_frames *
		audio_stream_frame_bytes(output_buffers[0].data);

	buffer_stream_invalidate(src_buf, source_bytes);
	/* process data */
	sad->process(mod, sad->src_input, &output_buffers[0],
		     avail_frames, sad->config.source_ch_map);

	sad->src_input->consumed = source_bytes;
	output_buffers[0].size = sink_bytes;

	return 0;
}

static int smart_amp_reset(struct processing_module *mod)
{
	struct comp_dev *dev = mod->dev;

	comp_dbg(dev, "smart_amp_reset()");

	return 0;
}

static int smart_amp_prepare(struct processing_module *mod)
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
			buffer_c->stream.channels = sad->config.feedback_channels;
			buffer_c->stream.rate = mod->priv.cfg.base_cfg.audio_fmt.sampling_frequency;
		}
		buffer_release(buffer_c);
	}

	sink_buffer = list_first_item(&dev->bsink_list, struct comp_buffer, source_list);
	buffer_c = buffer_acquire(sink_buffer);
	sad->out_channels = buffer_c->stream.channels;
	audio_stream_init_alignment_constants(1, 1, &buffer_c->stream);
	buffer_release(buffer_c);

	sad->input_stream_num = 0;
	sad->fb_input = NULL;
	sad->process = get_smart_amp_process(dev, buffer_c);
	if (!sad->process) {
		comp_err(dev, "smart_amp_prepare(): get_smart_amp_process failed");
		ret = -EINVAL;
	}
	return ret;
}

static struct module_interface smart_amp_interface = {
	.init  = smart_amp_init,
	.prepare = smart_amp_prepare,
	.process = smart_amp_process,
	.set_configuration = smart_amp_set_config,
	.get_configuration = smart_amp_get_config,
	.reset = smart_amp_reset,
	.free = smart_amp_free
};
DECLARE_MODULE_ADAPTER(smart_amp_interface, smart_amp_comp_uuid, smart_amp_comp_tr);
SOF_MODULE_INIT(smart_amp_test, sys_comp_module_smart_amp_interface_init);
#endif
