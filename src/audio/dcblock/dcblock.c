// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Google LLC. All rights reserved.
//
// Author: Sebastiano Carlucci <scarlucci@google.com>

#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/audio/format.h>
#include <sof/audio/pipeline.h>
#include <sof/audio/dcblock/dcblock.h>
#include <sof/common.h>
#include <sof/debug/panic.h>
#include <sof/ipc/msg.h>
#include <sof/lib/alloc.h>
#include <sof/lib/memory.h>
#include <sof/lib/uuid.h>
#include <sof/list.h>
#include <sof/platform.h>
#include <sof/string.h>
#include <sof/ut.h>
#include <sof/trace/trace.h>
#include <ipc/control.h>
#include <ipc/stream.h>
#include <ipc/topology.h>
#include <user/trace.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

static const struct comp_driver comp_dcblock;

/* b809efaf-5681-42b1-9ed6-04bb012dd384 */
DECLARE_SOF_RT_UUID("dcblock", dcblock_uuid, 0xb809efaf, 0x5681, 0x42b1,
		 0x9e, 0xd6, 0x04, 0xbb, 0x01, 0x2d, 0xd3, 0x84);

DECLARE_TR_CTX(dcblock_tr, SOF_UUID(dcblock_uuid), LOG_LEVEL_INFO);

/**
 * \brief Sets the DC Blocking filter in pass through mode.
 * The frequency response of a DCB filter is:
 * H(z) = (1 - z^-1)/(1-Rz^-1).
 * Setting R to 1 makes the filter act as a passthrough component.
 */
static void dcblock_set_passthrough(struct comp_data *cd)
{
	comp_cl_info(&comp_dcblock, "dcblock_set_passthrough()");
	int i;

	for (i = 0; i < PLATFORM_MAX_CHANNELS; i++)
		cd->R_coeffs[i] = ONE_Q2_30;
}

/**
 * \brief Initializes the state of the DC Blocking Filter
 */
static void dcblock_init_state(struct comp_data *cd)
{
	memset(cd->state, 0, sizeof(cd->state));
}

/**
 * \brief Creates DC Blocking Filter component.
 * \return Pointer to DC Blocking Filter component device.
 */
static struct comp_dev *dcblock_new(const struct comp_driver *drv,
				    struct sof_ipc_comp *comp)
{
	struct comp_dev *dev;
	struct comp_data *cd;
	struct sof_ipc_comp_process *dcblock;
	struct sof_ipc_comp_process *ipc_dcblock =
		(struct sof_ipc_comp_process *)comp;
	size_t bs = ipc_dcblock->size;
	int ret;

	comp_cl_info(&comp_dcblock, "dcblock_new()");

	dev = comp_alloc(drv, COMP_SIZE(struct sof_ipc_comp_process));
	if (!dev)
		return NULL;

	dcblock = COMP_GET_IPC(dev, sof_ipc_comp_process);
	ret = memcpy_s(dcblock, sizeof(*dcblock), ipc_dcblock,
		       sizeof(struct sof_ipc_comp_process));
	assert(!ret);

	cd = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(*cd));
	if (!cd) {
		rfree(dev);
		return NULL;
	}

	comp_set_drvdata(dev, cd);

	cd->dcblock_func = NULL;
	/**
	 * Copy over the coefficients from the blob to cd->R_coeffs
	 * Set passthrough if the size of the blob is invalid
	 */
	if (bs == sizeof(cd->R_coeffs)) {
		ret = memcpy_s(cd->R_coeffs, bs, ipc_dcblock->data, bs);
		assert(!ret);
	} else {
		if (bs > 0)
			comp_cl_warn(&comp_dcblock, "dcblock_new(), binary blob size %i, expected %i",
				     bs, sizeof(cd->R_coeffs));
		dcblock_set_passthrough(cd);
	}

	dev->state = COMP_STATE_READY;
	return dev;
}

/**
 * \brief Frees DC Blocking Filter component.
 * \param[in,out] dev DC Blocking Filter base component device.
 */
static void dcblock_free(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	comp_info(dev, "dcblock_free()");
	rfree(cd);
	rfree(dev);
}

static int dcblock_verify_params(struct comp_dev *dev,
				 struct sof_ipc_stream_params *params)
{
	int ret;

