/*
 * Copyright (c) 2016, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the Intel Corporation nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 *         Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

/**
 * \file audio/volume.c
 * \brief Volume component implementation
 * \authors Liam Girdwood <liam.r.girdwood@linux.intel.com>\n
 *          Keyon Jie <yang.jie@linux.intel.com>\n
 *          Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

#include <stddef.h>
#include <errno.h>
#include <sof/sof.h>
#include <sof/lock.h>
#include <sof/list.h>
#include <sof/stream.h>
#include <sof/alloc.h>
#include <sof/schedule.h>
#include <sof/clk.h>
#include <sof/ipc.h>
#include "volume.h"
#include <sof/math/numbers.h>

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
static uint64_t vol_work(void *data)
{
	struct comp_dev *dev = (struct comp_dev *)data;
	struct comp_data *cd = comp_get_drvdata(dev);
	uint32_t vol;
	int again = 0;
	int i, new_vol;

	/* inc/dec each volume if it's not at target */
	for (i = 0; i < PLATFORM_MAX_CHANNELS; i++) {

		/* skip if target reached */
		if (cd->volume[i] == cd->tvolume[i])
			continue;

		vol = cd->volume[i];

		if (cd->volume[i] < cd->tvolume[i]) {
			/* ramp up */
			vol += VOL_RAMP_STEP;

			/* ramp completed ? */
			if (vol >= cd->tvolume[i] || vol >= cd->max_volume) {
				vol_update(cd, i);
			} else {
				cd->volume[i] = vol;
				again = 1;
			}
		} else {
			/* ramp down */
			new_vol = vol - VOL_RAMP_STEP;
			if (new_vol <= 0) {
				/* cannot ramp down below 0 */
				vol_update(cd, i);
			} else {
				/* ramp completed ? */
				if (new_vol <= cd->tvolume[i] ||
				    new_vol <= cd->min_volume) {
					vol_update(cd, i);
				} else {
					cd->volume[i] = new_vol;
					again = 1;
				}
			}
		}

		/* sync host with new value */
		vol_sync_host(cd, i);
	}

	/* do we need to continue ramping */
	if (again)
		return VOL_RAMP_US;
	else
		return 0;
}

/**
 * \brief Validates and sets minimum and maximum volume levels.
 * \details If max_vol < min_vol or it's equals 0 then set max_vol = VOL_MAX
 * \param[in,out] cd Volume component private data.
 * \param[in] min_vol Minimum volume level
 * \param[in] max_vol Maximum volume level
 */
