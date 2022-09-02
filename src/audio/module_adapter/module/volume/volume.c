// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>
//         Tomasz Lauda <tomasz.lauda@linux.intel.com>

/**
 * \file
 * \brief Volume component implementation
 * \authors Liam Girdwood <liam.r.girdwood@linux.intel.com>\n
 *          Keyon Jie <yang.jie@linux.intel.com>\n
 *          Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

#include <sof/audio/coefficients/volume/windows_fade.h>
#include <sof/audio/component_ext.h>
#include <sof/audio/component.h>
#include <sof/audio/format.h>
#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/pipeline.h>
#include <sof/audio/ipc-config.h>
#include <sof/common.h>
#include <sof/debug/panic.h>
#include <sof/ipc/msg.h>
#include <rtos/alloc.h>
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

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

LOG_MODULE_REGISTER(volume, CONFIG_SOF_LOG_LEVEL);

#include <sof/audio/volume.h>

#if CONFIG_IPC_MAJOR_3
/* b77e677e-5ff4-4188-af14-fba8bdbf8682 */
DECLARE_SOF_RT_UUID("pga", volume_uuid, 0xb77e677e, 0x5ff4, 0x4188,
		    0xaf, 0x14, 0xfb, 0xa8, 0xbd, 0xbf, 0x86, 0x82);
#else
/* these ids aligns windows driver requirement to support windows driver */
/* 8a171323-94a3-4e1d-afe9-fe5dbaa4c393 */
DECLARE_SOF_RT_UUID("pga", volume_uuid, 0x8a171323, 0x94a3, 0x4e1d,
		    0xaf, 0xe9, 0xfe, 0x5d, 0xba, 0xa4, 0xc3, 0x93);

/* 61bca9a8-18d0-4a18-8e7b-2639219804b7 */
DECLARE_SOF_RT_UUID("gain", gain_uuid, 0x61bca9a8, 0x18d0, 0x4a18,
		    0x8e, 0x7b, 0x26, 0x39, 0x21, 0x98, 0x04, 0xb7);

DECLARE_TR_CTX(gain_tr, SOF_UUID(gain_uuid), LOG_LEVEL_INFO);
#endif

DECLARE_TR_CTX(volume_tr, SOF_UUID(volume_uuid), LOG_LEVEL_INFO);

#if CONFIG_FORMAT_S16LE
/**
 * \brief Used to find nearest zero crossing frame for 16 bit format.
 * \param[in,out] source Source buffer.
 * \param[in] frames Number of frames.
 * \param[in,out] prev_sum Previous sum of channel samples.
 */
