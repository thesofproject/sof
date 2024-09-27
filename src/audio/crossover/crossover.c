// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Google LLC. All rights reserved.
//
// Author: Sebastiano Carlucci <scarlucci@google.com>

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/audio/data_blob.h>
#include <sof/audio/format.h>
#include <sof/audio/pipeline.h>
#include <sof/audio/ipc-config.h>
#include <module/crossover/crossover_common.h>
#include <sof/common.h>
#include <rtos/panic.h>
#include <sof/ipc/msg.h>
#include <rtos/alloc.h>
#include <rtos/init.h>
#include <sof/lib/memory.h>
#include <sof/lib/uuid.h>
#include <sof/math/iir_df2t.h>
#include <sof/list.h>
#include <sof/platform.h>
#include <rtos/string.h>
#include <sof/trace/trace.h>
#include <sof/ut.h>
#include <ipc/control.h>
#include <ipc/stream.h>
#include <ipc/topology.h>
#include <user/trace.h>
#include <user/eq.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <limits.h>

#include "crossover_user.h"
#include "crossover.h"

LOG_MODULE_REGISTER(crossover, CONFIG_SOF_LOG_LEVEL);

SOF_DEFINE_REG_UUID(crossover);

DECLARE_TR_CTX(crossover_tr, SOF_UUID(crossover_uuid), LOG_LEVEL_INFO);

/**
 * \brief Reset the state (coefficients and delay) of the crossover filter
 *	  across all channels
 */
static void crossover_reset_state(struct comp_data *cd)
{
	int i;

	for (i = 0; i < PLATFORM_MAX_CHANNELS; i++)
		crossover_reset_state_ch(&cd->state[i]);
}

/**
 * \brief Returns the index i such that assign_sink[i] = pipe_id.
 *
 * The assign_sink array in the configuration maps to the pipeline ids.
 *
 * \return the position at which pipe_id is found in config->assign_sink.
 *	   -EINVAL if not found.
 */
int crossover_get_stream_index(struct processing_module *mod,
			       struct sof_crossover_config *config, uint32_t pipe_id)
{
	int i;
	uint32_t *assign_sink = config->assign_sink;

	for (i = 0; i < config->num_sinks; i++)
		if (assign_sink[i] == pipe_id)
			return i;

	comp_err(mod->dev, "crossover_get_stream_index() error: couldn't find any assignment for connected pipeline %u",
		 pipe_id);

	return -EINVAL;
}

/*
 * \brief Aligns the sinks with their respective assignments
 *	  in the configuration.
 *
 * Refer to sof/src/include/sof/crossover.h for more information on assigning
 * sinks to an output.
 *
 * \param[out] sinks array where the sinks are assigned
 * \return number of sinks assigned. This number should be equal to
 *	   config->num_sinks if no errors were found.
 */
static int crossover_assign_sinks(struct processing_module *mod,
				  struct output_stream_buffer *output_buffers,
				  struct output_stream_buffer **assigned_obufs,
				  bool *enabled)
{
	struct comp_data *cd = module_get_private_data(mod);
	struct sof_crossover_config *config = cd->config;
	struct comp_dev *dev = mod->dev;
	struct comp_buffer *sink;
	int num_sinks = 0;
	int i;
	int j = 0;

	comp_dev_for_each_consumer(dev, sink) {
		unsigned int sink_id, state;

		sink_id = crossover_get_sink_id(cd, buffer_pipeline_id(sink), j);
		state = comp_buffer_get_sink_state(sink);
		if (state != dev->state) {
			j++;
			continue;
		}

		/* If no config is set, then assign the sinks in order */
		if (!config) {
			assigned_obufs[num_sinks++] = &output_buffers[j];
			enabled[j++] = true;
			continue;
		}

		i = crossover_get_stream_index(mod, config, sink_id);

		/* If this sink buffer is not assigned
		 * in the configuration.
		 */
		if (i < 0) {
			comp_err(dev,
				 "crossover_assign_sinks(), could not find sink %d in config",
				 sink_id);
			break;
		}

		if (assigned_obufs[i]) {
			comp_err(dev,
				 "crossover_assign_sinks(), multiple sinks with id %d are assigned",
				 sink_id);
			break;
		}

		assigned_obufs[i] = &output_buffers[j];
		enabled[j++] = true;
		num_sinks++;
	}

	return num_sinks;
}

