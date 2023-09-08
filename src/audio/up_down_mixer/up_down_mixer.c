// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 Intel Corporation. All rights reserved.
//
// Author: Bartosz Kokoszko <bartoszx.kokoszko@intel.com>
// Author: Adrian Bonislawski <adrian.bonislawski@intel.com>

#include <sof/audio/coefficients/up_down_mixer/up_down_mixer.h>
#include <sof/audio/up_down_mixer/up_down_mixer.h>
#include <sof/audio/buffer.h>
#include <sof/audio/format.h>
#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/pipeline.h>
#include <rtos/panic.h>
#include <sof/ipc/msg.h>
#include <rtos/alloc.h>
#include <rtos/cache.h>
#include <rtos/init.h>
#include <sof/lib/memory.h>
#include <sof/lib/notifier.h>
#include <sof/lib/uuid.h>
#include <sof/list.h>
#include <rtos/string.h>
#include <sof/ut.h>
#include <sof/trace/trace.h>
#include <user/trace.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

LOG_MODULE_REGISTER(up_down_mixer, CONFIG_SOF_LOG_LEVEL);

/* these ids aligns windows driver requirement to support windows driver */
/* 42f8060c-832f-4dbf-b247-51e961997b34 */
DECLARE_SOF_RT_UUID("up_down_mixer", up_down_mixer_comp_uuid, 0x42f8060c, 0x832f,
		    0x4dbf, 0xb2, 0x47, 0x51, 0xe9, 0x61, 0x99, 0x7b, 0x34);

DECLARE_TR_CTX(up_down_mixer_comp_tr, SOF_UUID(up_down_mixer_comp_uuid),
	       LOG_LEVEL_INFO);

int32_t custom_coeffs[UP_DOWN_MIX_COEFFS_LENGTH];

static int set_downmix_coefficients(struct processing_module *mod,
				    const struct ipc4_audio_format *format,
				    const enum ipc4_channel_config out_channel_config,
				    const downmix_coefficients downmix_coefficients)
{
	struct up_down_mixer_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	int ret;

	if (cd->downmix_coefficients) {
		ret = memcpy_s(&custom_coeffs, sizeof(custom_coeffs), downmix_coefficients,
			       sizeof(int32_t) * UP_DOWN_MIX_COEFFS_LENGTH);

		if (ret < 0)
			return ret;

		cd->downmix_coefficients = custom_coeffs;

		return 0;
	}

	switch (format->ch_cfg) {
	case IPC4_CHANNEL_CONFIG_MONO:
	case IPC4_CHANNEL_CONFIG_STEREO:
	case IPC4_CHANNEL_CONFIG_2_POINT_1:
	case IPC4_CHANNEL_CONFIG_DUAL_MONO:
		cd->downmix_coefficients = k_lo_ro_downmix32bit;
		break;
	case IPC4_CHANNEL_CONFIG_3_POINT_0:
	case IPC4_CHANNEL_CONFIG_3_POINT_1:
		if (format->depth == IPC4_DEPTH_16BIT)
			cd->downmix_coefficients = k_half_scaled_lo_ro_downmix16bit;
		else
			cd->downmix_coefficients = k_half_scaled_lo_ro_downmix32bit;
		break;
	case IPC4_CHANNEL_CONFIG_QUATRO:
		if (out_channel_config == IPC4_CHANNEL_CONFIG_MONO) {
			cd->downmix_coefficients = (format->depth == IPC4_DEPTH_16BIT) ?
						    k_quatro_mono_scaled_lo_ro_downmix16bit :
						    k_quatro_mono_scaled_lo_ro_downmix32bit;
		} else { /*out_channel_config == IPC4_CHANNEL_CONFIG_STEREO*/
			cd->downmix_coefficients = (format->depth == IPC4_DEPTH_16BIT) ?
						    k_half_scaled_lo_ro_downmix16bit :
						    k_half_scaled_lo_ro_downmix32bit;
		}
		break;
	case IPC4_CHANNEL_CONFIG_4_POINT_0:
		if (format->depth == IPC4_DEPTH_16BIT) {
			cd->downmix_coefficients = k_scaled_lo_ro_downmix16bit;
		} else {
			if (out_channel_config == IPC4_CHANNEL_CONFIG_5_POINT_1)
				cd->downmix_coefficients = k_lo_ro_downmix32bit;
			else
				cd->downmix_coefficients = k_scaled_lo_ro_downmix32bit;
		}
		break;
	case IPC4_CHANNEL_CONFIG_5_POINT_0:
	case IPC4_CHANNEL_CONFIG_5_POINT_1:
	case IPC4_CHANNEL_CONFIG_7_POINT_1:
		cd->downmix_coefficients = k_scaled_lo_ro_downmix32bit;
		break;
	default:
		comp_err(dev, "set_downmix_coefficients(): invalid channel config.");
		return -EINVAL;
	}

	return 0;
}

