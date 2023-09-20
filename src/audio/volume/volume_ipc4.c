// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2023 Intel Corporation. All rights reserved.
//
// Author: Baofeng Tian <baofeng.tian@intel.com>

#include <sof/audio/coefficients/volume/windows_fade.h>
#include <sof/audio/component_ext.h>
#include <sof/audio/component.h>
#include <sof/audio/format.h>
#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/pipeline.h>
#include <sof/audio/ipc-config.h>
#include <sof/common.h>
#include <rtos/panic.h>
#include <sof/ipc/msg.h>
#include <rtos/alloc.h>
#include <rtos/init.h>
#include <sof/lib/cpu.h>
#include <sof/lib/memory.h>
#include <sof/lib/uuid.h>
#include <sof/list.h>
#include <sof/math/numbers.h>
#include <sof/platform.h>
#include <rtos/string.h>
#include <sof/trace/trace.h>
#include <sof/ut.h>
#include <ipc/control.h>
#include <ipc/stream.h>
#include <ipc/topology.h>
#include <user/trace.h>

LOG_MODULE_DECLARE(volume, CONFIG_SOF_LOG_LEVEL);

#include "volume.h"

void set_volume_process(struct vol_data *cd, struct comp_dev *dev, bool source_or_sink)
{
	cd->scale_vol = vol_get_processing_function(dev, cd);
}

static int set_volume_ipc4(struct vol_data *cd, uint32_t const channel,
			   uint32_t const target_volume,
			   uint32_t const curve_type,
			   uint64_t const curve_duration)
{
	/* update target volume in peak_regs */
	cd->peak_regs.target_volume[channel] = target_volume;
	/* update peak meter in peak_regs */
	cd->peak_regs.peak_meter[channel] = 0;
	cd->peak_cnt = 0;

	/* init target volume */
	cd->tvolume[channel] = target_volume;
	/* init ramp start volume*/
	cd->rvolume[channel] = 0;
	/* init muted volume */
	cd->mvolume[channel] = 0;
	/* set muted as false*/
	cd->muted[channel] = false;

	/* ATM there is support for the same ramp for all channels */
	cd->ramp_type = ipc4_curve_type_convert((enum ipc4_curve_type)curve_type);

	return 0;
}

/* In IPC4 driver sends volume in Q1.31 format. It is converted
 * into Q1.23 format to be processed by firmware.
 */
static uint32_t convert_volume_ipc4_to_ipc3(struct comp_dev *dev, uint32_t volume)
{
	return sat_int32(Q_SHIFT_RND((int64_t)volume, 31, VOL_QXY_Y));
}

static uint32_t convert_volume_ipc3_to_ipc4(uint32_t volume)
{
	/* In IPC4 volume is converted into Q1.23 format to be processed by firmware.
	 * Now convert it back to Q1.31
	 */
	return sat_int32(Q_SHIFT_LEFT((int64_t)volume, VOL_QXY_Y, 31));
}

static void init_ramp(struct vol_data *cd, uint32_t curve_duration, uint32_t target_volume)
{
	/* In IPC4 driver sends curve_duration in hundred of ns - it should be
	 * converted into ms value required by firmware
	 */
	if (cd->ramp_type == SOF_VOLUME_WINDOWS_NO_FADE) {
		cd->initial_ramp = 0;
		cd->ramp_finished = true;
	} else {
		cd->initial_ramp = Q_MULTSR_32X32((int64_t)curve_duration,
						  Q_CONVERT_FLOAT(1.0 / 10000, 31), 0, 31, 0);
	}

	if (!cd->initial_ramp) {
		/* In case when initial ramp time is equal to zero, vol_min and
		 * vol_max variables should be set to target_volume value
		 */
		cd->vol_min = target_volume;
		cd->vol_max = target_volume;
	} else {
		cd->vol_min = VOL_MIN;
		cd->vol_max = VOL_MAX;
	}
	cd->copy_gain = true;
}