/**
 * \brief Sets the state of a single LR4 filter.
 *
 * An LR4 filter is built by cascading two biquads in series.
 *
 * \param coef struct containing the coefficients of a butterworth
 *	       high/low pass filter.
 * \param[out] lr4 initialized struct
 */
static int crossover_init_coef_lr4(struct sof_eq_iir_biquad *coef,
				   struct iir_state_df2t *lr4)
{
	int ret;

	/* Only one set of coefficients is stored in config for both biquads
	 * in series due to identity. To maintain the structure of
	 * iir_state_df2t, it requires two copies of coefficients in a row.
	 */
	lr4->coef = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM,
			    sizeof(struct sof_eq_iir_biquad) * 2);
	if (!lr4->coef)
		return -ENOMEM;

	/* coefficients of the first biquad */
	ret = memcpy_s(lr4->coef, sizeof(struct sof_eq_iir_biquad),
		       coef, sizeof(struct sof_eq_iir_biquad));
	assert(!ret);

	/* coefficients of the second biquad */
	ret = memcpy_s(lr4->coef + SOF_EQ_IIR_NBIQUAD,
		       sizeof(struct sof_eq_iir_biquad),
		       coef, sizeof(struct sof_eq_iir_biquad));
	assert(!ret);

	/* LR4 filters are two 2nd order filters, so only need 4 delay slots
	 * delay[0..1] -> state for first biquad
	 * delay[2..3] -> state for second biquad
	 */
	lr4->delay = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM,
			     sizeof(uint64_t) * CROSSOVER_NUM_DELAYS_LR4);
	if (!lr4->delay)
		return -ENOMEM;

	lr4->biquads = 2;
	lr4->biquads_in_series = 2;

	return 0;
}

/**
 * \brief Initializes the crossover coefficients for one channel
 */
int crossover_init_coef_ch(struct sof_eq_iir_biquad *coef,
			   struct crossover_state *ch_state,
			   int32_t num_sinks)
{
	int32_t i;
	int32_t j = 0;
	int32_t num_lr4s = num_sinks == CROSSOVER_2WAY_NUM_SINKS ? 1 : 3;
	int err;

	for (i = 0; i < num_lr4s; i++) {
		/* Get the low pass coefficients */
		err = crossover_init_coef_lr4(&coef[j],
					      &ch_state->lowpass[i]);
		if (err < 0)
			return -EINVAL;
		/* Get the high pass coefficients */
		err = crossover_init_coef_lr4(&coef[j + 1],
					      &ch_state->highpass[i]);
		if (err < 0)
			return -EINVAL;
		j += 2;
	}

	return 0;
}

/**
 * \brief Initializes the coefficients of the crossover filter
 *	  and assign them to the first nch channels.
 *
 * \param nch number of channels in the audio stream.
 */
static int crossover_init_coef(struct processing_module *mod, int nch)
{
	struct comp_data *cd = module_get_private_data(mod);
	struct sof_eq_iir_biquad *crossover;
	struct sof_crossover_config *config = cd->config;
	int ch, err;

	if (!config) {
		comp_err(mod->dev, "crossover_init_coef(), no config is set");
		return -EINVAL;
	}

	/* Sanity checks */
	if (nch > PLATFORM_MAX_CHANNELS) {
		comp_err(mod->dev, "crossover_init_coef(), invalid channels count (%i)", nch);
		return -EINVAL;
	}

	comp_info(mod->dev, "crossover_init_coef(), initializing %i-way crossover",
		  config->num_sinks);

	/* Collect the coef array and assign it to every channel */
	crossover = config->coef;
	for (ch = 0; ch < nch; ch++) {
		err = crossover_init_coef_ch(crossover, &cd->state[ch],
					     config->num_sinks);
		/* Free all previously allocated blocks in case of an error */
		if (err < 0) {
			comp_err(mod->dev, "crossover_init_coef(), could not assign coefficients to ch %d",
				 ch);
			crossover_reset_state(cd);
			return err;
		}
	}

	return 0;
}

/**
 * \brief Setup the state, coefficients and processing functions for crossover.
 */
