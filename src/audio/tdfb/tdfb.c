// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Intel Corporation. All rights reserved.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

/* Note: The script tools/tune/tdfb/example_all.sh can be used to re-calculate
 * all the beamformer topology data files if need. It also creates the additional
 * data files for simulated tests with testbench. Matlab or Octave is needed.
 */

#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/audio/data_blob.h>
#include <sof/audio/ipc-config.h>
#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/pipeline.h>
#include <sof/ipc/msg.h>
#include <sof/lib/uuid.h>
#include <sof/math/fir_generic.h>
#include <sof/math/fir_hifi2ep.h>
#include <sof/math/fir_hifi3.h>
#include <sof/platform.h>
#include <sof/trace/trace.h>
#include <ipc/control.h>
#include <ipc/stream.h>
#include <ipc/topology.h>
#include <rtos/init.h>
#include <rtos/panic.h>
#include <rtos/string.h>
#include <sof/common.h>
#include <sof/list.h>
#include <sof/ut.h>
#include <user/trace.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

#include "tdfb.h"
#include "tdfb_comp.h"

LOG_MODULE_REGISTER(tdfb, CONFIG_SOF_LOG_LEVEL);

SOF_DEFINE_REG_UUID(tdfb);

static inline int set_func(struct processing_module *mod, enum sof_ipc_frame fmt)
{
	struct tdfb_comp_data *cd = module_get_private_data(mod);

	switch (fmt) {
#if CONFIG_FORMAT_S16LE
	case SOF_IPC_FRAME_S16_LE:
		comp_dbg(mod->dev, "SOF_IPC_FRAME_S16_LE");
		cd->tdfb_func = tdfb_fir_s16;
		break;
#endif /* CONFIG_FORMAT_S16LE */
#if CONFIG_FORMAT_S24LE
	case SOF_IPC_FRAME_S24_4LE:
		comp_dbg(mod->dev, "SOF_IPC_FRAME_S24_4LE");
		cd->tdfb_func = tdfb_fir_s24;
		break;
#endif /* CONFIG_FORMAT_S24LE */
#if CONFIG_FORMAT_S32LE
	case SOF_IPC_FRAME_S32_LE:
		comp_dbg(mod->dev, "SOF_IPC_FRAME_S32_LE");
		cd->tdfb_func = tdfb_fir_s32;
		break;
#endif /* CONFIG_FORMAT_S32LE */
	default:
		comp_err(mod->dev, "invalid frame_fmt");
		return -EINVAL;
	}
	return 0;
}

/*
 * Pass-through processing functions
 */

static void tdfb_pass_same_format(struct tdfb_comp_data *cd, struct input_stream_buffer *bsource,
				  struct output_stream_buffer *bsink, int frames)
{
	struct audio_stream *source = bsource->data;
	struct audio_stream *sink = bsink->data;

	audio_stream_copy(source, 0, sink, 0, frames * audio_stream_get_channels(source));
}

#if CONFIG_FORMAT_S16LE
static void tdfb_pass_s16(struct tdfb_comp_data *cd, struct input_stream_buffer *bsource,
			  struct output_stream_buffer *bsink, int frames)
{
	struct audio_stream *source = bsource->data;
	struct audio_stream *sink = bsink->data;
	int16_t *x = audio_stream_get_rptr(source);
	int16_t *y = audio_stream_get_wptr(sink);
	int16_t s0, s1;
	int fmax;
	int is, om;
	int f, i, j, k;
	const int in_nch = audio_stream_get_channels(source);
	const int out_nch = audio_stream_get_channels(sink);
	const int num_filters = cd->config->num_filters;
	int remaining_frames = frames;

	while (remaining_frames) {
		fmax = audio_stream_frames_without_wrap(source, x);
		f = MIN(remaining_frames, fmax);
		fmax = audio_stream_frames_without_wrap(sink, y);
		f = MIN(f, fmax);
		for (j = 0; j < f; j += 2) {
			for (i = 0; i < num_filters; i++) {
				is = cd->input_channel_select[i];
				om = cd->output_channel_mix[i];
				s0 = x[is];
				s1 = x[is + in_nch];
				for (k = 0; k < out_nch; k++) {
					if (om & 1) {
						y[k] = s0;
						y[k + out_nch] = s1;
					}
					om = om >> 1;
				}
			}
			x += 2 * in_nch;
			y += 2 * out_nch;
		}
		remaining_frames -= f;
		x = audio_stream_wrap(source, x);
		y = audio_stream_wrap(sink, y);
	}
}
#endif

