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
#include <sof/common.h>
#include <rtos/panic.h>
#include <sof/ipc/msg.h>
#include <rtos/alloc.h>
#include <rtos/init.h>
#include <sof/lib/uuid.h>
#include <sof/list.h>
#include <sof/platform.h>
#include <rtos/string.h>
#include <sof/ut.h>
#include <sof/trace/trace.h>
#include <ipc/control.h>
#include <ipc/stream.h>
#include <ipc/topology.h>
#include <user/trace.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

#include "dcblock.h"

LOG_MODULE_REGISTER(dcblock, CONFIG_SOF_LOG_LEVEL);

SOF_DEFINE_REG_UUID(dcblock);

DECLARE_TR_CTX(dcblock_tr, SOF_UUID(dcblock_uuid), LOG_LEVEL_INFO);

/**
 * \brief Sets the DC Blocking filter in pass through mode.
 * The frequency response of a DCB filter is:
 * H(z) = (1 - z^-1)/(1-Rz^-1).
 * Setting R to 1 makes the filter act as a passthrough component.
 */
static void dcblock_set_passthrough(struct processing_module *mod)
{
	struct comp_data *cd = module_get_private_data(mod);

	comp_info(mod->dev, "dcblock_set_passthrough()");
	int i;

	for (i = 0; i < PLATFORM_MAX_CHANNELS; i++)
		cd->R_coeffs[i] = ONE_Q2_30;
}

/**
 * \brief Copy the DC Blocking filter coefficients from received
 * configuration blob.
 */
