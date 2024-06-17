// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2017-2022 Intel Corporation. All rights reserved.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
//         Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>

#include "eq_iir.h"
#include <sof/audio/component.h>
#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/data_blob.h>
#include <sof/audio/buffer.h>
#include <sof/audio/format.h>
#include <sof/audio/pipeline.h>
#include <sof/audio/ipc-config.h>
#include <sof/common.h>
#include <rtos/panic.h>
#include <sof/ipc/msg.h>
#include <rtos/alloc.h>
#include <rtos/init.h>
#include <sof/lib/memory.h>
#include <sof/lib/uuid.h>
#include <sof/list.h>
#include <sof/platform.h>
#include <rtos/string.h>
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

LOG_MODULE_REGISTER(eq_iir, CONFIG_SOF_LOG_LEVEL);

SOF_DEFINE_REG_UUID(eq_iir);

DECLARE_TR_CTX(eq_iir_tr, SOF_UUID(eq_iir_uuid), LOG_LEVEL_INFO);

/*
 * End of EQ setup code. Next the standard component methods.
 */
static int eq_iir_init(struct processing_module *mod)
{
	struct module_data *md = &mod->priv;
	struct comp_dev *dev = mod->dev;
	struct module_config *cfg = &md->cfg;
	struct comp_data *cd;
	size_t bs = cfg->size;
	int i, ret;

	comp_info(dev, "eq_iir_init()");

	/* Check first before proceeding with dev and cd that coefficients blob size is sane */
	if (bs > SOF_EQ_IIR_MAX_SIZE) {
		comp_err(dev, "eq_iir_init(), coefficients blob size %zu exceeds maximum", bs);
		return -EINVAL;
	}

	cd = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(*cd));
	if (!cd)
		return -ENOMEM;

	md->private = cd;

	/* component model data handler */
	cd->model_handler = comp_data_blob_handler_new(dev);
	if (!cd->model_handler) {
		comp_err(dev, "eq_iir_init(): comp_data_blob_handler_new() failed.");
		ret = -ENOMEM;
		goto err;
	}

	/* Allocate and make a copy of the coefficients blob and reset IIR. If
	 * the EQ is configured later in run-time the size is zero.
	 */
	ret = comp_init_data_blob(cd->model_handler, bs, cfg->data);
	if (ret < 0) {
		comp_err(dev, "eq_iir_init(): comp_init_data_blob() failed with error: %d", ret);
		comp_data_blob_handler_free(cd->model_handler);
		goto err;
	}

	for (i = 0; i < PLATFORM_MAX_CHANNELS; i++)
		iir_reset_df1(&cd->iir[i]);

	return 0;
err:
	rfree(cd);
	return ret;
}

static int eq_iir_free(struct processing_module *mod)
{
	struct comp_data *cd = module_get_private_data(mod);

	eq_iir_free_delaylines(cd);
	comp_data_blob_handler_free(cd->model_handler);

	rfree(cd);
	return 0;
}


/* used to pass standard and bespoke commands (with data) to component */
static int eq_iir_set_config(struct processing_module *mod, uint32_t config_id,
			     enum module_cfg_fragment_position pos, uint32_t data_offset_size,
			     const uint8_t *fragment, size_t fragment_size, uint8_t *response,
			     size_t response_size)
{
	struct comp_data *cd = module_get_private_data(mod);

	comp_info(mod->dev, "eq_iir_set_config()");

	return comp_data_blob_set(cd->model_handler, pos, data_offset_size, fragment,
				  fragment_size);
}

static int eq_iir_get_config(struct processing_module *mod,
			     uint32_t config_id, uint32_t *data_offset_size,
			     uint8_t *fragment, size_t fragment_size)
{
	struct sof_ipc_ctrl_data *cdata = (struct sof_ipc_ctrl_data *)fragment;
	struct comp_data *cd = module_get_private_data(mod);

	comp_info(mod->dev, "eq_iir_get_config()");

	return comp_data_blob_get_cmd(cd->model_handler, cdata, fragment_size);
}

static int eq_iir_process(struct processing_module *mod,
			  struct input_stream_buffer *input_buffers, int num_input_buffers,
			  struct output_stream_buffer *output_buffers, int num_output_buffers)
{
	struct comp_data *cd = module_get_private_data(mod);
	struct audio_stream *source = input_buffers[0].data;
	struct audio_stream *sink = output_buffers[0].data;
	uint32_t frame_count = input_buffers[0].size;
	int ret;