static up_down_mixer_routine select_mix_out_stereo(struct comp_dev *dev,
						   const struct ipc4_audio_format *format)
{
	if (format->depth == IPC4_DEPTH_16BIT) {
		switch (format->ch_cfg) {
		case IPC4_CHANNEL_CONFIG_MONO:
			return shiftcopy16bit_mono;
		case IPC4_CHANNEL_CONFIG_DUAL_MONO:
		case IPC4_CHANNEL_CONFIG_STEREO:
			return shiftcopy16bit_stereo;
		case IPC4_CHANNEL_CONFIG_2_POINT_1:
		case IPC4_CHANNEL_CONFIG_3_POINT_0:
		case IPC4_CHANNEL_CONFIG_3_POINT_1:
		case IPC4_CHANNEL_CONFIG_QUATRO:
		case IPC4_CHANNEL_CONFIG_4_POINT_0:
		case IPC4_CHANNEL_CONFIG_5_POINT_0:
			return downmix16bit;
		case IPC4_CHANNEL_CONFIG_5_POINT_1:
			return downmix16bit_5_1;
		case IPC4_CHANNEL_CONFIG_INVALID:
		default:
			comp_err(dev, "select_mix_out_stereo(): invalid channel config.");
			/*
			 * This is a strange situation. We will allow to process it
			 * in the release code (hoping for the best) with downmix16bit,
			 * but will log err in debug to double check if everything is ok
			 */
			return NULL;
		}
	} else {
		switch (format->ch_cfg) {
		case IPC4_CHANNEL_CONFIG_MONO:
			return shiftcopy32bit_mono;
		case IPC4_CHANNEL_CONFIG_DUAL_MONO:
		case IPC4_CHANNEL_CONFIG_STEREO:
			return shiftcopy32bit_stereo;
		case IPC4_CHANNEL_CONFIG_2_POINT_1:
			return downmix32bit_2_1;
		case IPC4_CHANNEL_CONFIG_3_POINT_0:
			return downmix32bit_3_0;
		case IPC4_CHANNEL_CONFIG_3_POINT_1:
			return downmix32bit_3_1;
		case IPC4_CHANNEL_CONFIG_QUATRO:
			return downmix32bit;
		case IPC4_CHANNEL_CONFIG_4_POINT_0:
			return downmix32bit_4_0;
		case IPC4_CHANNEL_CONFIG_5_POINT_0:
			return downmix32bit_5_0_mono;
		case IPC4_CHANNEL_CONFIG_5_POINT_1:
			return downmix32bit_5_1;
		case IPC4_CHANNEL_CONFIG_7_POINT_1:
			return downmix32bit_7_1;
		case IPC4_CHANNEL_CONFIG_INVALID:
		default:
			comp_err(dev, "select_mix_out_stereo(): invalid channel config.");
			/*
			 * This is a strange situation. We will allow to process it
			 * in the release code (hoping for the best) with downmix32bit,
			 * but will log err in debug to double check if everything is ok.
			 */
			return NULL;
		}
	}
}

