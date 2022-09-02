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
#include <sof/audio/pipeline.h>
#include <sof/debug/panic.h>
#include <sof/ipc/msg.h>
#include <rtos/alloc.h>
#include <rtos/cache.h>
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

static const struct comp_driver comp_up_down_mixer;

LOG_MODULE_REGISTER(up_down_mixer, CONFIG_SOF_LOG_LEVEL);

/* these ids aligns windows driver requirement to support windows driver */
/* 42f8060c-832f-4dbf-b247-51e961997b34 */
DECLARE_SOF_RT_UUID("up_down_mixer", up_down_mixer_comp_uuid, 0x42f8060c, 0x832f,
		    0x4dbf, 0xb2, 0x47, 0x51, 0xe9, 0x61, 0x99, 0x7b, 0x34);

DECLARE_TR_CTX(up_down_mixer_comp_tr, SOF_UUID(up_down_mixer_comp_uuid),
	       LOG_LEVEL_INFO);

int32_t custom_coeffs[UP_DOWN_MIX_COEFFS_LENGTH];

static int set_downmix_coefficients(struct comp_dev *dev,
				    const struct ipc4_audio_format *format,
				    const enum ipc4_channel_config out_channel_config,
				    const downmix_coefficients downmix_coefficients)
{
	struct up_down_mixer_data *cd = comp_get_drvdata(dev);
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
			comp_err(dev, "select_mix_out_mono(): invalid channel config.");
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
			comp_err(dev, "select_mix_out_mono(): invalid channel config.");
			return NULL;
		}
	}
}

static int init_mix(struct comp_dev *dev,
		    const struct ipc4_audio_format *format,
		    enum ipc4_channel_config out_channel_config,
		    const downmix_coefficients downmix_coefficients)
{
	struct up_down_mixer_data *cd = comp_get_drvdata(dev);
	int ret;

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

	ret = set_downmix_coefficients(dev, format, out_channel_config, downmix_coefficients);
	if (ret < 0)
		return ret;

	return 0;
}

static void up_down_mixer_free(struct comp_dev *dev)
{
	struct up_down_mixer_data *cd = comp_get_drvdata(dev);

	rfree(cd->buf_in);
	rfree(cd->buf_out);
	rfree(cd);
	rfree(dev);
}

static int init_up_down_mixer(struct comp_dev *dev, struct comp_ipc_config *config, void *spec)
{
	struct ipc4_up_down_mixer_module_cfg *up_down_mixer = spec;
	struct up_down_mixer_data *cd;
	int ret;

	dev->ipc_config = *config;
	list_init(&dev->bsource_list);
	list_init(&dev->bsink_list);

	dcache_invalidate_region((__sparse_force void __sparse_cache *)spec,
				 sizeof(*up_down_mixer));
	cd = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(*cd));
	if (!cd) {
		comp_free(dev);
		return -ENOMEM;
	}

	comp_set_drvdata(dev, cd);

	/* Copy received data format to local structures */
	ret = memcpy_s(&cd->base, sizeof(struct ipc4_base_module_cfg),
		       &up_down_mixer->base_cfg,
		       sizeof(struct ipc4_base_module_cfg));
	if (ret < 0) {
		up_down_mixer_free(dev);
		return ret;
	}

	cd->buf_in = rballoc(0, SOF_MEM_CAPS_RAM, cd->base.ibs);
	cd->buf_out = rballoc(0, SOF_MEM_CAPS_RAM, cd->base.obs);
	if (!cd->buf_in || !cd->buf_out) {
		up_down_mixer_free(dev);
		return -ENOMEM;
	}

	switch (up_down_mixer->coefficients_select) {
	case DEFAULT_COEFFICIENTS:
		cd->out_channel_map = create_channel_map(up_down_mixer->out_channel_config);
		ret = init_mix(dev, &up_down_mixer->base_cfg.audio_fmt, up_down_mixer->out_channel_config, NULL);
		break;
	case CUSTOM_COEFFICIENTS:
		cd->out_channel_map = create_channel_map(up_down_mixer->out_channel_config);
		ret = init_mix(dev, &up_down_mixer->base_cfg.audio_fmt, up_down_mixer->out_channel_config,
			       up_down_mixer->coefficients);
		break;
	case DEFAULT_COEFFICIENTS_WITH_CHANNEL_MAP:
		cd->out_channel_map = up_down_mixer->channel_map;
		ret = init_mix(dev, &up_down_mixer->base_cfg.audio_fmt, up_down_mixer->out_channel_config, NULL);
		break;
	case CUSTOM_COEFFICIENTS_WITH_CHANNEL_MAP:
		cd->out_channel_map = up_down_mixer->channel_map;
		ret = init_mix(dev, &up_down_mixer->base_cfg.audio_fmt, up_down_mixer->out_channel_config,
			       up_down_mixer->coefficients);
		break;
	default:
		comp_err(dev, "init_up_down_mixer(): unsupported coefficient type");
		up_down_mixer_free(dev);
		return -EINVAL;
	}

	if (!cd->mix_routine || ret < 0) {
		up_down_mixer_free(dev);
		comp_err(dev, "init_up_down_mixer(): mix routine uninitialized.");
		return -EINVAL;
	}

	return 0;
}

