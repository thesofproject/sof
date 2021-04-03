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
#include <sof/ipc/msg.h>
#include <sof/lib/alloc.h>
#include <sof/lib/cpu.h>
#include <sof/lib/memory.h>
#include <sof/lib/uuid.h>
#include <sof/list.h>
#include <sof/math/numbers.h>
#include <sof/platform.h>
#include <sof/string.h>
#include <sof/trace/trace.h>
#include <sof/ut.h>
#include <ipc/control.h>
#include <ipc/stream.h>
#include <ipc/topology.h>
#include <user/trace.h>

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static const struct comp_driver comp_volume;

/* b77e677e-5ff4-4188-af14-fba8bdbf8682 */
DECLARE_SOF_RT_UUID("pga", volume_uuid, 0xb77e677e, 0x5ff4, 0x4188,
		    0xaf, 0x14, 0xfb, 0xa8, 0xbd, 0xbf, 0x86, 0x82);

DECLARE_TR_CTX(volume_tr, SOF_UUID(volume_uuid), LOG_LEVEL_INFO);

#if CONFIG_FORMAT_S16LE
/**
 * \brief Used to find nearest zero crossing frame for 16 bit format.
 * \param[in,out] source Source buffer.
 * \param[in] frames Number of frames.
 * \param[in,out] prev_sum Previous sum of channel samples.
 */
static uint32_t vol_zc_get_s16(const struct audio_stream *source,
			       uint32_t frames, int64_t *prev_sum)
{
	uint32_t buff_frag = frames * source->channels - 1;
	uint32_t curr_frames = frames;
	uint32_t channel;
	int16_t *src;
	int32_t sum;
	uint32_t i;

	for (i = 0; i < frames; i++) {
		sum = 0;

		for (channel = 0; channel < source->channels; channel++) {
			src = audio_stream_read_frag_s16(source, buff_frag);
			sum += *src;
			buff_frag--;
		}

		/* first sign change */
		if ((sum ^ *prev_sum) < 0)
			return curr_frames;

		*prev_sum = sum;
		curr_frames--;
	}

	/* sign change not detected, process all samples */
	return frames;
}

#endif /* CONFIG_FORMAT_S16LE */

#if CONFIG_FORMAT_S24LE
/**
 * \brief Used to find nearest zero crossing frame for 24 in 32 bit format.
 * \param[in,out] source Source buffer.
 * \param[in] frames Number of frames.
 * \param[in,out] prev_sum Previous sum of channel samples.
 */
static uint32_t vol_zc_get_s24(const struct audio_stream *source,
			       uint32_t frames, int64_t *prev_sum)
{
	uint32_t buff_frag = frames * source->channels - 1;
	uint32_t curr_frames = frames;
	uint32_t channel;
	int32_t *src;
	int64_t sum;
	uint32_t i;

	for (i = 0; i < frames; i++) {
		sum = 0;

		for (channel = 0; channel < source->channels; channel++) {
			src = audio_stream_read_frag_s32(source, buff_frag);
			sum += sign_extend_s24(*src);
			buff_frag--;
		}

		/* first sign change */
		if ((sum ^ *prev_sum) < 0)
			return curr_frames;

		*prev_sum = sum;
		curr_frames--;
	}

	/* sign change not detected, process all samples */
	return frames;
}

#endif /* CONFIG_FORMAT_S24LE */

#if CONFIG_FORMAT_S32LE
/**
 * \brief Used to find nearest zero crossing frame for 32 bit format.
 * \param[in,out] source Source buffer.
 * \param[in] frames Number of frames.
 * \param[in,out] prev_sum Previous sum of channel samples.
 */
static uint32_t vol_zc_get_s32(const struct audio_stream *source,
			       uint32_t frames, int64_t *prev_sum)
{
	uint32_t buff_frag = frames * source->channels - 1;
	uint32_t curr_frames = frames;
	uint32_t channel;
	int32_t *src;
	int64_t sum;
	uint32_t i;

	for (i = 0; i < frames; i++) {
		sum = 0;

		for (channel = 0; channel < source->channels; channel++) {
			src = audio_stream_read_frag_s32(source, buff_frag);
			sum += *src;
			buff_frag--;
		}

		/* first sign change */
		if ((sum ^ *prev_sum) < 0)
			return curr_frames;

		*prev_sum = sum;
		curr_frames--;
	}

	/* sign change not detected, process all samples */
	return frames;
}

