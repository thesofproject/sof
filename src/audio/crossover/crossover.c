// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Google LLC. All rights reserved.
//
// Author: Sebastiano Carlucci <scarlucci@google.com>

#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/audio/data_blob.h>
#include <sof/audio/format.h>
#include <sof/audio/pipeline.h>
#include <sof/audio/ipc-config.h>
#include <sof/audio/crossover/crossover.h>
#include <sof/audio/crossover/crossover_algorithm.h>
#include <sof/common.h>
#include <sof/debug/panic.h>
#include <sof/ipc/msg.h>
#include <rtos/alloc.h>
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
#include <user/crossover.h>
#include <user/eq.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <limits.h>

static const struct comp_driver comp_crossover;

/* 948c9ad1-806a-4131-ad6c-b2bda9e35a9f */
DECLARE_SOF_RT_UUID("crossover", crossover_uuid, 0x948c9ad1, 0x806a, 0x4131,
		    0xad, 0x6c, 0xb2, 0xbd, 0xa9, 0xe3, 0x5a, 0x9f);

DECLARE_TR_CTX(crossover_tr, SOF_UUID(crossover_uuid), LOG_LEVEL_INFO);

static inline void crossover_free_config(struct sof_crossover_config **config)
{
	rfree(*config);
	*config = NULL;
}

/**
 * \brief Reset the state of an LR4 filter.
 */
static inline void crossover_reset_state_lr4(struct iir_state_df2t *lr4)
{
	rfree(lr4->coef);
	rfree(lr4->delay);

	lr4->coef = NULL;
	lr4->delay = NULL;
}

/**
 * \brief Reset the state (coefficients and delay) of the crossover filter
 *	  of a single channel.
 */
inline void crossover_reset_state_ch(struct crossover_state *ch_state)
{
	int i;

	for (i = 0; i < CROSSOVER_MAX_LR4; i++) {
		crossover_reset_state_lr4(&ch_state->lowpass[i]);
		crossover_reset_state_lr4(&ch_state->highpass[i]);
	}
}

/**
 * \brief Reset the state (coefficients and delay) of the crossover filter
 *	  across all channels
 */