#if CONFIG_FORMAT_S24LE
static void tdfb_pass_s24(struct tdfb_comp_data *cd, struct input_stream_buffer *bsource,
			  struct output_stream_buffer *bsink, int frames)
{
	struct audio_stream *source = bsource->data;
	struct audio_stream *sink = bsink->data;
	int32_t *x = audio_stream_get_rptr(source);
	int32_t *y = audio_stream_get_wptr(sink);
	int32_t s0, s1;
	int fmax;
	int is, om;
	int f, i, j, k;
	const int in_nch = audio_stream_get_channels(source);
	const int out_nch = audio_stream_get_channels(sink);
	const int num_filters = cd->config->num_filters;
	int remaining_frames = frames;

	while (remaining_frames) {
		fmax = audio_stream_frames_without_wrap(source, x);
		f = MIN(remaining_frames, fmax);
		fmax = audio_stream_frames_without_wrap(sink, y);
		f = MIN(f, fmax);
		for (j = 0; j < f; j += 2) {
			for (i = 0; i < num_filters; i++) {
				is = cd->input_channel_select[i];
				om = cd->output_channel_mix[i];
				s0 = x[is];
				s1 = x[is + in_nch];
				for (k = 0; k < out_nch; k++) {
					if (om & 1) {
						y[k] = s0;
						y[k + out_nch] = s1;
					}
					om = om >> 1;
				}
			}
			x += 2 * in_nch;
			y += 2 * out_nch;
		}
		remaining_frames -= f;
		x = audio_stream_wrap(source, x);
		y = audio_stream_wrap(sink, y);
	}
}
#endif

#if CONFIG_FORMAT_S32LE
static void tdfb_pass_s32(struct tdfb_comp_data *cd, struct input_stream_buffer *bsource,
			  struct output_stream_buffer *bsink, int frames)
{
	struct audio_stream *source = bsource->data;
	struct audio_stream *sink = bsink->data;
	int32_t *x = audio_stream_get_rptr(source);
	int32_t *y = audio_stream_get_wptr(sink);
	int32_t s0, s1;
	int fmax;
	int is, om;
	int f, i, j, k;
	const int in_nch = audio_stream_get_channels(source);
	const int out_nch = audio_stream_get_channels(sink);
	const int num_filters = cd->config->num_filters;
	int remaining_frames = frames;

	while (remaining_frames) {
		fmax = audio_stream_frames_without_wrap(source, x);
		f = MIN(remaining_frames, fmax);
		fmax = audio_stream_frames_without_wrap(sink, y);
		f = MIN(f, fmax);
		for (j = 0; j < f; j += 2) {
			for (i = 0; i < num_filters; i++) {
				is = cd->input_channel_select[i];
				om = cd->output_channel_mix[i];
				s0 = x[is];
				s1 = x[is + in_nch];
				for (k = 0; k < out_nch; k++) {
					if (om & 1) {
						y[k] = s0;
						y[k + out_nch] = s1;
					}
					om = om >> 1;
				}
			}
			x += 2 * in_nch;
			y += 2 * out_nch;
		}
		remaining_frames -= f;
		x = audio_stream_wrap(source, x);
		y = audio_stream_wrap(sink, y);
	}
}
#endif

static inline int set_pass_func(struct processing_module *mod, enum sof_ipc_frame fmt)
{
	struct tdfb_comp_data *cd = module_get_private_data(mod);

	switch (fmt) {
#if CONFIG_FORMAT_S16LE
	case SOF_IPC_FRAME_S16_LE:
		comp_dbg(mod->dev, "SOF_IPC_FRAME_S16_LE");
		cd->tdfb_func = tdfb_pass_s16;
		break;
#endif /* CONFIG_FORMAT_S16LE */
#if CONFIG_FORMAT_S24LE
	case SOF_IPC_FRAME_S24_4LE:
		comp_dbg(mod->dev, "SOF_IPC_FRAME_S24_4LE");
		cd->tdfb_func = tdfb_pass_s24;
		break;
#endif /* CONFIG_FORMAT_S24LE */
#if CONFIG_FORMAT_S32LE
	case SOF_IPC_FRAME_S32_LE:
		comp_dbg(mod->dev, "SOF_IPC_FRAME_S32_LE");
		cd->tdfb_func = tdfb_pass_s32;
		break;
#endif /* CONFIG_FORMAT_S32LE */
	default:
		comp_err(mod->dev, "invalid frame_fmt");
		return -EINVAL;
	}
	return 0;
}

