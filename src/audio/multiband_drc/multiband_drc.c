// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Google LLC. All rights reserved.
//
// Author: Pin-chih Lin <johnylin@google.com>

#include <sof/audio/buffer.h>
#include <sof/audio/crossover/crossover_algorithm.h>
#include <sof/audio/drc/drc_algorithm.h>
#include <sof/audio/multiband_drc/multiband_drc.h>
#include <sof/audio/format.h>
#include <sof/audio/pipeline.h>
#include <sof/common.h>
#include <sof/debug/panic.h>
#include <sof/drivers/ipc.h>
#include <sof/lib/alloc.h>
#include <sof/lib/memory.h>
#include <sof/lib/uuid.h>
#include <sof/list.h>
#include <sof/math/numbers.h>
#include <sof/platform.h>
#include <sof/string.h>
#include <sof/ut.h>
#include <sof/trace/trace.h>
#include <ipc/control.h>
#include <ipc/stream.h>
#include <ipc/topology.h>
#include <user/eq.h>
#include <user/trace.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

static const struct comp_driver comp_multiband_drc;

/* 0d9f2256-8e4f-47b3-8448-239a334f1191 */
DECLARE_SOF_RT_UUID("multiband_drc", multiband_drc_uuid, 0x0d9f2256, 0x8e4f, 0x47b3,
		    0x84, 0x48, 0x23, 0x9a, 0x33, 0x4f, 0x11, 0x91);

DECLARE_TR_CTX(multiband_drc_tr, SOF_UUID(multiband_drc_uuid), LOG_LEVEL_INFO);

static inline void multiband_drc_iir_reset_state_ch(struct iir_state_df2t *iir)
{
	rfree(iir->coef);
	rfree(iir->delay);

	iir->coef = NULL;
	iir->delay = NULL;
}

static inline void multiband_drc_reset_state(struct multiband_drc_state *state)
{
	int i;

	/* Reset emphasis eq-iir state */
	for (i = 0; i < PLATFORM_MAX_CHANNELS; i++)
		multiband_drc_iir_reset_state_ch(&state->emphasis[i]);

	/* Reset crossover state */
	for (i = 0; i < PLATFORM_MAX_CHANNELS; i++)
		crossover_reset_state_ch(&state->crossover[i]);

	/* Reset drc kernel state */
	for (i = 0; i < SOF_MULTIBAND_DRC_MAX_BANDS; i++)
		drc_reset_state(&state->drc[i]);

	/* Reset deemphasis eq-iir state */
	for (i = 0; i < PLATFORM_MAX_CHANNELS; i++)
		multiband_drc_iir_reset_state_ch(&state->deemphasis[i]);
}

static int multiband_drc_eq_init_coef_ch(struct sof_eq_iir_biquad_df2t *coef,
					 struct iir_state_df2t *eq)
{
	int ret;

	eq->coef = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM,
			   sizeof(struct sof_eq_iir_biquad_df2t) * SOF_EMP_DEEMP_BIQUADS);
	if (!eq->coef)
		return -ENOMEM;

	/* Coefficients of the first biquad and second biquad */
	ret = memcpy_s(eq->coef, sizeof(struct sof_eq_iir_biquad_df2t) * SOF_EMP_DEEMP_BIQUADS,
		       coef, sizeof(struct sof_eq_iir_biquad_df2t) * SOF_EMP_DEEMP_BIQUADS);
	assert(!ret);

	/* EQ filters are two 2nd order filters, so only need 4 delay slots
	 * delay[0..1] -> state for first biquad
	 * delay[2..3] -> state for second biquad
	 */
	eq->delay = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM,
			    sizeof(uint64_t) * CROSSOVER_NUM_DELAYS_LR4);
	if (!eq->delay)
		return -ENOMEM;

	eq->biquads = SOF_EMP_DEEMP_BIQUADS;
	eq->biquads_in_series = SOF_EMP_DEEMP_BIQUADS;

	return 0;
}

