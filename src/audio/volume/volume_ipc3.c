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
	struct comp_buffer *bufferb;

	if (source_or_sink)
		bufferb = list_first_item(&dev->bsource_list,
					  struct comp_buffer, sink_list);
	else
		bufferb = list_first_item(&dev->bsink_list,
					  struct comp_buffer, source_list);

	cd->scale_vol = vol_get_processing_function(dev, bufferb, cd);
}

/**
 * \brief Ramps volume changes over time.
 * \param[in,out] mod Volume processing module handle
 */
static void volume_ramp_check(struct processing_module *mod)
{
	struct vol_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	int i;

	/* No need to ramp in idle state, jump volume to request. */
	cd->ramp_finished = false;
	if (dev->state == COMP_STATE_READY) {
		for (i = 0; i < PLATFORM_MAX_CHANNELS; i++)
			cd->volume[i] = cd->tvolume[i];

		cd->ramp_finished = true;
	}
}

int volume_init(struct processing_module *mod)
{
	struct module_data *md = &mod->priv;
	struct comp_dev *dev = mod->dev;
	struct module_config *cfg = &md->cfg;
	struct ipc_config_volume *vol = cfg->data;
	struct vol_data *cd;
	const size_t vol_size = sizeof(int32_t) * SOF_IPC_MAX_CHANNELS * 4;
	int i;

	if (!vol || cfg->size != sizeof(*vol)) {
		comp_err(dev, "volume_init(): No configuration data or bad data size %u",
			 cfg->size);
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

	md->private = cd;
	cd->is_passthrough = false;

	/* Set the default volumes. If IPC sets min_value or max_value to
	 * not-zero, use them. Otherwise set to internal limits and notify
	 * ramp step calculation about assumed range with the range set to
	 * zero.
	 */
	if (vol->min_value || vol->max_value) {
		if (vol->min_value < VOL_MIN) {
			/* Use VOL_MIN instead, no need to stop new(). */
			cd->vol_min = VOL_MIN;
			comp_err(dev, "volume_new(): vol->min_value was limited to VOL_MIN.");
		} else {
			cd->vol_min = vol->min_value;
		}

		if (vol->max_value > VOL_MAX) {
			/* Use VOL_MAX instead, no need to stop new(). */
			cd->vol_max = VOL_MAX;
			comp_err(dev, "volume_new(): vol->max_value was limited to VOL_MAX.");
		} else {
			cd->vol_max = vol->max_value;
		}

		cd->vol_ramp_range = vol->max_value - vol->min_value;
	} else {
		/* Legacy mode, set the limits to firmware capability where
		 * VOL_MAX is a very large gain to avoid restricting valid
		 * requests. The default ramp rate will be computed based
		 * on 0 - 1.0 gain range assumption when vol_ramp_range
		 * is set to 0.
		 */
		cd->vol_min = VOL_MIN;
		cd->vol_max = VOL_MAX;
		cd->vol_ramp_range = 0;
	}

	for (i = 0; i < PLATFORM_MAX_CHANNELS; i++) {
		cd->volume[i]  =  MAX(MIN(cd->vol_max, VOL_ZERO_DB),
				      cd->vol_min);
		cd->tvolume[i] = cd->volume[i];
		cd->mvolume[i] = cd->volume[i];
		cd->muted[i] = false;
	}

	switch (cd->ramp_type) {
#if CONFIG_COMP_VOLUME_LINEAR_RAMP
	case SOF_VOLUME_LINEAR:
	case SOF_VOLUME_LINEAR_ZC:
#endif
#if CONFIG_COMP_VOLUME_WINDOWS_FADE
	case SOF_VOLUME_WINDOWS_FADE:
#endif
		cd->ramp_type = vol->ramp;
		cd->initial_ramp = vol->initial_ramp;
		break;
	default:
		comp_err(dev, "volume_new(): invalid ramp type %d", vol->ramp);
		rfree(cd);
		rfree(cd->vol);
		return -EINVAL;
	}

	volume_reset_state(cd);

	return 0;
}

void volume_peak_free(struct vol_data *cd)
{
}

int volume_set_config(struct processing_module *mod, uint32_t config_id,
		      enum module_cfg_fragment_position pos, uint32_t data_offset_size,
		      const uint8_t *fragment, size_t fragment_size, uint8_t *response,
		      size_t response_size)
{
	struct sof_ipc_ctrl_data *cdata = (struct sof_ipc_ctrl_data *)fragment;
	struct vol_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	uint32_t val;
	uint32_t ch;
	int j;
	int ret = 0;

	comp_dbg(dev, "volume_set_config()");

	/* validate */
	if (cdata->num_elems == 0 || cdata->num_elems > SOF_IPC_MAX_CHANNELS) {
		comp_err(dev, "volume_set_config(): invalid cdata->num_elems");
		return -EINVAL;
	}

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_VOLUME:
		comp_dbg(dev, "volume_set_config(), SOF_CTRL_CMD_VOLUME, cdata->comp_id = %u",
			 cdata->comp_id);
		for (j = 0; j < cdata->num_elems; j++) {
			ch = cdata->chanv[j].channel;
			val = cdata->chanv[j].value;
			comp_dbg(dev, "volume_set_config(), channel = %d, value = %u",
				 ch, val);

			if (ch >= SOF_IPC_MAX_CHANNELS) {
				comp_err(dev, "volume_set_config(), illegal channel = %d",
					 ch);
				return -EINVAL;
			}

			if (cd->muted[ch]) {
				cd->mvolume[ch] = val;
			} else {
				ret = volume_set_chan(mod, ch, val, true);
				if (ret)
					return ret;
			}
		}

		volume_ramp_check(mod);
		break;

	case SOF_CTRL_CMD_SWITCH:
		comp_dbg(dev, "volume_set_config(), SOF_CTRL_CMD_SWITCH, cdata->comp_id = %u",
			 cdata->comp_id);
		for (j = 0; j < cdata->num_elems; j++) {
			ch = cdata->chanv[j].channel;
			val = cdata->chanv[j].value;
			comp_dbg(dev, "volume_set_config(), channel = %d, value = %u",
				 ch, val);
			if (ch >= SOF_IPC_MAX_CHANNELS) {
				comp_err(dev, "volume_set_config(), illegal channel = %d",
					 ch);
				return -EINVAL;
			}

			if (val)
				volume_set_chan_unmute(mod, ch);
			else
				volume_set_chan_mute(mod, ch);
		}

		volume_ramp_check(mod);
		break;

	default:
		comp_err(dev, "volume_set_config(): invalid cdata->cmd");
		return -EINVAL;
	}

	return 0;
}