static up_down_mixer_routine select_mix_out_mono(struct comp_dev *dev,
						 const struct ipc4_audio_format *format)
{
	if (format->depth == IPC4_DEPTH_16BIT) {
		switch (format->ch_cfg) {
		case IPC4_CHANNEL_CONFIG_STEREO:
			return downmix16bit_stereo;
		case IPC4_CHANNEL_CONFIG_3_POINT_1:
		case IPC4_CHANNEL_CONFIG_QUATRO:
		case IPC4_CHANNEL_CONFIG_4_POINT_0:
			return downmix16bit_4ch_mono;
		case IPC4_CHANNEL_CONFIG_INVALID:
		default:
			comp_err(dev, "select_mix_out_mono(): invalid channel config.");
			/*
			 * This is a strange situation. We will allow to process it
			 * in the release code (hoping for the best) with downmix16bit,
			 * but will log err in debug to double check if everything is ok
			 */
			return NULL;
		}
	} else {
		switch (format->ch_cfg) {
		case IPC4_CHANNEL_CONFIG_DUAL_MONO:
		case IPC4_CHANNEL_CONFIG_STEREO:
			return downmix32bit_stereo;
		case IPC4_CHANNEL_CONFIG_3_POINT_1:
			return downmix32bit_3_1_mono;
		case IPC4_CHANNEL_CONFIG_QUATRO:
			return downmix32bit_quatro_mono;
		case IPC4_CHANNEL_CONFIG_4_POINT_0:
			return downmix32bit_4_0_mono;
		case IPC4_CHANNEL_CONFIG_5_POINT_0:
			return downmix32bit_5_0_mono;
		case IPC4_CHANNEL_CONFIG_5_POINT_1:
			return downmix32bit_5_1_mono;
		case IPC4_CHANNEL_CONFIG_7_POINT_1:
			return downmix32bit_7_1_mono;
		case IPC4_CHANNEL_CONFIG_INVALID:
		default:
			comp_err(dev, "select_mix_out_mono(): invalid channel config.");
			/*
			 * This is a strange situation. We will allow to process it
			 * in the release code (hoping for the best) with downmix32bit,
			 * but will log err in debug to double check if everything is ok.
			 */
			return NULL;
		}
	}
}

static up_down_mixer_routine select_mix_out_5_1(struct comp_dev *dev,
						const struct ipc4_audio_format *format)
{
	if (format->depth == IPC4_DEPTH_16BIT) {
		switch (format->ch_cfg) {
		case IPC4_CHANNEL_CONFIG_MONO:
			return upmix16bit_1_to_5_1;
		case IPC4_CHANNEL_CONFIG_STEREO:
			return upmix16bit_2_0_to_5_1;
		case IPC4_CHANNEL_CONFIG_INVALID:
		default:
			comp_err(dev, "select_mix_out_5_1(): invalid channel config.");
			return NULL;
		}
	} else {
		switch (format->ch_cfg) {
		case IPC4_CHANNEL_CONFIG_MONO:
			return upmix32bit_1_to_5_1;
		case IPC4_CHANNEL_CONFIG_STEREO:
			return upmix32bit_2_0_to_5_1;
		case IPC4_CHANNEL_CONFIG_QUATRO:
			return upmix32bit_quatro_to_5_1;
		case IPC4_CHANNEL_CONFIG_4_POINT_0:
			return upmix32bit_4_0_to_5_1;
		case IPC4_CHANNEL_CONFIG_7_POINT_1:
			return downmix32bit_7_1_to_5_1;
		case IPC4_CHANNEL_CONFIG_INVALID:
		default:
			comp_err(dev, "select_mix_out_5_1(): invalid channel config.");
			return NULL;
		}
	}
}