	comp_dbg(dev, "dcblock_verify_params()");

	ret = comp_verify_params(dev, 0, params);
	if (ret < 0) {
		comp_err(dev, "dcblock_verify_params() error: comp_verify_params() failed.");
		return ret;
	}

	return 0;
}

/**
 * \brief Sets DC Blocking Filter component audio stream parameters.
 * \param[in,out] dev DC Blocking Filter base component device.
 * \return Error code.
 *
 * All done in prepare() since we need to know source and sink component params.
 */
static int dcblock_params(struct comp_dev *dev,
			  struct sof_ipc_stream_params *params)
{
	int err;

	comp_dbg(dev, "dcblock_params()");

	err = dcblock_verify_params(dev, params);
	if (err < 0) {
		comp_err(dev, "dcblock_params(): pcm params verification failed");
		return -EINVAL;
	}

	return 0;
}

static int dcblock_cmd_get_data(struct comp_dev *dev,
				struct sof_ipc_ctrl_data *cdata,
				size_t max_size)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	size_t resp_size;
	int ret = 0;

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_BINARY:
		comp_info(dev, "dcblock_cmd_get_data(), SOF_CTRL_CMD_BINARY");

		/* Copy coefficients back to user space */
		resp_size = sizeof(cd->R_coeffs);
		comp_info(dev, "dcblock_cmd_get_data(), resp_size %u",
			  resp_size);

		if (resp_size > max_size) {
			comp_err(dev, "response size %i exceeds maximum size %i ",
				 resp_size, max_size);
			ret = -EINVAL;
			break;
		}

		ret = memcpy_s(cdata->data->data, cdata->data->size,
			       cd->R_coeffs, resp_size);
		assert(!ret);

		cdata->data->abi = SOF_ABI_VERSION;
		cdata->data->size = resp_size;
		break;
	default:
		comp_err(dev, "dcblock_cmd_get_data(), invalid command");
		ret = -EINVAL;
	}

	return ret;
}

static int dcblock_cmd_set_data(struct comp_dev *dev,
				struct sof_ipc_ctrl_data *cdata)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	size_t req_size = sizeof(cd->R_coeffs);
	int ret = 0;

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_BINARY:
		comp_info(dev, "dcblock_cmd_set_data(), SOF_CTRL_CMD_BINARY");

		/* Retrieve the binary controls from the packet */
		ret = memcpy_s(cd->R_coeffs, req_size, cdata->data->data,
			       req_size);
		assert(!ret);

		break;
	default:
		comp_err(dev, "dcblock_set_data(), invalid command %i",
			 cdata->cmd);
		ret = -EINVAL;
	}

	return ret;
}

/**
 * \brief Handles incoming IPC commands for DC Blocking Filter component.
 */
static int dcblock_cmd(struct comp_dev *dev, int cmd, void *data,
		       int max_data_size)
{
	struct sof_ipc_ctrl_data *cdata = ASSUME_ALIGNED(data, 4);
	int ret = 0;

	comp_info(dev, "dcblock_cmd()");

	switch (cmd) {
	case COMP_CMD_SET_DATA:
		ret = dcblock_cmd_set_data(dev, cdata);
		break;
	case COMP_CMD_GET_DATA:
		ret = dcblock_cmd_get_data(dev, cdata, max_data_size);
		break;
	default:
		comp_err(dev, "dcblock_cmd(), invalid command (%i)", cmd);
		ret = -EINVAL;
	}

	return ret;
}

/**
 * \brief Sets DC Blocking Filter component state.
 * \param[in,out] dev DC Blocking Filter base component device.
 * \param[in] cmd Command type.
 * \return Error code.
 */
static int dcblock_trigger(struct comp_dev *dev, int cmd)
{
	comp_info(dev, "dcblock_trigger()");

	return comp_set_state(dev, cmd);
}

static void dcblock_process(struct comp_dev *dev, struct comp_buffer *source,
			    struct comp_buffer *sink, int frames,
			    uint32_t source_bytes, uint32_t sink_bytes)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	buffer_invalidate(source, source_bytes);

	cd->dcblock_func(dev, &source->stream, &sink->stream, frames);

	buffer_writeback(sink, sink_bytes);

	/* calc new free and available */
	comp_update_buffer_consume(source, source_bytes);
	comp_update_buffer_produce(sink, sink_bytes);
}

