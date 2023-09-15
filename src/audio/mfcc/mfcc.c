// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 Intel Corporation. All rights reserved.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

#include <sof/audio/mfcc/mfcc_comp.h>
#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/component.h>
#include <sof/audio/data_blob.h>
#include <sof/audio/buffer.h>
#include <sof/audio/format.h>
#include <sof/audio/pipeline.h>
#include <sof/audio/ipc-config.h>
#include <sof/common.h>
#include <rtos/panic.h>
#include <sof/ipc/msg.h>
#include <sof/lib/memory.h>
#include <sof/lib/uuid.h>
#include <sof/list.h>
#include <sof/platform.h>
#include <sof/ut.h>
#include <sof/trace/trace.h>
#include <ipc/control.h>
#include <ipc/stream.h>
#include <ipc/topology.h>
#include <user/mfcc.h>
#include <user/trace.h>
#include <rtos/string.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

LOG_MODULE_REGISTER(mfcc, CONFIG_SOF_LOG_LEVEL);

/* db10a773-1aa4-4cea-a21f-2d57a5c982eb */
DECLARE_SOF_RT_UUID("mfcc", mfcc_uuid, 0xdb10a773, 0x1aa4, 0x4cea,
		    0xa2, 0x1f, 0x2d, 0x57, 0xa5, 0xc9, 0x82, 0xeb);

DECLARE_TR_CTX(mfcc_tr, SOF_UUID(mfcc_uuid), LOG_LEVEL_INFO);

const struct mfcc_func_map mfcc_fm[] = {
#if CONFIG_FORMAT_S16LE
	{SOF_IPC_FRAME_S16_LE,  mfcc_s16_default},
#endif /* CONFIG_FORMAT_S16LE */
#if CONFIG_FORMAT_S24LE
	{SOF_IPC_FRAME_S24_4LE, NULL},
#endif /* CONFIG_FORMAT_S24LE */
#if CONFIG_FORMAT_S32LE
	{SOF_IPC_FRAME_S32_LE,  NULL},
#endif /* CONFIG_FORMAT_S32LE */
};

static mfcc_func mfcc_find_func(enum sof_ipc_frame source_format,
				enum sof_ipc_frame sink_format,
				const struct mfcc_func_map *map,
				int n)
{
	int i;

	/* Find suitable processing function from map. */
	for (i = 0; i < n; i++) {
		if (source_format == map[i].source)
			return map[i].func;
	}

	return NULL;
}

/*
 * End of MFCC setup code. Next the standard component methods.
 */

static int mfcc_init(struct processing_module *mod)
{
	struct module_data *md = &mod->priv;
	struct comp_dev *dev = mod->dev;
	struct module_config *cfg = &md->cfg;
	struct mfcc_comp_data *cd = NULL;
	size_t bs = cfg->size;
	int ret;

	comp_info(dev, "mfcc_init()");

	/* Check first that configuration blob size is sane */
	if (bs > SOF_MFCC_CONFIG_MAX_SIZE) {
		comp_err(dev, "mfcc_init() error: configuration blob size %u exceeds %d",
			 bs, SOF_MFCC_CONFIG_MAX_SIZE);
		return -EINVAL;
	}

	cd = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(*cd));
	if (!cd)
		return -ENOMEM;

	/* Handler for configuration data */
	md->private = cd;
	cd->model_handler = comp_data_blob_handler_new(dev);
	if (!cd->model_handler) {
		comp_err(dev, "mfcc_init(): comp_data_blob_handler_new() failed.");
		ret = -ENOMEM;
		goto err;
	}

	/* Get configuration data */
	ret = comp_init_data_blob(cd->model_handler, bs, cfg->init_data);
	if (ret < 0) {
		comp_err(mod->dev, "mfcc_init(): comp_init_data_blob() failed.");
		goto err_init;
	}

	return 0;

err_init:
	comp_data_blob_handler_free(cd->model_handler);

err:
	rfree(cd);
	return ret;
}

static int mfcc_free(struct processing_module *mod)
{
	struct mfcc_comp_data *cd = module_get_private_data(mod);

	comp_info(mod->dev, "mfcc_free()");
	comp_data_blob_handler_free(cd->model_handler);
	mfcc_free_buffers(cd);
	rfree(cd);
	return 0;
}

static int mfcc_get_config(struct processing_module *mod,
			   uint32_t config_id, uint32_t *data_offset_size,
			   uint8_t *fragment, size_t fragment_size)
{
	struct sof_ipc_ctrl_data *cdata = (struct sof_ipc_ctrl_data *)fragment;
	struct mfcc_comp_data *cd = module_get_private_data(mod);

	comp_info(mod->dev, "mfcc_get_config()");

	return comp_data_blob_get_cmd(cd->model_handler, cdata, fragment_size);
}

