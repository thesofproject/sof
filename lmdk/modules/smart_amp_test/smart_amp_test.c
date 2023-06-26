// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Intel Corporation. All rights reserved.
//
// Author: Bartosz Kokoszko <bartoszx.kokoszko@linux.intel.com>
// Author: Andrula Song <andrula.song@intel.com>
// Author: Chao Song <chao.song@linux.intel.com>

static int (*gcd)(int a, int b);
#define MODULE_BUILD

#include "../include/loadable_processing_module.h"
#include <rimage/sof/user/manifest.h>
#include <../include/smart_amp_test.h>
#include <../include/module_adapter/module/generic.h>
#include <../include/module_adapter/system_service/system_service.h>
#include <../include/buffer.h>
#include <../include/audio_stream.h>
#include <ipc4/module.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

enum mem_zone {
	SOF_MEM_ZONE_SYS = 0,		/**< System zone */
	SOF_MEM_ZONE_SYS_RUNTIME,	/**< System-runtime zone */
	SOF_MEM_ZONE_RUNTIME,		/**< Runtime zone */
	SOF_MEM_ZONE_BUFFER,		/**< Buffer zone */
	SOF_MEM_ZONE_RUNTIME_SHARED,	/**< Runtime shared zone */
	SOF_MEM_ZONE_SYS_SHARED,	/**< System shared zone */
};

/* FIXME: only one instance is supported ATM */
static struct smart_amp_data smart_amp_priv;

static int smart_amp_init(struct processing_module *mod)
{
	struct smart_amp_data *sad;
	struct comp_dev *dev = mod->dev;
	struct module_data *mod_data = &mod->priv;
	int bs;
	int ret;
	const struct ipc4_base_module_extended_cfg *base_cfg = mod_data->cfg.init_data;

#if 0
	sad = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(*sad));
	if (!sad)
		return -ENOMEM;
#else
	sad = &smart_amp_priv;
#endif

	mod_data->private = sad;

	gcd = mod->sys_service->math_gcd;

	/* component model data handler */
	sad->model_handler = mod->sys_service->data_blob_handler_new(dev);
	if (!sad->model_handler) {
		ret = -ENOMEM;
		goto sad_fail;
	}

	if (base_cfg->base_cfg_ext.nb_input_pins != SMART_AMP_NUM_IN_PINS ||
	    base_cfg->base_cfg_ext.nb_output_pins != SMART_AMP_NUM_OUT_PINS) {
		ret = -EINVAL;
		goto sad_fail;
	}

	/* Copy the pin formats */
	bs = sizeof(sad->ipc4_cfg.input_pins) + sizeof(sad->ipc4_cfg.output_pin);
	mod->sys_service->SafeMemcpy(sad->ipc4_cfg.input_pins, bs,
				     base_cfg->base_cfg_ext.pin_formats, bs);

	mod->simple_copy = true;

	return 0;

sad_fail:
	mod->sys_service->data_blob_handler_free(sad->model_handler);
//	rfree(sad);
	return ret;
}

static int smart_amp_set_config(struct processing_module *mod, uint32_t config_id,
				enum module_cfg_fragment_position pos, uint32_t data_offset_size,
				const uint8_t *fragment, size_t fragment_size, uint8_t *response,
				size_t response_size)
{
	struct comp_dev *dev = mod->dev;
	struct smart_amp_data *sad = module_get_private_data(mod);

	switch (config_id) {
	case SMART_AMP_SET_MODEL:
		return mod->sys_service->data_blob_set(sad->model_handler, pos,
					 data_offset_size, fragment, fragment_size);
	case SMART_AMP_SET_CONFIG:
		if (fragment_size != sizeof(sad->config))
			return -EINVAL;
		mod->sys_service->SafeMemcpy(&sad->config, sizeof(sad->config),
					     fragment, fragment_size);
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

	switch (config_id) {
	case SMART_AMP_GET_CONFIG:
		ret = mod->sys_service->SafeMemcpy(fragment, fragment_size,
						   &sad->config, sizeof(sad->config));
		if (ret) {
			return ret;
		}
		*data_offset_size = sizeof(sad->config);
		return 0;
	default:
		return -EINVAL;
	}
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

static int smart_amp_free(struct processing_module *mod)
{
	struct smart_amp_data *sad = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;

	mod->sys_service->data_blob_handler_free(sad->model_handler);
//	rfree(sad);
	return 0;
}

static int smart_amp_reset(struct processing_module *mod)
{
	return 0;
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

	mod->sys_service->VecMemset(params, 0, sizeof(*params));
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
		struct ipc4_audio_format out_fmt;

		mod->sys_service->SafeMemcpy(&out_fmt, sizeof(out_fmt),
					     &sink_fmt->audio_fmt, sizeof(sink_fmt->audio_fmt));

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
		sink_c->buffer_fmt = out_fmt.interleaving_style;
		params->frame_fmt = audio_stream_get_frm_fmt(&sink_c->stream);
		sink_c->hw_params_configured = true;
		buffer_release(sink_c);
	}
}

static int smart_amp_params(struct processing_module *mod)
{
	struct sof_ipc_stream_params *params = mod->stream_params;
	struct comp_dev *dev = mod->dev;
	int ret;

	smart_amp_set_params(mod);
	ret = mod->sys_service->comp_verify_params(dev, BUFF_PARAMS_CHANNELS, params);
	if (ret < 0) {
		return -EINVAL;
	}
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

	if (!sad->process) {;
		ret = -EINVAL;
	}
	return ret;
}

struct module_interface smart_amp_test_interface = {
	.init  = smart_amp_init,
	.prepare = smart_amp_prepare,
	.process = smart_amp_process,
	.set_configuration = smart_amp_set_config,
	.get_configuration = smart_amp_get_config,
	.reset = smart_amp_reset,
	.free = smart_amp_free
};

void *loadable_module_main(void *mod_cfg, void *parent_ppl, void **mod_ptr)
{
	return &smart_amp_test_interface;
}

DECLARE_LOADABLE_MODULE(smart_amp_test)

__attribute__((section(".module")))
const struct sof_man_module_manifest main_manifest = {
	.module = {
		.name = "SMATEST",
		.uuid = {0x1E, 0x96, 0x7A, 0x16, 0xE4, 0x8A, 0xEA, 0x11,
			 0x89, 0xF1, 0x00, 0x0C, 0x29, 0xCE, 0x16, 0x35},
		.entry_point = (uint32_t)MODULE_PACKAGE_ENTRY_POINT_NAME(smart_amp_test),
		.type = { .load_type = SOF_MAN_MOD_TYPE_MODULE,
		.domain_ll = 1 },
		.affinity_mask = 1,
	}
};
