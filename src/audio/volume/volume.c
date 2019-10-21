// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>
//         Tomasz Lauda <tomasz.lauda@linux.intel.com>

/**
 * \file audio/volume.c
 * \brief Volume component implementation
 * \authors Liam Girdwood <liam.r.girdwood@linux.intel.com>\n
 *          Keyon Jie <yang.jie@linux.intel.com>\n
 *          Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

#include <sof/audio/component.h>
#include <sof/audio/format.h>
#include <sof/audio/pipeline.h>
#include <sof/audio/volume.h>
#include <sof/common.h>
#include <sof/debug/panic.h>
#include <sof/drivers/ipc.h>
#include <sof/lib/alloc.h>
#include <sof/lib/cache.h>
#include <sof/list.h>
#include <sof/math/numbers.h>
#include <sof/platform.h>
#include <sof/schedule/schedule.h>
#include <sof/schedule/task.h>
#include <sof/string.h>
#include <sof/trace/trace.h>
#include <ipc/control.h>
#include <ipc/stream.h>
#include <ipc/topology.h>
#include <user/trace.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

/**
 * \brief Synchronize host mmap() volume with real value.
 * \param[in,out] cd Volume component private data.
 * \param[in] chan Channel number.
 */
static void vol_sync_host(struct comp_data *cd, uint32_t chan)
{
	if (!cd->hvol)
		return;

	if (chan < SOF_IPC_MAX_CHANNELS) {
		cd->hvol[chan].value = cd->volume[chan];
	} else {
		trace_volume_error("vol_sync_host() error: "
				   "chan = %u < SOF_IPC_MAX_CHANNELS", chan);
	}
}

/**
 * \brief Update volume with target value.
 * \param[in,out] cd Volume component private data.
 * \param[in] chan Channel number.
 */
static void vol_update(struct comp_data *cd, uint32_t chan)
{
	cd->volume[chan] = cd->tvolume[chan];
	vol_sync_host(cd, chan);
}

/**
 * \brief Ramps volume changes over time.
 * \param[in,out] data Volume base component device.
 * \param[in] delay Update time.
 * \return Time until next work.
 */
static enum task_state vol_work(void *data)
{
	struct comp_dev *dev = (struct comp_dev *)data;
	struct comp_data *cd = comp_get_drvdata(dev);
	int32_t vol;
	int again = 0;
	int i;

	/* inc/dec each volume if it's not at target */
	for (i = 0; i < PLATFORM_MAX_CHANNELS; i++) {
		/* skip if target reached */
		if (cd->volume[i] == cd->tvolume[i])
			continue;

		/* Update volume gain with ramp. Linear ramp increment is
		 * in compatible Q1.16 format with volume.
		 */
		vol = cd->volume[i];
		vol += cd->ramp_increment[i];
		if (cd->volume[i] < cd->tvolume[i]) {
			/* ramp up, check if ramp completed */
			if (vol >= cd->tvolume[i] || vol >= cd->vol_max) {
				vol_update(cd, i);
			} else {
				cd->volume[i] = vol;
				again = 1;
			}
		} else {
			/* ramp down */
			if (vol <= 0) {
				/* cannot ramp down below 0 */
				vol_update(cd, i);
			} else {
				/* ramp completed ? */
				if (vol <= cd->tvolume[i] ||
				    vol <= cd->vol_min) {
					vol_update(cd, i);
				} else {
					cd->volume[i] = vol;
					again = 1;
				}
			}
		}

		/* sync host with new value */
		vol_sync_host(cd, i);
	}

	/* do we need to continue ramping */
	return again ? SOF_TASK_STATE_RESCHEDULE : SOF_TASK_STATE_COMPLETED;
}

/**
 * \brief Creates volume component.
 * \param[in,out] data Volume base component device.
 * \param[in] delay Update time.
 * \return Pointer to volume base component device.
 */
static struct comp_dev *volume_new(struct sof_ipc_comp *comp)
{
	struct comp_dev *dev;
	struct sof_ipc_comp_volume *vol;
	struct sof_ipc_comp_volume *ipc_vol =
		(struct sof_ipc_comp_volume *)comp;
	struct comp_data *cd;
	int i;

	trace_volume("volume_new()");

	if (IPC_IS_SIZE_INVALID(ipc_vol->config)) {
		IPC_SIZE_ERROR_TRACE(TRACE_CLASS_VOLUME, ipc_vol->config);
		return NULL;
	}

