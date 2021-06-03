// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Intel Corporation. All rights reserved.
//
// Author: Rander Wang <rander.wang@linux.intel.com>

#include <sof/audio/buffer.h>
#include <sof/audio/component_ext.h>
#include <sof/audio/format.h>
#include <sof/audio/pipeline.h>
#include <sof/common.h>
#include <sof/debug/panic.h>
#include <sof/drivers/interrupt.h>
#include <sof/ipc/msg.h>
#include <sof/drivers/interrupt.h>
#include <sof/drivers/timer.h>
#include <sof/lib/alloc.h>
#include <sof/lib/cache.h>
#include <sof/lib/memory.h>
#include <sof/lib/notifier.h>
#include <sof/lib/uuid.h>
#include <sof/list.h>
#include <sof/string.h>
#include <sof/ut.h>
#include <sof/trace/trace.h>
#include <ipc4/copier.h>
#include <ipc4/module.h>
#include <ipc4/error_status.h>
#include <ipc4/gateway.h>
#include <ipc/dai.h>
#include <user/trace.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

static const struct comp_driver comp_copier;

/* 9ba00c83-ca12-4a83-943c-1fa2e82f9dda */
DECLARE_SOF_RT_UUID("copier", copier_comp_uuid, 0x9ba00c83, 0xca12, 0x4a83,
		 0x94, 0x3c, 0x1f, 0xa2, 0xe8, 0x2f, 0x9d, 0xda);

DECLARE_TR_CTX(copier_comp_tr, SOF_UUID(copier_comp_uuid), LOG_LEVEL_INFO);

static enum sof_ipc_frame convert_fmt(int format) {
	enum sof_ipc_frame in;

	switch (format) {
	case DEPTH_16BIT:
		in = SOF_IPC_FRAME_S16_LE;
		break;
	case DEPTH_24BIT:
		in = SOF_IPC_FRAME_S24_4LE;
		break;
	case DEPTH_32BIT:
		in = SOF_IPC_FRAME_S32_LE;
		break;
	default:
		comp_cl_err(&comp_copier, "unsupported format %d", format);
		return SOF_IPC_FRAME_S16_LE;
	};

	return in;
}

static struct comp_dev *create_host(struct comp_ipc_config *config,
				struct CopierModuleCfg *copier_cfg, int dir, struct pipeline *pipeline)
{
	struct sof_uuid host = {0x8b9d100c, 0x6d78, 0x418f, {0x90, 0xa3, 0xe0,
			0xe8, 0x05, 0xd0, 0x85, 0x2b}};
	struct ipc_config_host ipc_host;
	const struct comp_driver *drv;
	struct comp_dev *dev;

	drv= ipc4_get_drv((uint8_t *)&host);
	if (!drv)
		return NULL;

	config->type = SOF_COMP_HOST;

	memset(&ipc_host, 0, sizeof(ipc_host));
	ipc_host.direction = dir;

	dev = drv->ops.create(drv, config, &ipc_host);
	if (!dev)
		return NULL;

	list_init(&dev->bsource_list);
	list_init(&dev->bsink_list);

	return dev;
};

static void connect_comp_to_buffer(struct comp_dev *dev, struct comp_buffer *buf, int dir)
{
	uint32_t flags;

	irq_local_disable(flags);
	list_item_prepend(buffer_comp_list(buf, dir), comp_buffer_list(dev, dir));
	buffer_set_comp(buf, dev, dir);
	comp_writeback(dev);
	irq_local_enable(flags);

	dcache_writeback_invalidate_region(buf, sizeof(*buf));
}

static struct comp_buffer *create_buffer(struct comp_dev *dev, struct CopierModuleCfg *copier,
						int dir)
{
	struct sof_ipc_buffer ipc_buf;
	struct comp_buffer *buf;

	memset(&ipc_buf, 0, sizeof(ipc_buf));
	ipc_buf.size = copier->base.obs;
	ipc_buf.comp.id = dev->ipc_config.id + 1;
	ipc_buf.comp.pipeline_id = dev->ipc_config.pipeline_id;
	ipc_buf.comp.core = dev->ipc_config.core;
	buf = buffer_new(&ipc_buf);
	if (!buf)
		return NULL;

	list_init(&buf->source_list);
	list_init(&buf->sink_list);

	connect_comp_to_buffer(dev, buf, PPL_CONN_DIR_COMP_TO_BUFFER);
	return buf;
}

