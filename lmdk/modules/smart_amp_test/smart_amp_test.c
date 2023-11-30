// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Intel Corporation. All rights reserved.
//
// Author: Bartosz Kokoszko <bartoszx.kokoszko@linux.intel.com>
// Author: Andrula Song <andrula.song@intel.com>
// Author: Chao Song <chao.song@linux.intel.com>

#define MODULE_BUILD

#include "../include/loadable_processing_module.h"
#include <rimage/sof/user/manifest.h>
#include <ipc4/base-config.h>
#include <module/interface.h>
#include <sof/audio/module_adapter/library/native_system_service.h>
#include <../include/module_adapter/module/generic.h>
#include <module/base.h>
#include <ipc/stream.h>
#include <../include/smart_amp_test.h>
#include <../include/module_adapter/system_service/system_service.h>
#include <../include/buffer.h>
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

/** \brief Retrieves the component device buffer list. */
#define comp_buffer_list(comp, dir) \
	((dir) == PPL_DIR_DOWNSTREAM ? &(comp)->bsink_list :	\
	 &(comp)->bsource_list)

/* Function overwrites PCM parameters (frame_fmt, buffer_fmt, channels, rate)
 * with buffer parameters when specific flag is set.
 */
static void comp_update_params(uint32_t flag,
			       struct sof_ipc_stream_params *params,
			       struct comp_buffer *buffer)
{
	if (flag & BUFF_PARAMS_FRAME_FMT)
		params->frame_fmt = audio_stream_get_frm_fmt(&buffer->stream);

	if (flag & BUFF_PARAMS_BUFFER_FMT)
		params->buffer_fmt = audio_stream_get_buffer_fmt(&buffer->stream);

	if (flag & BUFF_PARAMS_CHANNELS)
		params->channels = audio_stream_get_channels(&buffer->stream);

	if (flag & BUFF_PARAMS_RATE)
		params->rate = audio_stream_get_rate(&buffer->stream);
}

int buffer_set_params(struct comp_buffer  *buffer,
		      struct sof_ipc_stream_params *params, bool force_update)
{
	int ret;
	int i;

	if (!params)
		return -EINVAL;

	if (buffer->hw_params_configured && !force_update)
		return 0;

	ret = audio_stream_set_params(&buffer->stream, params);
	if (ret < 0)
		return -EINVAL;

	audio_stream_set_buffer_fmt(&buffer->stream, params->buffer_fmt);
	for (i = 0; i < SOF_IPC_MAX_CHANNELS; i++)
		buffer->chmap[i] = params->chmap[i];

	buffer->hw_params_configured = true;

	return 0;
}

int comp_verify_params(struct comp_dev *dev, uint32_t flag,
		       struct sof_ipc_stream_params *params)
{
	struct list_item *buffer_list;
	struct list_item *source_list;
	struct list_item *sink_list;
	struct list_item *clist;
	struct comp_buffer *sinkb;
	struct comp_buffer *buf;
	int dir = dev->direction;

	if (!params)
		return -EINVAL;

	source_list = comp_buffer_list(dev, PPL_DIR_UPSTREAM);
	sink_list = comp_buffer_list(dev, PPL_DIR_DOWNSTREAM);

	/* searching for endpoint component e.g. HOST, DETECT_TEST, which
	 * has only one sink or one source buffer.
	 */
	if (list_is_empty(source_list) != list_is_empty(sink_list)) {
		if (list_is_empty(sink_list))
			buf = list_first_item(source_list,
					      struct comp_buffer,
					      sink_list);
		else
			buf = list_first_item(sink_list,
					      struct comp_buffer,
					      source_list);

		/* update specific pcm parameter with buffer parameter if
		 * specific flag is set.
		 */
		comp_update_params(flag, params, buf);

		/* overwrite buffer parameters with modified pcm
		 * parameters
		 */
		buffer_set_params(buf, params, BUFFER_UPDATE_FORCE);

		/* set component period frames */
		component_set_nearest_period_frames(dev, audio_stream_get_rate(&buf->stream));
	} else {
		/* for other components we iterate over all downstream buffers
		 * (for playback) or upstream buffers (for capture).
		 */
		buffer_list = comp_buffer_list(dev, dir);

		list_for_item(clist, buffer_list) {
			buf = buffer_from_list(clist, dir);
			comp_update_params(flag, params, buf);
			buffer_set_params(buf, params, BUFFER_UPDATE_FORCE);
		}

		/* fetch sink buffer in order to calculate period frames */
		sinkb = list_first_item(&dev->bsink_list, struct comp_buffer,
					source_list);

		component_set_nearest_period_frames(dev, audio_stream_get_rate(&buf->stream));
	}