	dev = rzalloc(RZONE_RUNTIME, SOF_MEM_CAPS_RAM,
		      COMP_SIZE(struct sof_ipc_comp_volume));
	if (!dev)
		return NULL;

	vol = (struct sof_ipc_comp_volume *)&dev->comp;
	assert(!memcpy_s(vol, sizeof(*vol), ipc_vol,
			 sizeof(struct sof_ipc_comp_volume)));

	cd = rzalloc(RZONE_RUNTIME, SOF_MEM_CAPS_RAM, sizeof(*cd));
	if (!cd) {
		rfree(dev);
		return NULL;
	}

	comp_set_drvdata(dev, cd);
	schedule_task_init(&cd->volwork, SOF_SCHEDULE_LL_TIMER,
			   SOF_TASK_PRI_MED, vol_work, NULL, dev, 0, 0);

	/* Set the default volumes. If IPC sets min_value or max_value to
	 * not-zero, use them. Otherwise set to internal limits and notify
	 * ramp step calculation about assumed range with the range set to
	 * zero.
	 */
	if (vol->min_value || vol->max_value) {
		if (vol->min_value < VOL_MIN) {
			/* Use VOL_MIN instead, no need to stop new(). */
			cd->vol_min = VOL_MIN;
			trace_volume_error("volume_new(): vol->min_value "
					   "was limited to VOL_MIN.");
		} else {
			cd->vol_min = vol->min_value;
		}

		if (vol->max_value > VOL_MAX) {
			/* Use VOL_MAX instead, no need to stop new(). */
			cd->vol_max = VOL_MAX;
			trace_volume_error("volume_new(): vol->max_value "
					   "was limited to VOL_MAX.");
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
		cd->tvolume[i] =  cd->volume[i];
	}

	trace_volume("vol->initial_ramp = %d, vol->ramp = %d, "
		     "vol->min_value = %d, vol->max_value = %d",
		     vol->initial_ramp, vol->ramp,
		     vol->min_value, vol->max_value);

	dev->state = COMP_STATE_READY;
	return dev;
}

/**
 * \brief Frees volume component.
 * \param[in,out] dev Volume base component device.
 */
static void volume_free(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	trace_volume("volume_free()");

	rfree(cd);
	rfree(dev);
}

/**
 * \brief Sets volume component audio stream parameters.
 * \param[in,out] dev Volume base component device.
 * \return Error code.
 *
 * All done in prepare() since we need to know source and sink component params.
 */
static int volume_params(struct comp_dev *dev)
{
	trace_volume("volume_params()");

	return 0;
}

/**
 * \brief Sets channel target volume.
 * \param[in,out] dev Volume base component device.
 * \param[in] chan Channel number.
 * \param[in] vol Target volume.
 */
static inline int volume_set_chan(struct comp_dev *dev, int chan, uint32_t vol)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct sof_ipc_comp_volume *pga =
		COMP_GET_IPC(dev, sof_ipc_comp_volume);
	int32_t v = vol;
	int32_t delta;
	int32_t delta_abs;
	int32_t inc;
	int32_t t_us;

	/* Limit received volume gain to MIN..MAX range before applying it.
	 * MAX is needed for now for the generic C gain arithmetics to prevent
	 * multiplication overflow with the 32 bit value. Non-zero MIN option
	 * can be useful to prevent totally muted small volume gain.
	 */
	if (v <= VOL_MIN) {
		/* No need to fail, just trace the event. */
		trace_volume_error("volume_set_chan: Limited request %d to "
				   "VOL_MIN.", v);
		v = VOL_MIN;
	}

	if (v > VOL_MAX) {
		/* No need to fail, just trace the event. */
		trace_volume_error("volume_set_chan: Limited request %d to "
				   "VOL_MAX.", v);
		v = VOL_MAX;
	}

	cd->tvolume[chan] = v;