static struct comp_dev *create_dai(struct comp_ipc_config *config, struct CopierModuleCfg *copier,
				union ipc4_connector_node_id *node_id, struct pipeline *pipeline)
{
	struct sof_uuid id = {0xc2b00d27, 0xffbc, 0x4150, {0xa5, 0x1a, 0x24,
				0x5c, 0x79, 0xc5, 0xe5, 0x4b}};
	const struct comp_driver *drv;
	struct ipc_config_dai dai;
	struct comp_dev *dev;
	int ret;

	drv= ipc4_get_drv((uint8_t *)&id);
	if (!drv)
		return NULL;

	config->type = SOF_COMP_DAI;

	memset(&dai, 0, sizeof(dai));
	dai.dai_index = node_id->f.v_index;
	dai.rate = copier->out_fmt.sampling_frequency;
	dai.channels = copier->out_fmt.channels_count;
	dai.direction= node_id->f.dma_type % 2;
	dai.is_config_blob = true;

	switch (node_id->f.dma_type) {
	case ipc4_hda_link_output_class:
	case ipc4_hda_link_input_class:
		dai.type = SOF_DAI_INTEL_HDA;
		dai.link_dma_ch = node_id->f.v_index;
		dai.is_config_blob = false;
		break;
	case ipc4_i2s_link_output_class:
	case ipc4_i2s_link_input_class:
		dai.type = SOF_DAI_INTEL_SSP;
		dai.is_config_blob = true;
		break;
	case ipc4_alh_link_output_class:
	case ipc4_alh_link_input_class:
		dai.type = SOF_DAI_INTEL_ALH;
		dai.is_config_blob = true;
		break;
	case ipc4_dmic_link_input_class:
		dai.type = SOF_DAI_INTEL_DMIC;
		dai.is_config_blob = true;
		break;
	default:
		return NULL;
	}

	dev = drv->ops.create(drv, config, &dai);
	if (!dev)
		return NULL;

	if (dai.direction == SOF_IPC_STREAM_PLAYBACK)
		pipeline->sink_comp = dev;
	else
		pipeline->source_comp = dev;

	pipeline->sched_id = config->id;

	list_init(&dev->bsource_list);
	list_init(&dev->bsink_list);

	ret = comp_dai_config(dev, &dai, &copier->gtw_cfg);
	if (ret < 0)
		return NULL;

	return dev;
}