	return 0;
}

/* FIXME: only one instance is supported ATM */
static struct smart_amp_data smart_amp_priv;

/* This function returns the greatest common divisor of two numbers
 * If both parameters are 0, gcd(0, 0) returns 0
 * If first parameters is 0 or second parameter is 0, gcd(0, b) returns b
 * and gcd(a, 0) returns a, because everything divides 0.
 */

int gcd(int a, int b)
{
	if (a == 0)
		return b;

	if (b == 0)
		return a;

	/* If the numbers are negative, convert them to positive numbers
	 * gcd(a, b) = gcd(-a, -b) = gcd(-a, b) = gcd(a, -b)
	 */

	if (a < 0)
		a = -a;

	if (b < 0)
		b = -b;

	int aux;
	int k;

	/* Find the greatest power of 2 that devides both a and b */
	for (k = 0; ((a | b) & 1) == 0; k++) {
		a >>= 1;
		b >>= 1;
	}

	/* divide by 2 until a becomes odd */
	while ((a & 1) == 0)
		a >>= 1;

	do {
		/*if b is even, remove all factors of 2*/
		while ((b & 1) == 0)
			b >>= 1;

		/* both a and b are odd now. Swap so a <= b
		 * then set b = b - a, which is also even
		 */
		if (a > b) {
			aux = a;
			a = b;
			b = aux;
		}

		b = b - a;

	} while (b != 0);

	/* restore common factors of 2 */
	return a << k;
}

static int smart_amp_init(struct processing_module *mod)
{
	struct smart_amp_data *sad;
	struct comp_dev *dev = mod->dev;
	struct module_data *mod_data = &mod->priv;
	struct native_system_service_api *sys_service = mod->sys_service;
	int bs;
	int ret;

	const struct ipc4_base_module_extended_cfg *base_cfg = mod_data->cfg.init_data;
	sad = &smart_amp_priv;
	mod_data->private = sad;

	if (base_cfg->base_cfg_ext.nb_input_pins != SMART_AMP_NUM_IN_PINS ||
	    base_cfg->base_cfg_ext.nb_output_pins != SMART_AMP_NUM_OUT_PINS) {
		ret = -EINVAL;
		goto sad_fail;
	}

	/* Copy the pin formats */
	bs = sizeof(sad->ipc4_cfg.input_pins) + sizeof(sad->ipc4_cfg.output_pin);
	sys_service->safe_memcpy(sad->ipc4_cfg.input_pins, bs,
				     base_cfg->base_cfg_ext.pin_formats, bs);

	mod->max_sources = SMART_AMP_NUM_IN_PINS;

	return 0;

sad_fail:
	return ret;
}

