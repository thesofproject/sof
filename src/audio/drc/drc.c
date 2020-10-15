// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Google LLC. All rights reserved.
//
// Author: Pin-chih Lin <johnylin@google.com>

#include <sof/audio/component.h>
#include <sof/audio/buffer.h>
#include <sof/audio/drc/drc.h>
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

static const struct comp_driver comp_drc;

/* b36ee4da-006f-47f9-a06d-fecbe2d8b6ce */
DECLARE_SOF_RT_UUID("drc", drc_uuid, 0xb36ee4da, 0x006f, 0x47f9,
		    0xa0, 0x6d, 0xfe, 0xcb, 0xe2, 0xd8, 0xb6, 0xce);

DECLARE_TR_CTX(drc_tr, SOF_UUID(drc_uuid), LOG_LEVEL_INFO);

static inline void drc_reset_state(struct drc_state *state)
{
	int i;

	rfree(state->pre_delay_buffers[0]);
	for (i = 0; i < PLATFORM_MAX_CHANNELS; ++i) {
		state->pre_delay_buffers[i] = NULL;
	}

	state->detector_average = 0;
	state->compressor_gain = Q_CONVERT_FLOAT(1.0f, 30);

	state->last_pre_delay_frames = DRC_DEFAULT_PRE_DELAY_FRAMES;
	state->pre_delay_read_index = 0;
	state->pre_delay_write_index = DRC_DEFAULT_PRE_DELAY_FRAMES;

	state->envelope_rate = 0;
	state->scaled_desired_gain = 0;

	state->processed = 0;

	state->max_attack_compression_diff_db = INT32_MIN;

	state->max_l2d = 0;
	state->max_l2d_o = 0;
	state->min_l2d_o = 0;
	state->max_logf = 0;
	state->max_logf_o = 0;
	state->min_logf_o = 0;
	state->max_asin = 0;
	state->max_pow_x = 0;
}

static inline int drc_init_pre_delay_buffers(struct drc_state *state,
					     size_t sample_bytes,
					     int channels)
{
	size_t bytes_per_channel = sample_bytes * DRC_MAX_PRE_DELAY_FRAMES;
	size_t bytes_total = bytes_per_channel * channels;
	int i;

	/* Allocate pre-delay (lookahead) buffers */
	state->pre_delay_buffers[0] =
		rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, bytes_total);
	if (!state->pre_delay_buffers[0])
		return -ENOMEM;

	memset(state->pre_delay_buffers[0], 0, bytes_total);

	for (i = 1; i < channels; ++i) {
		state->pre_delay_buffers[i] =
			state->pre_delay_buffers[i - 1] + bytes_per_channel;
	}

	return 0;
}

static inline int drc_set_pre_delay_time(struct drc_state *state,
					 int32_t pre_delay_time,
					 int32_t rate)
{
	int32_t pre_delay_frames;

	/* Re-configure look-ahead section pre-delay if delay time has changed. */
	pre_delay_frames = Q_MULTSR_32X32((int64_t)pre_delay_time, rate, 30, 0, 0);
	if (pre_delay_frames < 0)
		return -EINVAL;
	pre_delay_frames = MIN(pre_delay_frames, DRC_MAX_PRE_DELAY_FRAMES - 1);

	/* Make pre_delay_frames multiplies of DIVISION_FRAMES. This way we
	 * won't split a division of samples into two blocks of memory, so it is
	 * easier to process. This may make the actual delay time slightly less
	 * than the specified value, but the difference is less than 1ms. */
	pre_delay_frames &= ~DRC_DIVISION_FRAMES_MASK;

	/* We need at least one division buffer, so the incoming data won't
	 * overwrite the output data */
	pre_delay_frames = MAX(pre_delay_frames, DRC_DIVISION_FRAMES);

	if (state->last_pre_delay_frames != pre_delay_frames) {
		state->last_pre_delay_frames = pre_delay_frames;
		state->pre_delay_read_index = 0;
		state->pre_delay_write_index = pre_delay_frames;
	}
	return 0;
}

static int drc_setup(struct drc_comp_data *cd, uint16_t channels, uint32_t rate)
{
	uint32_t sample_bytes = get_sample_bytes(cd->source_format);
	int ret = 0;

	/* Reset any previous state */
	drc_reset_state(&cd->state);

	/* Allocate pre-delay buffers */
	ret = drc_init_pre_delay_buffers(&cd->state, (size_t)sample_bytes, (int)channels);
	if (ret < 0)
		return ret;

	/* Set pre-dely time */
	return drc_set_pre_delay_time(&cd->state, cd->config->params.pre_delay_time, rate);
}

/*
 * End of DRC setup code. Next the standard component methods.
 */

static struct comp_dev *drc_new(const struct comp_driver *drv,
				struct sof_ipc_comp *comp)
{
	struct comp_dev *dev;
	struct drc_comp_data *cd;
	struct sof_ipc_comp_process *ipc_drc =
		(struct sof_ipc_comp_process *)comp;
	size_t bs = ipc_drc->size;
	int ret;

	comp_cl_info(&comp_drc, "drc_new()");

