// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2025 Intel Corporation.

#include <sof/audio/module_adapter/module/generic.h>
#include <rtos/init.h>
#include "template_comp.h"

LOG_MODULE_REGISTER(template_comp, CONFIG_SOF_LOG_LEVEL);

/* Use e.g. command uuidgen from package uuid-runtime, add it to
 * uuid-registry.txt in SOF top level.
 */
SOF_DEFINE_REG_UUID(template_comp);

DECLARE_TR_CTX(template_comp_tr, SOF_UUID(template_comp_uuid), LOG_LEVEL_INFO);

static int template_comp_init(struct processing_module *mod)
{
	struct module_data *md = &mod->priv;
	struct comp_dev *dev = mod->dev;
	struct template_comp_comp_data *cd;

	comp_info(dev, "template_comp_init()");

	cd = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(*cd));
	if (!cd)
		return -ENOMEM;

	md->private = cd;
	return 0;
}

__cold static int template_comp_free(struct processing_module *mod)
{
	struct template_comp_comp_data *cd = module_get_private_data(mod);

	assert_can_be_cold();

	comp_info(mod->dev, "template_comp_free()");
	rfree(cd);
	return 0;
}

#if CONFIG_FORMAT_S16LE
static void template_comp_s16(const struct processing_module *mod,
			      const struct audio_stream *source,
			      struct audio_stream *sink,
			      uint32_t frames)
{
	struct template_comp_comp_data *cd = module_get_private_data(mod);
	int16_t *x = audio_stream_get_rptr(source);
	int16_t *y = audio_stream_get_wptr(sink);
	int nbuf;
	int nfrm;
	int ch;
	int i;

	while (frames) {
		nbuf = audio_stream_frames_without_wrap(source, x);
		nfrm = MIN(frames, nbuf);
		nbuf = audio_stream_frames_without_wrap(sink, y);
		nfrm = MIN(nfrm, nbuf);
		for (i = 0; i < nfrm; i++) {
			for (ch = 0; ch < cd->channels; ch++) {
				*y = *(x + cd->channels_order[ch]);
				y++;
			}
			x += cd->channels;
		}
		frames -= nfrm;
		x = audio_stream_wrap(source, x);
		y = audio_stream_wrap(sink, y);
	}
}
#endif /* CONFIG_FORMAT_S16LE */

#if CONFIG_FORMAT_S32LE
/* Same function works for s24 and s32 since
 * the samples values are not modified.
 */
static void template_comp_s32(const struct processing_module *mod,
			      const struct audio_stream *source,
			      struct audio_stream *sink,
			      uint32_t frames)
{
	struct template_comp_comp_data *cd = module_get_private_data(mod);
	int32_t *x = audio_stream_get_rptr(source);
	int32_t *y = audio_stream_get_wptr(sink);
	int nbuf;
	int nfrm;
	int ch;
	int i;

	while (frames) {
		nbuf = audio_stream_frames_without_wrap(source, x);
		nfrm = MIN(frames, nbuf);
		nbuf = audio_stream_frames_without_wrap(sink, y);
		nfrm = MIN(nfrm, nbuf);
		for (i = 0; i < nfrm; i++) {
			for (ch = 0; ch < cd->channels; ch++) {
				*y = *(x + cd->channels_order[ch]);
				y++;
			}
			x += cd->channels;
		}
		frames -= nfrm;
		x = audio_stream_wrap(source, x);
		y = audio_stream_wrap(sink, y);
	}
}
#endif /* CONFIG_FORMAT_S32LE */

const struct template_comp_proc_fnmap template_comp_proc_fnmap[] = {
#if CONFIG_FORMAT_S16LE
	{ SOF_IPC_FRAME_S16_LE, template_comp_s16 },
#endif
#if CONFIG_FORMAT_S24LE
	{ SOF_IPC_FRAME_S24_4LE, template_comp_s32 },
#endif
#if CONFIG_FORMAT_S32LE
	{ SOF_IPC_FRAME_S32_LE, template_comp_s32 },
#endif
};

static inline template_comp_func template_comp_find_proc_func(enum sof_ipc_frame src_fmt)
{
	int i;

	/* Find suitable processing function from map */
	for (i = 0; i < ARRAY_SIZE(template_comp_proc_fnmap); i++)
		if (src_fmt == template_comp_proc_fnmap[i].frame_fmt)
			return template_comp_proc_fnmap[i].template_comp_proc_func;

	return NULL;
}

static int template_comp_process(struct processing_module *mod,
				 struct input_stream_buffer *input_buffers,
				 int num_input_buffers,
				 struct output_stream_buffer *output_buffers,
				 int num_output_buffers)
{
	struct template_comp_comp_data *cd =  module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	struct audio_stream *source = input_buffers[0].data;
	struct audio_stream *sink = output_buffers[0].data;
	int frames = input_buffers[0].size;

	comp_dbg(dev, "template_comp_process()");

	if (cd->enable) {
		cd->template_comp_func(mod, source, sink, frames);
	} else {
		/* copy from source to sink */
		audio_stream_copy(source, 0, sink, 0, cd->channels * frames);
	}

	/* calc new free and available */
	module_update_buffer_position(&input_buffers[0], &output_buffers[0], frames);
	return 0;
}

