// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Intel Corporation. All rights reserved.
//
// Author: Rander Wang <rander.wang@linux.intel.com>

#include <sof/audio/buffer.h>
#include <sof/audio/component_ext.h>
#include <sof/audio/format.h>
#include <sof/audio/pipeline.h>
#include <sof/audio/volume.h>
#include <sof/common.h>
#include <sof/debug/panic.h>
#include <sof/drivers/interrupt.h>
#include <sof/ipc/msg.h>
#include <sof/drivers/interrupt.h>
#include <sof/drivers/timer.h>
#include <sof/lib/alloc.h>
#include <sof/lib/cache.h>
#include <sof/lib/memory.h>
#include <sof/lib/notifier.h>
#include <sof/lib/uuid.h>
#include <sof/list.h>
#include <sof/string.h>
#include <sof/ut.h>
#include <sof/trace/trace.h>
#include <ipc4/module.h>
#include <ipc4/peak_volume.h>
#include <ipc4/fw_reg.h>
#include <ipc/dai.h>
#include <user/trace.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

/* Purpose of Peak Volume/Meter (PeakVol) is to:
 * 1. Measure input volume (amplitude)
 * 2. Change signal volume (optional)
 * 3. Fade signal	(optional)
 * Fading signal consumes a lot of MCPS. It is recommended
 * to use signal fading only for playback purposes.
 */

static const struct comp_driver comp_peakvol;
static const struct comp_driver comp_gain;

/* these ids aligns windows driver requirement to support windows driver */
/* 8a171323-94a3-4e1d-afe9-fe5dbaa4c393 */
DECLARE_SOF_RT_UUID("peak_volume", peakvol_comp_uuid, 0x8a171323, 0x94a3, 0x4e1d,
		    0xaf, 0xe9, 0xfe, 0x5d, 0xba, 0xa4, 0xc3, 0x93);

/* 61bca9a8-18d0-4a18-8e7b-2639219804b7 */
DECLARE_SOF_RT_UUID("gain", gain_comp_uuid, 0x61bca9a8, 0x18d0, 0x4a18,
		    0x8e, 0x7b, 0x26, 0x39, 0x21, 0x98, 0x04, 0xb7);

DECLARE_TR_CTX(peakvol_comp_tr, SOF_UUID(peakvol_comp_uuid), LOG_LEVEL_INFO);

struct peakvol_data {
	struct ipc4_base_module_cfg base;
	vol_scale_func proc_peakvol;
	enum ipc4_vol_mode mode;

	/* store peak volume in mailbox */
	uint32_t mailbox_offset;

	uint32_t active_channels;

	/* these values will be stored to mailbox for host */
	struct ipc4_peak_volume_regs peak_regs;
};

static int set_volume(struct peakvol_data *cd, uint32_t const channel, uint32_t const target_volume,
		      uint32_t const curve_type, uint64_t const curve_duration)
{
	cd->peak_regs.target_volume_[channel] = target_volume;

	/* TODO: add curve support */
	if (curve_type != IPC4_AUDIO_CURVE_TYPE_NONE)
		comp_cl_warn(&comp_peakvol, "curve is not support now");

	return 0;
}

static int init_peakvol(struct comp_dev *dev, struct comp_ipc_config *config,
			void *spec, enum ipc4_vol_mode mode)
{
	struct ipc4_peak_volume_module_cfg *peakvol = spec;
	struct peakvol_data *cd;
	uint8_t channel_cfg;
	uint8_t channel;

	dev->ipc_config = *config;
	list_init(&dev->bsource_list);
	list_init(&dev->bsink_list);

	dcache_invalidate_region(spec, sizeof(*peakvol));
	cd = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(cd->base));
	if (!cd) {
		comp_free(dev);
		return -ENOMEM;
	}

	mailbox_hostbox_read(&cd->base, sizeof(cd->base), 0, sizeof(cd->base));
	cd->active_channels = cd->base.audio_fmt.channels_count;
	cd->mode = mode;

	for (channel = 0; channel < cd->active_channels; channel++) {
		if (peakvol->config[0].channel_id == IPC4_ALL_CHANNELS_MASK)
			channel_cfg = 0;
		else
			channel_cfg = channel;

		cd->peak_regs.current_volume_[channel] = 0;
		set_volume(cd, channel, peakvol->config[channel_cfg].target_volume,
			   peakvol->config[channel_cfg].curve_type,
			   peakvol->config[channel_cfg].curve_duration);
		cd->peak_regs.peak_meter_[channel] = 0;
	}

	comp_set_drvdata(dev, cd);

	return 0;
}