int volume_init(struct processing_module *mod)
{
	struct module_data *md = &mod->priv;
	struct module_config *cfg = &md->cfg;
	struct comp_dev *dev = mod->dev;
	const struct ipc4_peak_volume_module_cfg *vol = cfg->init_data;
	uint32_t target_volume[SOF_IPC_MAX_CHANNELS];
	struct vol_data *cd;
	const size_t vol_size = sizeof(int32_t) * SOF_IPC_MAX_CHANNELS * 4;
	uint32_t channels_count;
	uint8_t channel_cfg;
	uint8_t channel;
	uint32_t instance_id = IPC4_INST_ID(dev_comp_id(dev));

	if (instance_id >= IPC4_MAX_PEAK_VOL_REG_SLOTS) {
		comp_err(dev, "instance_id %u out of array bounds.", instance_id);
		return -EINVAL;
	}
	channels_count = mod->priv.cfg.base_cfg.audio_fmt.channels_count;
	if (channels_count > SOF_IPC_MAX_CHANNELS || !channels_count) {
		comp_err(dev, "volume_init(): Invalid channels count %u", channels_count);
		return -EINVAL;
	}

	cd = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(struct vol_data));
	if (!cd)
		return -ENOMEM;

	/*
	 * malloc memory to store current volume 4 times to ensure the address
	 * is 8-byte aligned for multi-way xtensa intrinsic operations.
	 */
	cd->vol = rmalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, vol_size);
	if (!cd->vol) {
		rfree(cd);
		comp_err(dev, "volume_init(): Failed to allocate %d", vol_size);
		return -ENOMEM;
	}

	/* malloc memory to store temp peak volume 4 times to ensure the address
	 * is 8-byte aligned for multi-way xtensa intrinsic operations.
	 */
	cd->peak_vol = rmalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, vol_size);
	if (!cd->peak_vol) {
		rfree(cd->vol);
		rfree(cd);
		comp_err(dev, "volume_init(): Failed to allocate %d for peak_vol", vol_size);
		return -ENOMEM;
	}

	md->private = cd;

	for (channel = 0; channel < channels_count ; channel++) {
		if (vol->config[0].channel_id == IPC4_ALL_CHANNELS_MASK)
			channel_cfg = 0;
		else
			channel_cfg = channel;

		target_volume[channel] =
			convert_volume_ipc4_to_ipc3(dev, vol->config[channel].target_volume);

		set_volume_ipc4(cd, channel,
				target_volume[channel_cfg],
				vol->config[channel_cfg].curve_type,
				vol->config[channel_cfg].curve_duration);
	}

	init_ramp(cd, vol->config[0].curve_duration, target_volume[0]);

	cd->mailbox_offset = offsetof(struct ipc4_fw_registers, peak_vol_regs);
	cd->mailbox_offset += instance_id * sizeof(struct ipc4_peak_volume_regs);

	cd->attenuation = 0;
	cd->is_passthrough = false;

	volume_reset_state(cd);

	return 0;
}

void volume_peak_free(struct vol_data *cd)
{
	struct ipc4_peak_volume_regs regs;

	/* clear mailbox */
	memset_s(&regs, sizeof(regs), 0, sizeof(regs));
	mailbox_sw_regs_write(cd->mailbox_offset, &regs, sizeof(regs));
	rfree(cd->peak_vol);
}

static int volume_set_volume(struct processing_module *mod, const uint8_t *data, int data_size)
{
	struct vol_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	struct ipc4_peak_volume_config cdata;
	uint32_t i, channels_count;

	if (data_size < sizeof(cdata)) {
		comp_err(dev, "error: data_size %d should be bigger than %d", data_size,
			 sizeof(cdata));
		return -EINVAL;
	}

	cdata = *(const struct ipc4_peak_volume_config *)data;
	cdata.target_volume = convert_volume_ipc4_to_ipc3(dev, cdata.target_volume);

	if (cdata.channel_id != IPC4_ALL_CHANNELS_MASK &&
	    cdata.channel_id >= SOF_IPC_MAX_CHANNELS) {
		comp_err(dev, "Invalid channel_id %u", cdata.channel_id);
		return -EINVAL;
	}

	init_ramp(cd, cdata.curve_duration, cdata.target_volume);
	cd->ramp_finished = true;

	channels_count = mod->priv.cfg.base_cfg.audio_fmt.channels_count;
	if (channels_count > SOF_IPC_MAX_CHANNELS) {
		comp_err(dev, "Invalid channels count %u", channels_count);
		return -EINVAL;
	}

	if (cdata.channel_id == IPC4_ALL_CHANNELS_MASK) {
		for (i = 0; i < channels_count; i++) {
			set_volume_ipc4(cd, i, cdata.target_volume,
					cdata.curve_type,
					cdata.curve_duration);

			volume_set_chan(mod, i, cd->tvolume[i], true);
			if (cd->volume[i] != cd->tvolume[i])
				cd->ramp_finished = false;
		}
	} else {
		set_volume_ipc4(cd, cdata.channel_id, cdata.target_volume,
				cdata.curve_type,
				cdata.curve_duration);

		volume_set_chan(mod, cdata.channel_id, cd->tvolume[cdata.channel_id],
				true);
		if (cd->volume[cdata.channel_id] != cd->tvolume[cdata.channel_id])
			cd->ramp_finished = false;
	}

	cd->is_passthrough = cd->ramp_finished;

	for (i = 0; i < channels_count; i++) {
		if (cd->volume[i] != VOL_ZERO_DB) {
			cd->is_passthrough = false;
			break;
		}
	}

	cd->scale_vol = vol_get_processing_function(dev, cd);

	volume_prepare_ramp(dev, cd);

	return 0;
}

