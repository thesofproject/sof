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
#include <sof/lib/memory.h>
#include <sof/lib/uuid.h>
#include <sof/math/fir_generic.h>
#include <sof/math/fir_hifi2ep.h>
#include <sof/math/fir_hifi3.h>
#include <sof/platform.h>
#include <sof/trace/trace.h>
#include <ipc/control.h>
#include <ipc/stream.h>
#include <ipc/topology.h>
#include <rtos/alloc.h>
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

/* dd511749-d9fa-455c-b3a7-13585693f1af */
DECLARE_SOF_RT_UUID("tdfb", tdfb_uuid,  0xdd511749, 0xd9fa, 0x455c, 0xb3, 0xa7,
		    0x13, 0x58, 0x56, 0x93, 0xf1, 0xaf);

DECLARE_TR_CTX(tdfb_tr, SOF_UUID(tdfb_uuid), LOG_LEVEL_INFO);

static inline int set_func(struct processing_module *mod, enum sof_ipc_frame fmt)
{
	struct tdfb_comp_data *cd = module_get_private_data(mod);

	switch (fmt) {
#if CONFIG_FORMAT_S16LE
	case SOF_IPC_FRAME_S16_LE:
		comp_dbg(mod->dev, "set_func(), SOF_IPC_FRAME_S16_LE");
		cd->tdfb_func = tdfb_fir_s16;
		break;
#endif /* CONFIG_FORMAT_S16LE */
#if CONFIG_FORMAT_S24LE
	case SOF_IPC_FRAME_S24_4LE:
		comp_dbg(mod->dev, "set_func(), SOF_IPC_FRAME_S24_4LE");
		cd->tdfb_func = tdfb_fir_s24;
		break;
#endif /* CONFIG_FORMAT_S24LE */
#if CONFIG_FORMAT_S32LE
	case SOF_IPC_FRAME_S32_LE:
		comp_dbg(mod->dev, "set_func(), SOF_IPC_FRAME_S32_LE");
		cd->tdfb_func = tdfb_fir_s32;
		break;
#endif /* CONFIG_FORMAT_S32LE */
	default:
		comp_err(mod->dev, "set_func(), invalid frame_fmt");
		return -EINVAL;
	}
	return 0;
}

/*
 * Control code functions next. The processing is in fir_ C modules.
 */