/*
 * Control code functions next. The processing is in fir_ C modules.
 */

static void tdfb_free_delaylines(struct processing_module *mod)
{
	struct tdfb_comp_data *cd = module_get_private_data(mod);
	struct fir_state_32x16 *fir = cd->fir;
	int i = 0;

	/* Free the common buffer for all EQs and point then
	 * each FIR channel delay line to NULL.
	 */
	mod_free(mod, cd->fir_delay);
	cd->fir_delay = NULL;
	cd->fir_delay_size = 0;
	for (i = 0; i < PLATFORM_MAX_CHANNELS; i++)
		fir[i].delay = NULL;
}

static int16_t *tdfb_filter_seek(struct sof_tdfb_config *config, int num_filters)
{
	struct sof_fir_coef_data *coef_data;
	int i;
	int16_t *coefp = ASSUME_ALIGNED(&config->data[0], 2); /* 2 for 16 bits data */

	/* Note: FIR coefficients are int16_t. An uint_8 type pointer coefp
	 * is used for jumping in the flexible array member structs coef_data.
	 */
	for (i = 0; i < num_filters; i++) {
		coef_data = (struct sof_fir_coef_data *)coefp;
		coefp = coef_data->coef + coef_data->length;
	}

	return coefp;
}

static int wrap_180(int a)
{
	if (a > 180)
		return ((a + 180) % 360) - 180;

	if (a < -180)
		return 180 - ((180 - a) % 360);

	return a;
}

/* Walk the TDFB blob and verify that every FIR section and trailing array
 * fits exactly inside the IPC payload. The walk mirrors tdfb_init_coef()
 * but stays bounded so a malformed blob cannot push tdfb_filter_seek()
 * past the buffer end at setup time. The channel-vs-stream relationships
 * are also checked here so a mismatching blob cannot replace the working
 * configuration during streaming.
 */