static int volume_set_attenuation(struct processing_module *mod, const uint8_t *data,
				  int data_size)
{
	struct vol_data *cd = module_get_private_data(mod);
	enum sof_ipc_frame frame_fmt, valid_fmt;
	struct comp_dev *dev = mod->dev;
	uint32_t attenuation;

	/* only support attenuation in format of 32bit */
	if (data_size > sizeof(uint32_t)) {
		comp_err(dev, "attenuation data size %d is incorrect", data_size);
		return -EINVAL;
	}

	attenuation = *(const uint32_t *)data;
	if (attenuation > 31) {
		comp_err(dev, "attenuation %d is out of range", attenuation);
		return -EINVAL;
	}

	audio_stream_fmt_conversion(mod->priv.cfg.base_cfg.audio_fmt.depth,
				    mod->priv.cfg.base_cfg.audio_fmt.valid_bit_depth,
				    &frame_fmt, &valid_fmt,
				    mod->priv.cfg.base_cfg.audio_fmt.s_type);

	if (frame_fmt < SOF_IPC_FRAME_S24_4LE) {
		comp_err(dev, "frame_fmt %d isn't supported by attenuation",
			 frame_fmt);
		return -EINVAL;
	}

	cd->attenuation = attenuation;

	return 0;
}

int volume_set_config(struct processing_module *mod, uint32_t config_id,
		      enum module_cfg_fragment_position pos, uint32_t data_offset_size,
		      const uint8_t *fragment, size_t fragment_size, uint8_t *response,
		      size_t response_size)
{
	struct comp_dev *dev = mod->dev;
	int ret;

	comp_dbg(dev, "volume_set_config()");

	dcache_invalidate_region((__sparse_force void __sparse_cache *)fragment, fragment_size);

	ret = module_set_configuration(mod, config_id, pos, data_offset_size, fragment,
				       fragment_size, response, response_size);
	if (ret < 0)
		return ret;

	/* return if more fragments are expected or if the module is not prepared */
	if (pos != MODULE_CFG_FRAGMENT_LAST && pos != MODULE_CFG_FRAGMENT_SINGLE)
		return 0;

	switch (config_id) {
	case IPC4_VOLUME:
		return volume_set_volume(mod, fragment, fragment_size);
	case IPC4_SET_ATTENUATION:
		return volume_set_attenuation(mod, fragment, fragment_size);
	default:
		comp_err(dev, "unsupported param %d", config_id);
		return -EINVAL;
	}

	return 0;
}

int volume_get_config(struct processing_module *mod,
		      uint32_t config_id, uint32_t *data_offset_size,
		      uint8_t *fragment, size_t fragment_size)
{
	struct vol_data *cd = module_get_private_data(mod);
	struct ipc4_peak_volume_config *cdata;
	int i;

	comp_dbg(mod->dev, "volume_get_large_config()");

	cdata = (struct ipc4_peak_volume_config *)ASSUME_ALIGNED(fragment, 8);

	switch (config_id) {
	case IPC4_VOLUME:
		for (i = 0; i < cd->channels; i++) {
			uint32_t volume = cd->peak_regs.target_volume[i];

			cdata[i].channel_id = i;
			cdata[i].target_volume = convert_volume_ipc3_to_ipc4(volume);
		}
		*data_offset_size = sizeof(*cdata) * cd->channels;
		break;
	default:
		comp_err(mod->dev, "unsupported param %d", config_id);
		return -EINVAL;
	}

	return 0;
}

static int volume_params(struct processing_module *mod)
{
	struct sof_ipc_stream_params *params = mod->stream_params;
	struct comp_buffer *sinkb, *sourceb;
	struct comp_dev *dev = mod->dev;

	comp_dbg(dev, "volume_params()");

	ipc4_base_module_cfg_to_stream_params(&mod->priv.cfg.base_cfg, params);

	component_set_nearest_period_frames(dev, params->rate);

	/* volume component will only ever have 1 sink buffer */
	sinkb = list_first_item(&dev->bsink_list, struct comp_buffer, source_list);
	ipc4_update_buffer_format(sinkb, &mod->priv.cfg.base_cfg.audio_fmt);

	sourceb = list_first_item(&dev->bsource_list, struct comp_buffer, sink_list);
	ipc4_update_buffer_format(sourceb, &mod->priv.cfg.base_cfg.audio_fmt);

	return 0;
}

void volume_update_current_vol_ipc4(struct vol_data *cd)
{
	int i;

	assert(cd);

	for (i = 0; i < cd->channels; i++)
		cd->peak_regs.current_volume[i] = cd->volume[i];
}

int volume_peak_prepare(struct vol_data *cd, struct processing_module *mod)
{
	int ret;

	cd->peak_cnt = 0;
#if CONFIG_COMP_PEAK_VOL
	cd->peak_report_cnt = CONFIG_PEAK_METER_UPDATE_PERIOD * 1000 / mod->dev->period;
	if (cd->peak_report_cnt == 0)
		cd->peak_report_cnt = 1;
#endif
	ret = volume_params(mod);
	if (ret < 0)
		return ret;

	return 0;
}