static struct comp_dev *peakvol_new(const struct comp_driver *drv,
				    struct comp_ipc_config *config,
				    void *spec)
{
	struct peakvol_data *cd;
	struct comp_dev *dev;
	int ret;

	comp_cl_dbg(&comp_peakvol, "peakvol_new()");

	dev = comp_alloc(drv, sizeof(*dev));
	if (!dev)
		return NULL;

	ret = init_peakvol(dev, config, spec, IPC4_MODE_PEAKVOL);
	if (ret < 0) {
		comp_free(dev);
		return NULL;
	}

	cd = comp_get_drvdata(dev);
	cd->mailbox_offset = offsetof(struct ipc4_fw_registers, peak_vol_regs);
	cd->mailbox_offset += IPC4_INST_ID(config->id) * sizeof(struct ipc4_peak_volume_regs);

	dev->state = COMP_STATE_READY;
	return dev;
}

static struct comp_dev *gain_new(const struct comp_driver *drv, struct comp_ipc_config *config,
				 void *spec)
{
	struct comp_dev *dev;
	int ret;

	comp_cl_dbg(&comp_gain, "gain_new()");

	dev = comp_alloc(drv, sizeof(*dev));
	if (!dev)
		return NULL;

	ret = init_peakvol(dev, config, spec, IPC4_MODE_GAIN);
	if (ret < 0) {
		comp_free(dev);
		return NULL;
	}

	dev->state = COMP_STATE_READY;
	return dev;
}

static void peakvol_free(struct comp_dev *dev)
{
	struct peakvol_data *cd = comp_get_drvdata(dev);
	struct ipc4_peak_volume_regs regs;

	/* clear mailbox */
	memset_s(&regs, sizeof(regs), 0, sizeof(regs));
	mailbox_sw_regs_write(cd->mailbox_offset, &regs, sizeof(regs));

	rfree(cd);
	rfree(dev);
}

static void gain_free(struct comp_dev *dev)
{
	struct peakvol_data *cd = comp_get_drvdata(dev);

	rfree(cd);
	rfree(dev);
}

static inline void update_peakvol_in_mailbox(struct peakvol_data *cd)
{
	mailbox_sw_regs_write(cd->mailbox_offset, &cd->peak_regs, sizeof(cd->peak_regs));
}

/* TODO: add curve support in 16bit & 32bit functions */
static void peakvol_process_sample_16bit(struct comp_dev *dev, struct audio_stream *sink,
					 const struct audio_stream *source, uint32_t frames)
{
	struct peakvol_data *cd = comp_get_drvdata(dev);
	uint32_t buff_frag = 0;
	uint32_t channel;
	int16_t *src;
	int16_t *dest;
	int32_t i;

	for (channel = 0; channel < sink->channels; channel++) {
		cd->peak_regs.peak_meter_[channel] = 0;
		cd->peak_regs.current_volume_[channel] = cd->peak_regs.target_volume_[channel];
	}

	/* Samples are Q1.15 --> Q1.15 and volume is Q1.31 */
	for (i = 0; i < frames; i++) {
		for (channel = 0; channel < sink->channels; channel++) {
			src = audio_stream_read_frag_s16(source, buff_frag);
			dest = audio_stream_write_frag_s16(sink, buff_frag);

			*dest = q_multsr_sat_32x32_16
				(*src, cd->peak_regs.target_volume_[channel],
				 Q_SHIFT_BITS_32(15, 31, 15));

			if (*dest > cd->peak_regs.peak_meter_[channel])
				cd->peak_regs.peak_meter_[channel] = *dest;

			buff_frag++;
		}
	}

	if (cd->mode == IPC4_MODE_PEAKVOL)
		update_peakvol_in_mailbox(cd);
}

static void peakvol_process_sample_32bit(struct comp_dev *dev, struct audio_stream *sink,
					 const struct audio_stream *source, uint32_t frames)
{
	struct peakvol_data *cd = comp_get_drvdata(dev);
	uint32_t buff_frag = 0;
	uint32_t channel;
	int32_t *src;
	int32_t *dest;
	int32_t i;

	for (channel = 0; channel < sink->channels; channel++) {
		cd->peak_regs.peak_meter_[channel] = 0;
		cd->peak_regs.current_volume_[channel] = cd->peak_regs.target_volume_[channel];
	}

	/* Samples are Q1.31 --> Q1.31 and volume is Q1.31 */
	for (i = 0; i < frames; i++) {
		for (channel = 0; channel < sink->channels; channel++) {
			src = audio_stream_read_frag_s32(source, buff_frag);
			dest = audio_stream_write_frag_s32(sink, buff_frag);

			*dest = q_multsr_sat_32x32
				(*src, cd->peak_regs.target_volume_[channel],
				 Q_SHIFT_BITS_64(31, 31, 31));

			if (*dest > cd->peak_regs.peak_meter_[channel])
				cd->peak_regs.peak_meter_[channel] = *dest;

			buff_frag++;
		}
	}

	if (cd->mode == IPC4_MODE_PEAKVOL)
		update_peakvol_in_mailbox(cd);
}