static int tdfb_validate_config(struct comp_dev *dev,
				struct sof_tdfb_config *config,
				size_t config_size)
{
	struct processing_module *mod = comp_mod(dev);
	struct tdfb_comp_data *cd = module_get_private_data(mod);
	struct sof_fir_coef_data *coef_data;
	struct sof_tdfb_angle *filter_angles;
	int16_t *input_channel_select;
	uint8_t *p;
	uint8_t *end;
	size_t remaining;
	size_t step;
	size_t expected;
	int16_t max_ch;
	int total_filters;
	int i;

	if ((size_t)config->size != config_size) {
		comp_err(dev, "blob size %zu / header size %u mismatch",
			 config_size, config->size);
		return -EINVAL;
	}
	if (!config->num_output_channels ||
	    config->num_output_channels > PLATFORM_MAX_CHANNELS) {
		comp_err(dev, "invalid num_output_channels %u",
			 config->num_output_channels);
		return -EINVAL;
	}
	if (!config->num_filters || config->num_filters > SOF_TDFB_FIR_MAX_COUNT) {
		comp_err(dev, "invalid num_filters %u", config->num_filters);
		return -EINVAL;
	}
	if (!config->num_angles || config->num_angles > SOF_TDFB_MAX_ANGLES) {
		comp_err(dev, "invalid num_angles %u", config->num_angles);
		return -EINVAL;
	}
	if (!config->angle_enum_mult) {
		comp_err(dev, "invalid angle_enum_mult");
		return -EINVAL;
	}
	if (config->beam_off_defined > 1) {
		comp_err(dev, "invalid beam_off_defined %u",
			 config->beam_off_defined);
		return -EINVAL;
	}
	if (config->num_mic_locations > SOF_TDFB_MAX_MICROPHONES) {
		comp_err(dev, "invalid num_mic_locations %u",
			 config->num_mic_locations);
		return -EINVAL;
	}

	total_filters = config->num_filters * (config->num_angles + config->beam_off_defined);
	p = (uint8_t *)&config->data[0];
	end = (uint8_t *)config + config_size;
	for (i = 0; i < total_filters; i++) {
		remaining = end - p;
		if (remaining < sizeof(struct sof_fir_coef_data)) {
			comp_err(dev, "FIR %d header out of bounds", i);
			return -EINVAL;
		}
		coef_data = (struct sof_fir_coef_data *)p;
		if (fir_delay_size(coef_data) <= 0) {
			comp_err(dev, "FIR %d invalid length %d",
				 i, coef_data->length);
			return -EINVAL;
		}
		step = sizeof(struct sof_fir_coef_data) +
			(size_t)coef_data->length * sizeof(int16_t);
		if (step > remaining) {
			comp_err(dev, "FIR %d coefs out of bounds", i);
			return -EINVAL;
		}
		p += step;
	}

	/* p now points at input_channel_select[]. The remaining bytes must
	 * hold exactly: 3 per-filter int16 arrays (input_channel_select,
	 * output_channel_mix, output_stream_mix), an optional beam-off output
	 * channel mix, num_angles angle entries and num_mic_locations
	 * microphone entries.
	 */
	input_channel_select = (int16_t *)p;
	expected = ((size_t)config->num_filters * SOF_TDFB_CONFIG_FILTER_CONTROL_NUM_WORDS +
		    (size_t)config->beam_off_defined * config->num_filters) *
			sizeof(int16_t) +
		   (size_t)config->num_angles * sizeof(struct sof_tdfb_angle) +
		   (size_t)config->num_mic_locations *
			sizeof(struct sof_tdfb_mic_location);
	if ((size_t)(end - p) != expected) {
		comp_err(dev, "blob trailer size mismatch: have %zu, expected %zu",
			 (size_t)(end - p), expected);
		return -EINVAL;
	}

	/* Each filter_angles[].filter_index points at the first filter of a
	 * num_filters-wide bank, so it must be in [0, total_filters -
	 * num_filters]. Validating here ensures the runtime path in
	 * tdfb_init_coef() cannot be reached with an out-of-range index.
	 */
	filter_angles = (struct sof_tdfb_angle *)
		(p + ((size_t)config->num_filters * SOF_TDFB_CONFIG_FILTER_CONTROL_NUM_WORDS +
		(size_t)config->beam_off_defined * config->num_filters) * sizeof(int16_t));
	for (i = 0; i < config->num_angles; i++) {
		if (filter_angles[i].filter_index < 0 ||
		    filter_angles[i].filter_index + config->num_filters > total_filters) {
			comp_err(dev, "invalid filter_index %d for angle %d",
				 filter_angles[i].filter_index, i);
			return -EINVAL;
		}
	}

	/* The blob must match the running stream. Skip these checks when no
	 * stream is bound yet (cached channel counts are zero before prepare).
	 */
	if (cd->sink_channels && config->num_output_channels != cd->sink_channels) {
		comp_err(dev, "blob num_output_channels %u does not match sink %d",
			 config->num_output_channels, cd->sink_channels);
		return -EINVAL;
	}
	if (cd->source_channels) {
		max_ch = 0;
		for (i = 0; i < config->num_filters; i++) {
			if (input_channel_select[i] < 0) {
				comp_err(dev, "invalid channel select for filter %d", i);
				return -EINVAL;
			}
			if (input_channel_select[i] > max_ch)
				max_ch = input_channel_select[i];
		}
		if (max_ch + 1 > cd->source_channels) {
			comp_err(dev, "blob needs %d source channels, stream has %d",
				 max_ch + 1, cd->source_channels);
			return -EINVAL;
		}
	}

	return 0;
}

static int tdfb_validator(struct comp_dev *dev, void *new_data, uint32_t new_data_size)
{
	if (new_data_size < sizeof(struct sof_tdfb_config) ||
	    new_data_size > SOF_TDFB_MAX_SIZE) {
		comp_err(dev, "invalid configuration blob, size %u", new_data_size);
		return -EINVAL;
	}

	return tdfb_validate_config(dev, new_data, new_data_size);
}