static void vol_set_min_max_levels(struct comp_data *cd,
				   uint32_t min_vol, uint32_t max_vol)
{
	if (max_vol < min_vol || max_vol == 0)
		cd->max_volume = VOL_ZERO_DB;
	else
		cd->max_volume = max_vol;
	cd->min_volume = min_vol;
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
	int i, err;

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
	err = memcpy_s(vol, sizeof(*vol), ipc_vol,
		sizeof(struct sof_ipc_comp_volume));
	if (err) {
		trace_volume_error("volume_new() could not coppy data");
		rfree(dev);
		return NULL;
	}

	cd = rzalloc(RZONE_RUNTIME, SOF_MEM_CAPS_RAM, sizeof(*cd));
	if (!cd) {
		rfree(dev);
		return NULL;
	}

	comp_set_drvdata(dev, cd);
	schedule_task_init(&cd->volwork, SOF_SCHEDULE_LL, SOF_TASK_PRI_MED,
			   vol_work, dev, 0, 0);
	/* set volume min/max levels */
	vol_set_min_max_levels(cd, ipc_vol->min_value, ipc_vol->max_value);

	/* set the default volumes */
	for (i = 0; i < PLATFORM_MAX_CHANNELS; i++) {
		cd->volume[i]  =  MAX(MIN(cd->max_volume,
					  VOL_ZERO_DB), cd->min_volume);
		cd->tvolume[i] =  cd->volume[i];
	}

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
static inline void volume_set_chan(struct comp_dev *dev, int chan, uint32_t vol)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	uint32_t v = vol;

	/* Limit received volume gain to MIN..MAX range before applying it.
	 * MAX is needed for now for the generic C gain arithmetics to prevent
	 * multiplication overflow with the 32 bit value. Non-zero MIN option
	 * can be useful to prevent totally muted small volume gain.
	 */
	if (v <= cd->min_volume)
		v = cd->min_volume;

	if (v > cd->max_volume)
		v = cd->max_volume;

	cd->tvolume[chan] = v;
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
				     cdata->chanv[j].channel, cdata->chanv[j].value);
			i = cdata->chanv[j].channel;
			if ((i >= 0) && (i < SOF_IPC_MAX_CHANNELS)) {
				volume_set_chan(dev, i, cdata->chanv[j].value);
			} else {
				trace_volume_error("volume_ctrl_set_cmd() "
						   "error: "
						   "SOF_CTRL_CMD_VOLUME, "
						   "invalid i = %u", i);
			}
		}
		schedule_task(&cd->volwork, VOL_RAMP_US, 0, 0);
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
			if ((i >= 0) && (i < SOF_IPC_MAX_CHANNELS)) {
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
		schedule_task(&cd->volwork, VOL_RAMP_US, 0, 0);
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
				   "invalid cdata->num_elems",
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
			trace_volume("volume_ctrl_set_cmd(), "
				     "channel = %u, value = %u",
				     cdata->chanv[j].channel,
				     cdata->chanv[j].value);
		}
	} else {
		trace_volume_error("volume_ctrl_set_cmd() error: "
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
	struct comp_data *cd = comp_get_drvdata(dev);
	struct comp_buffer *sink;
	struct comp_buffer *source;
	uint32_t frames;
	uint32_t source_bytes;
	uint32_t sink_bytes;

	tracev_volume("volume_copy()");

	/* volume component will only ever have 1 source and 1 sink buffer */
	source = list_first_item(&dev->bsource_list, struct comp_buffer,
				 sink_list);
	sink = list_first_item(&dev->bsink_list, struct comp_buffer,
			       source_list);

	/* check for underrun */
	if (source->avail == 0) {
		trace_volume_error("volume_copy() error: "
				   "source component buffer"
				   " has not enough data available");
		comp_underrun(dev, source, 0, 0);
		return -EIO;
	}

	/* check for overrun */
	if (sink->free == 0) {
		trace_volume_error("volume_copy() error: "
				   "sink component buffer"
				   " has not enough free bytes for copy");
		comp_overrun(dev, sink, 0, 0);
		return -EIO;
	}

	frames = comp_avail_frames(source, sink);
	source_bytes = frames * comp_frame_bytes(source->source);
	sink_bytes = frames * comp_frame_bytes(sink->sink);

	tracev_volume("volume_copy(), source_bytes = 0x%x, sink_bytes = 0x%x",
		      source_bytes, sink_bytes);

	/* copy and scale volume */
	cd->scale_vol(dev, sink, source, frames);

	/* calculate new free and available */
	comp_update_buffer_produce(sink, sink_bytes);
	comp_update_buffer_consume(source, source_bytes);

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
	if (ret)
		return ret;

	/* volume components will only ever have 1 source and 1 sink buffer */
	sourceb = list_first_item(&dev->bsource_list,
				  struct comp_buffer, sink_list);
	sinkb = list_first_item(&dev->bsink_list,
				struct comp_buffer, source_list);

	/* get source data format */
	comp_set_period_bytes(sourceb->source, dev->frames, &cd->source_format,
			      &source_period_bytes);

	/* get sink data format */
	comp_set_period_bytes(sinkb->sink, dev->frames, &cd->sink_format,
			      &sink_period_bytes);

	/* Rewrite params format for this component to match the host side. */
	if (dev->params.direction == SOF_IPC_STREAM_PLAYBACK)
		dev->params.frame_fmt = cd->source_format;
	else
		dev->params.frame_fmt = cd->sink_format;

	/* set downstream buffer size */
	ret = buffer_set_size(sinkb, sink_period_bytes * config->periods_sink);
	if (ret < 0) {
		trace_volume_error("volume_prepare() error: "
				   "buffer_set_size() failed");
		goto err;
	}

	/* validate */
	if (!sink_period_bytes) {
		trace_volume_error("volume_prepare() error: "
				   "sink_period_bytes = 0, dev->frames ="
				   " %u, sinkb->sink->frame_bytes = %u",
				   dev->frames, sinkb->sink->frame_bytes);
		ret = -EINVAL;
		goto err;
	}
	if (!source_period_bytes) {
		trace_volume_error("volume_prepare() error: "
				   "source_period_bytes = 0, "
				   "dev->frames = %u, "
				   "sourceb->source->frame_bytes = %u",
				   dev->frames, sourceb->source->frame_bytes);
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

DECLARE_COMPONENT(sys_comp_volume_init);