static uint32_t vol_zc_get_s16(const struct audio_stream __sparse_cache *source,
			       uint32_t frames, int64_t *prev_sum)
{
	uint32_t curr_frames = frames;
	int32_t sum;
	int16_t *x = source->r_ptr;
	int bytes;
	int nmax;
	int i, j, n;
	const int nch = source->channels;
	int remaining_samples = frames * nch;

	x = audio_stream_wrap(source, x + remaining_samples - 1); /* Go to last channel */
	while (remaining_samples) {
		bytes = audio_stream_rewind_bytes_without_wrap(source, x);
		nmax = VOL_BYTES_TO_S16_SAMPLES(bytes) + 1;
		n = MIN(nmax, remaining_samples);
		for (i = 0; i < n; i += nch) {
			sum = 0;
			for (j = 0; j < nch; j++) {
				sum += *x;
				x--;
			}

			/* first sign change */
			if ((sum ^ *prev_sum) < 0)
				return curr_frames;

			*prev_sum = sum;
			curr_frames--;
		}
		remaining_samples -= n;
		x = audio_stream_rewind_wrap(source, x);
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
static uint32_t vol_zc_get_s24(const struct audio_stream __sparse_cache *source,
			       uint32_t frames, int64_t *prev_sum)
{
	int64_t sum;
	uint32_t curr_frames = frames;
	int32_t *x = source->r_ptr;
	int bytes;
	int nmax;
	int i, j, n;
	const int nch = source->channels;
	int remaining_samples = frames * nch;

	x = audio_stream_wrap(source, x + remaining_samples - 1); /* Go to last channel */
	while (remaining_samples) {
		bytes = audio_stream_rewind_bytes_without_wrap(source, x);
		nmax = VOL_BYTES_TO_S32_SAMPLES(bytes) + 1;
		n = MIN(nmax, remaining_samples);
		for (i = 0; i < n; i += nch) {
			sum = 0;
			for (j = 0; j < nch; j++) {
				sum += sign_extend_s24(*x);
				x--;
			}

			/* first sign change */
			if ((sum ^ *prev_sum) < 0)
				return curr_frames;

			*prev_sum = sum;
			curr_frames--;
		}
		remaining_samples -= n;
		x = audio_stream_rewind_wrap(source, x);
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
static uint32_t vol_zc_get_s32(const struct audio_stream __sparse_cache *source,
			       uint32_t frames, int64_t *prev_sum)
{
	int64_t sum;
	uint32_t curr_frames = frames;
	int32_t *x = source->r_ptr;
	int bytes;
	int nmax;
	int i, j, n;
	const int nch = source->channels;
	int remaining_samples = frames * nch;

	x = audio_stream_wrap(source, x + remaining_samples - 1); /* Go to last channel */
	while (remaining_samples) {
		bytes = audio_stream_rewind_bytes_without_wrap(source, x);
		nmax = VOL_BYTES_TO_S32_SAMPLES(bytes) + 1;
		n = MIN(nmax, remaining_samples);
		for (i = 0; i < n; i += nch) {
			sum = 0;
			for (j = 0; j < nch; j++) {
				sum += *x;
				x--;
			}

			/* first sign change */
			if ((sum ^ *prev_sum) < 0)
				return curr_frames;

			*prev_sum = sum;
			curr_frames--;
		}
		remaining_samples -= n;
		x = audio_stream_rewind_wrap(source, x);
	}

	/* sign change not detected, process all samples */
	return frames;
}

#endif /* CONFIG_FORMAT_S32LE */

/** \brief Map of formats with dedicated zc functions. */
static const struct comp_zc_func_map zc_func_map[] = {
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

#if CONFIG_COMP_VOLUME_LINEAR_RAMP
/**
 * \brief Calculate linear ramp function
 * \param[in,out] dev Component data: ramp start gain, actual gain
 * \param[in] ramp_time Time spent since ramp start as milliseconds Q29.3
 * \param[in] channel Current channel to update
 */
static int32_t volume_linear_ramp(struct processing_module *mod, int32_t ramp_time, int channel)
{
	struct vol_data *cd = module_get_private_data(mod);

	return cd->rvolume[channel] + ramp_time * cd->ramp_coef[channel];
}
#endif

#if CONFIG_COMP_VOLUME_WINDOWS_FADE
/**
 * \brief Calculate windows fade ramp function
 * \param[in,out] dev Component data: target gain, ramp start gain, ramp duration
 * \param[in] ramp_time Time spent since ramp start as milliseconds Q29.3
 * \param[in] channel Current channel to update
 */

static int32_t volume_windows_fade_ramp(struct processing_module *mod, int32_t ramp_time,
					int channel)
{
	struct vol_data *cd = module_get_private_data(mod);
	int32_t time_ratio; /* Q2.30 */
	int32_t pow_value; /* Q2.30 */
	int32_t volume_delta = cd->tvolume[channel] - cd->rvolume[channel]; /* Q16.16 */

	time_ratio = (((int64_t)ramp_time) << 30) / (cd->initial_ramp << 3);
	pow_value = volume_pow_175(time_ratio);
	return cd->rvolume[channel] + Q_MULTSR_32X32((int64_t)volume_delta, pow_value, 16, 30, 16);
}
#endif

/**
 * \brief Ramps volume changes over time.
 * \param[in,out] mod Volume processing module handle

 */
static void volume_ramp(struct processing_module *mod)
{
	struct vol_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	int32_t vol;
	int32_t ramp_time;
	int i;
	bool ramp_finished = true;

	/* No need to ramp in idle state, jump volume to request. */
	if (dev->state == COMP_STATE_READY) {
		for (i = 0; i < PLATFORM_MAX_CHANNELS; i++)
			cd->volume[i] = cd->tvolume[i];

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
		vol = cd->ramp_func(mod, ramp_time, i);
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
}

/**
 * \brief Reset state except controls.
 */
static void reset_state(struct vol_data *cd)
{
	int i;

	for (i = 0; i < PLATFORM_MAX_CHANNELS; i++) {
		cd->rvolume[i] = 0;
		cd->ramp_coef[i] = 0;
	}

	cd->channels = 0;
	cd->ramp_finished = true;
	cd->vol_ramp_active = false;
	cd->vol_ramp_frames = 0;
	cd->vol_ramp_elapsed_frames = 0;
	cd->sample_rate = 0;
}

#if CONFIG_IPC_MAJOR_3
static int volume_init(struct processing_module *mod)
{
	struct module_data *md = &mod->priv;
	struct comp_dev *dev = mod->dev;
	struct module_config *cfg = &md->cfg;
	struct ipc_config_volume *vol = cfg->data;
	struct vol_data *cd;
	const size_t vol_size = sizeof(int32_t) * SOF_IPC_MAX_CHANNELS * 4;
	int i;

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

	cd->ramp_type = vol->ramp;
	cd->initial_ramp = vol->initial_ramp;

	switch (cd->ramp_type) {
#if CONFIG_COMP_VOLUME_LINEAR_RAMP
	case SOF_VOLUME_LINEAR:
	case SOF_VOLUME_LINEAR_ZC:
		cd->ramp_func = &volume_linear_ramp;
		break;
#endif
#if CONFIG_COMP_VOLUME_WINDOWS_FADE
	case SOF_VOLUME_WINDOWS_FADE:
		cd->ramp_func = &volume_windows_fade_ramp;
		break;
#endif
	default:
		comp_err(dev, "volume_new(): invalid ramp type %d", vol->ramp);
		rfree(cd);
		rfree(cd->vol);
		return -EINVAL;
	}

	reset_state(cd);

	return 0;
}

/* CONFIG_IPC_MAJOR_3 */
#elif CONFIG_IPC_MAJOR_4
static int set_volume_ipc4(struct vol_data *cd, uint32_t const channel,
			   uint32_t const target_volume,
			   uint32_t const curve_type,
			   uint64_t const curve_duration)
{
	/* update target volume in peak_regs */
	cd->peak_regs.target_volume[channel] = target_volume;
	/* update peak meter in peak_regs */
	cd->peak_regs.peak_meter[channel] = 0;

	/* init target volume */
	cd->tvolume[channel] = target_volume;
	/* init ramp start volume*/
	cd->rvolume[channel] = 0;
	/* init muted volume */
	cd->mvolume[channel] = 0;
	/* set muted as false*/
	cd->muted[channel] = false;
#if CONFIG_COMP_VOLUME_WINDOWS_FADE
	/* ATM there is support for the same ramp for all channels */
	if (curve_type == IPC4_AUDIO_CURVE_TYPE_WINDOWS_FADE) {
		cd->ramp_type = SOF_VOLUME_WINDOWS_FADE;
		cd->ramp_func =  &volume_windows_fade_ramp;
	} else {
		cd->ramp_type = SOF_VOLUME_WINDOWS_NO_FADE;
	}
#endif
	return 0;
}

/* In IPC4 driver sends volume in Q1.31 format. It is converted
 * into Q1.23 format to be processed by firmware.
 */
static inline uint32_t convert_volume_ipc4_to_ipc3(struct comp_dev *dev, uint32_t volume)
{
	/* Limit received volume gain to MIN..MAX range before applying it.
	 * MAX is needed for now for the generic C gain arithmetics to prevent
	 * multiplication overflow with the 32 bit value. Non-zero MIN option
	 * can be useful to prevent totally muted small volume gain.
	 */

	return sat_int24(Q_SHIFT_RND(volume, 31, 23));
}

static inline uint32_t convert_volume_ipc3_to_ipc4(uint32_t volume)
{
	/* In IPC4 volume is converted into Q1.23 format to be processed by firmware.
	 * Now convert it back to Q1.31
	 */
	return sat_int32(Q_SHIFT_LEFT((int64_t)volume, 23, 31));
}

static inline void init_ramp(struct vol_data *cd, uint32_t curve_duration, uint32_t target_volume)
{
	/* In IPC4 driver sends curve_duration in hundred of ns - it should be
	 * converted into ms value required by firmware
	 */
	cd->initial_ramp = Q_MULTSR_32X32((int64_t)curve_duration,
					  Q_CONVERT_FLOAT(1.0 / 10000, 31), 0, 31, 0);

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
}

static int volume_init(struct processing_module *mod)
{
	struct module_data *md = &mod->priv;
	struct module_config *cfg = &md->cfg;
	struct comp_dev *dev = mod->dev;
	struct ipc4_peak_volume_module_cfg *vol = cfg->data;
	struct vol_data *cd;
	const size_t vol_size = sizeof(int32_t) * SOF_IPC_MAX_CHANNELS * 4;
	uint32_t channels_count;
	uint8_t channel_cfg;
	uint8_t channel;
	uint32_t instance_id;

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

	mailbox_hostbox_read(&cd->base, sizeof(cd->base), 0, sizeof(cd->base));

	channels_count = cd->base.audio_fmt.channels_count;

	for (channel = 0; channel < channels_count ; channel++) {
		if (vol->config[0].channel_id == IPC4_ALL_CHANNELS_MASK)
			channel_cfg = 0;
		else
			channel_cfg = channel;

		vol->config[channel].target_volume =
			convert_volume_ipc4_to_ipc3(dev, vol->config[channel].target_volume);

		set_volume_ipc4(cd, channel,
				vol->config[channel_cfg].target_volume,
				vol->config[channel_cfg].curve_type,
				vol->config[channel_cfg].curve_duration);
	}

	init_ramp(cd, vol->config[0].curve_duration, vol->config[0].target_volume);

	instance_id = IPC4_INST_ID(dev_comp_id(dev));
	if (instance_id >= IPC4_MAX_PEAK_VOL_REG_SLOTS) {
		comp_err(dev, "instance_id %u out of array bounds.", instance_id);
		return -EINVAL;
	}

	cd->mailbox_offset = offsetof(struct ipc4_fw_registers, peak_vol_regs);
	cd->mailbox_offset += instance_id * sizeof(struct ipc4_peak_volume_regs);

	reset_state(cd);

	return 0;
}
#endif /* CONFIG_IPC_MAJOR_4 */

static inline void prepare_ramp(struct comp_dev *dev, struct vol_data *cd)
{
	int ramp_update_us;

	/* Determine ramp update rate depending on requested ramp length. To
	 * ensure evenly updated gain envelope with limited fraction resolution
	 * four presets are used.
	 */
	if (cd->initial_ramp < VOL_RAMP_UPDATE_THRESHOLD_FASTEST_MS)
		ramp_update_us = VOL_RAMP_UPDATE_FASTEST_US;
	else if (cd->initial_ramp < VOL_RAMP_UPDATE_THRESHOLD_FAST_MS)
		ramp_update_us = VOL_RAMP_UPDATE_FAST_US;
	else if (cd->initial_ramp < VOL_RAMP_UPDATE_THRESHOLD_SLOW_MS)
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
}

/**
 * \brief Frees volume component.
 * \param[in,out] mod Volume processing module handle

 */
static int volume_free(struct processing_module *mod)
{
	struct vol_data *cd = module_get_private_data(mod);
#if CONFIG_IPC_MAJOR_4
	struct ipc4_peak_volume_regs regs;

	/* clear mailbox */
	memset_s(&regs, sizeof(regs), 0, sizeof(regs));
	mailbox_sw_regs_write(cd->mailbox_offset, &regs, sizeof(regs));
#endif

	comp_dbg(mod->dev, "volume_free()");

	rfree(cd->vol);
	rfree(cd);

	return 0;
}

/**
 * \brief Sets channel target volume.
 * \param[in,out] mod Volume processing module handle
 * \param[in] chan Channel number.
 * \param[in] vol Target volume.
 * \param[in] constant_rate_ramp When true do a constant rate
 *	      and variable time length ramp. When false do
 *	      a fixed length and variable rate ramp.
 */
static inline int volume_set_chan(struct processing_module *mod, int chan,
				  int32_t vol, bool constant_rate_ramp)
{
	struct vol_data *cd = module_get_private_data(mod);
	int32_t v = vol;
	int32_t delta;
	int32_t delta_abs;
	int32_t coef;

	/* Limit received volume gain to MIN..MAX range before applying it.
	 * MAX is needed for now for the generic C gain arithmetic to prevent
	 * multiplication overflow with the 32 bit value. Non-zero MIN option
	 * can be useful to prevent totally muted small volume gain.
	 */
	if (v < VOL_MIN) {
		/* No need to fail, just trace the event. */
		comp_warn(mod->dev, "volume_set_chan: Limited request %d to min. %d",
			  v, VOL_MIN);
		v = VOL_MIN;
	}

	if (v > VOL_MAX) {
		/* No need to fail, just trace the event. */
		comp_warn(mod->dev, "volume_set_chan: Limited request %d to max. %d",
			  v, VOL_MAX);
		v = VOL_MAX;
	}

	cd->tvolume[chan] = v;
	cd->rvolume[chan] = cd->volume[chan];
	cd->vol_ramp_elapsed_frames = 0;

	/* Check ramp type */
	if (cd->ramp_type == SOF_VOLUME_LINEAR ||
	    cd->ramp_type == SOF_VOLUME_LINEAR_ZC) {
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
		if (cd->initial_ramp > 0) {
			if (constant_rate_ramp && cd->vol_ramp_range > 0)
				coef = cd->vol_ramp_range;
			else
				coef = delta_abs;

			/* Divide and round to nearest. Note that there will
			 * be some accumulated error in ramp time the longer
			 * the ramp and the smaller the transition is.
			 */
			coef = (2 * coef / cd->initial_ramp + 1) >> 1;
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
		comp_dbg(mod->dev, "cd->ramp_coef[%d] = %d", chan, cd->ramp_coef[chan]);
	}

	return 0;
}

/**
 * \brief Mutes channel.
 * \param[in,out] mod Volume processing module handle

 * \param[in] chan Channel number.
 */
static inline void volume_set_chan_mute(struct processing_module *mod, int chan)
{
	struct vol_data *cd = module_get_private_data(mod);

	if (!cd->muted[chan]) {
		cd->mvolume[chan] = cd->tvolume[chan];
		volume_set_chan(mod, chan, 0, true);
		cd->muted[chan] = true;
	}
}

/**
 * \brief Unmutes channel.
 * \param[in,out] mod Volume processing module handle

 * \param[in] chan Channel number.
 */
static inline void volume_set_chan_unmute(struct processing_module *mod, int chan)
{
	struct vol_data *cd = module_get_private_data(mod);

	if (cd->muted[chan]) {
		cd->muted[chan] = false;
		volume_set_chan(mod, chan, cd->mvolume[chan], true);
	}
}

#if CONFIG_IPC_MAJOR_3
static int volume_set_config(struct processing_module *mod, uint32_t config_id,
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
			comp_info(dev, "volume_set_config(), channel = %d, value = %u",
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

		if (!cd->vol_ramp_active) {
			cd->ramp_finished = false;
			volume_ramp(mod);
		}
		break;

	case SOF_CTRL_CMD_SWITCH:
		comp_dbg(dev, "volume_set_config(), SOF_CTRL_CMD_SWITCH, cdata->comp_id = %u",
			 cdata->comp_id);
		for (j = 0; j < cdata->num_elems; j++) {
			ch = cdata->chanv[j].channel;
			val = cdata->chanv[j].value;
			comp_info(dev, "volume_set_config(), channel = %d, value = %u",
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

		if (!cd->vol_ramp_active) {
			cd->ramp_finished = false;
			volume_ramp(mod);
		}
		break;

	default:
		comp_err(dev, "volume_set_config(): invalid cdata->cmd");
		return -EINVAL;
	}

	return 0;
}

static int volume_get_config(struct processing_module *mod,
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
			comp_info(dev, "volume_get_config(), channel = %u, value = %u",
				  cdata->chanv[j].channel,
				  cdata->chanv[j].value);
		}
		break;
	case SOF_CTRL_CMD_SWITCH:
		for (j = 0; j < cdata->num_elems; j++) {
			cdata->chanv[j].channel = j;
			cdata->chanv[j].value = !cd->muted[j];
			comp_info(dev, "volume_get_config(), channel = %u, value = %u",
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

/* CONFIG_IPC_MAJOR_3 */
#elif CONFIG_IPC_MAJOR_4
static int volume_set_config(struct processing_module *mod, uint32_t config_id,
			     enum module_cfg_fragment_position pos, uint32_t data_offset_size,
			     const uint8_t *fragment, size_t fragment_size, uint8_t *response,
			     size_t response_size)
{
	struct vol_data *cd = module_get_private_data(mod);
	struct module_data *md = &mod->priv;
	struct comp_dev *dev = mod->dev;
	struct ipc4_peak_volume_config *cdata;
	int i, ret;

	comp_dbg(dev, "volume_set_config()");

	dcache_invalidate_region((__sparse_force void __sparse_cache *)fragment, fragment_size);

	ret = module_set_configuration(mod, config_id, pos, data_offset_size, fragment,
				       fragment_size, response, response_size);
	if (ret < 0)
		return ret;

	/* return if more fragments are expected or if the module is not prepared */
	if ((pos != MODULE_CFG_FRAGMENT_LAST && pos != MODULE_CFG_FRAGMENT_SINGLE) ||
	    md->state < MODULE_INITIALIZED)
		return 0;

	cdata = (struct ipc4_peak_volume_config *)ASSUME_ALIGNED(fragment, 8);
	cdata->target_volume = convert_volume_ipc4_to_ipc3(dev, cdata->target_volume);

	init_ramp(cd, cdata->curve_duration, cdata->target_volume);
	cd->ramp_finished = true;

	switch (config_id) {
	case IPC4_VOLUME:
		if (cdata->channel_id == IPC4_ALL_CHANNELS_MASK) {
			for (i = 0; i < cd->base.audio_fmt.channels_count; i++) {
				set_volume_ipc4(cd, i, cdata->target_volume,
						cdata->curve_type,
						cdata->curve_duration);

				cd->volume[i] = cd->vol_min;
				volume_set_chan(mod, i, cd->tvolume[i], true);
				if (cd->volume[i] != cd->tvolume[i])
					cd->ramp_finished = false;
			}
		} else {
			set_volume_ipc4(cd, cdata->channel_id, cdata->target_volume,
					cdata->curve_type,
					cdata->curve_duration);

			cd->volume[cdata->channel_id] = cd->vol_min;
			volume_set_chan(mod, cdata->channel_id, cd->tvolume[cdata->channel_id],
					true);
			if (cd->volume[cdata->channel_id] != cd->tvolume[cdata->channel_id])
				cd->ramp_finished = false;
		}

		prepare_ramp(dev, cd);
		break;

	default:
		comp_err(dev, "unsupported param %d", config_id);
		return -EINVAL;
	}

	return 0;
}

static int volume_get_config(struct processing_module *mod,
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
	struct vol_data *cd = module_get_private_data(mod);
	struct sof_ipc_stream_params *params = mod->stream_params;
	struct sof_ipc_stream_params vol_params;
	struct comp_dev *dev = mod->dev;
	struct comp_buffer *sinkb;
	struct comp_buffer __sparse_cache *sink_c;
	uint32_t __sparse_cache valid_fmt, frame_fmt;
	int i, ret;

	comp_dbg(dev, "volume_params()");

	vol_params = *params;
	vol_params.channels = cd->base.audio_fmt.channels_count;
	vol_params.rate = cd->base.audio_fmt.sampling_frequency;
	vol_params.buffer_fmt = cd->base.audio_fmt.interleaving_style;

	audio_stream_fmt_conversion(cd->base.audio_fmt.depth,
				    cd->base.audio_fmt.valid_bit_depth,
				    &frame_fmt, &valid_fmt,
				    cd->base.audio_fmt.s_type);

	vol_params.frame_fmt = frame_fmt;

	for (i = 0; i < SOF_IPC_MAX_CHANNELS; i++)
		vol_params.chmap[i] = (cd->base.audio_fmt.ch_map >> i * 4) & 0xf;

	component_set_nearest_period_frames(dev, vol_params.rate);

	/* volume component will only ever have 1 sink buffer */
	sinkb = list_first_item(&dev->bsink_list, struct comp_buffer, source_list);
	sink_c = buffer_acquire(sinkb);
	ret = buffer_set_params(sink_c, &vol_params, true);
	buffer_release(sink_c);

	return ret;
}
#endif /* CONFIG_IPC_MAJOR_4 */

static void volume_update_current_vol_ipc4(struct vol_data *cd)
{
#if CONFIG_IPC_MAJOR_4
	int i;

	assert(cd);

	for (i = 0; i < cd->channels; i++)
		cd->peak_regs.current_volume[i] = cd->volume[i];
#endif
}

/*
 * \brief Copies and processes stream data.
 * \param[in,out] mod Volume processing module handle

 * \return Error code.
 */
static int volume_process(struct processing_module *mod,
			  struct input_stream_buffer *input_buffers, int num_input_buffers,
			  struct output_stream_buffer *output_buffers, int num_output_buffers)
{
	struct vol_data *cd = module_get_private_data(mod);
	uint32_t avail_frames = input_buffers[0].size;
	uint32_t frames;
	int64_t prev_sum = 0;

	comp_dbg(mod->dev, "volume_process()");

	while (avail_frames) {
		volume_update_current_vol_ipc4(cd);

		if (cd->ramp_finished || cd->vol_ramp_frames > avail_frames) {
			/* without ramping process all at once */
			frames = avail_frames;
		} else if (cd->ramp_type == SOF_VOLUME_LINEAR_ZC) {
			/* with ZC ramping look for next ZC offset */
			frames = cd->zc_get(input_buffers[0].data, cd->vol_ramp_frames, &prev_sum);
		} else {
			/* without ZC process max ramp chunk */
			frames = cd->vol_ramp_frames;
		}

		/* copy and scale volume */
		cd->scale_vol(mod, &input_buffers[0], &output_buffers[0], frames);

		if (cd->vol_ramp_active)
			cd->vol_ramp_elapsed_frames += frames;

		if (!cd->ramp_finished)
			volume_ramp(mod);

		avail_frames -= frames;
	}

	return 0;
}

/*
 * \brief Retrieves volume zero crossing function.
 * \param[in,out] dev Volume base component device.
 */
static vol_zc_func vol_get_zc_function(struct comp_dev *dev,
				       struct comp_buffer __sparse_cache *sinkb)
{
	int i;

	/* map the zc function to frame format */
	for (i = 0; i < ARRAY_SIZE(zc_func_map); i++) {
		if (sinkb->stream.frame_fmt == zc_func_map[i].frame_fmt)
			return zc_func_map[i].func;
	}

	return NULL;
}

/**
 * \brief Set volume frames alignment limit.
 * \param[in,out] source Structure pointer of source.
 * \param[in,out] sink Structure pointer of sink.
 */
static void volume_set_alignment(struct audio_stream __sparse_cache *source,
				 struct audio_stream __sparse_cache *sink)
{
#if XCHAL_HAVE_HIFI3 || XCHAL_HAVE_HIFI4

	/* Both source and sink buffer in HiFi 3 or HiFi4 processing version,
	 * xtensa intrinsics ask for 8-byte aligned. 5.1 format SSE audio
	 * requires 16-byte aligned.
	 */
	const uint32_t byte_align = source->channels == 6 ? 16 : 8;

	/*There is no limit for frame number, so both source and sink set it to be 1*/
	const uint32_t frame_align_req = 1;

#else

	/* Since the generic version process signal sample by sample, so there is no
	 * limit for it, then set the byte_align and frame_align_req to be 1.
	 */
	const uint32_t byte_align = 1;
	const uint32_t frame_align_req = 1;

#endif

	audio_stream_init_alignment_constants(byte_align, frame_align_req, source);
	audio_stream_init_alignment_constants(byte_align, frame_align_req, sink);
}

/**
 * \brief Prepares volume component for processing.
 * \param[in,out] mod Volume processing module handle

 * \return Error code.
 *
 * Volume component is usually first and last in pipelines so it makes sense
 * to also do some type of conversion here.
 */
static int volume_prepare(struct processing_module *mod)
{
	struct vol_data *cd = module_get_private_data(mod);
	struct module_data *md = &mod->priv;
	struct comp_dev *dev = mod->dev;
	struct comp_buffer *sourceb, *sinkb;
	struct comp_buffer __sparse_cache *source_c, *sink_c;
	uint32_t sink_period_bytes;
	int ret;
	int i;

	comp_dbg(dev, "volume_prepare()");

#if CONFIG_IPC_MAJOR_4
	ret = volume_params(mod);
	if (ret < 0)
		return ret;
#endif

	/* volume component will only ever have 1 sink and source buffer */
	sinkb = list_first_item(&dev->bsink_list,
				struct comp_buffer, source_list);
	sourceb = list_first_item(&dev->bsource_list,
				  struct comp_buffer, sink_list);

	sink_c = buffer_acquire(sinkb);
	source_c = buffer_acquire(sourceb);

	volume_set_alignment(&source_c->stream, &sink_c->stream);

	buffer_release(source_c);

	/* get sink period bytes */
	sink_period_bytes = audio_stream_period_bytes(&sink_c->stream,
						      dev->frames);

	if (sink_c->stream.size < sink_period_bytes) {
		comp_err(dev, "volume_prepare(): sink buffer size %d is insufficient < %d",
			 sink_c->stream.size, sink_period_bytes);
		ret = -ENOMEM;
		goto err;
	}

	cd->scale_vol = vol_get_processing_function(dev, sink_c);
	if (!cd->scale_vol) {
		comp_err(dev, "volume_prepare(): invalid cd->scale_vol");

		ret = -EINVAL;
		goto err;
	}

	cd->zc_get = vol_get_zc_function(dev, sink_c);
	if (!cd->zc_get) {
		comp_err(dev, "volume_prepare(): invalid cd->zc_get");
		ret = -EINVAL;
		goto err;
	}

	if (cd->initial_ramp && !cd->ramp_func) {
		comp_err(dev, "volume_prepare(): invalid cd->ramp_func");
		ret = -EINVAL;
		goto err;
	}

	/* Set current volume to min to ensure ramp starts from minimum
	 * to previous volume request. Copy() checks for ramp finished
	 * and executes it if it has not yet finished as result of
	 * driver commands. Ramp is not constant rate to ensure it lasts
	 * for entire topology specified time.
	 */
	cd->ramp_finished = true;
	cd->channels = sink_c->stream.channels;
	cd->sample_rate = sink_c->stream.rate;

	buffer_release(sink_c);

	for (i = 0; i < cd->channels; i++) {
		cd->volume[i] = cd->vol_min;
		volume_set_chan(mod, i, cd->tvolume[i], false);
		if (cd->volume[i] != cd->tvolume[i])
			cd->ramp_finished = false;
	}

	prepare_ramp(dev, cd);

	/*
	 * volume component does not do any format conversion, so use the buffer size for source
	 * and sink
	 */
	md->mpd.in_buff_size = sink_period_bytes;
	md->mpd.out_buff_size = sink_period_bytes;

	/*
	 * also set the simple_copy flag as the volume component always produces period_bytes
	 * every period and has only 1 input/output buffer
	 */
	mod->simple_copy = true;

	return 0;

err:
	buffer_release(sink_c);
	comp_set_state(dev, COMP_TRIGGER_RESET);
	return ret;
}

/**
 * \brief Resets volume component.
 * \param[in,out] mod Volume processing module handle

 * \return Error code.
 */
static int volume_reset(struct processing_module *mod)
{
	struct vol_data *cd = module_get_private_data(mod);

	comp_dbg(mod->dev, "volume_reset()");
	reset_state(cd);
	return 0;
}

#if CONFIG_COMP_LEGACY_INTERFACE

static const struct comp_driver comp_volume;

static struct comp_dev *volume_new(const struct comp_driver *drv,
				   struct comp_ipc_config *config,
				   void *spec)
{
	struct processing_module *mod;
	struct module_config *dst;
	struct module_data *md;
	struct comp_dev *dev;
	int ret;

	comp_cl_dbg(&comp_volume, "volume_new()");

	dev = comp_alloc(drv, sizeof(*dev));
	if (!dev)
		return NULL;

	dev->ipc_config = *config;

	mod = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(*mod));
	if (!mod) {
		comp_err(dev, "module_adapter_new(), failed to allocate memory for module");
		goto fail;
	}

	comp_set_drvdata(dev, mod);
	mod->dev = dev;

	md = &mod->priv;
	dst = &md->cfg;
	dst->data = rballoc(0, SOF_MEM_CAPS_RAM, sizeof(struct ipc_config_volume));
	if (!dst->data) {
		ret = -ENOMEM;
		goto mod_free;
	}

	ret = memcpy_s(dst->data, sizeof(struct ipc_config_volume), spec,
		       sizeof(struct ipc_config_volume));
	if (ret < 0)
		goto mod_free;
	dst->size = sizeof(struct ipc_config_volume);
	dst->avail = true;

	ret = volume_init(mod);
	if (ret < 0)
		goto mod_free;

	dev->state = COMP_STATE_READY;
	return dev;
mod_free:
	rfree(mod);
fail:
	rfree(dev);
	return NULL;
}

static void volume_legacy_free(struct comp_dev *dev)
{
	struct processing_module *mod = comp_get_drvdata(dev);
	struct module_data *md = &mod->priv;
	struct module_config *cfg = &md->cfg;

	comp_dbg(dev, "volume_legacy_free()");

	if (cfg->data)
		rfree(cfg->data);

	volume_free(mod);

	rfree(mod);
	rfree(dev);
}

static int volume_cmd(struct comp_dev *dev, int cmd, void *data,
		      int max_data_size)
{
	struct sof_ipc_ctrl_data *cdata = ASSUME_ALIGNED(data, 4);
	struct processing_module *mod = comp_get_drvdata(dev);

	comp_dbg(dev, "volume_cmd()");

	switch (cmd) {
	case COMP_CMD_SET_VALUE:
		return volume_set_config(mod, 0, 0, 0, (const uint8_t *)cdata, 0, NULL, 0);
	case COMP_CMD_GET_VALUE:
		return volume_get_config(mod, 0, NULL, (uint8_t *)cdata, max_data_size);
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
	struct processing_module *mod = comp_get_drvdata(dev);
	struct comp_copy_limits c;
	struct comp_buffer *source, *sink;
	struct comp_buffer __sparse_cache *source_c, *sink_c;
	struct input_stream_buffer input_buffer;
	struct output_stream_buffer output_buffer;
	uint32_t source_bytes;
	int ret;

	comp_dbg(dev, "volume_copy()");

	source = list_first_item(&dev->bsource_list, struct comp_buffer,
				 sink_list);
	sink = list_first_item(&dev->bsink_list, struct comp_buffer,
			       source_list);

	source_c = buffer_acquire(source);
	sink_c = buffer_acquire(sink);

	/* Get aligned source, sink, number of frames etc. to process. */
	comp_get_copy_limits_frame_aligned(source_c, sink_c, &c);

	comp_dbg(dev, "volume_copy(), source_bytes = 0x%x, sink_bytes = 0x%x",
		 c.source_bytes, c.sink_bytes);

	source_bytes = c.frames * c.source_frame_bytes;
	buffer_stream_invalidate(source_c, source_bytes);

	input_buffer.size = c.frames;
	input_buffer.data = &source_c->stream;
	input_buffer.consumed = 0;
	output_buffer.size = 0;
	output_buffer.data = &sink_c->stream;

	ret = volume_process(mod, &input_buffer, 1, &output_buffer, 1);
	if (ret < 0)
		goto out;

	buffer_stream_writeback(sink_c, output_buffer.size);

	/* calculate new free and available */
	comp_update_buffer_produce(sink_c, output_buffer.size);
	comp_update_buffer_consume(source_c, input_buffer.consumed);

out:
	buffer_release(sink_c);
	buffer_release(source_c);

	return ret;
}

/**
 * \brief Prepares volume component for processing.
 * \param[in,out] dev Volume base component device.
 * \return Error code.
 *
 * Volume component is usually first and last in pipelines so it makes sense
 * to also do some type of conversion here.
 */
static int volume_legacy_prepare(struct comp_dev *dev)
{
	struct processing_module *mod = comp_get_drvdata(dev);
	int ret;

	ret = comp_set_state(dev, COMP_TRIGGER_PREPARE);
	if (ret < 0)
		return ret;

	if (ret == COMP_STATUS_STATE_ALREADY_SET)
		return PPL_STATUS_PATH_STOP;

	ret = volume_prepare(mod);
	if (ret < 0)
		return ret;

	return 0;
}

/**
 * \brief Resets volume component.
 * \param[in,out] dev Volume base component device.
 * \return Error code.
 */
static int volume_legacy_reset(struct comp_dev *dev)
{
	struct processing_module *mod = comp_get_drvdata(dev);

	comp_dbg(dev, "volume_legacy_reset()");
	volume_reset(mod);
	comp_set_state(dev, COMP_TRIGGER_RESET);
	return 0;
}

/** \brief Volume component definition. */
static const struct comp_driver comp_volume = {
	.type	= SOF_COMP_VOLUME,
	.uid	= SOF_RT_UUID(volume_uuid),
	.tctx	= &volume_tr,
	.ops	= {
		.create	= volume_new,
		.free		= volume_legacy_free,
		.cmd		= volume_cmd,
		.trigger	= volume_trigger,
		.copy		= volume_copy,
		.prepare	= volume_legacy_prepare,
		.reset		= volume_legacy_reset,
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
#else
static struct module_interface volume_interface = {
	.init  = volume_init,
	.prepare = volume_prepare,
	.process = volume_process,
	.set_configuration = volume_set_config,
	.get_configuration = volume_get_config,
	.reset = volume_reset,
	.free = volume_free
};

DECLARE_MODULE_ADAPTER(volume_interface, volume_uuid, volume_tr);

#if CONFIG_COMP_GAIN
static struct module_interface gain_interface = {
	.init  = volume_init,
	.prepare = volume_prepare,
	.process = volume_process,
	.set_configuration = volume_set_config,
	.get_configuration = volume_get_config,
	.reset = volume_reset,
	.free = volume_free
};

DECLARE_MODULE_ADAPTER(gain_interface, gain_uuid, gain_tr);
#endif
#endif