	/* Check for changed configuration */
	if (comp_is_new_data_blob_available(cd->model_handler)) {
		cd->config = comp_get_data_blob(cd->model_handler, NULL, NULL);
		ret = eq_iir_new_blob(mod, cd, audio_stream_get_frm_fmt(source),
				      audio_stream_get_frm_fmt(sink),
				      audio_stream_get_channels(source));
		if (ret)
			return ret;
	}

	if (frame_count) {
		cd->eq_iir_func(mod, &input_buffers[0], &output_buffers[0], frame_count);
		module_update_buffer_position(&input_buffers[0], &output_buffers[0], frame_count);
	}
	return 0;
}

/**
 * \brief Set EQ IIR frames alignment limit.
 * \param[in,out] source Structure pointer of source.
 * \param[in,out] sink Structure pointer of sink.
 */
static void eq_iir_set_alignment(struct audio_stream *source,
				 struct audio_stream *sink)
{
	const uint32_t byte_align = 8;
	const uint32_t frame_align_req = 2;

	audio_stream_set_align(byte_align, frame_align_req, source);
	audio_stream_set_align(byte_align, frame_align_req, sink);
}

static int eq_iir_prepare(struct processing_module *mod,
			  struct sof_source **sources, int num_of_sources,
			  struct sof_sink **sinks, int num_of_sinks)
{
	struct comp_data *cd = module_get_private_data(mod);
	struct comp_buffer *sourceb, *sinkb;
	struct comp_dev *dev = mod->dev;
	enum sof_ipc_frame source_format;
	enum sof_ipc_frame sink_format;
	int channels;
	int ret = 0;

	comp_dbg(dev, "eq_iir_prepare()");

	ret = eq_iir_prepare_sub(mod);
	if (ret < 0)
		return ret;

	/* EQ component will only ever have 1 source and 1 sink buffer */
	sourceb = list_first_item(&dev->bsource_list, struct comp_buffer, sink_list);
	sinkb = list_first_item(&dev->bsink_list, struct comp_buffer, source_list);
	eq_iir_set_alignment(&sourceb->stream, &sinkb->stream);

	/* get source and sink data format */
	channels = audio_stream_get_channels(&sinkb->stream);
	source_format = audio_stream_get_frm_fmt(&sourceb->stream);
	sink_format = audio_stream_get_frm_fmt(&sinkb->stream);

	cd->config = comp_get_data_blob(cd->model_handler, NULL, NULL);

	/* Initialize EQ */
	comp_info(dev, "eq_iir_prepare(), source_format=%d, sink_format=%d",
		  source_format, sink_format);

	eq_iir_set_passthrough_func(cd, source_format, sink_format);

	/* Initialize EQ */
	if (cd->config) {
		ret = eq_iir_new_blob(mod, cd, source_format, sink_format, channels);
		if (ret)
			return ret;
	}

	if (!cd->eq_iir_func) {
		comp_err(dev, "eq_iir_prepare(), No processing function found");
		ret = -EINVAL;
	}

	return ret;
}

static int eq_iir_reset(struct processing_module *mod)
{
	struct comp_data *cd = module_get_private_data(mod);
	int i;

	eq_iir_free_delaylines(cd);

	cd->eq_iir_func = NULL;
	for (i = 0; i < PLATFORM_MAX_CHANNELS; i++)
		iir_reset_df1(&cd->iir[i]);

	return 0;
}

static const struct module_interface eq_iir_interface = {
	.init = eq_iir_init,
	.prepare = eq_iir_prepare,
	.process_audio_stream = eq_iir_process,
	.set_configuration = eq_iir_set_config,
	.get_configuration = eq_iir_get_config,
	.reset = eq_iir_reset,
	.free = eq_iir_free
};

DECLARE_MODULE_ADAPTER(eq_iir_interface, eq_iir_uuid, eq_iir_tr);
SOF_MODULE_INIT(eq_iir, sys_comp_module_eq_iir_interface_init);

#if CONFIG_COMP_IIR_MODULE
/* modular: llext dynamic link */

#include <module/module/api_ver.h>
#include <module/module/llext.h>
#include <rimage/sof/user/manifest.h>

#define UUID_EQIIR 0xE6, 0xC0, 0x50, 0x51, 0xF9, 0x27, 0xC8, 0x4E, \
		0x83, 0x51, 0xC7, 0x05, 0xB6, 0x42, 0xD1, 0x2F

SOF_LLEXT_MOD_ENTRY(eq_iir, &eq_iir_interface);

static const struct sof_man_module_manifest mod_manifest __section(".module") __used =
	SOF_LLEXT_MODULE_MANIFEST("EQIIR", eq_iir_llext_entry, 1, UUID_EQIIR, 40);

SOF_LLEXT_BUILDINFO;

#endif