#endif /* CONFIG_FORMAT_S32LE */

/** \brief Map of formats with dedicated zc functions. */
const struct comp_zc_func_map zc_func_map[] = {
#if CONFIG_FORMAT_S16LE
	{ SOF_IPC_FRAME_S16_LE, vol_zc_get_s16 },
#endif /* CONFIG_FORMAT_S16LE */
#if CONFIG_FORMAT_S24LE
	{ SOF_IPC_FRAME_S24_4LE, vol_zc_get_s24 },
#endif /* CONFIG_FORMAT_S24LE */
#if CONFIG_FORMAT_S32LE
	{ SOF_IPC_FRAME_S32_LE, vol_zc_get_s32 },
#endif /* CONFIG_FORMAT_S32LE */
};

/**
 * \brief Synchronize host mmap() volume with real value.
 * \param[in,out] cd Volume component private data.
 * \param[in] num_channels Update channels 0 to num_channels -1.
 */
static void vol_sync_host(struct comp_dev *dev, unsigned int num_channels)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int n;

	if (!cd->hvol) {
		comp_dbg(dev, "vol_sync_host() Warning: null hvol, no update");
		return;
	}

	if (num_channels < SOF_IPC_MAX_CHANNELS) {
		for (n = 0; n < num_channels; n++)
			cd->hvol[n].value = cd->volume[n];
	} else {
		comp_err(dev, "vol_sync_host(): channels count %d exceeds SOF_IPC_MAX_CHANNELS",
			 num_channels);
	}
}

/**
 * \brief Ramps volume changes over time.
 * \param[in,out] dev Volume base component device.
 */
static void volume_ramp(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int32_t vol;
	int32_t ramp_time;
	int i;
	bool ramp_finished = true;

	/* No need to ramp in idle state, jump volume to request. */
	if (dev->state == COMP_STATE_READY) {
		for (i = 0; i < PLATFORM_MAX_CHANNELS; i++)
			cd->volume[i] = cd->tvolume[i];

		vol_sync_host(dev, PLATFORM_MAX_CHANNELS);
		cd->ramp_finished = true;
		return;
	}

	/* The first is set and cleared to indicate ongoing ramp, the
	 * latter is set once to enable self launched ramp only once
	 * in stream start.
	 */
	cd->vol_ramp_active = true;

	/* Current ramp time in Q29.3 milliseconds. Note that max. ramp length
	 * can be 1.3s at 192 kHz rate and 5.5s at 48 kHz rate without
	 * exceeding int32_t range. Value 8000 is 1000 for converting to
	 * milliseconds times 8 (2 ^ 3) for fraction.
	 */
	ramp_time = cd->vol_ramp_elapsed_frames * 8000 / cd->sample_rate;

	/* Update each volume if it's not at target for active channels */
	for (i = 0; i < cd->channels; i++) {
		/* skip if target reached */
		if (cd->volume[i] == cd->tvolume[i])
			continue;

		/* Update volume gain with ramp. The ramp gain value is
		 * calculated from previous gain and ramp time. The slope
		 * coefficient is calculated in volume_set_chan().
		 */
		vol = cd->rvolume[i] + ramp_time * cd->ramp_coef[i];
		if (cd->volume[i] < cd->tvolume[i]) {
			/* ramp up, check if ramp completed */
			if (vol >= cd->tvolume[i] || vol >= cd->vol_max) {
				cd->ramp_coef[i] = 0;
				cd->volume[i] = cd->tvolume[i];
			} else {
				ramp_finished = false;
				cd->volume[i] = vol;
			}
		} else {
			/* ramp down */
			if (vol <= 0) {
				/* cannot ramp down below 0 */
				cd->volume[i] = cd->tvolume[i];
				cd->ramp_coef[i] = 0;
			} else {
				/* ramp completed ? */
				if (vol <= cd->tvolume[i] ||
				    vol <= cd->vol_min) {
					cd->ramp_coef[i] = 0;
					cd->volume[i] = cd->tvolume[i];
				} else {
					ramp_finished = false;
					cd->volume[i] = vol;
				}
			}
		}

	}

	if (ramp_finished) {
		cd->ramp_finished = true;
		cd->vol_ramp_active = false;
	}

	/* sync host with new value */
	vol_sync_host(dev, cd->channels);
}

