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
#include <sof/audio/sink_source_utils.h>
#include <sof/audio/ipc-config.h>
#include <sof/common.h>
#include <rtos/panic.h>
#include <sof/ipc/msg.h>
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
#include <user/eq.h>
#include <user/trace.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

LOG_MODULE_REGISTER(eq_iir, CONFIG_SOF_LOG_LEVEL);

SOF_DEFINE_REG_UUID(eq_iir);

/*
 * End of EQ setup code. Next the standard component methods.
 */
static int eq_iir_init(struct processing_module *mod)
{
	struct module_data *md = &mod->priv;
	struct comp_dev *dev = mod->dev;
	struct comp_data *cd;
	int i;

	comp_info(dev, "entry");

	cd = mod_zalloc(mod, sizeof(*cd));
	if (!cd)
		return -ENOMEM;

	md->private = cd;

	/* component model data handler */
	cd->model_handler = mod_data_blob_handler_new(mod);
	if (!cd->model_handler) {
		comp_err(dev, "mod_data_blob_handler_new() failed.");
		mod_free(mod, cd);
		return -ENOMEM;
	}

	/* Reject malformed blobs at IPC time so a bad run-time update cannot
	 * replace the working configuration.
	 */
	comp_data_blob_set_validator(cd->model_handler, eq_iir_validate_config);

	for (i = 0; i < PLATFORM_MAX_CHANNELS; i++)
		iir_reset_df1(&cd->iir[i]);

	return 0;
}

static int eq_iir_free(struct processing_module *mod)
{
	struct comp_data *cd = module_get_private_data(mod);

	eq_iir_free_delaylines(mod);
	mod_data_blob_handler_free(mod, cd->model_handler);

	mod_free(mod, cd);
	return 0;
}


/* used to pass standard and bespoke commands (with data) to component */
static int eq_iir_set_config(struct processing_module *mod, uint32_t config_id,
			     enum module_cfg_fragment_position pos, uint32_t data_offset_size,
			     const uint8_t *fragment, size_t fragment_size, uint8_t *response,
			     size_t response_size)
{
	struct comp_data *cd = module_get_private_data(mod);

	comp_info(mod->dev, "entry");

	return comp_data_blob_set(cd->model_handler, pos, data_offset_size, fragment,
				  fragment_size);
}

static int eq_iir_get_config(struct processing_module *mod,
			     uint32_t config_id, uint32_t *data_offset_size,
			     uint8_t *fragment, size_t fragment_size)
{
	struct sof_ipc_ctrl_data *cdata = (struct sof_ipc_ctrl_data *)fragment;
	struct comp_data *cd = module_get_private_data(mod);

	comp_info(mod->dev, "entry");

	return comp_data_blob_get_cmd(cd->model_handler, cdata, fragment_size);
}

static int eq_iir_process(struct processing_module *mod,
			  struct sof_source **sources, int num_input_buffers,
			  struct sof_sink **sinks, int num_output_buffers)
{
	struct comp_data *cd = module_get_private_data(mod);
	struct sof_source *source;
	struct sof_sink *sink;
	struct cir_buf_source source_buf;
	struct cir_buf_sink sink_buf;
	size_t source_frame_bytes;
	size_t sink_frame_bytes;
	size_t source_bytes;
	size_t sink_bytes;
	size_t source_buf_size;
	size_t sink_buf_size;
	size_t frame_count;
	int ret;

	if (num_input_buffers != 1 || num_output_buffers != 1) {
		comp_err(mod->dev, "EQ IIR supports one source and one sink");
		return -EINVAL;
	}

	source = sources[0];
	sink = sinks[0];
	cd->channels = source_get_channels(source);
	cd->frame_bytes = source_get_frame_bytes(source);

	/* Check for changed configuration. The IPC-time validator installed
	 * in eq_iir_init() has already structurally validated the blob, so
	 * only NULL needs to be guarded here.
	 */
	if (comp_is_new_data_blob_available(cd->model_handler)) {
		cd->config = comp_get_data_blob(cd->model_handler, &cd->config_size, NULL);
		if (!cd->config)
			return -EINVAL;

		ret = eq_iir_new_blob(mod, source_get_frm_fmt(source),
				      sink_get_frm_fmt(sink),
				      source_get_channels(source));
		if (ret)
			return ret;
	}

	if (!cd->eq_iir_func)
		return -EINVAL;

	frame_count = source_sink_avail_frames_aligned(source, sink);
	if (!frame_count)
		return 0;

	source_frame_bytes = source_get_frame_bytes(source);
	sink_frame_bytes = sink_get_frame_bytes(sink);
	source_bytes = frame_count * source_frame_bytes;
	sink_bytes = frame_count * sink_frame_bytes;

	ret = source_get_data(source, source_bytes, &source_buf.ptr,
			      &source_buf.buf_start, &source_buf_size);
	if (ret < 0)
		return ret;
	if (source_buf_size < source_bytes) {
		source_release_data(source, 0);
		return -ENOSPC;
	}
	source_buf.buf_end = (const char *)source_buf.buf_start + source_buf_size;