static int crossover_setup(struct processing_module *mod, int nch)
{
	struct comp_data *cd = module_get_private_data(mod);
	int ret = 0;

	/* Reset any previous state */
	crossover_reset_state(cd);

	/* Assign LR4 coefficients from config */
	ret = crossover_init_coef(mod, nch);

	return ret;
}

/**
 * \brief Creates a Crossover Filter component.
 * \return Pointer to Crossover Filter component device.
 */
static int crossover_init(struct processing_module *mod)
{
	struct module_data *md = &mod->priv;
	struct comp_dev *dev = mod->dev;
	struct comp_data *cd;
	const struct module_config *ipc_crossover = &md->cfg;
	size_t bs = ipc_crossover->size;
	int ret;

	comp_info(dev, "crossover_init()");

	/* Check that the coefficients blob size is sane */
	if (bs > SOF_CROSSOVER_MAX_SIZE) {
		comp_err(dev, "crossover_init(), blob size (%d) exceeds maximum allowed size (%i)",
			 bs, SOF_CROSSOVER_MAX_SIZE);
		return -ENOMEM;
	}

	cd = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(*cd));
	if (!cd)
		return -ENOMEM;

	md->private = cd;

	/* Handler for configuration data */
	cd->model_handler = comp_data_blob_handler_new(dev);
	if (!cd->model_handler) {
		comp_err(dev, "crossover_init(): comp_data_blob_handler_new() failed.");
		ret = -ENOMEM;
		goto cd_fail;
	}

	/* Get configuration data and reset Crossover state */
	ret = comp_init_data_blob(cd->model_handler, bs, ipc_crossover->data);
	if (ret < 0) {
		comp_err(dev, "crossover_init(): comp_init_data_blob() failed.");
		goto cd_fail;
	}

	ret = crossover_output_pin_init(mod);
	if (ret < 0) {
		comp_err(dev, "crossover_init(): crossover_init_output_pins() failed.");
		goto cd_fail;
	}

	crossover_reset_state(cd);
	return 0;

cd_fail:
	comp_data_blob_handler_free(cd->model_handler);
	rfree(cd);
	return ret;
}

/**
 * \brief Frees Crossover Filter component.
 */
static int crossover_free(struct processing_module *mod)
{
	struct comp_data *cd = module_get_private_data(mod);

	comp_info(mod->dev, "crossover_free()");

	comp_data_blob_handler_free(cd->model_handler);

	crossover_reset_state(cd);

	rfree(cd);
	return 0;
}

/**
 * \brief Verifies that the config is formatted correctly.
 *
 * The function can only be called after the buffers have been initialized.
 */
static int crossover_validate_config(struct processing_module *mod,
				     struct sof_crossover_config *config)
{
	struct comp_dev *dev = mod->dev;
	uint32_t size = config->size;
	int32_t num_assigned_sinks;

	if (size > SOF_CROSSOVER_MAX_SIZE || !size) {
		comp_err(dev, "crossover_validate_config(), size %d is invalid", size);
		return -EINVAL;
	}

	if (config->num_sinks > SOF_CROSSOVER_MAX_STREAMS ||
	    config->num_sinks < 2) {
		comp_err(dev, "crossover_validate_config(), invalid num_sinks %i, expected number between 2 and %i",
			 config->num_sinks, SOF_CROSSOVER_MAX_STREAMS);
		return -EINVAL;
	}

	/* Align the crossover's sinks, to their respective configuration in
	 * the config.
	 */
	num_assigned_sinks = crossover_check_sink_assign(mod, config);

	/* Config is invalid if the number of assigned sinks
	 * is different than what is configured.
	 */
	if (num_assigned_sinks != config->num_sinks) {
		comp_err(dev, "crossover_validate_config(), number of assigned sinks %d, expected from config %d",
			 num_assigned_sinks, config->num_sinks);
		return -EINVAL;
	}

	return 0;
}

/* used to pass standard and bespoke commands (with data) to component */
static int crossover_set_config(struct processing_module *mod, uint32_t config_id,
				enum module_cfg_fragment_position pos, uint32_t data_offset_size,
				const uint8_t *fragment, size_t fragment_size, uint8_t *response,
				size_t response_size)
{
	struct comp_data *cd = module_get_private_data(mod);
	int ret;

	comp_info(mod->dev, "crossover_set_config()");

	ret = crossover_check_config(mod, fragment);
	if (ret < 0)
		return ret;

	return comp_data_blob_set(cd->model_handler, pos, data_offset_size, fragment,
				  fragment_size);
}

