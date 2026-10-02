/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2026 Intel Corporation. All rights reserved.
 */

#include <sof/audio/component_ext.h>
#include <sof/audio/format.h>
#include <sof/audio/pipeline.h>
#include <sof/audio/ipc-config.h>
#include <sof/audio/ecns.h>
#include <sof/common.h>
#include <sof/lib/memory.h>
#include <sof/ut.h>
#include <sof/trace/trace.h>
#include <ipc4/base_fw.h>
#include <ipc4/header.h>
#include <ipc/stream.h>
#include <rtos/init.h>
#include <zephyr/kernel.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

LOG_MODULE_REGISTER(ecns, CONFIG_SOF_LOG_LEVEL);

SOF_DEFINE_REG_UUID(ecns);
DECLARE_TR_CTX(ecns_tr, SOF_UUID(ecns_uuid), LOG_LEVEL_INFO);

struct ecns_comp_data {
	struct ipc4_base_module_cfg base_cfg;
	uint32_t sample_rate;
	uint32_t channels;
	uint32_t sample_width;
	uint32_t copy_count;
	uint32_t test_signal_enabled;
	uint16_t test_signal_val;
};

static struct comp_dev *ecns_new(const struct comp_driver *drv,
				 const struct comp_ipc_config *config,
				 const void *spec)
{
	struct comp_dev *dev;
	struct ecns_comp_data *cd;

	comp_cl_info(&drv->tctx, "ecns_new");

	dev = comp_alloc(drv, sizeof(*dev));
	if (!dev)
		return NULL;

	dev->ipc_config = *config;

	cd = rzalloc(SOF_MEM_FLAG_USER, sizeof(*cd));
	if (!cd) {
		comp_free_device(dev);
		return NULL;
	}

	const struct ipc4_base_module_cfg *base_cfg = spec;
	if (base_cfg) {
		memcpy_s(&cd->base_cfg, sizeof(cd->base_cfg), base_cfg, sizeof(*base_cfg));
		cd->sample_rate  = base_cfg->audio_fmt.sampling_frequency;
		cd->channels     = base_cfg->audio_fmt.channels_count;
		cd->sample_width = base_cfg->audio_fmt.valid_bit_depth;
	} else {
		cd->sample_rate  = 48000;
		cd->channels     = ECNS_IN_CHANNELS;
		cd->sample_width = 16;
	}

	comp_set_drvdata(dev, cd);
	dev->direction     = SOF_IPC_STREAM_CAPTURE;
	dev->direction_set = true;
	dev->state         = COMP_STATE_READY;

	comp_info(dev, "ecns_new: rate=%u in_ch=%u sample_width=%u",
		  cd->sample_rate, cd->channels, cd->sample_width);

	return dev;
}

static void ecns_free(struct comp_dev *dev)
{
	struct ecns_comp_data *cd = comp_get_drvdata(dev);

	comp_info(dev, "ecns_free");
	rfree(cd);
	comp_free_device(dev);
}

static int ecns_params(struct comp_dev *dev, struct sof_ipc_stream_params *params)
{
	struct ecns_comp_data *cd = comp_get_drvdata(dev);

	comp_info(dev, "ecns_params");

	memset_s(params, sizeof(*params), 0, sizeof(*params));
	params->channels = cd->base_cfg.audio_fmt.channels_count ?
			   cd->base_cfg.audio_fmt.channels_count : ECNS_IN_CHANNELS;
	params->rate     = cd->base_cfg.audio_fmt.sampling_frequency ?
			   cd->base_cfg.audio_fmt.sampling_frequency : 48000;
	params->sample_container_bytes = cd->base_cfg.audio_fmt.depth ?
					 cd->base_cfg.audio_fmt.depth / 8 : 2;
	params->sample_valid_bytes     = cd->base_cfg.audio_fmt.valid_bit_depth ?
					 cd->base_cfg.audio_fmt.valid_bit_depth / 8 : 2;
	params->buffer_fmt = cd->base_cfg.audio_fmt.interleaving_style;

	component_set_nearest_period_frames(dev, params->rate);

	return 0;
}