static struct comp_dev *copier_new(const struct comp_driver *drv,
				struct comp_ipc_config *config,
				void *spec)
{
	struct CopierModuleCfg *copier = spec;
	union ipc4_connector_node_id node_id;
	struct ipc *ipc = ipc_get();
	struct copier_data *cd;
	struct comp_dev *dev;
	size_t size, config_size;

	comp_cl_dbg(&comp_copier, "copier_new()");

	dev = comp_alloc(drv, sizeof(*dev));
	if (!dev)
		return NULL;

	dcache_invalidate_region(spec, sizeof(*copier));

	config->frame_fmt = (copier->out_fmt.depth >> 3) - 2;
	dev->ipc_config = *config;

	config_size = copier->gtw_cfg.config_length * sizeof(uint32_t);
	size = sizeof(*cd) + config_size;
	cd = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, size);
	if (!cd)
		goto error;

	size = config_size + sizeof(*copier);
	mailbox_hostbox_read(&cd->config, size, 0, size);
	comp_set_drvdata(dev, cd);

	list_init(&dev->bsource_list);
	list_init(&dev->bsink_list);

	if (IPC4_INVALID_NODE_ID == copier->gtw_cfg.node_id) {
		//nothing to do for converter
	} else {
		struct ipc_comp_dev *ipc_pipe;

		node_id.dw = copier->gtw_cfg.node_id;
		cd->direction = node_id.f.dma_type % 2;

		/* check whether pipeline id is already taken or in use */
		ipc_pipe = ipc_get_comp_by_ppl_id(ipc, COMP_TYPE_PIPELINE, config->pipeline_id);
		if (!ipc_pipe) {
			tr_err(&ipc_tr, "pipeline %d is not existed", config->pipeline_id);
			goto error_cd;
		}

		comp_cl_err(&comp_copier, "dma_type %d", (int)node_id.f.dma_type);

		switch (node_id.f.dma_type) {
		case ipc4_hda_host_output_class:
		case ipc4_hda_host_input_class:
			cd->host = create_host(config, copier, cd->direction, ipc_pipe->pipeline);
			if (!cd->host) {
				comp_cl_err(&comp_copier, "unenable to create host");
				goto error_cd;
			}

			if (cd->direction == SOF_IPC_STREAM_PLAYBACK)
				ipc_pipe->pipeline->source_comp = dev;
			else
				ipc_pipe->pipeline->sink_comp = dev;

			break;
		case ipc4_hda_link_output_class:
		case ipc4_hda_link_input_class:
		case ipc4_dmic_link_input_class:
		case ipc4_i2s_link_output_class:
		case ipc4_i2s_link_input_class:
		case ipc4_alh_link_output_class:
		case ipc4_alh_link_input_class:
		cd->dai = create_dai(config, &cd->config, &node_id, ipc_pipe->pipeline);
		if (!cd->dai) {
			comp_cl_err(&comp_copier, "unenable to create dai");
			goto error_cd;
		}

		if (cd->direction == SOF_IPC_STREAM_PLAYBACK)
                       ipc_pipe->pipeline->sink_comp = dev;
                else
                       ipc_pipe->pipeline->source_comp = dev;

		break;
		default:
			comp_cl_err(&comp_copier, "unsupported dma type %x", (uint32_t)node_id.f.dma_type);
			goto error_cd;
		};
	}

	if (cd->host && cd->direction == SOF_IPC_STREAM_PLAYBACK) {
		cd->buf = create_buffer(dev, copier, cd->direction);
		if (!cd->buf)
			goto error_cd;
	} else if (cd->dai && cd->direction == SOF_IPC_STREAM_CAPTURE) {
		cd->buf = create_buffer(dev, copier, cd->direction);
		if (!cd->buf)
			goto error_cd;
	} else if (!cd->host && !cd->dai) {
		/* pcm converter */
		cd->buf = create_buffer(dev, copier, cd->direction);
		if (!cd->buf)
			goto error_cd;
	}

	dev->direction = cd->direction;
	dev->state = COMP_STATE_READY;
	return dev;

error_cd:
	rfree(cd);
error:
	rfree(dev);
	return NULL;
}

static void copier_free(struct comp_dev *dev)
{
	struct copier_data *cd = comp_get_drvdata(dev);

	if (cd->host)
		cd->host->drv->ops.free(cd->host);
	else if (cd->dai)
		cd->dai->drv->ops.free(cd->dai);

	if (cd->buf)
		buffer_free(cd->buf);

	rfree(cd);
	rfree(dev);
}