static int template_comp_params(struct processing_module *mod)
{
	struct sof_ipc_stream_params *params = mod->stream_params;
	struct sof_ipc_stream_params comp_params;
	struct comp_dev *dev = mod->dev;
	struct comp_buffer *sinkb;
	enum sof_ipc_frame valid_fmt, frame_fmt;
	int i;

	comp_dbg(dev, "template_comp_params()");

	comp_params = *params;
	comp_params.channels = mod->priv.cfg.base_cfg.audio_fmt.channels_count;
	comp_params.rate = mod->priv.cfg.base_cfg.audio_fmt.sampling_frequency;
	comp_params.buffer_fmt = mod->priv.cfg.base_cfg.audio_fmt.interleaving_style;

	audio_stream_fmt_conversion(mod->priv.cfg.base_cfg.audio_fmt.depth,
				    mod->priv.cfg.base_cfg.audio_fmt.valid_bit_depth,
				    &frame_fmt, &valid_fmt,
				    mod->priv.cfg.base_cfg.audio_fmt.s_type);

	comp_params.frame_fmt = frame_fmt;

	for (i = 0; i < SOF_IPC_MAX_CHANNELS; i++)
		comp_params.chmap[i] = (mod->priv.cfg.base_cfg.audio_fmt.ch_map >> i * 4) & 0xf;

	component_set_nearest_period_frames(dev, comp_params.rate);
	sinkb = comp_dev_get_first_data_consumer(dev);
	return buffer_set_params(sinkb, &comp_params, true);
}

static int template_comp_prepare(struct processing_module *mod,
				 struct sof_source **sources, int num_of_sources,
				 struct sof_sink **sinks, int num_of_sinks)
{
	struct template_comp_comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	struct comp_buffer *sourceb;
	int ret;
	int i;

	comp_info(dev, "template_comp_prepare()");

	ret = template_comp_params(mod);
	if (ret < 0)
		return ret;

	/* Assume 1 source and 1 sink buffer only */
	sourceb = comp_dev_get_first_data_producer(dev);

	/* get source data format */
	cd->source_format = audio_stream_get_frm_fmt(&sourceb->stream);
	cd->channels = audio_stream_get_channels(&sourceb->stream);

	/* Initialize channels order for reversing */
	for (i = 0; i < cd->channels; i++)
		cd->channels_order[i] = cd->channels - i - 1;

	cd->template_comp_func = template_comp_find_proc_func(cd->source_format);
	if (!cd->template_comp_func) {
		comp_err(dev, "No processing function found for format %d.",
			 cd->source_format);
		return -EINVAL;
	}

	return 0;
}

static int template_comp_reset(struct processing_module *mod)
{
	struct template_comp_comp_data *cd = module_get_private_data(mod);

	comp_info(mod->dev, "template_comp_reset()");

	/* reset component data same as in init() */
	cd->channels = 0;

	return 0;
}

__cold static int template_comp_set_config(struct processing_module *mod,
					   uint32_t param_id,
					   enum module_cfg_fragment_position pos,
					   uint32_t data_offset_size,
					   const uint8_t *fragment,
					   size_t fragment_size,
					   uint8_t *response,
					   size_t response_size)
{
	struct sof_ipc4_control_msg_payload *ctl = (struct sof_ipc4_control_msg_payload *)fragment;
	struct template_comp_comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;

	switch (param_id) {
	case SOF_IPC4_SWITCH_CONTROL_PARAM_ID:
		comp_dbg(dev, "SOF_IPC4_SWITCH_CONTROL_PARAM_ID id = %d, num_elems = %d",
			 ctl->id, ctl->num_elems);

		if (ctl->id == 0 && ctl->num_elems == 1) {
			cd->enable = ctl->chanv[0].value;
			comp_info(dev, "process_enabled = %d", cd->enable);
		} else {
			comp_err(dev, "Illegal control id = %d, num_elems = %d",
				 ctl->id, ctl->num_elems);
			return -EINVAL;
		}

		return 0;

	case SOF_IPC4_ENUM_CONTROL_PARAM_ID:
		comp_err(dev, "template_comp_set_ipc_config(), illegal control.");
		return -EINVAL;
	}

	comp_dbg(mod->dev, "template_comp_set_ipc_config(), SOF_CTRL_CMD_BINARY");
	return -EINVAL;
}

static const struct module_interface template_comp_interface = {
	.init = template_comp_init,
	.prepare = template_comp_prepare,
	.process_audio_stream = template_comp_process,
	.set_configuration = template_comp_set_config,
	.reset = template_comp_reset,
	.free = template_comp_free
};

#if CONFIG_COMP_TEMPLATE_COMP_MODULE
/* modular: llext dynamic link */

#include <module/module/api_ver.h>
#include <module/module/llext.h>
#include <rimage/sof/user/manifest.h>

SOF_LLEXT_MOD_ENTRY(template_comp, &template_comp_interface);

static const struct sof_man_module_manifest mod_manifest __section(".module") __used =
	SOF_LLEXT_MODULE_MANIFEST("TEMPLATE", template_comp_llext_entry, 1,
				  SOF_REG_UUID(template_comp), 40);

SOF_LLEXT_BUILDINFO;

#else

DECLARE_MODULE_ADAPTER(template_comp_interface, template_comp_uuid, template_comp_tr);
SOF_MODULE_INIT(template_comp, sys_comp_module_template_comp_interface_init);

#endif