static int tdfb_init_coef(struct processing_module *mod)
{
	struct tdfb_comp_data *cd = module_get_private_data(mod);
	struct sof_fir_coef_data *coef_data;
	struct sof_tdfb_config *config = cd->config;
	struct comp_dev *dev = mod->dev;
	int16_t *output_channel_mix_beam_off = NULL;
	int16_t *coefp;
	int size_sum = 0;
	int min_delta_idx; /* Index to beam angle with smallest delta vs. target */
	int min_delta; /* Smallest angle difference found in degrees */
	int num_filters;
	int target_az; /* Target azimuth angle in degrees */
	int delta; /* Target minus found angle in degrees absolute value */
	int idx;
	int i;

	/* Blob layout, bounds, size and stream channel relationships are
	 * pre-validated by tdfb_validator() / tdfb_validate_config().
	 */

	/* Skip filter coefficients */
	num_filters = config->num_filters * (config->num_angles + config->beam_off_defined);
	coefp = tdfb_filter_seek(config, num_filters);

	/* Get shortcuts to input and output configuration */
	cd->input_channel_select = coefp;
	coefp += config->num_filters;
	cd->output_channel_mix = coefp;
	coefp += config->num_filters;
	cd->output_stream_mix = coefp;
	coefp += config->num_filters;

	/* Check if there's beam-off configured, then get pointers to beam angles
	 * data and microphone locations.
	 */
	if (config->beam_off_defined) {
		output_channel_mix_beam_off = coefp;
		coefp += config->num_filters;
	}
	cd->filter_angles = (struct sof_tdfb_angle *)coefp;
	cd->mic_locations = (struct sof_tdfb_mic_location *)
		(&cd->filter_angles[config->num_angles]);

	/* Skip to requested coefficient set */
	min_delta = 360;
	min_delta_idx = 0;
	target_az = wrap_180(cd->az_value * config->angle_enum_mult + config->angle_enum_offs);

	for (i = 0; i < config->num_angles; i++) {
		delta = ABS(target_az - wrap_180(cd->filter_angles[i].azimuth));
		if (delta < min_delta) {
			min_delta = delta;
			min_delta_idx = i;
		}
	}

	idx = cd->filter_angles[min_delta_idx].filter_index;
	if (cd->beam_on) {
		comp_info(dev, "angle request %d, found %d, idx %d",
			  target_az, cd->filter_angles[min_delta_idx].azimuth, idx);
	} else if (config->beam_off_defined) {
		cd->output_channel_mix = output_channel_mix_beam_off;
		idx = config->num_filters * config->num_angles;
		comp_info(dev, "configure beam off");
	} else {
		comp_info(dev, "beam off is not defined, using filter %d, idx %d",
			  cd->filter_angles[min_delta_idx].azimuth, idx);
	}

	if (idx < 0 || idx + (int)config->num_filters > num_filters) {
		comp_err(dev, "invalid filter_index %d for angle %d",
			 idx, cd->filter_angles[min_delta_idx].azimuth);
		return -EINVAL;
	}

	/* Seek to proper filter for requested angle or beam off configuration */
	coefp = tdfb_filter_seek(config, idx);

	/* Initialize filter bank. FIR header bounds and length validity were
	 * already checked when the blob entered the component.
	 */
	for (i = 0; i < config->num_filters; i++) {
		coef_data = (struct sof_fir_coef_data *)coefp;
		size_sum += fir_delay_size(coef_data);
		fir_init_coef(&cd->fir[i], coef_data);
		coefp = coef_data->coef + coef_data->length;
	}

	return size_sum;
}

static void tdfb_init_delay(struct tdfb_comp_data *cd)
{
	int32_t *fir_delay = cd->fir_delay;
	int i;

	/* Initialize second phase to set delay lines pointers */
	for (i = 0; i < cd->config->num_filters; i++) {
		if (cd->fir[i].length > 0)
			fir_init_delay(&cd->fir[i], &fir_delay);
	}
}

static int tdfb_setup(struct processing_module *mod, int source_nch, int sink_nch,
		      enum sof_ipc_frame fmt)
{
	struct tdfb_comp_data *cd = module_get_private_data(mod);
	int delay_size;

	/* If beam on, restore processing function. If off, use for same source and
	 * sink format the efficient 1:1 copy, otherwise faster pass-through processing
	 * functions those copy selected source channels to selected sink channels.
	 */
	if (cd->beam_on) {
		set_func(mod, fmt);
	} else {
		if (source_nch == sink_nch)
			cd->tdfb_func = tdfb_pass_same_format;
		else
			set_pass_func(mod, fmt);
	}