static int multiband_drc_init_coef(struct multiband_drc_comp_data *cd, int16_t nch, uint32_t rate)
{
	struct sof_eq_iir_biquad_df2t *crossover;
	struct sof_eq_iir_biquad_df2t *emphasis;
	struct sof_eq_iir_biquad_df2t *deemphasis;
	struct sof_multiband_drc_config *config = cd->config;
	struct multiband_drc_state *state = &cd->state;
	uint32_t sample_bytes = get_sample_bytes(cd->source_format);
	int num_bands = cd->config->num_bands;
	int i, ch, ret;

	if (!config) {
		comp_cl_err(&comp_multiband_drc, "multiband_drc_init_coef(), no config is set");
		return -EINVAL;
	}

	/* Sanity checks */
	if (nch > PLATFORM_MAX_CHANNELS) {
		comp_cl_err(&comp_multiband_drc,
			    "multiband_drc_init_coef(), invalid channels count(%i)",
			    nch);
		return -EINVAL;
	}
	if (config->num_bands > SOF_MULTIBAND_DRC_MAX_BANDS) {
		comp_cl_err(&comp_multiband_drc,
			    "multiband_drc_init_coef(), invalid bands count(%i)",
			    config->num_bands);
		return -EINVAL;
	}

	comp_cl_info(&comp_multiband_drc,
		     "multiband_drc_init_coef(), initiliazing %i-way crossover",
		     config->num_bands);

	/* Crossover: collect the coef array and assign it to every channel */
	crossover = config->crossover_coef;
	for (ch = 0; ch < nch; ch++) {
		ret = crossover_init_coef_ch(crossover, &state->crossover[ch],
					     config->num_bands);
		/* Free all previously allocated blocks in case of an error */
		if (ret < 0) {
			comp_cl_err(&comp_multiband_drc,
				    "multiband_drc_init_coef(), could not assign coeffs to ch %d",
				    ch);
			goto err;
		}
	}

	comp_cl_info(&comp_multiband_drc, "multiband_drc_init_coef(), initiliazing emphasis_eq");

	/* Emphasis: collect the coef array and assign it to every channel */
	emphasis = config->emp_coef;
	for (ch = 0; ch < nch; ch++) {
		ret = multiband_drc_eq_init_coef_ch(emphasis, &state->emphasis[ch]);
		/* Free all previously allocated blocks in case of an error */
		if (ret < 0) {
			comp_cl_err(&comp_multiband_drc,
				    "multiband_drc_init_coef(), could not assign coeffs to ch %d",
				    ch);
			goto err;
		}
	}

	comp_cl_info(&comp_multiband_drc, "multiband_drc_init_coef(), initiliazing deemphasis_eq");

	/* Deemphasis: collect the coef array and assign it to every channel */
	deemphasis = config->deemp_coef;
	for (ch = 0; ch < nch; ch++) {
		ret = multiband_drc_eq_init_coef_ch(deemphasis, &state->deemphasis[ch]);
		/* Free all previously allocated blocks in case of an error */
		if (ret < 0) {
			comp_cl_err(&comp_multiband_drc,
				    "multiband_drc_init_coef(), could not assign coeffs to ch %d",
				    ch);
			goto err;
		}
	}

	/* Allocate all DRC pre-delay buffers and set delay time with band number */
	for (i = 0; i < num_bands; i++) {
		comp_cl_info(&comp_multiband_drc,
			     "multiband_drc_init_coef(), initiliazing drc band %d", i);

		ret = drc_init_pre_delay_buffers(&state->drc[i], (size_t)sample_bytes, (int)nch);
		if (ret < 0) {
			comp_cl_err(&comp_multiband_drc,
				    "multiband_drc_init_coef(), could not init pre delay buffers");
			goto err;
		}

		ret = drc_set_pre_delay_time(&state->drc[i],
					     cd->config->drc_coef[i].pre_delay_time, rate);
		if (ret < 0) {
			comp_cl_err(&comp_multiband_drc,
				    "multiband_drc_init_coef(), could not set pre delay time");
			goto err;
		}
	}

	return 0;

err:
	multiband_drc_reset_state(state);
	return ret;
}