/**
 * \brief Creates volume component.
 * \param[in,out] data Volume base component device.
 * \param[in] delay Update time.
 * \return Pointer to volume base component device.
 */
static struct comp_dev *volume_new(const struct comp_driver *drv,
				   struct sof_ipc_comp *comp)
{
	struct comp_dev *dev;
	struct sof_ipc_comp_volume *vol;
	struct sof_ipc_comp_volume *ipc_vol =
		(struct sof_ipc_comp_volume *)comp;
	struct comp_data *cd;
	int i;
	int ret;

	comp_cl_dbg(&comp_volume, "volume_new()");

	dev = comp_alloc(drv, COMP_SIZE(struct sof_ipc_comp_volume));
	if (!dev)
		return NULL;

	vol = COMP_GET_IPC(dev, sof_ipc_comp_volume);
	ret = memcpy_s(vol, sizeof(*vol), ipc_vol,
		       sizeof(struct sof_ipc_comp_volume));
	assert(!ret);

	cd = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(*cd));
	if (!cd) {
		rfree(dev);
		return NULL;
	}

	comp_set_drvdata(dev, cd);

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

	cd->vol_ramp_active = false;
	cd->channels = 0; /* To be set in prepare() */

	comp_info(dev, "vol->initial_ramp = %d, vol->ramp = %d, vol->min_value = %d, vol->max_value = %d",
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

	comp_dbg(dev, "volume_free()");

	rfree(cd);
	rfree(dev);
}

/**
 * \brief Sets channel target volume.
 * \param[in,out] dev Volume base component device.
 * \param[in] chan Channel number.
 * \param[in] vol Target volume.
 * \param[in] constant_rate_ramp When true do a constant rate
 *	      and variable time length ramp. When false do
 *	      a fixed length and variable rate ramp.
 */
static inline int volume_set_chan(struct comp_dev *dev, int chan,
				  int32_t vol, bool constant_rate_ramp)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct sof_ipc_comp_volume *pga =
		COMP_GET_IPC(dev, sof_ipc_comp_volume);
	int32_t v = vol;
	int32_t delta;
	int32_t delta_abs;
	int32_t coef;

	/* Limit received volume gain to MIN..MAX range before applying it.
	 * MAX is needed for now for the generic C gain arithmetics to prevent
	 * multiplication overflow with the 32 bit value. Non-zero MIN option
	 * can be useful to prevent totally muted small volume gain.
	 */
	if (v < VOL_MIN) {
		/* No need to fail, just trace the event. */
		comp_warn(dev, "volume_set_chan: Limited request %d to min. %d",
			  v, VOL_MIN);
		v = VOL_MIN;
	}

	if (v > VOL_MAX) {
		/* No need to fail, just trace the event. */
		comp_warn(dev, "volume_set_chan: Limited request %d to max. %d",
			  v, VOL_MAX);
		v = VOL_MAX;
	}

	cd->tvolume[chan] = v;
	cd->rvolume[chan] = cd->volume[chan];
	cd->vol_ramp_elapsed_frames = 0;

	/* Check ramp type */
	switch (pga->ramp) {
	case SOF_VOLUME_LINEAR:
	case SOF_VOLUME_LINEAR_ZC:
		/* Get volume transition delta and absolute value */
		delta = cd->tvolume[chan] - cd->volume[chan];
		delta_abs = ABS(delta);

		/* The ramp length (initial_ramp [ms]) describes time of mute
		 * to vol_max unmuting. Normally the volume ramp has a
		 * constant linear slope defined this way and variable
		 * completion time. However in streaming start it is feasible
		 * to apply the entire topology defined ramp time to unmute to
		 * any used volume. In this case the ramp rate is not constant.
		 * Note also the legacy mode without known vol_ramp_range where
		 * the volume transition always uses the topology defined time.
		 */
		if (pga->initial_ramp > 0) {
			if (constant_rate_ramp && cd->vol_ramp_range > 0)
				coef = cd->vol_ramp_range;
			else
				coef = delta_abs;

			/* Divide and round to nearest. Note that there will
			 * be some accumulated error in ramp time the longer
			 * the ramp and the smaller the transition is.
			 */
			coef = (2 * coef / pga->initial_ramp + 1) >> 1;
		} else {
			coef = delta_abs;
		}

		/* Scale coefficient by 1/8, round */
		coef = ((coef >> 2) + 1) >> 1;

		/* Ensure ramp coefficient is at least min. non-zero
		 * fractional value.
		 */
		coef = MAX(coef, 1);

		/* Invert sign for volume down ramp step */
		if (delta < 0)
			coef = -coef;

		cd->ramp_coef[chan] = coef;
		comp_dbg(dev, "cd->ramp_coef[%d] = %d", chan,
			 cd->ramp_coef[chan]);
		break;
	case SOF_VOLUME_LOG:
	case SOF_VOLUME_LOG_ZC:
	default:
		comp_err(dev, "volume_set_chan(): invalid ramp type %d",
			 pga->ramp);
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

	if (!cd->muted[chan]) {
		cd->mvolume[chan] = cd->tvolume[chan];
		volume_set_chan(dev, chan, 0, true);
		cd->muted[chan] = true;
	}
}