static int mfcc_set_config(struct processing_module *mod, uint32_t config_id,
			   enum module_cfg_fragment_position pos, uint32_t data_offset_size,
			   const uint8_t *fragment, size_t fragment_size, uint8_t *response,
			   size_t response_size)
{
	struct mfcc_comp_data *cd = module_get_private_data(mod);

	comp_info(mod->dev, "mfcc_set_config()");

	return comp_data_blob_set(cd->model_handler, pos, data_offset_size,
				  fragment, fragment_size);
}

static int mfcc_process(struct processing_module *mod,
			struct input_stream_buffer *input_buffers, int num_input_buffers,
			struct output_stream_buffer *output_buffers, int num_output_buffers)
{
	struct mfcc_comp_data *cd = module_get_private_data(mod);
	struct audio_stream *source = input_buffers->data;
	struct audio_stream *sink = output_buffers->data;
	int frames = input_buffers->size;

	comp_dbg(mod->dev, "mfcc_process(), start");

	frames = MIN(frames, cd->max_frames);
	cd->mfcc_func(mod, input_buffers, output_buffers, frames);

	/* TODO: use module_update_buffer_position() from #6194 */
	input_buffers->consumed += audio_stream_frame_bytes(source) * frames;
	output_buffers->size += audio_stream_frame_bytes(sink) * frames;
	comp_dbg(mod->dev, "mfcc_process(), done");
	return 0;
}

static void mfcc_set_alignment(struct audio_stream *source, struct audio_stream *sink)
{
	const uint32_t byte_align = 1;
	const uint32_t frame_align_req = 1;

	audio_stream_init_alignment_constants(byte_align, frame_align_req, source);
	audio_stream_init_alignment_constants(byte_align, frame_align_req, sink);
}

static int mfcc_prepare(struct processing_module *mod,
			struct sof_source **sources, int num_of_sources,
			struct sof_sink **sinks, int num_of_sinks)
{
	struct mfcc_comp_data *cd = module_get_private_data(mod);
	struct comp_buffer *sourceb;
	struct comp_buffer *sinkb;
	struct comp_dev *dev = mod->dev;
	enum sof_ipc_frame source_format;
	enum sof_ipc_frame sink_format;
	uint32_t sink_period_bytes;
	int ret;

	comp_info(dev, "mfcc_prepare()");

	/* MFCC component will only ever have 1 source and 1 sink buffer */
	sourceb = list_first_item(&dev->bsource_list, struct comp_buffer, sink_list);
	sinkb = list_first_item(&dev->bsink_list, struct comp_buffer, source_list);

	/* get source data format */
	source_format = audio_stream_get_frm_fmt(&sourceb->stream);

	/* set align requirements */
	mfcc_set_alignment(&sourceb->stream, &sinkb->stream);

	/* get sink data format and period bytes */
	sink_format = audio_stream_get_frm_fmt(&sinkb->stream);
	sink_period_bytes = audio_stream_period_bytes(&sinkb->stream, dev->frames);
	comp_info(dev, "mfcc_prepare(), source_format = %d, sink_format = %d",
		  source_format, sink_format);
	if (audio_stream_get_size(&sinkb->stream) < sink_period_bytes) {
		comp_err(dev, "mfcc_prepare(): sink buffer size %d is insufficient < %d",
			 audio_stream_get_size(&sinkb->stream), sink_period_bytes);
		ret = -ENOMEM;
		goto err;
	}

	cd->config = comp_get_data_blob(cd->model_handler, NULL, NULL);

	/* Initialize MFCC, max_frames is set to dev->frames + 4 */
	if (cd->config) {
		ret = mfcc_setup(mod, dev->frames + 4, audio_stream_get_rate(&sourceb->stream),
				 audio_stream_get_channels(&sourceb->stream));
		if (ret < 0) {
			comp_err(dev, "mfcc_prepare(), setup failed.");
			goto err;
		}
	}

	cd->mfcc_func = mfcc_find_func(source_format, sink_format, mfcc_fm, ARRAY_SIZE(mfcc_fm));
	if (!cd->mfcc_func) {
		comp_err(dev, "mfcc_prepare(), No proc func");
		ret = -EINVAL;
		goto err;
	}

	return 0;

err:
	comp_set_state(dev, COMP_TRIGGER_RESET);
	return ret;
}

static int mfcc_reset(struct processing_module *mod)
{
	struct mfcc_comp_data *cd = module_get_private_data(mod);

	comp_info(mod->dev, "mfcc_reset()");

	/* Reset to similar state as init() */
	cd->mfcc_func = NULL;
	return 0;
}

static const struct module_interface mfcc_interface = {
	.init = mfcc_init,
	.free = mfcc_free,
	.set_configuration = mfcc_set_config,
	.get_configuration = mfcc_get_config,
	.process_audio_stream = mfcc_process,
	.prepare = mfcc_prepare,
	.reset = mfcc_reset,
};

DECLARE_MODULE_ADAPTER(mfcc_interface, mfcc_uuid, mfcc_tr);
