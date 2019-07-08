// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 NXP. All rights reserved.
//
// Author: Paul Olaru <paul.olaru@nxp.com>

#include <stdint.h>
#include <sof/list.h>
#include <sof/alloc.h>
#include <sof/ipc.h>
#include <sof/audio/component.h>
#include <sof/audio/format.h>
#include <sof/audio/module.h>

#define MODULE_TYPE_PASSTHROUGH 0

struct passthrough_private {
	void (*fn)(struct comp_dev *dev,
		   struct comp_buffer *source,
		   struct comp_buffer *sink,
		   uint32_t frames);
	struct comp_buffer *sourceb;
	struct comp_buffer *sinkb;
};

static int passthrough_new(struct comp_dev *dev)
{
	struct passthrough_private *dat;

	dat = rzalloc(RZONE_RUNTIME, SOF_MEM_CAPS_RAM,
		      sizeof(struct passthrough_private));
	if (!dat)
		return -ENOMEM;
	module_set_drvdata(dev, dat);

	return 0; /* Success */
}

static void passthrough_free(struct comp_dev *dev)
{
	rfree(module_get_drvdata(dev));
	module_set_drvdata(dev, NULL);
}

static int passthrough_params(struct comp_dev *dev)
{
	(void)dev;
	return 0;
}

static int passthrough_cmd(struct comp_dev *dev, int cmd, void *data,
			   int max_data_size)
{
	(void)dev;
	(void)cmd;
	(void)data;
	(void)max_data_size;
	return 0;
}

static void passthrough_copy_16(struct comp_dev *dev,
				struct comp_buffer *source,
				struct comp_buffer *sink,
				uint32_t frames)
{
	int16_t *src;
	int16_t *dst;
	uint32_t i;

	for (i = 0; i < frames; i++) {
		src = buffer_read_frag_s16(source, i);
		dst = buffer_write_frag_s16(sink, i);
		*dst = *src;
	}
}

static void passthrough_copy_24(struct comp_dev *dev,
				struct comp_buffer *source,
				struct comp_buffer *sink,
				uint32_t frames)
{
	int32_t *src;
	int32_t *dst;
	uint32_t i;

	for (i = 0; i < frames; i++) {
		src = buffer_read_frag_s32(source, i);
		dst = buffer_write_frag_s32(sink, i);
		*dst = *src;
	}
}

static void passthrough_copy_32(struct comp_dev *dev,
				struct comp_buffer *source,
				struct comp_buffer *sink,
				uint32_t frames)
{
	int32_t *src;
	int32_t *dst;
	uint32_t i;

	for (i = 0; i < frames; i++) {
		src = buffer_read_frag_s32(source, i);
		dst = buffer_write_frag_s32(sink, i);
		*dst = *src;
	}
}

static int passthrough_copy(struct comp_dev *dev)
{
	/* This one should actually copy something */
	struct passthrough_private *priv = module_get_drvdata(dev);
	struct comp_copy_limits cl;
	int ret = comp_get_copy_limits(dev, &cl);

	if (ret < 0)
		return ret;

	priv->fn(dev, cl.source, cl.sink, cl.frames);
	return 0;
}

static int passthrough_reset(struct comp_dev *dev)
{
	struct passthrough_private *priv = module_get_drvdata(dev);

	priv->fn = 0;
	comp_set_state(dev, COMP_TRIGGER_RESET);
	return 0;
}

static int passthrough_prepare(struct comp_dev *dev)
{
	/* Just accept anything */
	/* Configure the private structure */
	struct passthrough_private *priv = module_get_drvdata(dev);
	int ret;
	struct comp_buffer *sinkb;
	struct comp_buffer *sourceb;

	ret = comp_set_state(dev, COMP_TRIGGER_PREPARE);
	if (ret < 0)
		return ret;

	if (ret == COMP_STATUS_STATE_ALREADY_SET)
		return PPL_STATUS_PATH_STOP;

	sourceb = list_first_item(&dev->bsource_list,
				  struct comp_buffer, sink_list);
	sinkb = list_first_item(&dev->bsink_list,
				struct comp_buffer, source_list);

	priv->sourceb = sourceb;
	priv->sinkb = sinkb;

	enum sof_ipc_frame source_format = comp_frame_fmt(sourceb->source);
	enum sof_ipc_frame sink_format = comp_frame_fmt(sinkb->sink);

	if (source_format != sink_format)
		return -EINVAL; /* Unable to do conversions, simple component */

	switch (source_format) {
	case SOF_IPC_FRAME_S16_LE:
		priv->fn = passthrough_copy_16;
		break;
	case SOF_IPC_FRAME_S24_4LE:
		priv->fn = passthrough_copy_24;
		break;
	case SOF_IPC_FRAME_S32_LE:
		priv->fn = passthrough_copy_32;
		break;
	default:
		return -EINVAL; /* Unsupported format */
	}
	return 0;
}

static int passthrough_trigger(struct comp_dev *dev, int cmd)
{
	return comp_set_state(dev, cmd);
}

static struct registered_module mod = {
	.module_type = MODULE_TYPE_PASSTHROUGH,
	.ops = {
		.new		= passthrough_new,
		.free		= passthrough_free,
		.params		= passthrough_params,
		.cmd		= passthrough_cmd,
		.copy		= passthrough_copy,
		.prepare	= passthrough_prepare,
		.reset		= passthrough_reset,
		.trigger	= passthrough_trigger,
	},
};

static void comp_module_passthrough_init(void)
{
	register_module(&mod);
}

DECLARE_MODULE(comp_module_passthrough_init);