static int copier_prepare(struct comp_dev *dev)
{
	struct copier_data *cd = comp_get_drvdata(dev);
	enum sof_ipc_frame in, out;
	struct comp_buffer *buffer;
	int ret = 0, dir;

	comp_info(dev, "copier_prepare()");

	if (cd->buf) {
		dir = PPL_CONN_DIR_COMP_TO_BUFFER;
		buffer = cd->buf;
		list_item_del(buffer->source_list.next);
		list_item_del(dev->bsink_list.next);
	} else {
		dir = PPL_CONN_DIR_BUFFER_TO_COMP;
		buffer = list_first_item(&dev->bsource_list, struct comp_buffer, sink_list);
		list_item_del(buffer->sink_list.next);
		list_item_del(dev->bsource_list.next);
	}

	/* cannot configure DAI while active */
	if (dev->state == COMP_STATE_ACTIVE) {
		comp_info(dev, "copier_config_prepare(): Component is in active state.");
		return 0;
	}

	ret = comp_set_state(dev, COMP_TRIGGER_PREPARE);
	if (ret < 0)
		return ret;

	if (ret == COMP_STATUS_STATE_ALREADY_SET)
		return PPL_STATUS_PATH_STOP;

	in = convert_fmt(cd->config.base.audio_fmt.valid_bit_depth);
	out = convert_fmt(cd->config.out_fmt.valid_bit_depth);

	cd->converter = pcm_get_conversion_function(in, out);

	if (cd->host) {
		connect_comp_to_buffer(cd->host, buffer, dir);
		ret = cd->host->drv->ops.prepare(cd->host);
		connect_comp_to_buffer(dev, buffer, dir);
	} else if (cd->dai) {
		connect_comp_to_buffer(cd->dai, buffer, dir);
		ret = cd->dai->drv->ops.prepare(cd->dai);
		connect_comp_to_buffer(dev, buffer, dir);
	}

	return ret;
}

static int copier_reset(struct comp_dev *dev)
{
	struct copier_data *cd = comp_get_drvdata(dev);
	int ret = 0;

	comp_info(dev, "copier_reset()");

	if (dev->state == COMP_STATE_ACTIVE) {
		comp_info(dev, "copier_config(): Component is in active state. Ignore resetting");
		return 0;
	}

	if (cd->host)
		ret = cd->host->drv->ops.reset(cd->host);
	else if (cd->dai)
		ret = cd->dai->drv->ops.reset(cd->dai);

	comp_set_state(dev, COMP_TRIGGER_RESET);

	return ret;
}

static int copier_comp_trigger(struct comp_dev *dev, int cmd)
{
	struct copier_data *cd = comp_get_drvdata(dev);
	struct comp_buffer *buffer;
	int ret = 0, dir;

	comp_dbg(dev, "copier_comp_trigger()");

	comp_set_state(dev, cmd);

	if (cd->buf) {
		dir = PPL_CONN_DIR_COMP_TO_BUFFER;
		buffer = cd->buf;
		list_item_del(buffer->source_list.next);
		list_item_del(dev->bsink_list.next);
	} else {
		dir = PPL_CONN_DIR_BUFFER_TO_COMP;
		buffer = list_first_item(&dev->bsource_list, struct comp_buffer, sink_list);
		list_item_del(buffer->sink_list.next);
		list_item_del(dev->bsource_list.next);
	}

	if (cd->host) {
		connect_comp_to_buffer(cd->host, buffer, dir);
		ret = cd->host->drv->ops.trigger(cd->host, cmd);
		connect_comp_to_buffer(dev, buffer, dir);
	} else if (cd->dai) {
		connect_comp_to_buffer(cd->dai, buffer, dir);
		ret = cd->dai->drv->ops.trigger(cd->dai, cmd);
		connect_comp_to_buffer(dev, buffer, dir);
	}

	if (ret < 0)
		ret = IPC4_FAILURE;

	return ret;
}

/* copy and process stream data from source to sink buffers */
static int copier_copy(struct comp_dev *dev)
{
	struct copier_data *cd = comp_get_drvdata(dev);
	struct comp_buffer *buffer;
	int ret = 0, dir;

	comp_info(dev, "copier_copy()");

	if (cd->buf) {
		dir = PPL_CONN_DIR_COMP_TO_BUFFER;
		buffer = cd->buf;
		list_item_del(buffer->source_list.next);
		list_item_del(dev->bsink_list.next);
	} else {
		dir = PPL_CONN_DIR_BUFFER_TO_COMP;
		buffer = list_first_item(&dev->bsource_list, struct comp_buffer, sink_list);
		list_item_del(buffer->sink_list.next);
		list_item_del(dev->bsource_list.next);
	}

	if (cd->host) {
		connect_comp_to_buffer(cd->host, buffer, dir);
		ret = cd->host->drv->ops.copy(cd->host);
		connect_comp_to_buffer(dev, buffer, dir);
	} else if (cd->dai) {
		connect_comp_to_buffer(cd->dai, buffer, dir);
		ret = cd->dai->drv->ops.copy(cd->dai);
		connect_comp_to_buffer(dev, buffer, dir);
	} else {
		/* todo add coverter support */
		//cd->converter();
	}

	return ret;
}
static void update_internal_comp(struct comp_dev *parent, struct comp_dev *child)
{
	child->period = parent->period;
	child->pipeline = parent->pipeline;
	child->priority = parent->priority;
	child->direction = parent->direction;
}