	/* Check first before proceeding with dev and cd that coefficients
	 * blob size is sane.
	 */
	if (bs > SOF_DRC_MAX_SIZE) {
		comp_cl_err(&comp_drc, "drc_new(), error: configuration blob size = %u > %d",
			    bs, SOF_DRC_MAX_SIZE);
		return NULL;
	}

	dev = comp_alloc(drv, COMP_SIZE(struct sof_ipc_comp_process));
	if (!dev)
		return NULL;

	ret = memcpy_s(COMP_GET_IPC(dev, sof_ipc_comp_process),
		       sizeof(struct sof_ipc_comp_process), ipc_drc,
		       sizeof(struct sof_ipc_comp_process));
	assert(!ret);

	dev->state = COMP_STATE_INIT;

	cd = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(*cd));
	if (!cd) {
		rfree(dev);
		return NULL;
	}

	comp_set_drvdata(dev, cd);

	cd->drc_func = NULL;

	/* Handler for configuration data */
	cd->model_handler = comp_data_blob_handler_new(dev);
	if (!cd->model_handler) {
		comp_cl_err(&comp_drc, "drc_new(): comp_data_blob_handler_new() failed.");
		rfree(dev);
		rfree(cd);
		return NULL;
	}

	/* Get configuration data and reset DRC state */
	ret = comp_init_data_blob(cd->model_handler, bs, ipc_drc->data);
	if (ret < 0) {
		comp_cl_err(&comp_drc, "drc_new(): comp_init_data_blob() failed.");
		rfree(dev);
		rfree(cd);
		return NULL;
	}
	drc_reset_state(&cd->state);

	dev->state = COMP_STATE_READY;
	return dev;
}

static void drc_free(struct comp_dev *dev)
{
	struct drc_comp_data *cd = comp_get_drvdata(dev);

	comp_info(dev, "drc_free()");

	comp_data_blob_handler_free(cd->model_handler);

	rfree(cd);
	rfree(dev);
}

static int drc_params(struct comp_dev *dev,
		      struct sof_ipc_stream_params *params)
{
	int ret;

	comp_info(dev, "drc_params()");

	comp_dbg(dev, "drc_verify_params()");
	ret = comp_verify_params(dev, 0, params);
	if (ret < 0) {
		comp_err(dev, "drc_params(): comp_verify_params() failed.");
		return -EINVAL;
	}

	/* All configuration work is postponed to prepare(). */
	return 0;
}