	/* Set coefficients for each channel from coefficient blob */
	delay_size = tdfb_init_coef(mod);
	if (delay_size < 0)
		return delay_size; /* Contains error code */

	/* If all channels were set to bypass there's no need to
	 * allocate delay. Just return with success.
	 */
	if (!delay_size)
		return 0;

	if (delay_size > cd->fir_delay_size) {
		/* Free existing FIR channels data if it was allocated */
		tdfb_free_delaylines(mod);

		/* Allocate all FIR channels data in a big chunk and clear it */
		cd->fir_delay = mod_balloc(mod, delay_size);
		if (!cd->fir_delay) {
			comp_err(mod->dev, "delay allocation failed for size %d",
				 delay_size);
			return -ENOMEM;
		}

		memset(cd->fir_delay, 0, delay_size);
		cd->fir_delay_size = delay_size;
	}

	/* Assign delay line to all channel filters */
	tdfb_init_delay(cd);

	return 0;
}

/*
 * End of algorithm code. Next the standard component methods.
 */

static int tdfb_init(struct processing_module *mod)
{
	struct module_data *md = &mod->priv;
	struct comp_dev *dev = mod->dev;
	struct tdfb_comp_data *cd;
	int ret;
	int i;

	comp_info(dev, "entry");

	cd = mod_zalloc(mod, sizeof(*cd));
	if (!cd)
		return -ENOMEM;

	md->private = cd;

	/* Defaults for processing function pointer tdfb_func, fir_delay
	 * pointer, are NULL. Fir_delay_size is zero from rzalloc().
	 */

	/* Defaults for enum controls are zeros from rzalloc()
	 * az_value is zero, beam off is false, and update is false.
	 */

	/* Initialize IPC for direction of arrival estimate update */
	ret = tdfb_ipc_notification_init(mod);
	if (ret)
		goto err_free_cd;

	/* Handler for configuration data */
	cd->model_handler = mod_data_blob_handler_new(mod);
	if (!cd->model_handler) {
		comp_err(dev, "mod_data_blob_handler_new() failed.");
		ret = -ENOMEM;
		goto err;
	}

	/* Reject malformed blobs at IPC time so a bad run-time update cannot
	 * replace the working configuration. The channel-count checks inside
	 * the validator are skipped until tdfb_prepare() caches the stream
	 * channel counts.
	 */
	comp_data_blob_set_validator(cd->model_handler, tdfb_validator);

	for (i = 0; i < PLATFORM_MAX_CHANNELS; i++)
		fir_reset(&cd->fir[i]);

	 /* Allow different number  of channels in source and sink, in other
	  * aspects TDFB is simple component type.
	  */
	mod->verify_params_flags = BUFF_PARAMS_CHANNELS;
	return 0;

err:
	/* These are null if not used for IPC version */
	mod_free(mod, cd->ctrl_data);
	ipc_msg_free(cd->msg);
	mod_data_blob_handler_free(mod, cd->model_handler);

err_free_cd:
	mod_free(mod, cd);
	return ret;

}

static int tdfb_free(struct processing_module *mod)
{
	struct tdfb_comp_data *cd = module_get_private_data(mod);

	comp_dbg(mod->dev, "entry");

	ipc_msg_free(cd->msg);
	tdfb_free_delaylines(mod);
	mod_data_blob_handler_free(mod, cd->model_handler);
	tdfb_direction_free(mod);
	mod_free(mod, cd->ctrl_data);
	mod_free(mod, cd);
	return 0;
}

static int tdfb_get_config(struct processing_module *mod,
			   uint32_t param_id, uint32_t *data_offset_size,
			   uint8_t *fragment, size_t fragment_size)
{
	return tdfb_get_ipc_config(mod, param_id, data_offset_size, fragment, fragment_size);
}

static int tdfb_set_config(struct processing_module *mod, uint32_t param_id,
			   enum module_cfg_fragment_position pos, uint32_t data_offset_size,
			   const uint8_t *fragment, size_t fragment_size, uint8_t *response,
			   size_t response_size)
{
	return tdfb_set_ipc_config(mod, param_id, pos, data_offset_size, fragment,
				   fragment_size, response, response_size);
}