static void dcblock_copy_coefficients(struct processing_module *mod)
{
	struct comp_data *cd = module_get_private_data(mod);

	memcpy_s(cd->R_coeffs, sizeof(cd->R_coeffs), cd->config, sizeof(cd->R_coeffs));
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
 * \return success.
 */
static int dcblock_init(struct processing_module *mod)
{
	struct module_data *md = &mod->priv;
	struct comp_dev *dev = mod->dev;
	struct module_config *ipc_dcblock = &md->cfg;
	struct comp_data *cd;
	size_t bs = ipc_dcblock->size;
	int ret;

	comp_info(dev, "dcblock_init()");

	cd = rzalloc(SOF_MEM_FLAG_USER, sizeof(*cd));
	if (!cd)
		return -ENOMEM;

	md->private = cd;
	cd->dcblock_func = NULL;

	/* component model data handler */
	cd->model_handler = comp_data_blob_handler_new(dev);
	if (!cd->model_handler) {
		comp_err(dev, "comp_data_blob_handler_new() failed.");
		ret = -ENOMEM;
		goto err_cd;
	}

	ret = comp_init_data_blob(cd->model_handler, bs, ipc_dcblock->data);
	if (ret < 0) {
		comp_err(dev, "comp_init_data_blob() failed with error: %d", ret);
		goto err_model_cd;
	}

	return 0;

err_model_cd:
	comp_data_blob_handler_free(cd->model_handler);

err_cd:
	rfree(cd);
	return ret;
}

/**
 * \brief Frees DC Blocking Filter component.
 * \param[in,out] dev DC Blocking Filter base component device.
 */
static int dcblock_free(struct processing_module *mod)
{
	struct comp_data *cd = module_get_private_data(mod);

	comp_info(mod->dev, "dcblock_free()");
	comp_data_blob_handler_free(cd->model_handler);
	rfree(cd);
	return 0;
}

/**
 * \brief Handles incoming get commands for DC Blocking Filter component.
 */
static int dcblock_get_config(struct processing_module *mod,
			      uint32_t config_id, uint32_t *data_offset_size,
			      uint8_t *fragment, size_t fragment_size)
{
	return dcblock_get_ipc_config(mod, fragment, fragment_size);
}

/**
 * \brief Handles incoming set commands for DC Blocking Filter component.
 */
static int dcblock_set_config(struct processing_module *mod, uint32_t config_id,
			      enum module_cfg_fragment_position pos, uint32_t data_offset_size,
			      const uint8_t *fragment, size_t fragment_size, uint8_t *response,
			      size_t response_size)
{

	return dcblock_set_ipc_config(mod, pos, data_offset_size, fragment, fragment_size);
}

/**
 * \brief Copies and processes stream data.
 * \param[in,out] dev DC Blocking Filter module.
 * \return Error code.
 */
static int dcblock_process(struct processing_module *mod,
			   struct input_stream_buffer *input_buffers,
			   int num_input_buffers,
			   struct output_stream_buffer *output_buffers,
			   int num_output_buffers)
{
	struct comp_data *cd = module_get_private_data(mod);
	struct audio_stream *source = input_buffers[0].data;
	struct audio_stream *sink = output_buffers[0].data;
	uint32_t frames = input_buffers[0].size;

	comp_dbg(mod->dev, "dcblock_process()");

	cd->dcblock_func(cd, source, sink, frames);

	module_update_buffer_position(&input_buffers[0], &output_buffers[0], frames);
	return 0;
}

/**
 * \brief Prepares DC Blocking Filter component for processing.
 * \param[in,out] dev DC Blocking Filter base component device.
 * \return Error code.
 */
static int dcblock_prepare(struct processing_module *mod,
			   struct sof_source **sources, int num_of_sources,
			   struct sof_sink **sinks, int num_of_sinks)
{
	struct comp_data *cd = module_get_private_data(mod);
	struct comp_buffer *sourceb, *sinkb;
	struct comp_dev *dev = mod->dev;

	comp_info(dev, "dcblock_prepare()");

	/* DC Filter component will only ever have one source and sink buffer */
	sourceb = comp_dev_get_first_data_producer(dev);
	sinkb = comp_dev_get_first_data_consumer(dev);
	if (!sourceb || !sinkb) {
		comp_err(dev, "no source or sink buffer");
		return -ENOTCONN;
	}

	dcblock_params(mod);

	/* get source data format */
	cd->source_format = audio_stream_get_frm_fmt(&sourceb->stream);

	/* get sink data format and period bytes */
	cd->sink_format = audio_stream_get_frm_fmt(&sinkb->stream);

	dcblock_init_state(cd);
	cd->dcblock_func = dcblock_find_func(cd->source_format);
	if (!cd->dcblock_func) {
		comp_err(dev, "dcblock_prepare(), No processing function matching frames format");
		return -EINVAL;
	}

	comp_info(mod->dev, "dcblock_prepare(), source_format=%d, sink_format=%d",
		  cd->source_format, cd->sink_format);

	cd->config = comp_get_data_blob(cd->model_handler, NULL, NULL);
	if (cd->config)
		dcblock_copy_coefficients(mod);
	else
		dcblock_set_passthrough(mod);

	return 0;
}

/**
 * \brief Resets DC Blocking Filter component.
 * \param[in,out] dev DC Blocking Filter base component device.
 * \return Error code.
 */
static int dcblock_reset(struct processing_module *mod)
{
	struct comp_data *cd = module_get_private_data(mod);

	comp_info(mod->dev, "dcblock_reset()");

	dcblock_init_state(cd);

	cd->dcblock_func = NULL;

	return 0;
}

static const struct module_interface dcblock_interface = {
	.init = dcblock_init,
	.prepare = dcblock_prepare,
	.process_audio_stream = dcblock_process,
	.set_configuration = dcblock_set_config,
	.get_configuration = dcblock_get_config,
	.reset = dcblock_reset,
	.free = dcblock_free,
};

#if CONFIG_COMP_DCBLOCK_MODULE
/* modular: llext dynamic link */

#include <module/module/api_ver.h>
#include <module/module/llext.h>
#include <rimage/sof/user/manifest.h>

SOF_LLEXT_MOD_ENTRY(dcblock, &dcblock_interface);

static const struct sof_man_module_manifest mod_manifest __section(".module") __used =
	SOF_LLEXT_MODULE_MANIFEST("DCBLOCK", dcblock_llext_entry, 1, SOF_REG_UUID(dcblock), 40);

SOF_LLEXT_BUILDINFO;

#else

DECLARE_MODULE_ADAPTER(dcblock_interface, dcblock_uuid, dcblock_tr);
SOF_MODULE_INIT(dcblock, sys_comp_module_dcblock_interface_init);

#endif