static int crossover_get_config(struct processing_module *mod,
				uint32_t config_id, uint32_t *data_offset_size,
				uint8_t *fragment, size_t fragment_size)
{
	struct comp_data *cd = module_get_private_data(mod);
	struct sof_ipc_ctrl_data *cdata = (struct sof_ipc_ctrl_data *)fragment;
	int ret;

	comp_info(mod->dev, "crossover_get_config()");

	ret = crossover_check_config(mod, fragment);
	if (ret < 0)
		return ret;

	return comp_data_blob_get_cmd(cd->model_handler, cdata, fragment_size);
}

/**
 * \brief Copies and processes stream data.
 * \param[in,out] dev Crossover Filter base component device.
 * \return Error code.
 */
static int crossover_process_audio_stream(struct processing_module *mod,
					  struct input_stream_buffer *input_buffers,
					  int num_input_buffers,
					  struct output_stream_buffer *output_buffers,
					  int num_output_buffers)
{
	struct output_stream_buffer *assigned_obufs[SOF_CROSSOVER_MAX_STREAMS] = { NULL };
	bool enabled_buffers[PLATFORM_MAX_STREAMS] = { false };
	struct comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	struct audio_stream *source = input_buffers[0].data;
	uint32_t num_sinks;
	uint32_t num_assigned_sinks = 0;
	/* The frames count to process from module adapter applies for source buffer and
	 * all sink buffers. The function module_single_source_setup() checks the frames
	 * avail/free from all source and sink combinations.
	 */
	uint32_t frames = input_buffers[0].size;
	uint32_t frame_bytes = audio_stream_frame_bytes(input_buffers[0].data);
	uint32_t processed_bytes;
	int ret;
	int i;

	comp_dbg(dev, "crossover_process_audio_stream()");

	/* Check for changed configuration */
	if (comp_is_new_data_blob_available(cd->model_handler)) {
		cd->config = comp_get_data_blob(cd->model_handler, NULL, NULL);
		ret = crossover_setup(mod, audio_stream_get_channels(source));
		if (ret < 0) {
			comp_err(dev, "crossover_process_audio_stream(), failed Crossover setup");
			return ret;
		}
	}

	/* Use the assign_sink array from the config to route
	 * the output to the corresponding sinks.
	 * It is possible for an assigned sink to be in a different
	 * state than the component. Therefore not all sinks are guaranteed
	 * to be assigned: sink[i] can be NULL, 0 <= i <= config->num_sinks
	 */
	num_assigned_sinks = crossover_assign_sinks(mod, output_buffers, assigned_obufs,
						    enabled_buffers);
	if (cd->config && num_assigned_sinks != cd->config->num_sinks)
		comp_dbg(dev, "crossover_copy(), number of assigned sinks (%i) does not match number of sinks in config (%i).",
			 num_assigned_sinks, cd->config->num_sinks);

	/* If no config is set then assign the number of sinks to the number
	 * of sinks that were assigned.
	 */
	if (cd->config)
		num_sinks = cd->config->num_sinks;
	else
		num_sinks = num_assigned_sinks;

	/* Process crossover */
	if (!frames)
		return -ENODATA;

	cd->crossover_process(cd, input_buffers, assigned_obufs, num_sinks, frames);

	processed_bytes = frames * frame_bytes;
	mod->input_buffers[0].consumed = processed_bytes;
	for (i = 0; i < num_output_buffers; i++) {
		if (enabled_buffers[i])
			mod->output_buffers[i].size = processed_bytes;
	}

	return 0;
}

/**
 * \brief Prepares Crossover Filter component for processing.
 * \param[in,out] dev Crossover Filter base component device.
 * \return Error code.
 */
static int crossover_prepare(struct processing_module *mod,
			     struct sof_source **sources, int num_of_sources,
			     struct sof_sink **sinks, int num_of_sinks)
{
	struct comp_data *cd =  module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	struct comp_buffer *source, *sink;
	int channels;
	int ret = 0;

	comp_info(dev, "crossover_prepare()");

	crossover_params(mod);