static int ecns_prepare(struct comp_dev *dev)
{
	comp_info(dev, "ecns_prepare");
	return comp_set_state(dev, COMP_TRIGGER_PREPARE);
}

static int ecns_reset(struct comp_dev *dev)
{
	struct ecns_comp_data *cd = comp_get_drvdata(dev);

	comp_info(dev, "ecns_reset");
	cd->copy_count = 0;
	cd->test_signal_val = 0;
	return comp_set_state(dev, COMP_TRIGGER_RESET);
}

static int ecns_trigger(struct comp_dev *dev, int cmd)
{
	comp_info(dev, "ecns_trigger cmd=%d", cmd);
	return comp_set_state(dev, cmd);
}

/*
 * Main 20ms DP processing function:
 * Consumes:
 *   - Pin 0 in: 16 kHz stream from dmic16k (2 channels, 16-bit)
 *   - Pin 1 in: 48 kHz stream from dmic01 (2 channels, 16-bit)
 * Produces:
 *   - Pin 0 out: 16 kHz mono clean stream to KPB (Left channel only, 320 frames per 20ms)
 *   - Pin 1 out: 48 kHz stereo clean stream to Host PCM 11 (Straight copy, 960 frames per 20ms)
 * Sampling rates remain identical between input and output on each pin.
 */