static int multiband_drc_setup(struct multiband_drc_comp_data *cd, int16_t channels, uint32_t rate)
{
	int ret;

	/* Reset any previous state */
	multiband_drc_reset_state(&cd->state);

	/* Setup Crossover, Emphasis EQ, Deemphasis EQ, and DRC */
	ret = multiband_drc_init_coef(cd, channels, rate);
	if (ret < 0)
		return ret;

	return 0;
}

/*
 * End of Multiband DRC setup code. Next the standard component methods.
 */

static struct comp_dev *multiband_drc_new(const struct comp_driver *drv,
					  struct sof_ipc_comp *comp)
{
	struct comp_dev *dev;
	struct multiband_drc_comp_data *cd;
	struct sof_ipc_comp_process *ipc_multiband_drc =
		(struct sof_ipc_comp_process *)comp;
	size_t bs = ipc_multiband_drc->size;
	int ret;

	comp_cl_info(&comp_multiband_drc, "multiband_drc_new()");

	/* Check first before proceeding with dev and cd that coefficients
	 * blob size is sane.
	 */
	if (bs > SOF_MULTIBAND_DRC_MAX_BLOB_SIZE) {
		comp_cl_err(&comp_multiband_drc,
			    "multiband_drc_new(), error: configuration blob size = %u > %d",
			    bs, SOF_MULTIBAND_DRC_MAX_BLOB_SIZE);
		return NULL;
	}

	dev = comp_alloc(drv, COMP_SIZE(struct sof_ipc_comp_process));
	if (!dev)
		return NULL;

	ret = memcpy_s(COMP_GET_IPC(dev, sof_ipc_comp_process),
		       sizeof(struct sof_ipc_comp_process), ipc_multiband_drc,
		       sizeof(struct sof_ipc_comp_process));
	assert(!ret);

	cd = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(*cd));
	if (!cd) {
		rfree(dev);
		return NULL;
	}

	comp_set_drvdata(dev, cd);

	cd->multiband_drc_func = NULL;
	cd->crossover_split = NULL;

	/* Handler for configuration data */
	cd->model_handler = comp_data_blob_handler_new(dev);
	if (!cd->model_handler) {
		comp_cl_err(&comp_multiband_drc,
			    "multiband_drc_new(): comp_data_blob_handler_new() failed.");
		rfree(dev);
		rfree(cd);
		return NULL;
	}

	/* Get configuration data and reset DRC state */
	ret = comp_init_data_blob(cd->model_handler, bs, ipc_multiband_drc->data);
	if (ret < 0) {
		comp_cl_err(&comp_multiband_drc,
			    "multiband_drc_new(): comp_init_data_blob() failed.");
		rfree(dev);
		rfree(cd);
		return NULL;
	}
	multiband_drc_reset_state(&cd->state);

	dev->state = COMP_STATE_READY;
	return dev;
}

static void multiband_drc_free(struct comp_dev *dev)
{
	struct multiband_drc_comp_data *cd = comp_get_drvdata(dev);

	comp_info(dev, "multiband_drc_free()");

	comp_data_blob_handler_free(cd->model_handler);

	rfree(cd);
	rfree(dev);
}

static int multiband_drc_params(struct comp_dev *dev, struct sof_ipc_stream_params *params)
{
	int ret;

	comp_dbg(dev, "multiband_drc_params()");

	comp_dbg(dev, "multiband_drc_verify_params()");
	ret = comp_verify_params(dev, 0, params);
	if (ret < 0) {
		comp_err(dev, "multiband_drc_params(): comp_verify_params() failed.");
		return -EINVAL;
	}

	/* All configuration work is postponed to prepare(). */
	return 0;
}