	/* Crossover has a variable number of sinks */
	mod->max_sinks = SOF_CROSSOVER_MAX_STREAMS;
	source = comp_dev_get_first_data_producer(dev);

	/* Get source data format */
	cd->source_format = audio_stream_get_frm_fmt(&source->stream);
	channels = audio_stream_get_channels(&source->stream);

	/* Validate frame format and buffer size of sinks */
	comp_dev_for_each_consumer(dev, sink) {
		if (cd->source_format != audio_stream_get_frm_fmt(&sink->stream)) {
			comp_err(dev, "crossover_prepare(): Source fmt %d and sink fmt %d are different.",
				 cd->source_format, audio_stream_get_frm_fmt(&sink->stream));
			ret = -EINVAL;
		}

		if (ret < 0)
			return ret;
	}

	comp_info(dev, "crossover_prepare(), source_format=%d, sink_formats=%d, nch=%d",
		  cd->source_format, cd->source_format, channels);

	cd->config = comp_get_data_blob(cd->model_handler, NULL, NULL);

	/* Initialize Crossover */
	if (cd->config && crossover_validate_config(mod, cd->config) < 0) {
		/* If config is invalid then delete it */
		comp_err(dev, "crossover_prepare(), invalid binary config format");
		crossover_free_config(&cd->config);
	}

	if (cd->config) {
		ret = crossover_setup(mod, channels);
		if (ret < 0) {
			comp_err(dev, "crossover_prepare(), setup failed");
			return ret;
		}

		cd->crossover_process = crossover_find_proc_func(cd->source_format);
		if (!cd->crossover_process) {
			comp_err(dev, "crossover_prepare(), No processing function matching frame_fmt %i",
				 cd->source_format);
			return -EINVAL;
		}

		cd->crossover_split = crossover_find_split_func(cd->config->num_sinks);
		if (!cd->crossover_split) {
			comp_err(dev, "crossover_prepare(), No split function matching num_sinks %i",
				 cd->config->num_sinks);
			return -EINVAL;
		}
	} else {
		comp_info(dev, "crossover_prepare(), setting crossover to passthrough mode");

		cd->crossover_process = crossover_find_proc_func_pass(cd->source_format);
		if (!cd->crossover_process) {
			comp_err(dev, "crossover_prepare(), No passthrough function matching frame_fmt %i",
				 cd->source_format);
			return -EINVAL;
		}
	}

	return 0;
}

/**
 * \brief Resets Crossover Filter component.
 * \param[in,out] dev Crossover Filter base component device.
 * \return Error code.
 */
static int crossover_reset(struct processing_module *mod)
{
	struct comp_data *cd = module_get_private_data(mod);

	comp_info(mod->dev, "crossover_reset()");

	crossover_reset_state(cd);

	cd->crossover_process = NULL;
	cd->crossover_split = NULL;

	return 0;
}

/** \brief Crossover Filter component definition. */
static const struct module_interface crossover_interface = {
	.init = crossover_init,
	.prepare = crossover_prepare,
	.process_audio_stream = crossover_process_audio_stream,
	.set_configuration = crossover_set_config,
	.get_configuration = crossover_get_config,
	.reset = crossover_reset,
	.free = crossover_free
};

DECLARE_MODULE_ADAPTER(crossover_interface, crossover_uuid, crossover_tr);
SOF_MODULE_INIT(crossover, sys_comp_module_crossover_interface_init);

#if CONFIG_COMP_CROSSOVER_MODULE
/* modular: llext dynamic link */

#include <module/module/api_ver.h>
#include <module/module/llext.h>
#include <rimage/sof/user/manifest.h>

#define UUID_CROSSOVER 0xD1, 0x9A, 0x8C, 0x94, 0x6A, 0x80, 0x31, 0x41, 0x6C, 0xAD, \
		0xB2, 0xBD, 0xA9, 0xE3, 0x5A, 0x9F

SOF_LLEXT_MOD_ENTRY(crossover, &crossover_interface);

static const struct sof_man_module_manifest mod_manifest __section(".module") __used =
	SOF_LLEXT_MODULE_MANIFEST("XOVER", crossover_llext_entry, 1, UUID_CROSSOVER, 40);

SOF_LLEXT_BUILDINFO;

#endif