static int init_mix(struct processing_module *mod,
		    const struct ipc4_audio_format *format,
		    enum ipc4_channel_config out_channel_config,
		    const downmix_coefficients downmix_coefficients)
{
	struct up_down_mixer_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;

	if (!format)
		return -EINVAL;

	if (out_channel_config == IPC4_CHANNEL_CONFIG_MONO) {
		/* Select dowm mixing routine. */
		cd->mix_routine = select_mix_out_mono(dev, format);

		/* Update audio format. */
		cd->out_fmt[0].channels_count = 1;
		cd->out_fmt[0].ch_cfg = IPC4_CHANNEL_CONFIG_MONO;
		cd->out_fmt[0].ch_map = create_channel_map(IPC4_CHANNEL_CONFIG_MONO);

	} else if (out_channel_config == IPC4_CHANNEL_CONFIG_STEREO) {
		/* DOWN_MIX */
		if (format->interleaving_style != IPC4_CHANNELS_INTERLEAVED ||
		    format->depth == IPC4_DEPTH_8BIT)
			return -EINVAL;

		/* Select dowm mixing routine. */
		cd->mix_routine = select_mix_out_stereo(dev, format);

		/* Update audio format. */
		cd->out_fmt[0].channels_count = 2;
		cd->out_fmt[0].ch_cfg = IPC4_CHANNEL_CONFIG_STEREO;
		cd->out_fmt[0].ch_map = create_channel_map(IPC4_CHANNEL_CONFIG_STEREO);

	} else if (out_channel_config == IPC4_CHANNEL_CONFIG_5_POINT_1) {
		/* Select dowm mixing routine. */
		cd->mix_routine = select_mix_out_5_1(dev, format);

		/* Update audio format. */
		cd->out_fmt[0].channels_count = 6;
		cd->out_fmt[0].ch_cfg = IPC4_CHANNEL_CONFIG_5_POINT_1;
		cd->out_fmt[0].ch_map = create_channel_map(IPC4_CHANNEL_CONFIG_5_POINT_1);

	} else if (out_channel_config == IPC4_CHANNEL_CONFIG_7_POINT_1 &&
		   format->ch_cfg == IPC4_CHANNEL_CONFIG_STEREO) {
		/* Select up mixing routine. */
		cd->mix_routine = upmix32bit_2_0_to_7_1;

		if (format->depth == IPC4_DEPTH_16BIT)
			return -EINVAL;
	} else {
		return -EINVAL;
	}

	/* Update audio format. */
	cd->out_fmt[0].valid_bit_depth = IPC4_DEPTH_24BIT;
	cd->out_fmt[0].depth = IPC4_DEPTH_32BIT;

	cd->in_channel_no = format->channels_count;
	cd->in_channel_map = format->ch_map;
	cd->in_channel_config = format->ch_cfg;

	return set_downmix_coefficients(mod, format, out_channel_config, downmix_coefficients);
}

static int up_down_mixer_free(struct processing_module *mod)
{
	struct up_down_mixer_data *cd = module_get_private_data(mod);

	rfree(cd->buf_in);
	rfree(cd->buf_out);
	rfree(cd);

	return 0;
}