static int ecns_copy(struct comp_dev *dev)
{
	struct ecns_comp_data *cd = comp_get_drvdata(dev);
	struct comp_buffer *source;
	struct comp_buffer *sink;
	struct comp_buffer *src0 = NULL;
	struct comp_buffer *src1 = NULL;
	struct comp_buffer *snk0 = NULL;
	struct comp_buffer *snk1 = NULL;
	bool copied_any = false;

	if (list_is_empty(&dev->bsource_list))
		return 0;

	/* Identify input pins (producers to ECNS) */
	comp_dev_for_each_producer(dev, source) {
		uint32_t pin = IPC4_SINK_QUEUE_ID(buf_get_id(source));
		uint32_t rate = audio_stream_get_rate(&source->stream);

		if (rate == 16000 || pin == ECNS_PIN_16K_IN) {
			if (!src0)
				src0 = source;
			else if (!src1)
				src1 = source;
		} else if (rate == 48000 || pin == ECNS_PIN_48K_IN) {
			if (!src1)
				src1 = source;
			else if (!src0)
				src0 = source;
		} else if (!src0) {
			src0 = source;
		} else if (!src1) {
			src1 = source;
		}
	}

	/* Identify output pins (consumers from ECNS) */
	comp_dev_for_each_consumer(dev, sink) {
		uint32_t pin = IPC4_SRC_QUEUE_ID(buf_get_id(sink));
		uint32_t rate = audio_stream_get_rate(&sink->stream);
		uint32_t ch = audio_stream_get_channels(&sink->stream);

		if (rate == 16000 || ch == 1 || pin == ECNS_PIN_16K_OUT) {
			if (!snk0)
				snk0 = sink;
			else if (!snk1)
				snk1 = sink;
		} else if (rate == 48000 || ch == 2 || pin == ECNS_PIN_48K_OUT) {
			if (!snk1)
				snk1 = sink;
			else if (!snk0)
				snk0 = sink;
		} else if (!snk0) {
			snk0 = sink;
		} else if (!snk1) {
			snk1 = sink;
		}
	}

	/* Process Pin 0: 16 kHz stream (up to 320 frames = 20ms) -> Left channel only to mono */
	if (src0) {
		uint32_t src0_ch = audio_stream_get_channels(&src0->stream);
		if (!src0_ch)
			src0_ch = 2;
		uint32_t src0_frame_bytes = src0_ch * sizeof(int16_t);
		uint32_t avail0_bytes = audio_stream_get_avail_bytes(&src0->stream);
		uint32_t avail0_frames = avail0_bytes / src0_frame_bytes;

		if (snk0) {
			uint32_t snk0_ch = audio_stream_get_channels(&snk0->stream);
			if (!snk0_ch)
				snk0_ch = 1;
			uint32_t snk0_frame_bytes = snk0_ch * sizeof(int16_t);
			uint32_t free0_bytes = audio_stream_get_free_bytes(&snk0->stream);
			uint32_t free0_frames = free0_bytes / snk0_frame_bytes;

			uint32_t frames0 = MIN(avail0_frames, free0_frames);
			frames0 = MIN(frames0, ECNS_FRAME_SAMPLES_16K);

			if (frames0 > 0) {
				uint32_t src_bytes = frames0 * src0_frame_bytes;
				uint32_t snk_bytes = frames0 * snk0_frame_bytes;

				buffer_stream_invalidate(src0, src_bytes);

				int16_t *src_ptr = audio_stream_get_rptr(&src0->stream);
				int16_t *snk_ptr = audio_stream_get_wptr(&snk0->stream);

				for (uint32_t i = 0; i < frames0; i++) {
					src_ptr = audio_stream_wrap(&src0->stream, src_ptr);
					snk_ptr = audio_stream_wrap(&snk0->stream, snk_ptr);

					/* Pick Left channel (channel 0) only, copy as mono */
					if (cd->test_signal_enabled)
						*snk_ptr = (int16_t)cd->test_signal_val++;
					else
						*snk_ptr = *src_ptr;

					for (uint32_t c = 1; c < snk0_ch; c++) {
						int16_t *snk_c = audio_stream_wrap(&snk0->stream, snk_ptr + c);
						*snk_c = 0;
					}

					src_ptr += src0_ch;
					snk_ptr += snk0_ch;
				}

				comp_update_buffer_consume(src0, src_bytes);
				buffer_stream_writeback(snk0, snk_bytes);
				comp_update_buffer_produce(snk0, snk_bytes);
				copied_any = true;
			}
		} else if (avail0_frames >= ECNS_FRAME_SAMPLES_16K) {
			uint32_t discard = MIN(avail0_frames, ECNS_FRAME_SAMPLES_16K);
			comp_update_buffer_consume(src0, discard * src0_frame_bytes);
		}
	}

	/* Process Pin 1: 48 kHz stream (up to 960 frames = 20ms) -> Straight 1-to-1 copy */
	if (src1) {
		uint32_t src1_ch = audio_stream_get_channels(&src1->stream);
		if (!src1_ch)
			src1_ch = 2;
		uint32_t src1_frame_bytes = src1_ch * sizeof(int16_t);
		uint32_t avail1_bytes = audio_stream_get_avail_bytes(&src1->stream);
		uint32_t avail1_frames = avail1_bytes / src1_frame_bytes;

		if (snk1) {
			uint32_t snk1_ch = audio_stream_get_channels(&snk1->stream);
			if (!snk1_ch)
				snk1_ch = 2;
			uint32_t snk1_frame_bytes = snk1_ch * sizeof(int16_t);
			uint32_t free1_bytes = audio_stream_get_free_bytes(&snk1->stream);
			uint32_t free1_frames = free1_bytes / snk1_frame_bytes;

			uint32_t frames1 = MIN(avail1_frames, free1_frames);
			frames1 = MIN(frames1, ECNS_FRAME_SAMPLES_48K);

			if (frames1 > 0) {
				uint32_t src_bytes = frames1 * src1_frame_bytes;
				uint32_t snk_bytes = frames1 * snk1_frame_bytes;

				buffer_stream_invalidate(src1, src_bytes);

				if (src1_ch == snk1_ch) {
					audio_stream_copy(&src1->stream, 0, &snk1->stream, 0,
							  frames1 * src1_ch);
				} else {
					int16_t *src_ptr = audio_stream_get_rptr(&src1->stream);
					int16_t *snk_ptr = audio_stream_get_wptr(&snk1->stream);
					uint32_t copy_ch = MIN(src1_ch, snk1_ch);

					for (uint32_t i = 0; i < frames1; i++) {
						src_ptr = audio_stream_wrap(&src1->stream, src_ptr);
						snk_ptr = audio_stream_wrap(&snk1->stream, snk_ptr);

						for (uint32_t c = 0; c < copy_ch; c++)
							snk_ptr[c] = src_ptr[c];
						for (uint32_t c = copy_ch; c < snk1_ch; c++)
							snk_ptr[c] = 0;

						src_ptr += src1_ch;
						snk_ptr += snk1_ch;
					}
				}

				comp_update_buffer_consume(src1, src_bytes);
				buffer_stream_writeback(snk1, snk_bytes);
				comp_update_buffer_produce(snk1, snk_bytes);
				copied_any = true;
			}
		} else if (avail1_frames >= ECNS_FRAME_SAMPLES_48K) {
			uint32_t discard = MIN(avail1_frames, ECNS_FRAME_SAMPLES_48K);
			comp_update_buffer_consume(src1, discard * src1_frame_bytes);
		}
	}

	return copied_any ? 0 : PPL_STATUS_PATH_STOP;
}