static int multiband_drc_cmd_get_data(struct comp_dev *dev,
				      struct sof_ipc_ctrl_data *cdata,
				      int max_size)
{
	struct multiband_drc_comp_data *cd = comp_get_drvdata(dev);
	int ret = 0;

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_BINARY:
		comp_dbg(dev, "multiband_drc_cmd_get_data(), SOF_CTRL_CMD_BINARY");
		ret = comp_data_blob_get_cmd(cd->model_handler, cdata, max_size);
		break;
	default:
		comp_err(dev, "multiband_drc_cmd_get_data() error: invalid cdata->cmd");
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int multiband_drc_cmd_set_data(struct comp_dev *dev,
				      struct sof_ipc_ctrl_data *cdata)
{
	struct multiband_drc_comp_data *cd = comp_get_drvdata(dev);
	int ret = 0;

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_BINARY:
		comp_dbg(dev, "multiband_drc_cmd_set_data(), SOF_CTRL_CMD_BINARY");
		ret = comp_data_blob_set_cmd(cd->model_handler, cdata);
		break;
	default:
		comp_err(dev, "multiband_drc_cmd_set_data() error: invalid cdata->cmd");
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int multiband_drc_cmd(struct comp_dev *dev, int cmd, void *data, int max_data_size)
{
	struct sof_ipc_ctrl_data *cdata = data;
	int ret = 0;

	comp_dbg(dev, "multiband_drc_cmd()");

	switch (cmd) {
	case COMP_CMD_SET_DATA:
		ret = multiband_drc_cmd_set_data(dev, cdata);
		break;
	case COMP_CMD_GET_DATA:
		ret = multiband_drc_cmd_get_data(dev, cdata, max_data_size);
		break;
	default:
		comp_err(dev, "multiband_drc_cmd(), invalid command");
		ret = -EINVAL;
	}

	return ret;
}

static int multiband_drc_trigger(struct comp_dev *dev, int cmd)
{
	int ret;

	comp_dbg(dev, "multiband_drc_trigger()");

	ret = comp_set_state(dev, cmd);
	if (ret == COMP_STATUS_STATE_ALREADY_SET)
		ret = PPL_STATUS_PATH_STOP;

	return ret;
}

static void multiband_drc_process(struct comp_dev *dev, struct comp_buffer *source,
				  struct comp_buffer *sink, int frames,
				  uint32_t source_bytes, uint32_t sink_bytes)
{
	struct multiband_drc_comp_data *cd = comp_get_drvdata(dev);

	buffer_invalidate(source, source_bytes);

	cd->multiband_drc_func(dev, &source->stream, &sink->stream, frames);

	buffer_writeback(sink, sink_bytes);

	/* calc new free and available */
	comp_update_buffer_consume(source, source_bytes);
	comp_update_buffer_produce(sink, sink_bytes);
}

static int multiband_drc_copy(struct comp_dev *dev)
{
	struct comp_copy_limits cl;
	struct multiband_drc_comp_data *cd = comp_get_drvdata(dev);
	struct comp_buffer *sourceb;
	struct comp_buffer *sinkb;
	int ret;

	comp_dbg(dev, "multiband_drc_copy()");

	sourceb = list_first_item(&dev->bsource_list, struct comp_buffer,
				  sink_list);
	sinkb = list_first_item(&dev->bsink_list, struct comp_buffer,
				source_list);

	/* Check for changed configuration */
	if (comp_is_new_data_blob_available(cd->model_handler)) {
		cd->config = comp_get_data_blob(cd->model_handler, NULL, NULL);
		ret = multiband_drc_setup(cd, (int16_t)sourceb->stream.channels,
					  sourceb->stream.rate);
		if (ret < 0) {
			comp_err(dev, "multiband_drc_copy(), failed DRC setup");
			return ret;
		}
	}

	/* Get source, sink, number of frames etc. to process. */
	comp_get_copy_limits_with_lock(sourceb, sinkb, &cl);

	/* Run Multiband DRC function */
	multiband_drc_process(dev, sourceb, sinkb, cl.frames, cl.source_bytes, cl.sink_bytes);

	return 0;
}

static int multiband_drc_prepare(struct comp_dev *dev)
{
	struct multiband_drc_comp_data *cd = comp_get_drvdata(dev);
	struct sof_ipc_comp_config *config = dev_comp_config(dev);
	struct comp_buffer *sourceb;
	struct comp_buffer *sinkb;
	uint32_t sink_period_bytes;
	int ret;

	comp_info(dev, "multiband_drc_prepare()");

	ret = comp_set_state(dev, COMP_TRIGGER_PREPARE);
	if (ret < 0)
		return ret;

	if (ret == COMP_STATUS_STATE_ALREADY_SET)
		return PPL_STATUS_PATH_STOP;

	/* DRC component will only ever have 1 source and 1 sink buffer */
	sourceb = list_first_item(&dev->bsource_list,
				  struct comp_buffer, sink_list);
	sinkb = list_first_item(&dev->bsink_list,
				struct comp_buffer, source_list);

	/* get source data format */
	cd->source_format = sourceb->stream.frame_fmt;

	/* Initialize DRC */
	comp_dbg(dev, "multiband_drc_prepare(), source_format=%d, sink_format=%d",
		 cd->source_format, cd->source_format);
	cd->config = comp_get_data_blob(cd->model_handler, NULL, NULL);
	if (cd->config) {
		ret = multiband_drc_setup(cd, sourceb->stream.channels, sourceb->stream.rate);
		if (ret < 0) {
			comp_err(dev, "multiband_drc_prepare() error: multiband_drc_setup failed.");
			goto err;
		}

		cd->multiband_drc_func = multiband_drc_find_proc_func(cd->source_format);
		if (!cd->multiband_drc_func) {
			comp_err(dev, "multiband_drc_prepare(), No proc func");
			ret = -EINVAL;
			goto err;
		}

		cd->crossover_split = crossover_find_split_func(cd->config->num_bands);
		if (!cd->crossover_split) {
			comp_err(dev, "multiband_drc_prepare(), No crossover_split for band num %i",
				 cd->config->num_bands);
			ret = -EINVAL;
			goto err;
		}
	} else {
		cd->multiband_drc_func = multiband_drc_find_proc_func_pass(cd->source_format);
		if (!cd->multiband_drc_func) {
			comp_err(dev, "multiband_drc_prepare(), No proc func passthrough");
			ret = -EINVAL;
			goto err;
		}
	}

	/* validate sink data format and period bytes */
	if (cd->source_format != sinkb->stream.frame_fmt) {
		comp_err(dev,
			 "multiband_drc_prepare(): Source fmt %d and sink fmt %d are different.",
			 cd->source_format, sinkb->stream.frame_fmt);
		ret = -EINVAL;
		goto err;
	}

	sink_period_bytes = audio_stream_period_bytes(&sinkb->stream,
						      dev->frames);

	if (sinkb->stream.size < config->periods_sink * sink_period_bytes) {
		comp_err(dev, "multiband_drc_prepare(), sink buffer size %d is insufficient",
			 sinkb->stream.size);
		ret = -ENOMEM;
		goto err;
	}

	return 0;

err:
	comp_set_state(dev, COMP_TRIGGER_RESET);
	return ret;
}

static int multiband_drc_reset(struct comp_dev *dev)
{
	struct multiband_drc_comp_data *cd = comp_get_drvdata(dev);

	comp_info(dev, "multiband_drc_reset()");

	multiband_drc_reset_state(&cd->state);

	cd->multiband_drc_func = NULL;
	cd->crossover_split = NULL;

	comp_set_state(dev, COMP_TRIGGER_RESET);
	return 0;
}

static const struct comp_driver comp_multiband_drc = {
	.uid = SOF_RT_UUID(multiband_drc_uuid),
	.tctx = &multiband_drc_tr,
	.ops = {
		.create  = multiband_drc_new,
		.free    = multiband_drc_free,
		.params  = multiband_drc_params,
		.cmd     = multiband_drc_cmd,
		.trigger = multiband_drc_trigger,
		.copy    = multiband_drc_copy,
		.prepare = multiband_drc_prepare,
		.reset   = multiband_drc_reset,
	},
};

static SHARED_DATA struct comp_driver_info comp_multiband_drc_info = {
	.drv = &comp_multiband_drc,
};

UT_STATIC void sys_comp_multiband_drc_init(void)
{
	comp_register(platform_shared_get(&comp_multiband_drc_info,
					  sizeof(comp_multiband_drc_info)));
}

DECLARE_MODULE(sys_comp_multiband_drc_init);