/*
 * copy and process stream data from source to sink buffers
 */

static int tdfb_process(struct processing_module *mod,
			struct input_stream_buffer *input_buffers, int num_input_buffers,
			struct output_stream_buffer *output_buffers, int num_output_buffers)
{
	struct comp_dev *dev = mod->dev;
	struct tdfb_comp_data *cd = module_get_private_data(mod);
	struct audio_stream *source = input_buffers[0].data;
	struct audio_stream *sink = output_buffers[0].data;
	int frame_count = input_buffers[0].size;
	int ret;

	comp_dbg(dev, "entry");

	/* Check for changed configuration. The IPC-time validator installed
	 * in tdfb_init() has already structurally validated the blob, so
	 * only NULL needs to be guarded here.
	 */
	if (comp_is_new_data_blob_available(cd->model_handler)) {
		cd->config = comp_get_data_blob(cd->model_handler, &cd->config_size, NULL);
		if (!cd->config)
			return -EINVAL;
		ret = tdfb_setup(mod, audio_stream_get_channels(source),
				 audio_stream_get_channels(sink),
				 audio_stream_get_frm_fmt(source));
		if (ret < 0) {
			comp_err(dev, "failed FIR setup");
			return ret;
		}
	}

	/* Handle enum controls */
	if (cd->update) {
		cd->update = false;
		ret = tdfb_setup(mod, audio_stream_get_channels(source),
				 audio_stream_get_channels(sink),
				 audio_stream_get_frm_fmt(source));
		if (ret < 0) {
			comp_err(dev, "failed FIR setup");
			return ret;
		}
	}

	/*
	 * Process only even number of frames with the FIR function. The
	 * optimized filter function loads the successive input samples from
	 * internal delay line with a 64 bit load operation.
	 */
	frame_count = MIN(frame_count, cd->max_frames) & ~0x1;
	if (frame_count) {
		cd->tdfb_func(cd, input_buffers, output_buffers, frame_count);
		module_update_buffer_position(input_buffers, output_buffers, frame_count);

		/* Update sound direction estimate */
		tdfb_direction_estimate(cd, frame_count, audio_stream_get_channels(source));
		comp_dbg(dev, "tdfb_dint %u %d %d %d", cd->direction.trigger, cd->direction.level,
			 (int32_t)(cd->direction.level_ambient >> 32), cd->direction.az_slow);

		if (cd->direction_updates && cd->direction_change) {
			tdfb_send_ipc_notification(mod);
			cd->direction_change = false;
			comp_dbg(dev, "tdfb_dupd %d %d",
				 cd->az_value_estimate, cd->direction.az_slow);
		}
	}

	return 0;
}

static void tdfb_set_alignment(struct audio_stream *source)
{
	const uint32_t byte_align = SOF_FRAME_BYTE_ALIGN;
	const uint32_t frame_align_req = 2; /* Process multiples of 2 frames */

	audio_stream_set_align(byte_align, frame_align_req, source);
}

static int tdfb_prepare(struct processing_module *mod,
			struct sof_source **sources, int num_of_sources,
			struct sof_sink **sinks, int num_of_sinks)
{
	struct tdfb_comp_data *cd = module_get_private_data(mod);
	struct comp_buffer *sourceb, *sinkb;
	struct comp_dev *dev = mod->dev;
	enum sof_ipc_frame frame_fmt;
	int source_channels;
	int sink_channels;
	int rate;
	int ret;

	comp_info(dev, "entry");

	/* Find source and sink buffers */
	sourceb = comp_dev_get_first_data_producer(dev);
	sinkb = comp_dev_get_first_data_consumer(dev);
	if (!sourceb || !sinkb) {
		comp_err(dev, "no source or sink buffer");
		return -ENOTCONN;
	}

	ret = tdfb_params(mod);
	if (ret) {
		comp_err(dev, "Failed tdfb_params()");
		return ret;
	}

	tdfb_set_alignment(&sourceb->stream);

	frame_fmt = audio_stream_get_frm_fmt(&sourceb->stream);
	source_channels = audio_stream_get_channels(&sourceb->stream);
	sink_channels = audio_stream_get_channels(&sinkb->stream);
	rate = audio_stream_get_rate(&sourceb->stream);