/**
 * \brief Unmutes channel.
 * \param[in,out] dev Volume base component device.
 * \param[in] chan Channel number.
 */
static inline void volume_set_chan_unmute(struct comp_dev *dev, int chan)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	if (cd->muted[chan]) {
		cd->muted[chan] = false;
		volume_set_chan(dev, chan, cd->mvolume[chan], true);
	}
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
	uint32_t val;
	int ch;
	int j;
	int ret = 0;

	/* validate */
	if (cdata->num_elems == 0 || cdata->num_elems > SOF_IPC_MAX_CHANNELS) {
		comp_err(dev, "volume_ctrl_set_cmd(): invalid cdata->num_elems");
		return -EINVAL;
	}

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_VOLUME:
		comp_dbg(dev, "volume_ctrl_set_cmd(), SOF_CTRL_CMD_VOLUME, cdata->comp_id = %u",
			 cdata->comp_id);
		for (j = 0; j < cdata->num_elems; j++) {
			ch = cdata->chanv[j].channel;
			val = cdata->chanv[j].value;
			comp_info(dev, "volume_ctrl_set_cmd(), channel = %d, value = %u",
				  ch, val);
			if (ch < 0 || ch >= SOF_IPC_MAX_CHANNELS) {
				comp_err(dev, "volume_ctrl_set_cmd(), illegal channel = %d",
					 ch);
				return -EINVAL;
			}

			if (cd->muted[ch]) {
				cd->mvolume[ch] = val;
			} else {
				ret = volume_set_chan(dev, ch, val, true);
				if (ret)
					return ret;
			}
		}

		if (!cd->vol_ramp_active) {
			cd->ramp_finished = false;
			volume_ramp(dev);
		}
		break;

	case SOF_CTRL_CMD_SWITCH:
		comp_dbg(dev, "volume_ctrl_set_cmd(), SOF_CTRL_CMD_SWITCH, cdata->comp_id = %u",
			 cdata->comp_id);
		for (j = 0; j < cdata->num_elems; j++) {
			ch = cdata->chanv[j].channel;
			val = cdata->chanv[j].value;
			comp_info(dev, "volume_ctrl_set_cmd(), channel = %d, value = %u",
				  ch, val);
			if (ch < 0 || ch >= SOF_IPC_MAX_CHANNELS) {
				comp_err(dev, "volume_ctrl_set_cmd(), illegal channel = %d",
					 ch);
				return -EINVAL;
			}

			if (val)
				volume_set_chan_unmute(dev, ch);
			else
				volume_set_chan_mute(dev, ch);
		}

		if (!cd->vol_ramp_active) {
			cd->ramp_finished = false;
			volume_ramp(dev);
		}
		break;

	default:
		comp_err(dev, "volume_ctrl_set_cmd(): invalid cdata->cmd");
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
		comp_err(dev, "volume_ctrl_get_cmd(): invalid cdata->num_elems %u",
			 cdata->num_elems);
		return -EINVAL;
	}

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_VOLUME:
		for (j = 0; j < cdata->num_elems; j++) {
			cdata->chanv[j].channel = j;
			cdata->chanv[j].value = cd->tvolume[j];
			comp_info(dev, "volume_ctrl_get_cmd(), channel = %u, value = %u",
				  cdata->chanv[j].channel,
				  cdata->chanv[j].value);
		}
		break;
	case SOF_CTRL_CMD_SWITCH:
		for (j = 0; j < cdata->num_elems; j++) {
			cdata->chanv[j].channel = j;
			cdata->chanv[j].value = !cd->muted[j];
			comp_info(dev, "volume_ctrl_get_cmd(), channel = %u, value = %u",
				  cdata->chanv[j].channel,
				  cdata->chanv[j].value);
		}
		break;
	default:
		comp_err(dev, "volume_ctrl_get_cmd(): invalid cdata->cmd");
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
	struct sof_ipc_ctrl_data *cdata = ASSUME_ALIGNED(data, 4);

	comp_dbg(dev, "volume_cmd()");

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
	comp_dbg(dev, "volume_trigger()");

	return comp_set_state(dev, cmd);
}

