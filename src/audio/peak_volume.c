// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Intel Corporation. All rights reserved.
//

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
#include <ipc4/peak_vol.h>
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

/* 8a171323-94a3-4e1d-afe9-fe5dbaa4c393 */
DECLARE_SOF_RT_UUID("peak_volume", peakvol_comp_uuid, 0x8a171323, 0x94a3, 0x4e1d,
		    0xaf, 0xe9, 0xfe, 0x5d, 0xba, 0xa4, 0xc3, 0x93);

DECLARE_TR_CTX(peakvol_comp_tr, SOF_UUID(peakvol_comp_uuid), LOG_LEVEL_INFO);

struct peakvol_data {
	struct ipc4_base_module_cfg base;
	struct comp_buffer *buf;
	vol_scale_func scale_vol;

	/* khz based, .e.g 48, 44.1 */
	size_t sampling_frequency;
	/* sample interval in hundreds nanoseconds */
	size_t sample_interval;
	size_t active_channels;
	/* Peak meter values. */
	uint32_t peak_meter[SOF_IPC_MAX_CHANNELS];
	/* Volume values. */
	uint32_t current_volume[SOF_IPC_MAX_CHANNELS];
	uint32_t target_volume[SOF_IPC_MAX_CHANNELS];

	/* Volume curve duration in Sa. */
	uint32_t trans_time[SOF_IPC_MAX_CHANNELS];
};

static void connect_comp_to_buffer(struct comp_dev *dev, struct comp_buffer *buf, int dir)
{
	uint32_t flags;

	irq_local_disable(flags);
	list_item_prepend(buffer_comp_list(buf, dir), comp_buffer_list(dev, dir));
	buffer_set_comp(buf, dev, dir);
	comp_writeback(dev);
	irq_local_enable(flags);

	dcache_writeback_invalidate_region(buf, sizeof(*buf));
}

static struct comp_buffer *create_buffer(struct comp_dev *dev, struct ipc4_audio_format *format,
					 uint32_t size)
{
	struct sof_ipc_buffer ipc_buf;
	struct comp_buffer *buf;
	int i;

	memset(&ipc_buf, 0, sizeof(ipc_buf));
	ipc_buf.size = size;
	ipc_buf.comp.id = dev->ipc_config.id + 1;
	ipc_buf.comp.pipeline_id = dev->ipc_config.pipeline_id;
	ipc_buf.comp.core = dev->ipc_config.core;
	buf = buffer_new(&ipc_buf);
	if (!buf)
		return NULL;

	list_init(&buf->source_list);
	list_init(&buf->sink_list);

	buf->stream.channels = format->channels_count;
	buf->stream.rate = format->sampling_frequency;
	buf->stream.frame_fmt  = (format->valid_bit_depth >> 3) - 2;
	buf->buffer_fmt = format->interleaving_style;

	for (i = 0; i < SOF_IPC_MAX_CHANNELS; i++)
		buf->chmap[i] = (format->ch_map >> i * 4) & 0xf;

	connect_comp_to_buffer(dev, buf, PPL_CONN_DIR_COMP_TO_BUFFER);

	return buf;
}

static int set_volume(struct peakvol_data *cd, uint32_t const channel, uint32_t const target_volume,
		      uint32_t const curve_type, uint64_t const curve_duration)
{
	cd->target_volume[channel] = target_volume;

	/* todo: add memory windows support for peak volume */

	if (curve_type == IPC4_AUDIO_CURVE_TYPE_NONE ||
	    cd->sample_interval >= curve_duration ||
	    cd->current_volume[channel] == target_volume) {
		/* Instant volume change. */
		cd->current_volume[channel] = target_volume;
		cd->trans_time[channel] = 0;
		return 0;
	}

	cd->sampling_frequency = cd->base.audio_fmt.sampling_frequency / 1000;
	cd->trans_time[channel] = ((curve_duration * cd->sampling_frequency) / 10000) - 1;

	if (cd->current_volume[channel] != target_volume && cd->trans_time[channel] == 0)
		cd->trans_time[channel] = 1;

	return 0;
}