	ret = sink_get_buffer(sink, sink_bytes, &sink_buf.ptr,
			      &sink_buf.buf_start, &sink_buf_size);
	if (ret < 0) {
		source_release_data(source, 0);
		return ret;
	}
	if (sink_buf_size < sink_bytes) {
		source_release_data(source, 0);
		sink_commit_buffer(sink, 0);
		return -ENOSPC;
	}
	sink_buf.buf_end = (char *)sink_buf.buf_start + sink_buf_size;

	cd->eq_iir_func(mod, &source_buf, &sink_buf, frame_count);

	ret = source_release_data(source, source_bytes);
	if (ret < 0) {
		sink_commit_buffer(sink, 0);
		return ret;
	}

	return sink_commit_buffer(sink, sink_bytes);
}

/**
 * \brief Set EQ IIR frames alignment limit.
 * \param[in,out] source Structure pointer of source.
 * \param[in,out] sink Structure pointer of sink.
 */
static int eq_iir_set_alignment(struct sof_source *source,
				struct sof_sink *sink)
{
	const uint32_t byte_align = SOF_FRAME_BYTE_ALIGN;
	const uint32_t frame_align_req = 2;
	int ret;

	ret = source_set_alignment_constants(source, byte_align, frame_align_req);
	if (ret < 0)
		return ret;

	return sink_set_alignment_constants(sink, byte_align, frame_align_req);
}

static int eq_iir_prepare(struct processing_module *mod,
			  struct sof_source **sources, int num_of_sources,
			  struct sof_sink **sinks, int num_of_sinks)
{
	struct comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	struct sof_source *source;
	struct sof_sink *sink;
	enum sof_ipc_frame source_format;
	enum sof_ipc_frame sink_format;
	int channels;
	int ret = 0;

	comp_dbg(dev, "entry");

	/* EQ component will only ever have 1 source and 1 sink buffer. */
	if (num_of_sources != 1 || num_of_sinks != 1) {
		comp_err(dev, "no source or sink buffer");
		return -ENOTCONN;
	}

	source = sources[0];
	sink = sinks[0];

	ret = eq_iir_prepare_sub(mod);
	if (ret < 0)
		return ret;

	ret = eq_iir_set_alignment(source, sink);
	if (ret < 0)
		return ret;

	/* get source and sink data format */
	channels = source_get_channels(source);
	source_format = source_get_frm_fmt(source);
	sink_format = sink_get_frm_fmt(sink);
	if (channels != sink_get_channels(sink)) {
		comp_err(dev, "source and sink channel counts do not match");
		return -EINVAL;
	}
	cd->channels = channels;
	cd->frame_bytes = source_get_frame_bytes(source);

	cd->config = comp_get_data_blob(cd->model_handler, &cd->config_size, NULL);

	/* Initialize EQ */
	comp_info(dev, "source_format=%d, sink_format=%d",
		  source_format, sink_format);

	eq_iir_set_passthrough_func(cd, source_format, sink_format);

	/* Initialize EQ */
	if (cd->config) {
		ret = eq_iir_new_blob(mod, source_format, sink_format, channels);
		if (ret)
			return ret;
	}

	if (!cd->eq_iir_func) {
		comp_err(dev, "No processing function found");
		ret = -EINVAL;
	}

	return ret;
}

static int eq_iir_reset(struct processing_module *mod)
{
	struct comp_data *cd = module_get_private_data(mod);
	int i;

	eq_iir_free_delaylines(mod);

	cd->eq_iir_func = NULL;
	cd->channels = 0;
	cd->frame_bytes = 0;
	for (i = 0; i < PLATFORM_MAX_CHANNELS; i++)
		iir_reset_df1(&cd->iir[i]);

	return 0;
}

static const struct module_interface eq_iir_interface = {
	.init = eq_iir_init,
	.prepare = eq_iir_prepare,
	.process = eq_iir_process,
	.set_configuration = eq_iir_set_config,
	.get_configuration = eq_iir_get_config,
	.reset = eq_iir_reset,
	.free = eq_iir_free
};

#if CONFIG_COMP_IIR_MODULE
/* modular: llext dynamic link */

#include <module/module/api_ver.h>
#include <module/module/llext.h>
#include <rimage/sof/user/manifest.h>

static const struct sof_man_module_manifest mod_manifest __section(".module") __used =
	SOF_LLEXT_MODULE_MANIFEST("EQIIR", &eq_iir_interface, 1, SOF_REG_UUID(eq_iir), 40);

SOF_LLEXT_BUILDINFO;

#else

/* unused with Zephyr, generates no output */
DECLARE_TR_CTX(eq_iir_tr, SOF_UUID(eq_iir_uuid), LOG_LEVEL_INFO);
DECLARE_MODULE_ADAPTER(eq_iir_interface, eq_iir_uuid, eq_iir_tr);
SOF_MODULE_INIT(eq_iir, sys_comp_module_eq_iir_interface_init);

#endif