/**
 * \brief Copies and processes stream data.
 * \param[in,out] dev Volume base component device.
 * \return Error code.
 */
static int volume_copy(struct comp_dev *dev)
{
	struct sof_ipc_comp_volume *pga =
		COMP_GET_IPC(dev, sof_ipc_comp_volume);
	struct comp_copy_limits c;
	struct comp_data *cd = comp_get_drvdata(dev);
	struct comp_buffer *source;
	struct comp_buffer *sink;
	uint32_t source_bytes;
	uint32_t sink_bytes;
	uint32_t frames;
	int64_t prev_sum = 0;

	comp_dbg(dev, "volume_copy()");

	source = list_first_item(&dev->bsource_list, struct comp_buffer,
				 sink_list);
	sink = list_first_item(&dev->bsink_list, struct comp_buffer,
			       source_list);

	/* Get source, sink, number of frames etc. to process. */
	comp_get_copy_limits_with_lock(source, sink, &c);

	comp_dbg(dev, "volume_copy(), source_bytes = 0x%x, sink_bytes = 0x%x",
		 c.source_bytes, c.sink_bytes);

	while (c.frames) {
		if (cd->ramp_finished || cd->vol_ramp_frames > c.frames) {
			/* without ramping process all at once */
			frames = c.frames;
		} else if (pga->ramp == SOF_VOLUME_LINEAR_ZC) {
			/* with ZC ramping look for next ZC offset */
			frames = cd->zc_get(&source->stream,
					    cd->vol_ramp_frames, &prev_sum);
		} else {
			/* without ZC process max ramp chunk */
			frames = cd->vol_ramp_frames;
		}

		source_bytes = frames * c.source_frame_bytes;
		sink_bytes = frames * c.sink_frame_bytes;

		/* copy and scale volume */
		buffer_invalidate(source, source_bytes);
		cd->scale_vol(dev, &sink->stream, &source->stream, frames);
		buffer_writeback(sink, sink_bytes);

		/* calculate new free and available */
		comp_update_buffer_produce(sink, sink_bytes);
		comp_update_buffer_consume(source, source_bytes);

		if (cd->vol_ramp_active)
			cd->vol_ramp_elapsed_frames += frames;

		if (!cd->ramp_finished)
			volume_ramp(dev);

		c.frames -= frames;
	}

	return 0;
}

/**
 * \brief Retrieves volume zero crossing function.
 * \param[in,out] dev Volume base component device.
 */