static int smart_amp_set_config(struct processing_module *mod, uint32_t config_id,
				enum module_cfg_fragment_position pos, uint32_t data_offset_size,
				const uint8_t *fragment, size_t fragment_size, uint8_t *response,
				size_t response_size)
{
	struct native_system_service_api *sys_service = mod->sys_service;
	struct comp_dev *dev = mod->dev;
	struct smart_amp_data *sad = module_get_private_data(mod);

	switch (config_id) {
	case SMART_AMP_SET_MODEL:
		return 0;
	case SMART_AMP_SET_CONFIG:
		if (fragment_size != sizeof(sad->config))
			return -EINVAL;
		sys_service->safe_memcpy(&sad->config, sizeof(sad->config),
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
	struct native_system_service_api *sys_service = mod->sys_service;
	struct smart_amp_data *sad = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	int ret;

	switch (config_id) {
	case SMART_AMP_GET_CONFIG:
		ret = sys_service->safe_memcpy(fragment, fragment_size,
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
	struct audio_stream *source = bsource->data;
	struct audio_stream *sink = bsink->data;
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
	struct audio_stream *source = bsource->data;
	struct audio_stream *sink = bsink->data;
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
					    struct comp_buffer *buf)
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
	struct input_stream_buffer *src_input = &input_buffers[0];
	struct input_stream_buffer *fb_input = NULL;
	uint32_t avail_passthrough_frames;
	struct comp_dev *dev = mod->dev;
	struct comp_buffer *fb_buf;
	struct comp_buffer *buf;
	uint32_t avail_frames = 0;
	uint32_t sink_bytes;
	uint32_t i;

	if (num_input_buffers == SMART_AMP_NUM_IN_PINS)
		for (i = 0; i < num_input_buffers; i++) {
			buf = container_of(input_buffers[i].data, struct comp_buffer, stream);

			if (IPC4_SINK_QUEUE_ID(buf->id) == SOF_SMART_AMP_FEEDBACK_QUEUE_ID) {
				fb_input = &input_buffers[i];
				fb_buf = buf;
			} else {
				src_input = &input_buffers[i];
			}
		}

	if (!src_input)
		return 0;

	avail_passthrough_frames = src_input->size;

	if (fb_input) {
		if (fb_buf->source && comp_get_state(dev, fb_buf->source) == dev->state) {
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

	return 0;
}

static int smart_amp_free(struct processing_module *mod)
{
	return 0;
}

static int smart_amp_reset(struct processing_module *mod)
{
	return 0;
}

static void smart_amp_set_params(struct processing_module *mod)
{
	const struct ipc4_audio_format *audio_fmt = &mod->priv.cfg.base_cfg.audio_fmt;
	struct native_system_service_api *sys_service = mod->sys_service;
	struct sof_ipc_stream_params *params = mod->stream_params;
	struct smart_amp_data *sad = module_get_private_data(mod);
	enum sof_ipc_frame frame_fmt, valid_fmt;
	struct comp_dev *dev = mod->dev;
	struct comp_buffer *sink;
	int i;

	sys_service->vec_memset(params, 0, sizeof(*params));
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

		sys_service->safe_memcpy(&out_fmt, sizeof(out_fmt),
					     &sink_fmt->audio_fmt, sizeof(sink_fmt->audio_fmt));

		sink = list_first_item(&dev->bsink_list, struct comp_buffer, source_list);

		audio_stream_fmt_conversion(out_fmt.depth,
					    out_fmt.valid_bit_depth,
					    &frame_fmt, &valid_fmt,
					    out_fmt.s_type);

		audio_stream_set_frm_fmt(&sink->stream, frame_fmt);
		audio_stream_set_valid_fmt(&sink->stream, valid_fmt);
		audio_stream_set_channels(&sink->stream, out_fmt.channels_count);
		audio_stream_set_rate(&sink->stream, out_fmt.sampling_frequency);
		params->frame_fmt = audio_stream_get_frm_fmt(&sink->stream);
		sink->hw_params_configured = true;
	}
}

static int smart_amp_params(struct processing_module *mod)
{
	struct sof_ipc_stream_params *params = mod->stream_params;
	struct comp_dev *dev = mod->dev;
	int ret;

	smart_amp_set_params(mod);
	ret = comp_verify_params(dev, BUFF_PARAMS_CHANNELS, params);
	if (ret < 0) {
		return -EINVAL;
	}
	return 0;
}

static int smart_amp_prepare(struct processing_module *mod,
			     struct sof_source **sources, int num_of_sources,
			     struct sof_sink **sinks, int num_of_sinks)
{
	struct smart_amp_data *sad = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	struct comp_buffer *source_buffer;
	struct comp_buffer *sink_buffer;
	struct list_item *blist;
	int ret;

	ret = smart_amp_params(mod);
	if (ret < 0)
		return ret;

	/* searching for stream and feedback source buffers */
	list_for_item(blist, &dev->bsource_list) {
		source_buffer = container_of(blist, struct comp_buffer,
					     sink_list);
		audio_stream_init_alignment_constants(1, 1, &source_buffer->stream);
		if (IPC4_SINK_QUEUE_ID(source_buffer->id) == SOF_SMART_AMP_FEEDBACK_QUEUE_ID) {
			audio_stream_set_channels(&source_buffer->stream, sad->config.feedback_channels);
			audio_stream_set_rate(&source_buffer->stream,
					      mod->priv.cfg.base_cfg.audio_fmt.sampling_frequency);
		}
	}

	sink_buffer = list_first_item(&dev->bsink_list, struct comp_buffer, source_list);
	sad->out_channels = audio_stream_get_channels(&sink_buffer->stream);
	audio_stream_init_alignment_constants(1, 1, &sink_buffer->stream);
	sad->process = get_smart_amp_process(dev, sink_buffer);

	if (!sad->process) {;
		ret = -EINVAL;
	}
	return ret;
}

struct module_interface smart_amp_test_interface = {
	.init  = smart_amp_init,
	.prepare = smart_amp_prepare,
	.process_audio_stream = smart_amp_process,
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