int volume_get_config(struct processing_module *mod,
		      uint32_t config_id, uint32_t *data_offset_size,
		      uint8_t *fragment, size_t fragment_size)
{
	struct sof_ipc_ctrl_data *cdata = (struct sof_ipc_ctrl_data *)fragment;
	struct vol_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	int j;

	comp_dbg(dev, "volume_get_config()");

	/* validate */
	if (cdata->num_elems == 0 || cdata->num_elems > SOF_IPC_MAX_CHANNELS) {
		comp_err(dev, "volume_get_config(): invalid cdata->num_elems %u",
			 cdata->num_elems);
		return -EINVAL;
	}

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_VOLUME:
		for (j = 0; j < cdata->num_elems; j++) {
			cdata->chanv[j].channel = j;
			cdata->chanv[j].value = cd->tvolume[j];
			comp_dbg(dev, "volume_get_config(), channel = %u, value = %u",
				 cdata->chanv[j].channel,
				 cdata->chanv[j].value);
		}
		break;
	case SOF_CTRL_CMD_SWITCH:
		for (j = 0; j < cdata->num_elems; j++) {
			cdata->chanv[j].channel = j;
			cdata->chanv[j].value = !cd->muted[j];
			comp_dbg(dev, "volume_get_config(), channel = %u, value = %u",
				 cdata->chanv[j].channel,
				 cdata->chanv[j].value);
		}
		break;
	default:
		comp_err(dev, "volume_get_config(): invalid cdata->cmd");
		return -EINVAL;
	}

	return 0;
}

void volume_update_current_vol_ipc4(struct vol_data *cd)
{
}

int volume_peak_prepare(struct vol_data *cd, struct processing_module *mod)
{
	return 0;
}