static vol_zc_func vol_get_zc_function(struct comp_dev *dev)
{
	struct comp_buffer *sinkb;
	int i;

	sinkb = list_first_item(&dev->bsink_list, struct comp_buffer,
				source_list);

	/* map the zc function to frame format */
	for (i = 0; i < ARRAY_SIZE(zc_func_map); i++) {
		if (sinkb->stream.frame_fmt != zc_func_map[i].frame_fmt)
			continue;

		return zc_func_map[i].func;
	}

	return NULL;
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
	struct sof_ipc_comp_config *config = dev_comp_config(dev);
	struct sof_ipc_comp_volume *pga;
	uint32_t sink_period_bytes;
	int ramp_update_us;
	int ret;
	int i;

	comp_dbg(dev, "volume_prepare()");

	ret = comp_set_state(dev, COMP_TRIGGER_PREPARE);
	if (ret < 0)
		return ret;

	if (ret == COMP_STATUS_STATE_ALREADY_SET)
		return PPL_STATUS_PATH_STOP;

	/* volume component will only ever have 1 sink buffer */
	sinkb = list_first_item(&dev->bsink_list,
				struct comp_buffer, source_list);

	/* get sink period bytes */
	sink_period_bytes = audio_stream_period_bytes(&sinkb->stream,
						      dev->frames);

	if (sinkb->stream.size < config->periods_sink * sink_period_bytes) {
		comp_err(dev, "volume_prepare(): sink buffer size %d is insufficient < %d * %d",
			 sinkb->stream.size, config->periods_sink, sink_period_bytes);
		ret = -ENOMEM;
		goto err;
	}

	cd->scale_vol = vol_get_processing_function(dev);
	if (!cd->scale_vol) {
		comp_err(dev, "volume_prepare(): invalid cd->scale_vol");

		ret = -EINVAL;
		goto err;
	}

	cd->zc_get = vol_get_zc_function(dev);
	if (!cd->zc_get) {
		comp_err(dev, "volume_prepare(): invalid cd->zc_get");
		ret = -EINVAL;
		goto err;
	}

	vol_sync_host(dev, PLATFORM_MAX_CHANNELS);

	/* Set current volume to min to ensure ramp starts from minimum
	 * to previous volume request. Copy() checks for ramp finished
	 * and executes it if it has not yet finished as result of
	 * driver commands. Ramp is not constant rate to ensure it lasts
	 * for entire topology specified time.
	 */
	cd->ramp_finished = true;
	cd->channels = sinkb->stream.channels;
	cd->sample_rate = sinkb->stream.rate;
	for (i = 0; i < cd->channels; i++) {
		cd->volume[i] = cd->vol_min;
		volume_set_chan(dev, i, cd->tvolume[i], false);
		if (cd->volume[i] != cd->tvolume[i])
			cd->ramp_finished = false;
	}

	/* Determine ramp update rate depending on requested ramp length. To
	 * ensure evenly updated gain envelope with limited fraction resolution
	 * four presets are used.
	 */
	pga = COMP_GET_IPC(dev, sof_ipc_comp_volume);
	if (pga->initial_ramp < VOL_RAMP_UPDATE_THRESHOLD_FASTEST_MS)
		ramp_update_us = VOL_RAMP_UPDATE_FASTEST_US;
	else if (pga->initial_ramp < VOL_RAMP_UPDATE_THRESHOLD_FAST_MS)
		ramp_update_us = VOL_RAMP_UPDATE_FAST_US;
	else if (pga->initial_ramp < VOL_RAMP_UPDATE_THRESHOLD_SLOW_MS)
		ramp_update_us = VOL_RAMP_UPDATE_SLOW_US;
	else
		ramp_update_us = VOL_RAMP_UPDATE_SLOWEST_US;

	/* The volume ramp is updated at least once per copy(). If the ramp update
	 * period is larger than schedule period the frames count for update is set
	 * to copy schedule equivalent number of frames. This also prevents a divide
	 * by zero to happen with a combinations of topology parameters for the volume
	 * component and the pipeline.
	 */
	if (ramp_update_us > dev->period)
		cd->vol_ramp_frames = dev->frames;
	else
		cd->vol_ramp_frames = dev->frames / (dev->period / ramp_update_us);

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
	comp_dbg(dev, "volume_reset()");

	comp_set_state(dev, COMP_TRIGGER_RESET);
	return 0;
}

/** \brief Volume component definition. */
static const struct comp_driver comp_volume = {
	.type	= SOF_COMP_VOLUME,
	.uid	= SOF_RT_UUID(volume_uuid),
	.tctx	= &volume_tr,
	.ops	= {
		.create		= volume_new,
		.free		= volume_free,
		.cmd		= volume_cmd,
		.trigger	= volume_trigger,
		.copy		= volume_copy,
		.prepare	= volume_prepare,
		.reset		= volume_reset,
	},
};

static SHARED_DATA struct comp_driver_info comp_volume_info = {
	.drv = &comp_volume,
};

/**
 * \brief Initializes volume component.
 */
UT_STATIC void sys_comp_volume_init(void)
{
	comp_register(platform_shared_get(&comp_volume_info,
					  sizeof(comp_volume_info)));
}

DECLARE_MODULE(sys_comp_volume_init);