	/* Check ramp type */
	switch (pga->ramp) {
	case SOF_VOLUME_LINEAR:

		/* Assume the ramp length (initial_ramp [ms]) describes
		 * time of mute to 0 dB ramp. The actual volume scale min/max
		 * is not known. If initial_ramp is zero (or negative) use
		 * step size that reaches 0 dB gain in one step (VOL_ZERO_DB).
		 */
		if (pga->initial_ramp > 0)
			inc = MAX(VOL_RAMP_STEP_CONST / pga->initial_ramp, 1);
		else
			inc = VOL_ZERO_DB;

		/* Scale the increment with actual volume range if it was
		 * passed via IPC.
		 */
		if (cd->vol_ramp_range)
			inc = q_multsr_32x32(inc, cd->vol_ramp_range,
					     Q_SHIFT_BITS_32(VOL_QXY_Y,
							     VOL_QXY_Y,
							     VOL_QXY_Y));

		delta = cd->tvolume[chan] - cd->volume[chan];
		delta_abs = ABS(delta);

		/* If the ramp completion would take longer than initial_ramp
		 * increase the step size to make this ramp complete at
		 * initial_ramp time. Calculate estimated ramp length [us].
		 */
		t_us = delta_abs * VOL_RAMP_UPDATE_US / inc;
		if (t_us > pga->initial_ramp * 1000) {
			inc = (int32_t)((int64_t)inc * delta_abs / VOL_ZERO_DB);
			inc = MAX(inc, 1);
		}

		/* Invert sign for volume down ramp step */
		if (delta < 0)
			inc = -inc;

		cd->ramp_increment[chan] = inc;
		tracev_volume("cd->ramp_increment[%d] = %d", chan,
			      cd->ramp_increment[chan]);
		break;
	case SOF_VOLUME_LOG:
	case SOF_VOLUME_LINEAR_ZC:
	case SOF_VOLUME_LOG_ZC:
	default:
		trace_volume_error("volume_set_chan() error: "
				   "invalid ramp type %d", pga->ramp);
		return -EINVAL;
	}

	return 0;
}

/**
 * \brief Mutes channel.
 * \param[in,out] dev Volume base component device.
 * \param[in] chan Channel number.
 */
static inline void volume_set_chan_mute(struct comp_dev *dev, int chan)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	/* Check if not muted already */
	if (cd->volume[chan] != 0)
		cd->mvolume[chan] = cd->volume[chan];
	cd->tvolume[chan] = 0;
}

/**
 * \brief Unmutes channel.
 * \param[in,out] dev Volume base component device.
 * \param[in] chan Channel number.
 */
static inline void volume_set_chan_unmute(struct comp_dev *dev, int chan)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	/* Check if muted */
	if (cd->volume[chan] == 0)
		cd->tvolume[chan] = cd->mvolume[chan];
}

/**
 * \brief Sets volume control command.
 * \param[in,out] dev Volume base component device.
 * \param[in,out] cdata Control command data.
 * \return Error code.
 */
static int volume_ctrl_set_cmd(struct comp_dev *dev,
			       struct sof_ipc_ctrl_data *cdata)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int i;
	int j;
	int ret = 0;

	/* validate */
	if (cdata->num_elems == 0 || cdata->num_elems > SOF_IPC_MAX_CHANNELS) {
		trace_volume_error("volume_ctrl_set_cmd() error: "
				   "invalid cdata->num_elems");
		return -EINVAL;
	}

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_VOLUME:
		trace_volume("volume_ctrl_set_cmd(), SOF_CTRL_CMD_VOLUME, "
			     "cdata->comp_id = %u", cdata->comp_id);
		for (j = 0; j < cdata->num_elems; j++) {
			trace_volume("volume_ctrl_set_cmd(), "
				     "SOF_CTRL_CMD_VOLUME, "
				     "channel = %u, value = %u",
				     cdata->chanv[j].channel,
				     cdata->chanv[j].value);
			i = cdata->chanv[j].channel;
			if (i >= 0 && i < SOF_IPC_MAX_CHANNELS) {
				ret = volume_set_chan(dev, i,
						      cdata->chanv[j].value);
			} else {
				trace_volume_error("volume_ctrl_set_cmd() "
						   "error: "
						   "SOF_CTRL_CMD_VOLUME, "
						   "invalid i = %u", i);
			}
			if (ret)
				return ret;
		}

		schedule_task(&cd->volwork, VOL_RAMP_UPDATE_US,
			      VOL_RAMP_UPDATE_US);
		break;

	case SOF_CTRL_CMD_SWITCH:
		trace_volume("volume_ctrl_set_cmd(), SOF_CTRL_CMD_SWITCH, "
			     "cdata->comp_id = %u", cdata->comp_id);
		for (j = 0; j < cdata->num_elems; j++) {
			trace_volume("volume_ctrl_set_cmd(), "
				     "SOF_CTRL_CMD_SWITCH, "
				     "channel = %u, value = %u",
				     cdata->chanv[j].channel,
				     cdata->chanv[j].value);
			i = cdata->chanv[j].channel;
			if (i >= 0 && i < SOF_IPC_MAX_CHANNELS) {
				if (cdata->chanv[j].value)
					volume_set_chan_unmute(dev, i);
				else
					volume_set_chan_mute(dev, i);
			} else {
				trace_volume_error("volume_ctrl_set_cmd() "
						   "error: "
						   "SOF_CTRL_CMD_SWITCH, "
						   "invalid i = %u", i);
			}
		}

		schedule_task(&cd->volwork, VOL_RAMP_UPDATE_US,
			      VOL_RAMP_UPDATE_US);
		break;

	default:
		trace_volume_error("volume_ctrl_set_cmd() error: "
				   "invalid cdata->cmd");
		return -EINVAL;
	}

	return 0;
}