static struct comp_dev *up_down_mixer_new(const struct comp_driver *drv,
					  struct comp_ipc_config *config,
					  void *spec)
{
	struct comp_dev *dev;
	int ret;

	comp_cl_info(&comp_up_down_mixer, "up_down_mixer_new()");

	dev = comp_alloc(drv, sizeof(*dev));
	if (!dev)
		return NULL;

	ret = init_up_down_mixer(dev, config, spec);
	if (ret < 0) {
		comp_free(dev);
		return NULL;
	}
	dev->state = COMP_STATE_READY;

	return dev;
}

static int up_down_mixer_prepare(struct comp_dev *dev)
{
	int ret;

	if (dev->state == COMP_STATE_ACTIVE) {
		comp_info(dev, "up_down_mixer_prepare(): Component is in active state.");
		return 0;
	}

	ret = comp_set_state(dev, COMP_TRIGGER_PREPARE);
	if (ret < 0)
		return ret;

	if (ret == COMP_STATUS_STATE_ALREADY_SET)
		return PPL_STATUS_PATH_STOP;

	return 0;
}

static int up_down_mixer_reset(struct comp_dev *dev)
{
	comp_set_state(dev, COMP_TRIGGER_RESET);
	return 0;
}

static int up_down_mixer_copy(struct comp_dev *dev)
{
	struct comp_buffer *source, *sink;
	struct comp_buffer __sparse_cache *source_c, *sink_c;
	uint32_t source_bytes, sink_bytes;
	uint32_t mix_frames;

	struct up_down_mixer_data *cd = comp_get_drvdata(dev);

	comp_dbg(dev, "up_down_mixer_copy()");

	source = list_first_item(&dev->bsource_list, struct comp_buffer,
				 sink_list);
	sink = list_first_item(&dev->bsink_list, struct comp_buffer,
			       source_list);

	source_c = buffer_acquire(source);
	sink_c = buffer_acquire(sink);

	mix_frames = MIN(audio_stream_get_avail_frames(&source_c->stream),
			 audio_stream_get_free_frames(&sink_c->stream));

	source_bytes = mix_frames * audio_stream_frame_bytes(&source_c->stream);
	sink_bytes = mix_frames * audio_stream_frame_bytes(&sink_c->stream);

	if (source_bytes) {
		buffer_stream_invalidate(source_c, source_bytes);
		audio_stream_copy_to_linear(&source_c->stream, 0, cd->buf_in, 0,
					    source_bytes /
					    audio_stream_sample_bytes(&source_c->stream));

		cd->mix_routine(cd, (uint8_t *)cd->buf_in, source_bytes, (uint8_t *)cd->buf_out);

		audio_stream_copy_from_linear(cd->buf_out, 0, &sink_c->stream, 0,
					      sink_bytes /
					      audio_stream_sample_bytes(&sink_c->stream));
		buffer_stream_writeback(sink_c, sink_bytes);

		comp_update_buffer_produce(sink_c, sink_bytes);
		comp_update_buffer_consume(source_c, source_bytes);
	}

	buffer_release(sink_c);
	buffer_release(source_c);

	return 0;
}

static int up_down_mixer_trigger(struct comp_dev *dev, int cmd)
{
	return comp_set_state(dev, cmd);
}

static int up_down_mixer_get_attribute(struct comp_dev *dev, uint32_t type, void *value)
{
	struct up_down_mixer_data *cd = comp_get_drvdata(dev);

	switch (type) {
	case COMP_ATTR_BASE_CONFIG:
		*(struct ipc4_base_module_cfg *)value = cd->base;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static const struct comp_driver comp_up_down_mixer = {
	.uid	= SOF_RT_UUID(up_down_mixer_comp_uuid),
	.tctx	= &up_down_mixer_comp_tr,
	.ops	= {
		.create			= up_down_mixer_new,
		.free			= up_down_mixer_free,
		.trigger		= up_down_mixer_trigger,
		.copy			= up_down_mixer_copy,
		.prepare		= up_down_mixer_prepare,
		.reset			= up_down_mixer_reset,
		.get_attribute		= up_down_mixer_get_attribute,
	},
};

static SHARED_DATA struct comp_driver_info comp_up_down_mixer_info = {
	.drv = &comp_up_down_mixer,
};

UT_STATIC void sys_comp_up_down_mixer_init(void)
{
	comp_register(platform_shared_get(&comp_up_down_mixer_info,
					  sizeof(comp_up_down_mixer_info)));
}

DECLARE_MODULE(sys_comp_up_down_mixer_init);
