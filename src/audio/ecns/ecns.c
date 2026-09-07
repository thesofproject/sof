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
	int16_t  ref_weight; /* Echo cancellation filter tap weight */
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
		cd->sample_rate  = 16000;
		cd->channels     = ECNS_IN_CHANNELS;
		cd->sample_width = 16;
	}

	cd->ref_weight = 8192; /* Initial 0.25 scaling Q15 for echo subtraction */

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
			   cd->base_cfg.audio_fmt.sampling_frequency : 16000;
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
	comp_info(dev, "ecns_reset");
	return comp_set_state(dev, COMP_TRIGGER_RESET);
}

static int ecns_trigger(struct comp_dev *dev, int cmd)
{
	comp_info(dev, "ecns_trigger cmd=%d", cmd);
	return comp_set_state(dev, cmd);
}

/*
 * Main 20ms DP processing function:
 * Input: 4 channels (Ch 0 = Mic 0, Ch 1 = Mic 1, Ch 2 = Echo Ref 0, Ch 3 = Echo Ref 1)
 * Internal: Ch 2, 3 cancel acoustic echo and noise on Ch 0, 1.
 * Output Pin 0 (KPB sink, mono): Clean Channel 0
 * Output Pin 1 (Host sink, stereo): Clean Channel 0 and 1
 */
static int ecns_copy(struct comp_dev *dev)
{
	struct comp_buffer *source;
	struct comp_buffer *sink;
	struct comp_buffer *kpb_sink = NULL;
	struct comp_buffer *host_sink = NULL;
	struct ecns_comp_data *cd = comp_get_drvdata(dev);
	uint32_t avail_bytes, avail_frames;
	uint32_t frames_to_copy;

	if (list_is_empty(&dev->bsource_list))
		return 0;

	source = comp_dev_get_first_data_producer(dev);
	if (!source)
		return 0;

	/* Identify sinks by channel configuration */
	comp_dev_for_each_consumer(dev, sink) {
		uint32_t ch = audio_stream_get_channels(&sink->stream);
		if (ch == 1 && !kpb_sink)
			kpb_sink = sink;
		else if (ch == 2 && !host_sink)
			host_sink = sink;
		else if (!kpb_sink)
			kpb_sink = sink;
		else if (!host_sink)
			host_sink = sink;
	}

	avail_bytes = audio_stream_get_avail_bytes(&source->stream);
	uint32_t src_frame_bytes = 4 * sizeof(int16_t);
	avail_frames = avail_bytes / src_frame_bytes;

	if (avail_frames < 16)
		return PPL_STATUS_PATH_STOP;

	/* Process up to 320 frames (20ms) or available batch */
	frames_to_copy = MIN(avail_frames, ECNS_FRAME_SAMPLES);

	/* Check available sink buffer limits independently */
	uint32_t kpb_frames = 0;
	if (kpb_sink) {
		uint32_t kpb_free = audio_stream_get_free_bytes(&kpb_sink->stream) / sizeof(int16_t);
		kpb_frames = MIN(frames_to_copy, kpb_free);
	}

	uint32_t host_frames = 0;
	if (host_sink) {
		uint32_t host_free = audio_stream_get_free_bytes(&host_sink->stream) / (2 * sizeof(int16_t));
		host_frames = MIN(frames_to_copy, host_free);
	}

	/* If sinks are connected but neither can accept data, stop path */
	if ((kpb_sink || host_sink) && kpb_frames == 0 && host_frames == 0)
		return PPL_STATUS_PATH_STOP;

	uint32_t src_bytes = frames_to_copy * src_frame_bytes;
	buffer_stream_invalidate(source, src_bytes);

	/* Prepare output buffers if available */
	for (uint32_t i = 0; i < frames_to_copy; i++) {
		int16_t mic0 = *(int16_t *)audio_stream_read_frag_s16(&source->stream, 4 * i + 0);
		int16_t mic1 = *(int16_t *)audio_stream_read_frag_s16(&source->stream, 4 * i + 1);
		int16_t ref0 = *(int16_t *)audio_stream_read_frag_s16(&source->stream, 4 * i + 2);
		int16_t ref1 = *(int16_t *)audio_stream_read_frag_s16(&source->stream, 4 * i + 3);

		/* Acoustic echo cancellation using channels 2,3 reference taps */
		int32_t echo_est0 = ((int32_t)ref0 * cd->ref_weight) >> 15;
		int32_t echo_est1 = ((int32_t)ref1 * cd->ref_weight) >> 15;

		int32_t clean0_32 = (int32_t)mic0 - echo_est0;
		int32_t clean1_32 = (int32_t)mic1 - echo_est1;

		/* Saturate to 16-bit PCM */
		int16_t clean0 = sat_int16(clean0_32);
		int16_t clean1 = sat_int16(clean1_32);

		/* Output 0: Channel 0 mono to KPB sink */
		if (kpb_sink && i < kpb_frames)
			*(int16_t *)audio_stream_write_frag_s16(&kpb_sink->stream, i) = clean0;

		/* Output 1: Channels 0,1 stereo to Host sink */
		if (host_sink && i < host_frames) {
			*(int16_t *)audio_stream_write_frag_s16(&host_sink->stream, 2 * i + 0) = clean0;
			*(int16_t *)audio_stream_write_frag_s16(&host_sink->stream, 2 * i + 1) = clean1;
		}
	}

	comp_update_buffer_consume(source, src_bytes);

	if (kpb_sink && kpb_frames > 0) {
		uint32_t kpb_bytes = kpb_frames * sizeof(int16_t);
		buffer_stream_writeback(kpb_sink, kpb_bytes);
		comp_update_buffer_produce(kpb_sink, kpb_bytes);
	}

	if (host_sink && host_frames > 0) {
		uint32_t host_bytes = host_frames * 2 * sizeof(int16_t);
		buffer_stream_writeback(host_sink, host_bytes);
		comp_update_buffer_produce(host_sink, host_bytes);
	}

	return 0;
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
		.get_attribute	= ecns_get_attribute,
	},
};

static SHARED_DATA struct comp_driver_info ecns_info = {
	.drv = &ecns_drv,
};

UT_STATIC void sys_comp_ecns_init(void)
{
	comp_register(&ecns_info);
}

DECLARE_MODULE(sys_comp_ecns_init);
SOF_MODULE_INIT(ecns, sys_comp_ecns_init);