/**
 * \brief Copies and processes stream data.
 * \param[in,out] dev DC Blocking Filter base component device.
 * \return Error code.
 */
static int dcblock_copy(struct comp_dev *dev)
{
	struct comp_copy_limits cl;
	struct comp_buffer *sourceb;
	struct comp_buffer *sinkb;

	comp_dbg(dev, "dcblock_copy()");

	sourceb = list_first_item(&dev->bsource_list, struct comp_buffer,
				  sink_list);
	sinkb = list_first_item(&dev->bsink_list, struct comp_buffer,
				source_list);

	/* Get source, sink, number of frames etc. to process. */
	comp_get_copy_limits_with_lock(sourceb, sinkb, &cl);

	dcblock_process(dev, sourceb, sinkb,
			cl.frames, cl.source_bytes, cl.sink_bytes);

	return 0;
}

/**
 * \brief Prepares DC Blocking Filter component for processing.
 * \param[in,out] dev DC Blocking Filter base component device.
 * \return Error code.
 */
static int dcblock_prepare(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct sof_ipc_comp_config *config = dev_comp_config(dev);
	struct comp_buffer *sourceb;
	struct comp_buffer *sinkb;
	uint32_t sink_period_bytes;
	int ret;

	comp_info(dev, "dcblock_prepare()");

	ret = comp_set_state(dev, COMP_TRIGGER_PREPARE);
	if (ret < 0)
		return ret;

	if (ret == COMP_STATUS_STATE_ALREADY_SET)
		return PPL_STATUS_PATH_STOP;

	/* DC Filter component will only ever have one source and sink buffer */
	sourceb = list_first_item(&dev->bsource_list,
				  struct comp_buffer, sink_list);
	sinkb = list_first_item(&dev->bsink_list,
				struct comp_buffer, source_list);

	/* get source data format */
	cd->source_format = sourceb->stream.frame_fmt;

	/* get sink data format and period bytes */
	cd->sink_format = sinkb->stream.frame_fmt;
	sink_period_bytes =
			audio_stream_period_bytes(&sinkb->stream, dev->frames);

	if (sinkb->stream.size < config->periods_sink * sink_period_bytes) {
		comp_err(dev, "dcblock_prepare(): sink buffer size %d is insufficient < %d * %d",
			 sinkb->stream.size, config->periods_sink, sink_period_bytes);
		ret = -ENOMEM;
		goto err;
	}

	dcblock_init_state(cd);

	cd->dcblock_func = dcblock_find_func(cd->source_format);
	if (!cd->dcblock_func) {
		comp_err(dev, "dcblock_prepare(), No processing function matching frames format");
		ret = -EINVAL;
		goto err;
	}

	comp_info(dev, "dcblock_prepare(), source_format=%d, sink_format=%d",
		  cd->source_format, cd->sink_format);

	return 0;

err:
	comp_set_state(dev, COMP_TRIGGER_RESET);
	return ret;
}

/**
 * \brief Resets DC Blocking Filter component.
 * \param[in,out] dev DC Blocking Filter base component device.
 * \return Error code.
 */
static int dcblock_reset(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	comp_info(dev, "dcblock_reset()");

	dcblock_init_state(cd);
	comp_set_state(dev, COMP_TRIGGER_RESET);

	return 0;
}

/** \brief DC Blocking Filter component definition. */
static const struct comp_driver comp_dcblock = {
	.type = SOF_COMP_DCBLOCK,
	.uid  = SOF_RT_UUID(dcblock_uuid),
	.tctx = &dcblock_tr,
	.ops  = {
		 .create	= dcblock_new,
		 .free		= dcblock_free,
		 .params	= dcblock_params,
		 .cmd		= dcblock_cmd,
		 .trigger	= dcblock_trigger,
		 .copy		= dcblock_copy,
		 .prepare	= dcblock_prepare,
		 .reset		= dcblock_reset,
	},
};

static SHARED_DATA struct comp_driver_info comp_dcblock_info = {
	.drv = &comp_dcblock,
};

UT_STATIC void sys_comp_dcblock_init(void)
{
	comp_register(platform_shared_get(&comp_dcblock_info,
					  sizeof(comp_dcblock_info)));
}

DECLARE_MODULE(sys_comp_dcblock_init);
