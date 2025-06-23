// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 Intel Corporation. All rights reserved.
//
// Author: Bartosz Kokoszko <bartoszx.kokoszko@intel.com>
// Author: Adrian Bonislawski <adrian.bonislawski@intel.com>

#include <sof/audio/buffer.h>
#include <sof/audio/format.h>
#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/pipeline.h>
#include <rtos/panic.h>
#include <sof/ipc/msg.h>
#include <rtos/alloc.h>
#include <rtos/cache.h>
#include <rtos/init.h>
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

#include "up_down_mixer_coef.h"
#include "up_down_mixer.h"

LOG_MODULE_REGISTER(up_down_mixer, CONFIG_SOF_LOG_LEVEL);

/* these ids aligns windows driver requirement to support windows driver */
SOF_DEFINE_REG_UUID(up_down_mixer);

DECLARE_TR_CTX(up_down_mixer_comp_tr, SOF_UUID(up_down_mixer_uuid),
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

	comp_info(dev, "set_downmix_coefficients()");

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
	comp_info(dev, "select_mix_out_stereo()");

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
	comp_info(dev, "select_mix_out_mono(");

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
	comp_info(dev, "select_mix_out_5_1()");

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

	comp_info(dev, "init_mix()");

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

	comp_info(mod->dev, "up_down_mixer_free()");

	rfree(cd->buf_in);
	rfree(cd->buf_out);
	rfree(cd);

	return 0;
}

