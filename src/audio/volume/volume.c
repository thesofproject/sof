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

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

LOG_MODULE_REGISTER(volume, CONFIG_SOF_LOG_LEVEL);

#include "volume_uuid.h"
#include "volume.h"

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
	uint32_t curr_frames = frames;
	int32_t sum;
	int16_t *x = audio_stream_get_rptr(source);
	int bytes;
	int nmax;
	int i, j, n;
	const int nch = audio_stream_get_channels(source);
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
static uint32_t vol_zc_get_s24(const struct audio_stream *source,
			       uint32_t frames, int64_t *prev_sum)
{
	int64_t sum;
	uint32_t curr_frames = frames;
	int32_t *x = audio_stream_get_rptr(source);
	int bytes;
	int nmax;
	int i, j, n;
	const int nch = audio_stream_get_channels(source);
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
static uint32_t vol_zc_get_s32(const struct audio_stream *source,
			       uint32_t frames, int64_t *prev_sum)
{
	int64_t sum;
	uint32_t curr_frames = frames;
	int32_t *x = audio_stream_get_rptr(source);
	int bytes;
	int nmax;
	int i, j, n;
	const int nch = audio_stream_get_channels(source);
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
static inline int32_t volume_linear_ramp(struct vol_data *cd, int32_t ramp_time, int channel)
{
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

static inline int32_t volume_windows_fade_ramp(struct vol_data *cd, int32_t ramp_time, int channel)
{
	int32_t time_ratio; /* Q2.30 */
	int32_t pow_value; /* Q2.30 */
	int32_t volume_delta = cd->tvolume[channel] - cd->rvolume[channel]; /* Q16.16 */

	if (!cd->initial_ramp)
		return cd->tvolume[channel];

	time_ratio = (((int64_t)ramp_time) << 30) / (cd->initial_ramp << 3);
	pow_value = volume_pow_175(time_ratio);
	return cd->rvolume[channel] + Q_MULTSR_32X32((int64_t)volume_delta, pow_value, 16, 30, 16);
}
#endif

/**
 * \brief Ramps volume changes over time.
 * \param[in,out] vol_data Volume component data
 */

/* Note: Using inline saves 0.4 MCPS */
static inline void volume_ramp(struct processing_module *mod)
{
	struct vol_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	int32_t new_vol;
	int32_t tvolume;
	int32_t volume;
	int i;

	cd->ramp_finished = true;
	cd->copy_gain = true;

	/* Current ramp time in Q29.3 milliseconds. Note that max. ramp length
	 * can be 1.3s at 192 kHz rate and 5.5s at 48 kHz rate without
	 * exceeding int32_t range. Inverse of sample rate is 1000/sample_rate
	 * for milliseconds.
	 */
#if defined CONFIG_COMP_VOLUME_WINDOWS_FADE || defined CONFIG_COMP_VOLUME_LINEAR_RAMP
	int32_t ramp_time = Q_MULTSR_32X32((int64_t)cd->vol_ramp_elapsed_frames,
					   cd->sample_rate_inv, 0, 31, 3);
#endif

	/* Update each volume if it's not at target for active channels */
	for (i = 0; i < cd->channels; i++) {
		/* skip if target reached */
		volume = cd->volume[i];
		tvolume = cd->tvolume[i];
		if (volume == tvolume)
			continue;

		/* Update volume gain with ramp. The ramp gain value is
		 * calculated from previous gain and ramp time. The slope
		 * coefficient is calculated in volume_set_chan().
		 */
		switch (cd->ramp_type) {
#if CONFIG_COMP_VOLUME_WINDOWS_FADE
		case SOF_VOLUME_WINDOWS_FADE:
			new_vol = volume_windows_fade_ramp(cd, ramp_time, i);
			break;
#endif
#if CONFIG_COMP_VOLUME_LINEAR_RAMP
		case SOF_VOLUME_LINEAR:
		case SOF_VOLUME_LINEAR_ZC:
			new_vol = volume_linear_ramp(cd, ramp_time, i);
			break;
#endif
		default:
			new_vol = tvolume;
		}

		if (volume < tvolume) {
			/* ramp up, check if ramp completed */
			if (new_vol < tvolume)
				cd->ramp_finished = false;
			else
				new_vol = tvolume;
		} else {
			/* ramp down */
			if (new_vol > tvolume)
				cd->ramp_finished = false;
			else
				new_vol = tvolume;
		}
		cd->volume[i] = new_vol;
	}

	cd->is_passthrough = cd->ramp_finished;
	for (i = 0; i < cd->channels; i++) {
		if (cd->volume[i] != VOL_ZERO_DB) {
			cd->is_passthrough = false;
			break;
		}
	}

	set_volume_process(cd, dev, true);
}

/**
 * \brief Reset state except controls.
 */
void volume_reset_state(struct vol_data *cd)
{
	int i;

	for (i = 0; i < PLATFORM_MAX_CHANNELS; i++) {
		cd->rvolume[i] = 0;
		cd->ramp_coef[i] = 0;
	}

	cd->channels = 0;
	cd->ramp_finished = true;
	cd->vol_ramp_frames = 0;
	cd->vol_ramp_elapsed_frames = 0;
	cd->sample_rate_inv = 0;
	cd->copy_gain = true;
	cd->is_passthrough = false;
}

void volume_prepare_ramp(struct comp_dev *dev, struct vol_data *cd)
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

	comp_dbg(mod->dev, "volume_free()");

	volume_peak_free(cd);
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
int volume_set_chan(struct processing_module *mod, int chan,
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
	if (v < cd->vol_min) {
		/* No need to fail, just trace the event. */
		comp_warn(mod->dev, "volume_set_chan: Limited request %d to min. %d",
			  v, cd->vol_min);
		v = cd->vol_min;
	}

	if (v > cd->vol_max) {
		/* No need to fail, just trace the event. */
		comp_warn(mod->dev, "volume_set_chan: Limited request %d to max. %d",
			  v, cd->vol_max);
		v = cd->vol_max;
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
void volume_set_chan_mute(struct processing_module *mod, int chan)
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
void volume_set_chan_unmute(struct processing_module *mod, int chan)
{
	struct vol_data *cd = module_get_private_data(mod);

	if (cd->muted[chan]) {
		cd->muted[chan] = false;
		volume_set_chan(mod, chan, cd->mvolume[chan], true);
	}
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

		if (!cd->ramp_finished) {
			volume_ramp(mod);
			cd->vol_ramp_elapsed_frames += frames;
		}

		/* copy and scale volume */
		cd->scale_vol(mod, &input_buffers[0], &output_buffers[0], frames, cd->attenuation);

		avail_frames -= frames;
	}
#if CONFIG_COMP_PEAK_VOL
	cd->peak_cnt++;
	if (cd->peak_cnt == cd->peak_report_cnt) {
		cd->peak_cnt = 0;
		peak_vol_update(cd);
		memset(cd->peak_regs.peak_meter, 0, sizeof(cd->peak_regs.peak_meter));
#ifdef VOLUME_HIFI4
		memset(cd->peak_vol, 0, sizeof(int32_t) * SOF_IPC_MAX_CHANNELS * 4);
#endif
	}
#endif

	return 0;
}

/*
 * \brief Retrieves volume zero crossing function.
 * \param[in,out] dev Volume base component device.
 */
static vol_zc_func vol_get_zc_function(struct comp_dev *dev,
				       struct comp_buffer *sinkb)
{
	int i;

	/* map the zc function to frame format */
	for (i = 0; i < ARRAY_SIZE(zc_func_map); i++) {
		if (audio_stream_get_valid_fmt(&sinkb->stream) == zc_func_map[i].frame_fmt)
			return zc_func_map[i].func;
	}

	return NULL;
}

/**
 * \brief Set volume frames alignment limit.
 * \param[in,out] source Structure pointer of source.
 * \param[in,out] sink Structure pointer of sink.
 */
static void volume_set_alignment(struct audio_stream *source,
				 struct audio_stream *sink)
{
#if XCHAL_HAVE_HIFI3 || XCHAL_HAVE_HIFI4

	/* Both source and sink buffer in HiFi 3 or HiFi4 processing version,
	 * xtensa intrinsics ask for 8-byte aligned. 5.1 format SSE audio
	 * requires 16-byte aligned.
	 */
	const uint32_t byte_align = audio_stream_get_channels(source) == 6 ? 16 : 8;

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
static int volume_prepare(struct processing_module *mod,
			  struct sof_source **sources, int num_of_sources,
			  struct sof_sink **sinks, int num_of_sinks)
{
	struct vol_data *cd = module_get_private_data(mod);
	struct module_data *md = &mod->priv;
	struct comp_dev *dev = mod->dev;
	struct comp_buffer *sourceb, *sinkb;
	uint32_t sink_period_bytes;
	int ret;
	int i;

	comp_dbg(dev, "volume_prepare()");

	ret = volume_peak_prepare(cd, mod);

	/* volume component will only ever have 1 sink and source buffer */
	sinkb = list_first_item(&dev->bsink_list,
				struct comp_buffer, source_list);
	sourceb = list_first_item(&dev->bsource_list,
				  struct comp_buffer, sink_list);

	volume_set_alignment(&sourceb->stream, &sinkb->stream);

	/* get sink period bytes */
	sink_period_bytes = audio_stream_period_bytes(&sinkb->stream,
						      dev->frames);

	if (audio_stream_get_size(&sinkb->stream) < sink_period_bytes) {
		comp_err(dev, "volume_prepare(): sink buffer size %d is insufficient < %d",
			 audio_stream_get_size(&sinkb->stream), sink_period_bytes);
		ret = -ENOMEM;
		goto err;
	}

	set_volume_process(cd, dev, false);

	if (!cd->scale_vol) {
		comp_err(dev, "volume_prepare(): invalid cd->scale_vol");

		ret = -EINVAL;
		goto err;
	}

	cd->zc_get = vol_get_zc_function(dev, sinkb);
	if (!cd->zc_get) {
		comp_err(dev, "volume_prepare(): invalid cd->zc_get");
		ret = -EINVAL;
		goto err;
	}

	/* Set current volume to min to ensure ramp starts from minimum
	 * to previous volume request. Copy() checks for ramp finished
	 * and executes it if it has not yet finished as result of
	 * driver commands. Ramp is not constant rate to ensure it lasts
	 * for entire topology specified time.
	 */
	cd->ramp_finished = false;

	cd->channels = audio_stream_get_channels(&sinkb->stream);
	if (cd->channels > SOF_IPC_MAX_CHANNELS) {
		ret = -EINVAL;
		goto err;
	}

	cd->sample_rate_inv = (int32_t)(1000LL * INT32_MAX /
					audio_stream_get_rate(&sinkb->stream));

	for (i = 0; i < cd->channels; i++) {
		cd->volume[i] = cd->vol_min;
		volume_set_chan(mod, i, cd->tvolume[i], false);
		if (cd->volume[i] != cd->tvolume[i])
			cd->ramp_finished = false;
	}

	volume_prepare_ramp(dev, cd);

	/*
	 * volume component does not do any format conversion, so use the buffer size for source
	 * and sink
	 */
	md->mpd.in_buff_size = sink_period_bytes;
	md->mpd.out_buff_size = sink_period_bytes;

	return 0;

err:
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
	volume_reset_state(cd);
	return 0;
}

static const struct module_interface volume_interface = {
	.init = volume_init,
	.prepare = volume_prepare,
	.process_audio_stream = volume_process,
	.set_configuration = volume_set_config,
	.get_configuration = volume_get_config,
	.reset = volume_reset,
	.free = volume_free
};

DECLARE_MODULE_ADAPTER(volume_interface, volume_uuid, volume_tr);
SOF_MODULE_INIT(volume, sys_comp_module_volume_interface_init);

#if CONFIG_COMP_GAIN
static const struct module_interface gain_interface = {
	.init = volume_init,
	.prepare = volume_prepare,
	.process_audio_stream = volume_process,
	.set_configuration = volume_set_config,
	.get_configuration = volume_get_config,
	.reset = volume_reset,
	.free = volume_free
};

DECLARE_MODULE_ADAPTER(gain_interface, gain_uuid, gain_tr);
SOF_MODULE_INIT(gain, sys_comp_module_gain_interface_init);
#endif