/**
 * \brief Gets volume control command.
 * \param[in,out] dev Volume base component device.
 * \param[in,out] cdata Control command data.
 * \return Error code.
 */
static int volume_ctrl_get_cmd(struct comp_dev *dev,
			       struct sof_ipc_ctrl_data *cdata, int size)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int j;

	/* validate */
	if (cdata->num_elems == 0 || cdata->num_elems > SOF_IPC_MAX_CHANNELS) {
		trace_volume_error("volume_ctrl_get_cmd() error: "
				   "invalid cdata->num_elems %u",
				   cdata->num_elems);
		return -EINVAL;
	}

	if (cdata->cmd == SOF_CTRL_CMD_VOLUME ||
	    cdata->cmd ==  SOF_CTRL_CMD_SWITCH) {
		trace_volume("volume_ctrl_get_cmd(), SOF_CTRL_CMD_VOLUME / "
			    "SOF_CTRL_CMD_SWITCH, cdata->comp_id = %u",
			    cdata->comp_id);
		for (j = 0; j < cdata->num_elems; j++) {
			cdata->chanv[j].channel = j;
			cdata->chanv[j].value = cd->tvolume[j];
			trace_volume("volume_ctrl_get_cmd(), "
				     "channel = %u, value = %u",
				     cdata->chanv[j].channel,
				     cdata->chanv[j].value);
		}
	} else {
		trace_volume_error("volume_ctrl_get_cmd() error: "
				   "invalid cdata->cmd");
		return -EINVAL;
	}

	return 0;
}

/**
 * \brief Used to pass standard and bespoke commands (with data) to component.
 * \param[in,out] dev Volume base component device.
 * \param[in] cmd Command type.
 * \param[in,out] data Control command data.
 * \return Error code.
 */
static int volume_cmd(struct comp_dev *dev, int cmd, void *data,
		      int max_data_size)
{
	struct sof_ipc_ctrl_data *cdata = data;

	trace_volume("volume_cmd()");

	switch (cmd) {
	case COMP_CMD_SET_VALUE:
		return volume_ctrl_set_cmd(dev, cdata);
	case COMP_CMD_GET_VALUE:
		return volume_ctrl_get_cmd(dev, cdata, max_data_size);
	default:
		return -EINVAL;
	}
}

/**
 * \brief Sets volume component state.
 * \param[in,out] dev Volume base component device.
 * \param[in] cmd Command type.
 * \return Error code.
 */
static int volume_trigger(struct comp_dev *dev, int cmd)
{
	trace_volume("volume_trigger()");

	return comp_set_state(dev, cmd);
}

/**
 * \brief Copies and processes stream data.
 * \param[in,out] dev Volume base component device.
 * \return Error code.
 */
static int volume_copy(struct comp_dev *dev)
{
	struct comp_copy_limits c;
	struct comp_data *cd = comp_get_drvdata(dev);
	int ret;

	tracev_volume("volume_copy()");

	/* Get source, sink, number of frames etc. to process. */
	ret = comp_get_copy_limits(dev, &c);
	if (ret < 0) {
		trace_volume_error("volume_copy(): "
				   "Failed comp_get_copy_limits()");
		return ret;
	}

	tracev_volume("volume_copy(), source_bytes = 0x%x, sink_bytes = 0x%x",
		      c.source_bytes, c.sink_bytes);

	/* copy and scale volume */
	cd->scale_vol(dev, c.sink, c.source, c.frames);

	/* calculate new free and available */
	comp_update_buffer_produce(c.sink, c.sink_bytes);
	comp_update_buffer_consume(c.source, c.source_bytes);

	return 0;
}

/**
 * \brief Prepares volume component for processing.
 * \param[in,out] dev Volume base component device.
 * \return Error code.
 *
 * Volume component is usually first and last in pipelines so it makes sense
 * to also do some type of conversion here.
 */