static int ecns_get_attribute(struct comp_dev *dev, uint32_t type, void *value)
{
	struct ecns_comp_data *cd = comp_get_drvdata(dev);

	if (type == COMP_ATTR_BASE_CONFIG) {
		*(struct ipc4_base_module_cfg *)value = cd->base_cfg;
		return 0;
	}
	return -EINVAL;
}

static int ecns_set_large_config(struct comp_dev *dev,
				 uint32_t param_id,
				 bool first_block,
				 bool last_block,
				 uint32_t data_offset,
				 const char *data)
{
	struct ecns_comp_data *cd = comp_get_drvdata(dev);

	switch (param_id) {
	case SOF_IPC4_SWITCH_CONTROL_PARAM_ID: {
		const struct sof_ipc4_control_msg_payload *cp =
			(const struct sof_ipc4_control_msg_payload *)data;

		if (cp->num_elems < 1)
			return -EINVAL;

		cd->test_signal_enabled = cp->chanv[0].value ? 1 : 0;
		if (cd->test_signal_enabled)
			cd->test_signal_val = 0;

		comp_info(dev, "ecns: test signal %s (kpb mono output)",
			  cd->test_signal_enabled ? "enabled" : "disabled");
		return 0;
	}
	default:
		return -EINVAL;
	}
}

static int ecns_get_large_config(struct comp_dev *dev,
				 uint32_t param_id,
				 bool first_block,
				 bool last_block,
				 uint32_t *data_offset,
				 char *data)
{
	struct ecns_comp_data *cd = comp_get_drvdata(dev);

	switch (param_id) {
	case SOF_IPC4_SWITCH_CONTROL_PARAM_ID: {
		struct sof_ipc4_control_msg_payload *cp =
			(struct sof_ipc4_control_msg_payload *)data;
		uint16_t ctl_id = cp->id;
		uint32_t resp_size = sizeof(struct sof_ipc4_control_msg_payload) +
				     sizeof(struct sof_ipc4_ctrl_value_chan);

		if (resp_size > *data_offset) {
			comp_err(dev, "wrong switch control response size %u vs %u",
				 resp_size, *data_offset);
			return -EINVAL;
		}

		*data_offset = resp_size;
		memset_s(cp, resp_size, 0, resp_size);
		cp->id = ctl_id;
		cp->num_elems = 1;
		cp->chanv[0].channel = 0;
		cp->chanv[0].value = cd->test_signal_enabled;
		return 0;
	}
	default:
		return -EINVAL;
	}
}

static const struct comp_driver ecns_drv = {
	.type	= SOF_COMP_NONE,
	.uid	= SOF_RT_UUID(ecns_uuid),
	.tctx	= &ecns_tr,
	.ops	= {
		.create		= ecns_new,
		.free		= ecns_free,
		.params		= ecns_params,
		.prepare	= ecns_prepare,
		.reset		= ecns_reset,
		.trigger	= ecns_trigger,
		.copy		= ecns_copy,
		.set_large_config = ecns_set_large_config,
		.get_large_config = ecns_get_large_config,
		.get_attribute	= ecns_get_attribute,
	},
};

static struct comp_driver_info ecns_info = {
	.drv = &ecns_drv,
};

UT_STATIC void sys_comp_ecns_init(void)
{
	comp_register(&ecns_info);
}

DECLARE_MODULE(sys_comp_ecns_init);
SOF_MODULE_INIT(ecns, sys_comp_ecns_init);