static int up_down_mixer_init(struct processing_module *mod)
{
	struct module_config *dst = &mod->priv.cfg;
	const struct ipc4_up_down_mixer_module_cfg *up_down_mixer = dst->init_data;
	struct module_data *mod_data = &mod->priv;
	struct comp_dev *dev = mod->dev;
	struct up_down_mixer_data *cd;
	int ret;

	cd = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(*cd));
	if (!cd) {
		comp_free(dev);
		return -ENOMEM;
	}

	mod_data->private = cd;

	cd->buf_in = rballoc(0, SOF_MEM_CAPS_RAM, mod->priv.cfg.base_cfg.ibs);
	cd->buf_out = rballoc(0, SOF_MEM_CAPS_RAM, mod->priv.cfg.base_cfg.obs);
	if (!cd->buf_in || !cd->buf_out) {
		ret = -ENOMEM;
		goto err;
	}

	switch (up_down_mixer->coefficients_select) {
	case DEFAULT_COEFFICIENTS:
		cd->out_channel_map = create_channel_map(up_down_mixer->out_channel_config);
		ret = init_mix(mod, &mod->priv.cfg.base_cfg.audio_fmt,
			       up_down_mixer->out_channel_config, NULL);
		break;
	case CUSTOM_COEFFICIENTS:
		cd->out_channel_map = create_channel_map(up_down_mixer->out_channel_config);
		ret = init_mix(mod, &mod->priv.cfg.base_cfg.audio_fmt,
			       up_down_mixer->out_channel_config, up_down_mixer->coefficients);
		break;
	case DEFAULT_COEFFICIENTS_WITH_CHANNEL_MAP:
		cd->out_channel_map = up_down_mixer->channel_map;
		ret = init_mix(mod, &mod->priv.cfg.base_cfg.audio_fmt,
			       up_down_mixer->out_channel_config, NULL);
		break;
	case CUSTOM_COEFFICIENTS_WITH_CHANNEL_MAP:
		cd->out_channel_map = up_down_mixer->channel_map;
		ret = init_mix(mod, &mod->priv.cfg.base_cfg.audio_fmt,
			       up_down_mixer->out_channel_config, up_down_mixer->coefficients);
		break;
	default:
		comp_err(dev, "up_down_mixer_init(): unsupported coefficient type");
		ret = -EINVAL;
		break;
	}

	if (ret < 0) {
		comp_err(dev, "up_down_mixer_init(): failed to initialize up_down_mix");
		goto err;
	}

	return 0;

err:
	up_down_mixer_free(mod);
	return ret;
}

/* just stubs for now. Remove these after making these ops optional in the module adapter */
static int up_down_mixer_prepare(struct processing_module *mod,
				 struct sof_source **sources, int num_of_sources,
				 struct sof_sink **sinks, int num_of_sinks)
{
	struct up_down_mixer_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;

	if (!cd->mix_routine) {
		comp_err(dev, "up_down_mixer_prepare(): mix routine not initialized");
		return -EINVAL;
	}

	return 0;
}

static int up_down_mixer_reset(struct processing_module *mod)
{
	return 0;
}

static int
up_down_mixer_process(struct processing_module *mod,
		      struct input_stream_buffer *input_buffers, int num_input_buffers,
		      struct output_stream_buffer *output_buffers, int num_output_buffers)
{
	struct up_down_mixer_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	uint32_t source_bytes, sink_bytes;
	uint32_t mix_frames;

	comp_dbg(dev, "up_down_mixer_process()");

	mix_frames = audio_stream_avail_frames(mod->input_buffers[0].data,
					       mod->output_buffers[0].data);

	source_bytes = mix_frames * audio_stream_frame_bytes(mod->input_buffers[0].data);
	sink_bytes = mix_frames * audio_stream_frame_bytes(mod->output_buffers[0].data);

	if (source_bytes) {
		uint32_t sink_sample_bytes;

		audio_stream_copy_to_linear(mod->input_buffers[0].data, 0, cd->buf_in, 0,
					    source_bytes /
					    audio_stream_sample_bytes(mod->input_buffers[0].data));

		cd->mix_routine(cd, (uint8_t *)cd->buf_in, source_bytes, (uint8_t *)cd->buf_out);

		sink_sample_bytes = audio_stream_sample_bytes(mod->output_buffers[0].data);
		audio_stream_copy_from_linear(cd->buf_out, 0, mod->output_buffers[0].data, 0,
					      sink_bytes / sink_sample_bytes);
		mod->output_buffers[0].size = sink_bytes;
		mod->input_buffers[0].consumed = source_bytes;
	}

	return 0;
}

static const struct module_interface up_down_mixer_interface = {
	.init = up_down_mixer_init,
	.prepare = up_down_mixer_prepare,
	.process_audio_stream = up_down_mixer_process,
	.reset = up_down_mixer_reset,
	.free = up_down_mixer_free
};

DECLARE_MODULE_ADAPTER(up_down_mixer_interface, up_down_mixer_comp_uuid, up_down_mixer_comp_tr);
SOF_MODULE_INIT(up_down_mixer, sys_comp_module_up_down_mixer_interface_init);