static void tdfb_free_delaylines(struct tdfb_comp_data *cd)
{
	struct fir_state_32x16 *fir = cd->fir;
	int i = 0;

	/* Free the common buffer for all EQs and point then
	 * each FIR channel delay line to NULL.
	 */
	rfree(cd->fir_delay);
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

static int tdfb_init_coef(struct processing_module *mod, int source_nch,
			  int sink_nch)
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
	int max_ch;
	int num_filters;
	int target_az; /* Target azimuth angle in degrees */
	int delta; /* Target minus found angle in degrees absolute value */
	int idx;
	int s;
	int i;

	/* Sanity checks */
	if (config->num_output_channels > PLATFORM_MAX_CHANNELS ||
	    !config->num_output_channels) {
		comp_err(dev, "tdfb_init_coef(), invalid num_output_channels %d",
			 config->num_output_channels);
		return -EINVAL;
	}

	if (config->num_output_channels != sink_nch) {
		comp_err(dev, "tdfb_init_coef(), stream output channels count %d does not match configuration %d",
			 sink_nch, config->num_output_channels);
		return -EINVAL;
	}

	if (config->num_filters > SOF_TDFB_FIR_MAX_COUNT) {
		comp_err(dev, "tdfb_init_coef(), invalid num_filters %d",
			 config->num_filters);
		return -EINVAL;
	}

	if (config->num_angles > SOF_TDFB_MAX_ANGLES) {
		comp_err(dev, "tdfb_init_coef(), invalid num_angles %d",
			 config->num_angles);
		return -EINVAL;
	}

	if (config->beam_off_defined > 1) {
		comp_err(dev, "tdfb_init_coef(), invalid beam_off_defined %d",
			 config->beam_off_defined);
		return -EINVAL;
	}

	if (config->num_mic_locations > SOF_TDFB_MAX_MICROPHONES) {
		comp_err(dev, "tdfb_init_coef(), invalid num_mic_locations %d",
			 config->num_mic_locations);
		return -EINVAL;
	}

	/* In SOF v1.6 - 1.8 based beamformer topologies the multiple angles, mic locations,
	 * and beam on/off switch were not defined. Return error if such configuration is seen.
	 * A most basic blob has num_angles equals 1. Mic locations data is optional.
	 */
	if (config->num_angles == 0 && config->num_mic_locations == 0) {
		comp_err(dev, "tdfb_init_coef(), ABI version less than 3.19.1 is not supported.");
		return -EINVAL;
	}

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

	/* Check if there's beam-off configured, then get pointers to beam angles data
	 * and microphone locations. Finally check that size matches.
	 */
	if (config->beam_off_defined) {
		output_channel_mix_beam_off = coefp;
		coefp += config->num_filters;
	}
	cd->filter_angles = (struct sof_tdfb_angle *)coefp;
	cd->mic_locations = (struct sof_tdfb_mic_location *)
		(&cd->filter_angles[config->num_angles]);
	if ((uint8_t *)&cd->mic_locations[config->num_mic_locations] !=
	    (uint8_t *)config + config->size) {
		comp_err(dev, "tdfb_init_coef(), invalid config size");
		return -EINVAL;
	}

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
		comp_info(dev, "tdfb_init_coef(), angle request %d, found %d, idx %d",
			  target_az, cd->filter_angles[min_delta_idx].azimuth, idx);
	} else if (config->beam_off_defined) {
		cd->output_channel_mix = output_channel_mix_beam_off;
		idx = config->num_filters * config->num_angles;
		comp_info(dev, "tdfb_init_coef(), configure beam off");
	} else {
		comp_info(dev, "tdfb_init_coef(), beam off is not defined, using filter %d, idx %d",
			  cd->filter_angles[min_delta_idx].azimuth, idx);
	}

	/* Seek to proper filter for requested angle or beam off configuration */
	coefp = tdfb_filter_seek(config, idx);

	/* Initialize filter bank */
	for (i = 0; i < config->num_filters; i++) {
		/* Get delay line size */
		coef_data = (struct sof_fir_coef_data *)coefp;
		s = fir_delay_size(coef_data);
		if (s > 0) {
			size_sum += s;
		} else {
			comp_err(dev, "tdfb_init_coef(), FIR length %d is invalid",
				 coef_data->length);
			return -EINVAL;
		}

		/* Initialize coefficients for FIR filter and find next
		 * filter.
		 */
		fir_init_coef(&cd->fir[i], coef_data);
		coefp = coef_data->coef + coef_data->length;
	}

	/* Find max used input channel */
	max_ch = 0;
	for (i = 0; i < config->num_filters; i++) {
		if (cd->input_channel_select[i] > max_ch)
			max_ch = cd->input_channel_select[i];
	}

	/* The stream must contain at least the number of channels that is
	 * used for filters input.
	 */
	if (max_ch + 1 > source_nch) {
		comp_err(dev, "tdfb_init_coef(), stream input channels count %d is not sufficient for configuration %d",
			 source_nch, max_ch + 1);
		return -EINVAL;
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

static int tdfb_setup(struct processing_module *mod, int source_nch, int sink_nch)
{
	struct tdfb_comp_data *cd = module_get_private_data(mod);
	int delay_size;

	/* Set coefficients for each channel from coefficient blob */
	delay_size = tdfb_init_coef(mod, source_nch, sink_nch);
	if (delay_size < 0)
		return delay_size; /* Contains error code */

	/* If all channels were set to bypass there's no need to
	 * allocate delay. Just return with success.
	 */
	if (!delay_size)
		return 0;

	if (delay_size > cd->fir_delay_size) {
		/* Free existing FIR channels data if it was allocated */
		tdfb_free_delaylines(cd);

		/* Allocate all FIR channels data in a big chunk and clear it */
		cd->fir_delay = rballoc(0, SOF_MEM_CAPS_RAM, delay_size);
		if (!cd->fir_delay) {
			comp_err(mod->dev, "tdfb_setup(), delay allocation failed for size %d",
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
	struct module_config *cfg = &md->cfg;
	struct tdfb_comp_data *cd;
	size_t bs = cfg->size;
	int ret;
	int i;

	comp_info(dev, "tdfb_init()");

	/* Check first that configuration blob size is sane */
	if (bs > SOF_TDFB_MAX_SIZE) {
		comp_err(dev, "tdfb_init() error: configuration blob size = %zu > %d",
			 bs, SOF_TDFB_MAX_SIZE);
		return -EINVAL;
	}

	cd = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(*cd));
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
	cd->model_handler = comp_data_blob_handler_new(dev);
	if (!cd->model_handler) {
		comp_err(dev, "tdfb_init(): comp_data_blob_handler_new() failed.");
		ret = -ENOMEM;
		goto err;
	}

	/* Get configuration data and reset FIR filters */
	ret = comp_init_data_blob(cd->model_handler, bs, cfg->data);
	if (ret < 0) {
		comp_err(dev, "tdfb_init(): comp_init_data_blob() failed.");
		goto err;
	}

	for (i = 0; i < PLATFORM_MAX_CHANNELS; i++)
		fir_reset(&cd->fir[i]);

	 /* Allow different number  of channels in source and sink, in other
	  * aspects TDFB is simple component type.
	  */
	mod->verify_params_flags = BUFF_PARAMS_CHANNELS;
	return 0;

err:
	/* These are null if not used for IPC version */
	rfree(cd->ctrl_data);
	ipc_msg_free(cd->msg);

err_free_cd:
	rfree(cd);
	return ret;

}

static int tdfb_free(struct processing_module *mod)
{
	struct tdfb_comp_data *cd = module_get_private_data(mod);

	comp_info(mod->dev, "tdfb_free()");

	ipc_msg_free(cd->msg);
	tdfb_free_delaylines(cd);
	comp_data_blob_handler_free(cd->model_handler);
	tdfb_direction_free(cd);
	rfree(cd->ctrl_data);
	rfree(cd);
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

	comp_dbg(dev, "tdfb_process()");

	/* Check for changed configuration */
	if (comp_is_new_data_blob_available(cd->model_handler)) {
		cd->config = comp_get_data_blob(cd->model_handler, NULL, NULL);
		ret = tdfb_setup(mod, audio_stream_get_channels(source),
				 audio_stream_get_channels(sink));
		if (ret < 0) {
			comp_err(dev, "tdfb_process(), failed FIR setup");
			return ret;
		}
	}

	/* Handle enum controls */
	if (cd->update) {
		cd->update = false;
		ret = tdfb_setup(mod, audio_stream_get_channels(source),
				 audio_stream_get_channels(sink));
		if (ret < 0) {
			comp_err(dev, "tdfb_process(), failed FIR setup");
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

static void tdfb_set_alignment(struct audio_stream *source,
			       struct audio_stream *sink)
{
	const uint32_t byte_align = 1;
	const uint32_t frame_align_req = 2; /* Process multiples of 2 frames */

	audio_stream_set_align(byte_align, frame_align_req, source);
	audio_stream_set_align(byte_align, frame_align_req, sink);
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

	comp_info(dev, "tdfb_prepare()");

	tdfb_params(mod);

	/* Find source and sink buffers */
	sourceb = list_first_item(&dev->bsource_list, struct comp_buffer, sink_list);
	sinkb = list_first_item(&dev->bsink_list, struct comp_buffer, source_list);
	tdfb_set_alignment(&sourceb->stream, &sinkb->stream);

	frame_fmt = audio_stream_get_frm_fmt(&sourceb->stream);
	source_channels = audio_stream_get_channels(&sourceb->stream);
	sink_channels = audio_stream_get_channels(&sinkb->stream);
	rate = audio_stream_get_rate(&sourceb->stream);

	/* Initialize filter */
	cd->config = comp_get_data_blob(cd->model_handler, NULL, NULL);
	if (!cd->config) {
		ret = -EINVAL;
		goto out;
	}

	ret = tdfb_setup(mod, source_channels, sink_channels);
	if (ret < 0) {
		comp_err(dev, "tdfb_prepare() error: tdfb_setup failed.");
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
	ret = tdfb_direction_init(cd, rate, source_channels);
	if (!ret) {
		comp_info(dev, "max_lag = %d, xcorr_size = %zu",
			  cd->direction.max_lag, cd->direction.d_size);
		comp_info(dev, "line_array = %d, a_step = %d, a_offs = %d",
			  (int)cd->direction.line_array, cd->config->angle_enum_mult,
			  cd->config->angle_enum_offs);
	}

out:
	if (ret < 0)
		comp_set_state(dev, COMP_TRIGGER_RESET);

	return ret;
}

static int tdfb_reset(struct processing_module *mod)
{
	struct tdfb_comp_data *cd = module_get_private_data(mod);
	int i;

	comp_info(mod->dev, "tdfb_reset()");

	tdfb_free_delaylines(cd);

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

DECLARE_MODULE_ADAPTER(tdfb_interface, tdfb_uuid, tdfb_tr);
SOF_MODULE_INIT(tdfb, sys_comp_module_tdfb_interface_init);