static inline void crossover_reset_state(struct comp_data *cd)
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
static int crossover_get_stream_index(struct sof_crossover_config *config,
					  uint32_t pipe_id)
{
	int i;
	uint32_t *assign_sink = config->assign_sink;

	for (i = 0; i < config->num_sinks; i++)
		if (assign_sink[i] == pipe_id)
			return i;

	comp_cl_err(&comp_crossover, "crossover_get_stream_index() error: couldn't find any assignment for connected pipeline %u",
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
static int crossover_assign_sinks(struct comp_dev *dev,
				  struct sof_crossover_config *config,
				  struct comp_buffer **sinks)
{
	struct comp_buffer *sink;
	struct comp_buffer __sparse_cache *sink_c;
	struct list_item *sink_list;
	int num_sinks = 0;
	int i;

	/* Align sink streams with their respective configurations */
	list_for_item(sink_list, &dev->bsink_list) {
		unsigned int pipeline_id, state;

		sink = container_of(sink_list, struct comp_buffer, source_list);
		sink_c = buffer_acquire(sink);

		pipeline_id = sink_c->pipeline_id;
		state = sink_c->sink->state;
		buffer_release(sink_c);

		if (state != dev->state)
			continue;

		/* If no config is set, then assign the sinks in order */
		if (!config) {
			sinks[num_sinks++] = sink;
			continue;
		}

		i = crossover_get_stream_index(config, pipeline_id);

		/* If this sink buffer is not assigned
		 * in the configuration.
		 */
		if (i < 0) {
			comp_err(dev,
				 "crossover_acquire_sinks(), could not find sink %d in config",
				 pipeline_id);
			break;
		}

		if (sinks[i]) {
			comp_err(dev,
				 "crossover_acquire_sinks(), multiple sinks from pipeline %d are assigned",
				 pipeline_id);
			break;
		}

		sinks[i] = sink;
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
static int crossover_init_coef_lr4(struct sof_eq_iir_biquad_df2t *coef,
				   struct iir_state_df2t *lr4)
{
	int ret;

	/* Only one set of coefficients is stored in config for both biquads
	 * in series due to identity. To maintain the structure of
	 * iir_state_df2t, it requires two copies of coefficients in a row.
	 */
	lr4->coef = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM,
			    sizeof(struct sof_eq_iir_biquad_df2t) * 2);
	if (!lr4->coef)
		return -ENOMEM;

	/* coefficients of the first biquad */
	ret = memcpy_s(lr4->coef, sizeof(struct sof_eq_iir_biquad_df2t),
		       coef, sizeof(struct sof_eq_iir_biquad_df2t));
	assert(!ret);

	/* coefficients of the second biquad */
	ret = memcpy_s(lr4->coef + SOF_EQ_IIR_NBIQUAD_DF2T,
		       sizeof(struct sof_eq_iir_biquad_df2t),
		       coef, sizeof(struct sof_eq_iir_biquad_df2t));
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
int crossover_init_coef_ch(struct sof_eq_iir_biquad_df2t *coef,
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
static int crossover_init_coef(struct comp_data *cd, int nch)
{
	struct sof_eq_iir_biquad_df2t *crossover;
	struct sof_crossover_config *config = cd->config;
	int ch, err;

	if (!config) {
		comp_cl_err(&comp_crossover, "crossover_init_coef(), no config is set");
		return -EINVAL;
	}

	/* Sanity checks */
	if (nch > PLATFORM_MAX_CHANNELS) {
		comp_cl_err(&comp_crossover, "crossover_init_coef(), invalid channels count (%i)",
			    nch);
		return -EINVAL;
	}

	comp_cl_info(&comp_crossover, "crossover_init_coef(), initializing %i-way crossover",
		     config->num_sinks);

	/* Collect the coef array and assign it to every channel */
	crossover = config->coef;
	for (ch = 0; ch < nch; ch++) {
		err = crossover_init_coef_ch(crossover, &cd->state[ch],
					     config->num_sinks);
		/* Free all previously allocated blocks in case of an error */
		if (err < 0) {
			comp_cl_err(&comp_crossover, "crossover_init_coef(), could not assign coefficients to ch %d",
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
static int crossover_setup(struct comp_data *cd, int nch)
{
	int ret = 0;

	/* Reset any previous state */
	crossover_reset_state(cd);

	/* Assign LR4 coefficients from config */
	ret = crossover_init_coef(cd, nch);

	return ret;
}

/**
 * \brief Creates a Crossover Filter component.
 * \return Pointer to Crossover Filter component device.
 */
static struct comp_dev *crossover_new(const struct comp_driver *drv,
				      struct comp_ipc_config *config,
				      void *spec)
{
	struct comp_dev *dev;
	struct comp_data *cd;
	struct ipc_config_process *ipc_crossover = spec;;
	size_t bs = ipc_crossover->size;
	int ret;

	comp_cl_info(&comp_crossover, "crossover_new()");

	/* Check that the coefficients blob size is sane */
	if (bs > SOF_CROSSOVER_MAX_SIZE) {
		comp_cl_err(&comp_crossover, "crossover_new(), blob size (%d) exceeds maximum allowed size (%i)",
			    bs, SOF_CROSSOVER_MAX_SIZE);
		return NULL;
	}

	dev = comp_alloc(drv, sizeof(*dev));
	if (!dev)
		return NULL;
	dev->ipc_config = *config;

	cd = rzalloc(SOF_MEM_ZONE_RUNTIME, 0,
		     SOF_MEM_CAPS_RAM, sizeof(*cd));
	if (!cd) {
		rfree(dev);
		return NULL;
	}

	comp_set_drvdata(dev, cd);

	cd->crossover_process = NULL;
	cd->crossover_split = NULL;
	cd->config = NULL;

	/* Handler for configuration data */
	cd->model_handler = comp_data_blob_handler_new(dev);
	if (!cd->model_handler) {
		comp_cl_err(&comp_crossover, "crossover_new(): comp_data_blob_handler_new() failed.");
		goto cd_fail;
	}

	/* Get configuration data and reset Crossover state */
	ret = comp_init_data_blob(cd->model_handler, bs, ipc_crossover->data);
	if (ret < 0) {
		comp_cl_err(&comp_crossover, "crossover_new(): comp_init_data_blob() failed.");
		goto cd_fail;
	}
	crossover_reset_state(cd);

	dev->state = COMP_STATE_READY;
	return dev;

cd_fail:
	comp_data_blob_handler_free(cd->model_handler);
	rfree(cd);
	rfree(dev);
	return NULL;
}

/**
 * \brief Frees Crossover Filter component.
 */
static void crossover_free(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	comp_info(dev, "crossover_free()");

	comp_data_blob_handler_free(cd->model_handler);

	crossover_reset_state(cd);

	rfree(cd);
	rfree(dev);
}

/**
 * \brief Verifies that the config is formatted correctly.
 *
 * The function can only be called after the buffers have been initialized.
 */
static int crossover_validate_config(struct comp_dev *dev,
				     struct sof_crossover_config *config)
{
	struct comp_buffer *sink;
	struct comp_buffer __sparse_cache *sink_c;
	struct list_item *sink_list;
	uint32_t size = config->size;
	int32_t num_assigned_sinks = 0;
	uint8_t assigned_sinks[SOF_CROSSOVER_MAX_STREAMS] = {0};
	int i;

	if (size > SOF_CROSSOVER_MAX_SIZE || !size) {
		comp_err(dev, "crossover_validate_config(), size %d is invalid",
			 size);
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
	list_for_item(sink_list, &dev->bsink_list) {
		unsigned int pipeline_id;

		sink = container_of(sink_list, struct comp_buffer, source_list);
		sink_c = buffer_acquire(sink);
		pipeline_id = sink_c->pipeline_id;
		buffer_release(sink_c);

		i = crossover_get_stream_index(config, pipeline_id);
		if (i < 0) {
			comp_warn(dev, "crossover_validate_config(), could not assign sink %d",
				  pipeline_id);
			break;
		}

		if (assigned_sinks[i]) {
			comp_warn(dev, "crossover_validate_config(), multiple sinks from pipeline %d are assigned",
				  pipeline_id);
			break;
		}

		assigned_sinks[i] = true;
		num_assigned_sinks++;
	}

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

static int crossover_verify_params(struct comp_dev *dev,
				   struct sof_ipc_stream_params *params)
{
	int ret;

	comp_dbg(dev, "crossover_verify_params()");

	ret = comp_verify_params(dev, 0, params);
	if (ret < 0) {
		comp_err(dev, "crossover_verify_params() error: comp_verify_params() failed.");
		return ret;
	}

	return 0;
}

/**
 * \brief Sets Crossover Filter component audio stream parameters.
 * \param[in,out] dev Crossover Filter base component device.
 * \return Error code.
 */
static int crossover_params(struct comp_dev *dev,
			    struct sof_ipc_stream_params *params)
{
	int err = 0;

	comp_dbg(dev, "crossover_params()");

	err = crossover_verify_params(dev, params);
	if (err < 0)
		comp_err(dev, "crossover_params(): pcm params verification failed");

	return err;
}

static int crossover_cmd_set_data(struct comp_dev *dev,
				  struct sof_ipc_ctrl_data *cdata)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int ret = 0;

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_BINARY:
		comp_info(dev, "crossover_cmd_set_data(), SOF_CTRL_CMD_BINARY");
		ret = comp_data_blob_set_cmd(cd->model_handler, cdata);
		break;
	default:
		comp_err(dev, "crossover_cmd_set_data(), invalid command");
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int crossover_cmd_get_data(struct comp_dev *dev,
				  struct sof_ipc_ctrl_data *cdata, int max_size)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int ret = 0;

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_BINARY:
		comp_info(dev, "crossover_cmd_get_data(), SOF_CTRL_CMD_BINARY");
		ret = comp_data_blob_get_cmd(cd->model_handler, cdata, max_size);
		break;
	default:
		comp_err(dev, "crossover_cmd_get_data(), invalid command");
		ret = -EINVAL;
		break;
	}
	return ret;
}

/**
 * \brief Handles incoming IPC commands for Crossover component.
 */
static int crossover_cmd(struct comp_dev *dev, int cmd, void *data,
			 int max_data_size)
{
	struct sof_ipc_ctrl_data *cdata = ASSUME_ALIGNED(data, 4);
	int ret = 0;

	comp_info(dev, "crossover_cmd()");

	switch (cmd) {
	case COMP_CMD_SET_DATA:
		ret = crossover_cmd_set_data(dev, cdata);
		break;
	case COMP_CMD_GET_DATA:
		ret = crossover_cmd_get_data(dev, cdata, max_data_size);
		break;
	default:
		comp_err(dev, "crossover_cmd(), invalid command");
		ret = -EINVAL;
	}

	return ret;
}

/**
 * \brief Sets Crossover Filter component state.
 * \param[in,out] dev Crossover Filter base component device.
 * \param[in] cmd Command type.
 * \return Error code.
 */
static int crossover_trigger(struct comp_dev *dev, int cmd)
{
	comp_info(dev, "crossover_trigger()");

	return comp_set_state(dev, cmd);
}

/**
 * \brief Copies and processes stream data.
 * \param[in,out] dev Crossover Filter base component device.
 * \return Error code.
 */
static int crossover_copy(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct comp_buffer *source;
	struct comp_buffer __sparse_cache *source_c;
	struct comp_buffer *sinks[SOF_CROSSOVER_MAX_STREAMS] = { NULL };
	struct comp_buffer __sparse_cache *sinks_c[SOF_CROSSOVER_MAX_STREAMS];
	int i, ret = 0;
	uint32_t num_sinks;
	uint32_t num_assigned_sinks = 0;
	uint32_t frames = UINT_MAX;
	uint32_t source_bytes, avail;
	uint32_t sinks_bytes[SOF_CROSSOVER_MAX_STREAMS] = { 0 };

	comp_dbg(dev, "crossover_copy()");

	source = list_first_item(&dev->bsource_list, struct comp_buffer,
				 sink_list);
	source_c = buffer_acquire(source);

	/* Check for changed configuration */
	if (comp_is_new_data_blob_available(cd->model_handler)) {
		cd->config = comp_get_data_blob(cd->model_handler, NULL, NULL);
		ret = crossover_setup(cd, source_c->stream.channels);
		if (ret < 0) {
			comp_err(dev, "crossover_copy(), failed Crossover setup");
			goto out;
		}
	}

	/* Check if source is active */
	if (source_c->source->state != dev->state) {
		ret = -EINVAL;
		goto out;
	}

	/* Use the assign_sink array from the config to route
	 * the output to the corresponding sinks.
	 * It is possible for an assigned sink to be in a different
	 * state than the component. Therefore not all sinks are guaranteed
	 * to be assigned: sink[i] can be NULL, 0 <= i <= config->num_sinks
	 */
	num_assigned_sinks = crossover_assign_sinks(dev, cd->config, sinks);
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

	/* Find the number of frames to copy over */
	for (i = 0; i < num_sinks; i++) {
		if (!sinks[i])
			continue;
		/*
		 * WARNING: if a different thread happens to lock the same
		 * buffers in different order, they can deadlock
		 */
		sinks_c[i] = buffer_acquire(sinks[i]);
		avail = audio_stream_avail_frames(&source_c->stream,
						  &sinks_c[i]->stream);
		frames = MIN(frames, avail);
	}

	source_bytes = frames * audio_stream_frame_bytes(&source_c->stream);

	for (i = 0; i < num_sinks; i++)
		if (sinks[i])
			sinks_bytes[i] = frames *
				audio_stream_frame_bytes(&sinks_c[i]->stream);

	/* Process crossover */
	buffer_stream_invalidate(source_c, source_bytes);
	cd->crossover_process(dev, source_c, sinks_c, num_sinks, frames);

	for (i = 0; i < num_sinks; i++) {
		if (!sinks[i])
			continue;
		buffer_stream_writeback(sinks_c[i], sinks_bytes[i]);
		comp_update_buffer_produce(sinks_c[i], sinks_bytes[i]);
	}

	/* Release buffers in reverse order */
	for (i = num_sinks - 1; i >= 0; i--)
		if (sinks[i])
			buffer_release(sinks_c[i]);

	comp_update_buffer_consume(source_c, source_bytes);

out:
	buffer_release(source_c);

	return ret;
}

/**
 * \brief Prepares Crossover Filter component for processing.
 * \param[in,out] dev Crossover Filter base component device.
 * \return Error code.
 */
static int crossover_prepare(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct comp_buffer *source, *sink;
	struct comp_buffer __sparse_cache *source_c, *sink_c;
	struct list_item *sink_list;
	int32_t sink_period_bytes;
	int ret;

	comp_info(dev, "crossover_prepare()");

	ret = comp_set_state(dev, COMP_TRIGGER_PREPARE);
	if (ret < 0)
		return ret;

	if (ret == COMP_STATUS_STATE_ALREADY_SET)
		return PPL_STATUS_PATH_STOP;

	/* Crossover has a variable number of sinks */
	source = list_first_item(&dev->bsource_list,
				 struct comp_buffer, sink_list);
	source_c = buffer_acquire(source);

	/* Get source data format */
	cd->source_format = source_c->stream.frame_fmt;

	/* Validate frame format and buffer size of sinks */
	list_for_item(sink_list, &dev->bsink_list) {
		sink = container_of(sink_list, struct comp_buffer, source_list);
		sink_c = buffer_acquire(sink);

		if (cd->source_format != sink_c->stream.frame_fmt) {
			comp_err(dev, "crossover_prepare(): Source fmt %d and sink fmt %d are different for sink %d.",
				 cd->source_format, sink_c->stream.frame_fmt,
				 sink_c->pipeline_id);
			ret = -EINVAL;
		} else {
			sink_period_bytes = audio_stream_period_bytes(&sink_c->stream,
								      dev->frames);
			if (sink_c->stream.size < sink_period_bytes) {
				comp_err(dev,
					 "crossover_prepare(), sink %d buffer size %d is insufficient",
					 sink_c->pipeline_id, sink_c->stream.size);
				ret = -ENOMEM;
			}
		}

		buffer_release(sink_c);

		if (ret < 0)
			goto out;
	}

	comp_info(dev, "crossover_prepare(), source_format=%d, sink_formats=%d, nch=%d",
		  cd->source_format, cd->source_format,
		  source_c->stream.channels);

	cd->config = comp_get_data_blob(cd->model_handler, NULL, NULL);

	/* Initialize Crossover */
	if (cd->config && crossover_validate_config(dev, cd->config) < 0) {
		/* If config is invalid then delete it */
		comp_err(dev, "crossover_prepare(), invalid binary config format");
		crossover_free_config(&cd->config);
	}

	if (cd->config) {
		ret = crossover_setup(cd, source_c->stream.channels);
		if (ret < 0) {
			comp_err(dev, "crossover_prepare(), setup failed");
			goto out;
		}

		cd->crossover_process =
			crossover_find_proc_func(cd->source_format);
		if (!cd->crossover_process) {
			comp_err(dev, "crossover_prepare(), No processing function matching frame_fmt %i",
				 cd->source_format);
			ret = -EINVAL;
			goto out;
		}

		cd->crossover_split =
			crossover_find_split_func(cd->config->num_sinks);
		if (!cd->crossover_split) {
			comp_err(dev, "crossover_prepare(), No split function matching num_sinks %i",
				 cd->config->num_sinks);
			ret = -EINVAL;
			goto out;
		}
	} else {
		comp_info(dev, "crossover_prepare(), setting crossover to passthrough mode");

		cd->crossover_process =
			crossover_find_proc_func_pass(cd->source_format);

		if (!cd->crossover_process) {
			comp_err(dev, "crossover_prepare(), No passthrough function matching frame_fmt %i",
				 cd->source_format);
			ret = -EINVAL;
			goto out;
		}
	}

out:
	if (ret < 0)
		comp_set_state(dev, COMP_TRIGGER_RESET);

	buffer_release(source_c);

	return ret;
}

/**
 * \brief Resets Crossover Filter component.
 * \param[in,out] dev Crossover Filter base component device.
 * \return Error code.
 */
static int crossover_reset(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	comp_info(dev, "crossover_reset()");

	crossover_reset_state(cd);

	cd->crossover_process = NULL;
	cd->crossover_split = NULL;

	comp_set_state(dev, COMP_TRIGGER_RESET);

	return 0;
}

/** \brief Crossover Filter component definition. */
static const struct comp_driver comp_crossover = {
	.uid	= SOF_RT_UUID(crossover_uuid),
	.tctx	= &crossover_tr,
	.ops	= {
		.create		= crossover_new,
		.free		= crossover_free,
		.params		= crossover_params,
		.cmd		= crossover_cmd,
		.trigger	= crossover_trigger,
		.copy		= crossover_copy,
		.prepare	= crossover_prepare,
		.reset		= crossover_reset,
	},
};

static SHARED_DATA struct comp_driver_info comp_crossover_info = {
	.drv = &comp_crossover,
};

UT_STATIC void sys_comp_crossover_init(void)
{
	comp_register(platform_shared_get(&comp_crossover_info,
					  sizeof(comp_crossover_info)));
}

DECLARE_MODULE(sys_comp_crossover_init);