static int volume_prepare(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct comp_buffer *sinkb;
	struct comp_buffer *sourceb;
	struct sof_ipc_comp_config *config = COMP_GET_CONFIG(dev);
	uint32_t source_period_bytes;
	uint32_t sink_period_bytes;
	int i;
	int ret;

	trace_volume("volume_prepare()");

	ret = comp_set_state(dev, COMP_TRIGGER_PREPARE);
	if (ret < 0)
		return ret;

	if (ret == COMP_STATUS_STATE_ALREADY_SET)
		return PPL_STATUS_PATH_STOP;

	/* volume components will only ever have 1 source and 1 sink buffer */
	sourceb = list_first_item(&dev->bsource_list,
				  struct comp_buffer, sink_list);
	sinkb = list_first_item(&dev->bsink_list,
				struct comp_buffer, source_list);

	/* get source data format and period bytes */
	cd->source_format = comp_frame_fmt(sourceb->source);
	source_period_bytes = comp_period_bytes(sourceb->source, dev->frames);

	/* get sink data format and period bytes */
	cd->sink_format = comp_frame_fmt(sinkb->sink);
	sink_period_bytes = comp_period_bytes(sinkb->sink, dev->frames);

	/* Rewrite params format for this component to match the host side. */
	if (dev->params.direction == SOF_IPC_STREAM_PLAYBACK)
		dev->params.frame_fmt = cd->source_format;
	else
		dev->params.frame_fmt = cd->sink_format;

	if (sinkb->size < config->periods_sink * sink_period_bytes) {
		trace_volume_error("volume_prepare() error: "
				  "sink buffer size is insufficient");
		ret = -ENOMEM;
		goto err;
	}

	/* validate */
	if (!sink_period_bytes) {
		trace_volume_error("volume_prepare() error: "
				   "sink_period_bytes = 0, dev->frames ="
				   " %u", dev->frames);
		ret = -EINVAL;
		goto err;
	}
	if (!source_period_bytes) {
		trace_volume_error("volume_prepare() error: "
				   "source_period_bytes = 0, "
				   "dev->frames = %u", dev->frames);
		ret = -EINVAL;
		goto err;
	}

	cd->scale_vol = vol_get_processing_function(dev);
	if (!cd->scale_vol) {
		trace_volume_error("volume_prepare() error: "
				   "invalid cd->scale_vol, "
				   "cd->source_format = %u, "
				   "cd->sink_format = %u, "
				   "dev->params.channels = %u",
				   cd->source_format, cd->sink_format,
				   dev->params.channels);
		ret = -EINVAL;
		goto err;
	}

	for (i = 0; i < PLATFORM_MAX_CHANNELS; i++)
		vol_sync_host(cd, i);

	return 0;

err:
	comp_set_state(dev, COMP_TRIGGER_RESET);
	return ret;
}

/**
 * \brief Resets volume component.
 * \param[in,out] dev Volume base component device.
 * \return Error code.
 */
static int volume_reset(struct comp_dev *dev)
{
	trace_volume("volume_reset()");

	comp_set_state(dev, COMP_TRIGGER_RESET);
	return 0;
}

/**
 * \brief Executes cache operation on volume component.
 * \param[in,out] dev Volume base component device.
 * \param[in] cmd Cache command.
 */
static void volume_cache(struct comp_dev *dev, int cmd)
{
	struct comp_data *cd;

	switch (cmd) {
	case CACHE_WRITEBACK_INV:
		trace_volume("volume_cache(), CACHE_WRITEBACK_INV");

		cd = comp_get_drvdata(dev);

		dcache_writeback_invalidate_region(cd, sizeof(*cd));
		dcache_writeback_invalidate_region(dev, sizeof(*dev));
		break;

	case CACHE_INVALIDATE:
		trace_volume("volume_cache(), CACHE_INVALIDATE");

		dcache_invalidate_region(dev, sizeof(*dev));

		cd = comp_get_drvdata(dev);
		dcache_invalidate_region(cd, sizeof(*cd));
		break;
	}
}

/** \brief Volume component definition. */
struct comp_driver comp_volume = {
	.type	= SOF_COMP_VOLUME,
	.ops	= {
		.new		= volume_new,
		.free		= volume_free,
		.params		= volume_params,
		.cmd		= volume_cmd,
		.trigger	= volume_trigger,
		.copy		= volume_copy,
		.prepare	= volume_prepare,
		.reset		= volume_reset,
		.cache		= volume_cache,
	},
};

/**
 * \brief Initializes volume component.
 */
static void sys_comp_volume_init(void)
{
	comp_register(&comp_volume);
}

DECLARE_MODULE(sys_comp_volume_init);