static struct comp_dev *peakvol_new(const struct comp_driver *drv,
				    struct comp_ipc_config *config,
				    void *spec)
{
	struct ipc4_peak_volume_module_cfg *peakvol = spec;
	struct comp_dev *dev;
	struct comp_buffer *buf;
	struct peakvol_data *cd;
	uint8_t channel_cfg;
	uint8_t channel;

	comp_cl_dbg(&comp_peakvol, "peakvol_new()");

	dev = comp_alloc(drv, sizeof(*dev));
	if (!dev)
		return NULL;

	list_init(&dev->bsource_list);
	list_init(&dev->bsink_list);

	dcache_invalidate_region(spec, sizeof(*peakvol));
	cd = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(cd->base));
	if (!cd) {
		comp_free(dev);
		return NULL;
	}

	mailbox_hostbox_read(&cd->base, sizeof(cd->base), 0, sizeof(cd->base));
	cd->sampling_frequency = cd->base.audio_fmt.sampling_frequency / 1000;
	cd->sample_interval = 10000 / cd->sampling_frequency;
	cd->active_channels = cd->base.audio_fmt.channels_count;

	for (channel = 0; channel < cd->active_channels; channel++) {
		if (peakvol->config[0].channel_id == IPC4_ALL_CHANNELS_MASK)
			channel_cfg = 0;
		else
			channel_cfg = channel;

		cd->current_volume[channel] = 0;
		set_volume(cd, channel, peakvol->config[channel_cfg].target_volume,
			   peakvol->config[channel_cfg].curve_type,
			   peakvol->config[channel_cfg].curve_duration);
		cd->peak_meter[channel] = 0;
	}

	buf = create_buffer(dev, &peakvol->base_cfg.audio_fmt, peakvol->base_cfg.obs);
	if (!buf) {
		comp_free(dev);
		rfree(cd);
		return NULL;
	}

	cd->buf = buf;
	comp_set_drvdata(dev, cd);

	dev->state = COMP_STATE_READY;
	return dev;
}

static void peakvol_free(struct comp_dev *dev)
{
	struct peakvol_data *cd = comp_get_drvdata(dev);

	rfree(cd);
	rfree(dev);
}

static int peakvol_prepare(struct comp_dev *dev)
{
	struct peakvol_data *cd = comp_get_drvdata(dev);
	int ret;

	comp_info(dev, "peakvol_prepare()");

	if (dev->state == COMP_STATE_ACTIVE) {
		comp_info(dev, "peakvol_config_prepare(): Component is in active state.");
		return 0;
	}

	cd->scale_vol = vol_get_processing_function(dev);
	if (!cd->scale_vol) {
		comp_err(dev, "peakvol_prepare(): invalid cd->scale_vol");

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

	comp_info(dev, "peakvol_reset()");

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
	struct comp_buffer *source;
	uint32_t size;

	comp_dbg(dev, "peakvol_copy()");

	source = list_first_item(&dev->bsource_list, struct comp_buffer, sink_list);
	size = cd->base.obs;

	/* todo: add curve support */

	buffer_invalidate(source, size);
	cd->scale_vol(dev, &cd->buf->stream, &source->stream, cd->sampling_frequency);
	buffer_writeback(cd->buf, size);

	comp_update_buffer_produce(cd->buf, size);
	comp_update_buffer_consume(source, size);

	return 0;
}

static int peakvol_cmd(struct comp_dev *dev, int cmd, void *data, int max_data_size)
{
	struct ipc4_peak_volume_config *cdata = ASSUME_ALIGNED(data, 8);
	struct peakvol_data *cd = comp_get_drvdata(dev);

	comp_dbg(dev, "peakvol_cmd()");

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
	.type	= SOF_COMP_COPIER,
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