	/* Cache stream channel counts for the blob validator before any
	 * validate_config() call runs.
	 */
	cd->source_channels = source_channels;
	cd->sink_channels = sink_channels;

	/* Initialize filter */
	cd->config = comp_get_data_blob(cd->model_handler, &cd->config_size, NULL);
	if (!cd->config) {
		ret = -EINVAL;
		goto out;
	}

	/* Re-validate now that the stream channel counts are cached: the
	 * IPC-time validator skips the channel checks before prepare, so the
	 * blob must be walked again here before tdfb_setup() uses it.
	 * Discard a malformed blob so the runtime path (tdfb_process())
	 * cannot dereference it.
	 */
	ret = tdfb_validator(dev, cd->config, (uint32_t)cd->config_size);
	if (ret < 0) {
		cd->config = NULL;
		goto out;
	}

	ret = tdfb_setup(mod, source_channels, sink_channels, frame_fmt);
	if (ret < 0) {
		comp_err(dev, "error: tdfb_setup failed.");
		goto out;
	}

	/* Clear in/out buffers */
	memset(cd->in, 0, TDFB_IN_BUF_LENGTH * sizeof(int32_t));
	memset(cd->out, 0, TDFB_IN_BUF_LENGTH * sizeof(int32_t));

	ret = set_func(mod, frame_fmt);
	if (ret)
		goto out;

	/* The max. amount of processing need to be limited for sound direction
	 * processing. Max frames is used in tdfb_direction_init() and copy().
	 */
	cd->max_frames = Q_MULTSR_16X16((int32_t)dev->frames, TDFB_MAX_FRAMES_MULT_Q14, 0, 14, 0);
	comp_dbg(dev, "dev_frames = %d, max_frames = %d", dev->frames, cd->max_frames);

	/* Initialize tracking */
	ret = tdfb_direction_init(mod, rate, source_channels);
	if (ret < 0)
		goto out;
	comp_info(dev, "max_lag = %d, xcorr_size = %zu",
		  cd->direction.max_lag, cd->direction.d_size);
	comp_info(dev, "line_array = %d, a_step = %d, a_offs = %d",
		  (int)cd->direction.line_array, cd->config->angle_enum_mult,
		  cd->config->angle_enum_offs);

	return 0;

out:
	comp_set_state(dev, COMP_TRIGGER_RESET);
	return ret;
}

static int tdfb_reset(struct processing_module *mod)
{
	struct tdfb_comp_data *cd = module_get_private_data(mod);
	int i;

	comp_dbg(mod->dev, "entry");

	cd->source_channels = 0;
	cd->sink_channels = 0;

	tdfb_free_delaylines(mod);

	cd->tdfb_func = NULL;
	for (i = 0; i < PLATFORM_MAX_CHANNELS; i++)
		fir_reset(&cd->fir[i]);

	/* Clear in/out buffers */
	memset(cd->in, 0, TDFB_IN_BUF_LENGTH * sizeof(int32_t));
	memset(cd->out, 0, TDFB_IN_BUF_LENGTH * sizeof(int32_t));

	return 0;
}

static const struct module_interface tdfb_interface = {
	.init = tdfb_init,
	.free = tdfb_free,
	.set_configuration = tdfb_set_config,
	.get_configuration = tdfb_get_config,
	.process_audio_stream = tdfb_process,
	.prepare = tdfb_prepare,
	.reset = tdfb_reset,
};

#if CONFIG_COMP_TDFB_MODULE
/* modular: llext dynamic link */

#include <module/module/api_ver.h>
#include <module/module/llext.h>
#include <rimage/sof/user/manifest.h>

static const struct sof_man_module_manifest mod_manifest __section(".module") __used =
	SOF_LLEXT_MODULE_MANIFEST("TDFB", &tdfb_interface, 1, SOF_REG_UUID(tdfb), 40);

SOF_LLEXT_BUILDINFO;

#else

DECLARE_TR_CTX(tdfb_tr, SOF_UUID(tdfb_uuid), LOG_LEVEL_INFO);
DECLARE_MODULE_ADAPTER(tdfb_interface, tdfb_uuid, tdfb_tr);
SOF_MODULE_INIT(tdfb, sys_comp_module_tdfb_interface_init);

#endif