static int peakvol_prepare(struct comp_dev *dev)
{
	struct peakvol_data *cd = comp_get_drvdata(dev);
	int ret;

	comp_dbg(dev, "peakvol_prepare()");

	if (dev->state == COMP_STATE_ACTIVE) {
		comp_info(dev, "peakvol_config_prepare(): Component is in active state.");
		return 0;
	}

	switch (cd->base.audio_fmt.depth) {
	case IPC4_DEPTH_16BIT:
		cd->proc_peakvol = peakvol_process_sample_16bit;
		break;
	case IPC4_DEPTH_32BIT:
		cd->proc_peakvol = peakvol_process_sample_32bit;
		break;
	default:
		comp_err(dev, "peakvol_prepare(): unsupported depth %d", cd->base.audio_fmt.depth);
		return -EINVAL;
	}

	ret = comp_set_state(dev, COMP_TRIGGER_PREPARE);
	if (ret < 0)
		return ret;

	if (ret == COMP_STATUS_STATE_ALREADY_SET)
		return PPL_STATUS_PATH_STOP;

	return 0;
}

static int peakvol_reset(struct comp_dev *dev)
{
	int ret = 0;

	comp_dbg(dev, "peakvol_reset()");

	if (dev->state == COMP_STATE_ACTIVE) {
		comp_info(dev, "peakvol_config() is in active state. Ignore resetting");
		return 0;
	}

	comp_set_state(dev, COMP_TRIGGER_RESET);

	return ret;
}

static int peakvol_trigger(struct comp_dev *dev, int cmd)
{
	comp_dbg(dev, "peakvol_trigger()");

	return comp_set_state(dev, cmd);
}

static int peakvol_copy(struct comp_dev *dev)
{
	struct peakvol_data *cd = comp_get_drvdata(dev);
	struct comp_copy_limits c;
	struct comp_buffer *source;
	struct comp_buffer *sink;
	uint32_t source_bytes;
	uint32_t sink_bytes;

	comp_dbg(dev, "peakvol_copy()");

	source = list_first_item(&dev->bsource_list, struct comp_buffer,
				 sink_list);
	sink = list_first_item(&dev->bsink_list, struct comp_buffer,
			       source_list);

	comp_get_copy_limits_with_lock(source, sink, &c);
	source_bytes = c.frames * c.source_frame_bytes;
	sink_bytes = c.frames * c.sink_frame_bytes;

	buffer_invalidate(source, source_bytes);
	cd->proc_peakvol(dev, &sink->stream, &source->stream, c.frames);
	buffer_writeback(sink, sink_bytes);

	comp_update_buffer_produce(sink, sink_bytes);
	comp_update_buffer_consume(source, source_bytes);

	return 0;
}

static int peakvol_cmd(struct comp_dev *dev, int cmd, void *data, int max_data_size)
{
	struct ipc4_peak_volume_config *cdata = ASSUME_ALIGNED(data, 8);
	struct peakvol_data *cd = comp_get_drvdata(dev);

	comp_dbg(dev, "peakvol_cmd()");

	dcache_invalidate_region(cdata, sizeof(*cdata));

	switch (cmd) {
	case IPC4_VOLUME:
		if (cdata->channel_id == IPC4_ALL_CHANNELS_MASK) {
			int i;

			for (i = 0; i < cd->active_channels; i++)
				set_volume(cd, i, cdata->target_volume, cdata->curve_type,
					   cdata->curve_duration);
		} else {
			set_volume(cd, cdata->channel_id, cdata->target_volume, cdata->curve_type,
				   cdata->curve_duration);
		}

		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static const struct comp_driver comp_peakvol = {
	.uid	= SOF_RT_UUID(peakvol_comp_uuid),
	.tctx	= &peakvol_comp_tr,
	.ops	= {
		.create			= peakvol_new,
		.free			= peakvol_free,
		.trigger		= peakvol_trigger,
		.cmd			= peakvol_cmd,
		.copy			= peakvol_copy,
		.prepare		= peakvol_prepare,
		.reset			= peakvol_reset,
	},
};

static SHARED_DATA struct comp_driver_info comp_peakvol_info = {
	.drv = &comp_peakvol,
};

UT_STATIC void sys_comp_peakvol_init(void)
{
	comp_register(platform_shared_get(&comp_peakvol_info,
					  sizeof(comp_peakvol_info)));
}

DECLARE_MODULE(sys_comp_peakvol_init);

static const struct comp_driver comp_gain = {
	.uid	= SOF_RT_UUID(gain_comp_uuid),
	.tctx	= &peakvol_comp_tr,
	.ops	= {
		.create			= gain_new,
		.free			= gain_free,
		.trigger		= peakvol_trigger,
		.cmd			= peakvol_cmd,
		.copy			= peakvol_copy,
		.prepare		= peakvol_prepare,
		.reset			= peakvol_reset,
	},
};

static SHARED_DATA struct comp_driver_info comp_gain_info = {
	.drv = &comp_gain,
};

UT_STATIC void sys_comp_gain_init(void)
{
	comp_register(platform_shared_get(&comp_gain_info,
					  sizeof(comp_gain_info)));
}

DECLARE_MODULE(sys_comp_gain_init);