static int drc_cmd_get_data(struct comp_dev *dev,
			    struct sof_ipc_ctrl_data *cdata, int max_size)
{
	struct drc_comp_data *cd = comp_get_drvdata(dev);
	int ret = 0;

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_BINARY:
		comp_info(dev, "drc_cmd_get_data(), SOF_CTRL_CMD_BINARY");
		ret = comp_data_blob_get_cmd(cd->model_handler, cdata, max_size);
		break;
	default:
		comp_err(dev, "drc_cmd_get_data() error: invalid cdata->cmd");
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int drc_cmd_set_data(struct comp_dev *dev,
			    struct sof_ipc_ctrl_data *cdata)
{
	struct drc_comp_data *cd = comp_get_drvdata(dev);
	int ret = 0;

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_BINARY:
		comp_info(dev, "drc_cmd_set_data(), SOF_CTRL_CMD_BINARY");
		ret = comp_data_blob_set_cmd(cd->model_handler, cdata);
		break;
	default:
		comp_err(dev, "drc_cmd_set_data() error: invalid cdata->cmd");
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int drc_cmd(struct comp_dev *dev, int cmd, void *data,
		   int max_data_size)
{
	struct sof_ipc_ctrl_data *cdata = data;
	int ret = 0;

	comp_info(dev, "drc_cmd()");

	switch (cmd) {
	case COMP_CMD_SET_DATA:
		ret = drc_cmd_set_data(dev, cdata);
		break;
	case COMP_CMD_GET_DATA:
		ret = drc_cmd_get_data(dev, cdata, max_data_size);
		break;
	default:
		comp_err(dev, "drc_cmd(), invalid command");
		ret = -EINVAL;
	}

	return ret;
}

static int drc_trigger(struct comp_dev *dev, int cmd)
{
	int ret;

	comp_info(dev, "drc_trigger()");

	ret = comp_set_state(dev, cmd);
	if (ret == COMP_STATUS_STATE_ALREADY_SET)
		ret = PPL_STATUS_PATH_STOP;

	return ret;
}

static void drc_process(struct comp_dev *dev, struct comp_buffer *source,
			struct comp_buffer *sink, int frames,
			uint32_t source_bytes, uint32_t sink_bytes)
{
	struct drc_comp_data *cd = comp_get_drvdata(dev);

	buffer_invalidate(source, source_bytes);

	cd->drc_func(dev, &source->stream, &sink->stream, frames);

	buffer_writeback(sink, sink_bytes);

	/* calc new free and available */
	comp_update_buffer_consume(source, source_bytes);
	comp_update_buffer_produce(sink, sink_bytes);
}

/* copy and process stream data from source to sink buffers */
static int drc_copy(struct comp_dev *dev)
{
	struct comp_copy_limits cl;
	struct drc_comp_data *cd = comp_get_drvdata(dev);
	struct comp_buffer *sourceb;
	struct comp_buffer *sinkb;
	int ret;

	comp_dbg(dev, "drc_copy()");

	sourceb = list_first_item(&dev->bsource_list, struct comp_buffer,
				  sink_list);
	sinkb = list_first_item(&dev->bsink_list, struct comp_buffer,
				source_list);

	/* Check for changed configuration */
	if (comp_is_new_data_blob_available(cd->model_handler)) {
		cd->config = comp_get_data_blob(cd->model_handler, NULL, NULL);
		ret = drc_setup(cd, sourceb->stream.channels, sourceb->stream.rate);
		if (ret < 0) {
			comp_err(dev, "drc_copy(), failed DRC setup");
			return ret;
		}
	}

	/* Get source, sink, number of frames etc. to process. */
	comp_get_copy_limits_with_lock(sourceb, sinkb, &cl);

	/* Run DRC function */
	drc_process(dev, sourceb, sinkb, cl.frames, cl.source_bytes,
		    cl.sink_bytes);

	return 0;
}

static int drc_prepare(struct comp_dev *dev)
{
	struct drc_comp_data *cd = comp_get_drvdata(dev);
	struct sof_ipc_comp_config *config = dev_comp_config(dev);
	struct comp_buffer *sourceb;
	struct comp_buffer *sinkb;
	uint32_t sink_period_bytes;
	int ret;

	comp_info(dev, "drc_prepare()");

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
	comp_info(dev, "drc_prepare(), source_format=%d, sink_format=%d",
		  cd->source_format, cd->source_format);
	cd->config = comp_get_data_blob(cd->model_handler, NULL, NULL);
	if (cd->config) {
		ret = drc_setup(cd, sourceb->stream.channels, sourceb->stream.rate);
		if (ret < 0) {
			comp_err(dev, "drc_prepare() error: drc_setup failed.");
			goto err;
		}

		cd->drc_func = drc_find_proc_func(cd->source_format);
		if (!cd->drc_func) {
			comp_err(dev, "drc_prepare(), No proc func");
			ret = -EINVAL;
			goto err;
		}
	} else {
		cd->drc_func = drc_find_proc_func_pass(cd->source_format);
		if (!cd->drc_func) {
			comp_err(dev, "drc_prepare(), No proc func passthrough");
			ret = -EINVAL;
			goto err;
		}
	}
	comp_info(dev, "drc_prepare(), DRC is configured.");

	/* validate sink data format and period bytes */
	if (cd->source_format != sinkb->stream.frame_fmt) {
		comp_err(dev, "drc_prepare(): Source fmt %d and sink fmt %d are different.",
			 cd->source_format, sinkb->stream.frame_fmt);
		ret = -EINVAL;
		goto err;
	}

	sink_period_bytes = audio_stream_period_bytes(&sinkb->stream,
						      dev->frames);

	if (sinkb->stream.size < config->periods_sink * sink_period_bytes) {
		comp_err(dev, "drc_prepare(), sink buffer size %d is insufficient",
			 sinkb->stream.size);
		ret = -ENOMEM;
		goto err;
	}

	return 0;

err:
	comp_set_state(dev, COMP_TRIGGER_RESET);
	return ret;
}

static int drc_reset(struct comp_dev *dev)
{
	struct drc_comp_data *cd = comp_get_drvdata(dev);

	comp_info(dev, "drc_reset()");

	comp_err(dev, "drc_reset(): max lin2db input = %f", cd->state.max_l2d);
	comp_err(dev, "drc_reset(): max lin2db output = %f, min = %f", cd->state.max_l2d_o, cd->state.min_l2d_o);
	comp_err(dev, "drc_reset(): max logf input = %f", cd->state.max_logf);
	comp_err(dev, "drc_reset(): max logf output = %f, min = %f", cd->state.max_logf_o, cd->state.min_logf_o);
	comp_err(dev, "drc_reset(): max asin input = %f", cd->state.max_asin);
	comp_err(dev, "drc_reset(): max powf input x = %f", cd->state.max_pow_x);

	drc_reset_state(&cd->state);

	cd->drc_func = NULL;

	comp_set_state(dev, COMP_TRIGGER_RESET);
	return 0;
}

static const struct comp_driver comp_drc = {
	.uid = SOF_RT_UUID(drc_uuid),
	.tctx = &drc_tr,
	.ops = {
		.create  = drc_new,
		.free    = drc_free,
		.params  = drc_params,
		.cmd     = drc_cmd,
		.trigger = drc_trigger,
		.copy    = drc_copy,
		.prepare = drc_prepare,
		.reset   = drc_reset,
	},
};

static SHARED_DATA struct comp_driver_info comp_drc_info = {
	.drv = &comp_drc,
};

UT_STATIC void sys_comp_drc_init(void)
{
	comp_register(platform_shared_get(&comp_drc_info,
					  sizeof(comp_drc_info)));
}

DECLARE_MODULE(sys_comp_drc_init);