static int up_down_mixer_init(struct processing_module *mod)
{
	struct up_down_mixer_config default_config;
	struct module_config *dst = &mod->priv.cfg;
	const struct ipc4_up_down_mixer_module_cfg *up_down_mixer_init = dst->init_data;
	const struct up_down_mixer_config *up_down_mixer = &up_down_mixer_init->config;
	struct module_data *mod_data = &mod->priv;
	struct comp_dev *dev = mod->dev;
	struct up_down_mixer_data *cd;
	size_t min_size;
	int ret;
	int i;

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

	comp_info(dev, "nb_input_pins = %d, nb_output_pins = %d",
		  dst->nb_input_pins, dst->nb_output_pins);

	min_size = sizeof(struct ipc4_base_module_cfg);
	if (dst->nb_input_pins > 0 && dst->nb_output_pins > 0)
		min_size += ipc4_calc_base_module_cfg_ext_size(dst->nb_input_pins,
							       dst->nb_output_pins);
	comp_info(dev, "ipc_config_size = %d, min_size = %d",
		  dev->ipc_config.ipc_config_size, min_size);

	if (dev->ipc_config.ipc_config_size > min_size) {
		comp_info(dev, "init configuration found");
		cd->has_init_mix_configuration = true;
		comp_info(dev, "init data: %d, %d, 0x%08x", up_down_mixer->out_channel_config,
			  up_down_mixer->coefficients_select, up_down_mixer->channel_map);
#if UP_DOWN_MIX_COEFFS_LENGTH == 8
		comp_info(dev, "coef[0..3]: %d, %d, %d, %d",
			  up_down_mixer->coefficients[0], up_down_mixer->coefficients[1],
			  up_down_mixer->coefficients[2], up_down_mixer->coefficients[3]);
		comp_info(dev, "coef[4..7]: %d, %d, %d, %d",
			  up_down_mixer->coefficients[4], up_down_mixer->coefficients[5],
			  up_down_mixer->coefficients[6], up_down_mixer->coefficients[7]);
#endif
	} else {
		comp_info(dev, "no configuration in init, using default");
		cd->has_init_mix_configuration = false;
#if FORCE_REQUEST_MONO
		default_config.out_channel_config = 0;
		default_config.coefficients_select = 2;
		default_config.channel_map = -15;
		for (i = 0; i < UP_DOWN_MIX_COEFFS_LENGTH; i++)
			default_config.coefficients[i] = 0;
#else
		default_config.out_channel_config = up_down_mixer_init->base_cfg.audio_fmt.ch_cfg;
		default_config.coefficients_select = DEFAULT_COEFFICIENTS;
		for (i = 0; i < UP_DOWN_MIX_COEFFS_LENGTH; i++)
			default_config.coefficients[i] = 0;
#endif
		up_down_mixer = &default_config;
	}

	switch (up_down_mixer->coefficients_select) {
	case DEFAULT_COEFFICIENTS:
		cd->out_channel_map = create_channel_map(up_down_mixer->out_channel_config);
		comp_info(dev, "c1 out_channel map = 0x%08x", cd->out_channel_map);
		ret = init_mix(mod, &mod->priv.cfg.base_cfg.audio_fmt,
			       up_down_mixer->out_channel_config, NULL);
		comp_info(dev, "c1 init_mix() done");
		break;
	case CUSTOM_COEFFICIENTS:
		cd->out_channel_map = create_channel_map(up_down_mixer->out_channel_config);
		comp_info(dev, "c2 out_channel map = 0x%08x", cd->out_channel_map);
		ret = init_mix(mod, &mod->priv.cfg.base_cfg.audio_fmt,
			       up_down_mixer->out_channel_config, up_down_mixer->coefficients);
		comp_info(dev, "c2 init_mix() done");
		break;
	case DEFAULT_COEFFICIENTS_WITH_CHANNEL_MAP:
		cd->out_channel_map = up_down_mixer->channel_map;
		comp_info(dev, "c3 out_channel map = 0x%08x", cd->out_channel_map);
		ret = init_mix(mod, &mod->priv.cfg.base_cfg.audio_fmt,
			       up_down_mixer->out_channel_config, NULL);
		comp_info(dev, "c3 init_mix() done");
		break;
	case CUSTOM_COEFFICIENTS_WITH_CHANNEL_MAP:
		cd->out_channel_map = up_down_mixer->channel_map;
		comp_info(dev, "c4 out_channel map = 0x%08x", cd->out_channel_map);
		ret = init_mix(mod, &mod->priv.cfg.base_cfg.audio_fmt,
			       up_down_mixer->out_channel_config, up_down_mixer->coefficients);
		comp_info(dev, "c4 init_mix() done");
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

	comp_info(dev, "up_down_mixer_init() ready");
	return 0;

err:
	comp_err(dev, "up_down_mixer_init() error");
	up_down_mixer_free(mod);
	return ret;
}

static int
up_down_mixer_process(struct processing_module *mod,
		      struct sof_source **input_buffers, int num_input_buffers,
		      struct sof_sink **output_buffers, int num_output_buffers)
{
	struct up_down_mixer_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;

	size_t output_frames, input_frames, ret, input_cirbuf_size, output_cirbuf_size;
	const uint8_t *input_pos, *input_start, *input_end;
	uint8_t *output_pos, *output_start, *output_end;
	int output_frames_without_wrap;
	int frames_without_wrap, frames, frames_remain;
	uint32_t input_bytes;

	comp_dbg(dev, "up_down_mixer_process()");

	output_frames = sink_get_free_frames(output_buffers[0]);
	input_frames = source_get_data_frames_available(input_buffers[0]);
	frames = MIN(input_frames, output_frames);

	const size_t output_frame_bytes = sink_get_frame_bytes(output_buffers[0]);

	ret = sink_get_buffer(output_buffers[0], frames * output_frame_bytes,
			      (void **)&output_pos, (void **)&output_start, &output_cirbuf_size);
	if (ret)
		return -ENODATA;

	const size_t input_frame_bytes = source_get_frame_bytes(input_buffers[0]);

	ret = source_get_data(input_buffers[0], frames * input_frame_bytes,
			      (const void **)&input_pos, (const void **)&input_start,
			      &input_cirbuf_size);
	if (ret) {
		sink_commit_buffer(output_buffers[0], 0);
		return -ENODATA;
	}

	input_end = input_start + input_cirbuf_size;
	output_end = output_start + output_cirbuf_size;
	frames_remain = frames;
	while (frames_remain) {
		frames_without_wrap = (input_end - input_pos) / input_frame_bytes;
		frames_without_wrap = MIN(frames_without_wrap, frames);
		output_frames_without_wrap = (output_end - output_pos) / output_frame_bytes;
		frames_without_wrap = MIN(frames_without_wrap, output_frames_without_wrap);
		input_bytes = input_frame_bytes * frames_without_wrap;
		cd->mix_routine(cd, (const void *)input_pos, input_bytes, (void *)output_pos);

		input_pos += input_bytes;
		output_pos += frames_without_wrap * output_frame_bytes;
		input_pos = (input_pos >= input_end) ? input_pos - input_cirbuf_size : input_pos;
		output_pos = (output_pos >= output_end) ?
			output_pos - output_cirbuf_size : output_pos;
		frames_remain -= frames_without_wrap;
	}

	/* Ignore possible -ENODATA, can happen in start with zero frames to process */
	sink_commit_buffer(output_buffers[0], frames * output_frame_bytes);
	source_release_data(input_buffers[0], frames * input_frame_bytes);
	return 0;
}

static const struct module_interface up_down_mixer_interface = {
	.init = up_down_mixer_init,
	.process = up_down_mixer_process,
	.free = up_down_mixer_free
};

DECLARE_MODULE_ADAPTER(up_down_mixer_interface, up_down_mixer_uuid, up_down_mixer_comp_tr);
SOF_MODULE_INIT(up_down_mixer, sys_comp_module_up_down_mixer_interface_init);