/* configure the DMA params */
static int copier_params(struct comp_dev *dev, struct sof_ipc_stream_params *params)
{
	struct copier_data *cd = comp_get_drvdata(dev);
	union ipc4_connector_node_id node_id;
	struct comp_buffer *buffer;
	int ret = 0, dir;
	int i;

	memset(params, 0, sizeof(*params));

	params->direction = cd->direction;
	if (!cd->dai) {
		params->channels = cd->config.out_fmt.channels_count;
		params->rate = cd->config.out_fmt.sampling_frequency;
		params->sample_container_bytes = cd->config.out_fmt.depth;
		params->sample_valid_bytes = cd->config.out_fmt.valid_bit_depth;
	} else {
		params->channels = cd->config.base.audio_fmt.channels_count;
		params->rate = cd->config.base.audio_fmt.sampling_frequency;
		params->sample_container_bytes = cd->config.base.audio_fmt.depth;
		params->sample_valid_bytes = cd->config.base.audio_fmt.valid_bit_depth;
	}

	node_id.dw = cd->config.gtw_cfg.node_id;
	params->stream_tag = node_id.f.v_index + 1;

	/* map ipc4_bit_depth to sof_ipc_frame */
	params->frame_fmt = (params->sample_container_bytes >> 3) - 2;
	params->buffer_fmt = params->frame_fmt;
	params->buffer.size = cd->config.base.obs;

	for (i = 0; i < SOF_IPC_MAX_CHANNELS; i++)
		params->chmap[i] = (cd->config.out_fmt.ch_map >> i * 4) & 0xf;

	if (cd->buf) {
		buffer_set_params(cd->buf, params, BUFFER_UPDATE_FORCE);
		dir = PPL_CONN_DIR_COMP_TO_BUFFER;
		buffer = cd->buf;
		list_item_del(buffer->source_list.next);
		list_item_del(dev->bsink_list.next);
	} else {
		dir = PPL_CONN_DIR_BUFFER_TO_COMP;
		buffer = list_first_item(&dev->bsource_list, struct comp_buffer, sink_list);
		list_item_del(buffer->sink_list.next);
		list_item_del(dev->bsource_list.next);
	}

	if (cd->host) {
		update_internal_comp(dev, cd->host);

		connect_comp_to_buffer(cd->host, buffer, dir);
		ret = cd->host->drv->ops.params(cd->host, params);
		connect_comp_to_buffer(dev, buffer, dir);
	} else if (cd->dai) {
		update_internal_comp(dev, cd->dai);

		connect_comp_to_buffer(cd->dai, buffer, dir);
		ret = cd->dai->drv->ops.params(cd->dai, params);
		connect_comp_to_buffer(dev, buffer, dir);
	}

	if (ret < 0)
		ret = IPC4_ERROR_INVALID_PARAM;

	return ret;
}

static const struct comp_driver comp_copier = {
	.type	= SOF_COMP_COPIER,
	.uid	= SOF_RT_UUID(copier_comp_uuid),
	.tctx	= &copier_comp_tr,
	.ops	= {
		.create			= copier_new,
		.free			= copier_free,
		.trigger		= copier_comp_trigger,
		.copy			= copier_copy,
		.params			= copier_params,
		.prepare		= copier_prepare,
		.reset			= copier_reset,
	},
};

static SHARED_DATA struct comp_driver_info comp_copier_info = {
	.drv = &comp_copier,
};

UT_STATIC void sys_comp_copier_init(void)
{
	comp_register(platform_shared_get(&comp_copier_info,
					  sizeof(comp_copier_info)));
}

DECLARE_MODULE(sys_comp_copier_init);
