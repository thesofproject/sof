// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
// Author: Rander Wang <rander.wang@linux.intel.com>

#include <sof/audio/buffer.h>
#include <sof/audio/component_ext.h>
#include <sof/audio/pipeline.h>
#include <sof/common.h>
#include <sof/drivers/interrupt.h>
#include <sof/ipc/topology.h>
#include <sof/ipc/common.h>
#include <ipc/dai.h>
#include <sof/ipc/msg.h>
#include <sof/lib/mailbox.h>
#include <sof/list.h>
#include <sof/platform.h>
#include <sof/lib/wait.h>

#include <sof/sof.h>
#include <sof/spinlock.h>
#include <rimage/cavs/cavs_ext_manifest.h>
#include <rimage/sof/user/manifest.h>
#include <ipc4/base-config.h>
#include <ipc4/copier.h>
#include <ipc4/header.h>
#include <ipc4/pipeline.h>
#include <ipc4/module.h>
#include <ipc4/error_status.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define IPC4_MOD_ID(x) ((x) >> 16)

extern struct tr_ctx comp_tr;

void ipc_build_stream_posn(struct sof_ipc_stream_posn *posn, uint32_t type,
			   uint32_t id)
{
}

void ipc_build_comp_event(struct sof_ipc_comp_event *event, uint32_t type,
			  uint32_t id)
{
}

void ipc_build_trace_posn(struct sof_ipc_dma_trace_posn *posn)
{
}

static int ipc4_add_comp_dev(struct comp_dev *dev)
{
	struct ipc_core_ctx *ctx = *arch_ipc_get();
	struct ipc *ipc = ipc_get();
	struct ipc_comp_dev *icd;

	/* allocate the IPC component container */
	icd = rzalloc(SOF_MEM_ZONE_RUNTIME_SHARED, 0, SOF_MEM_CAPS_RAM,
		      sizeof(struct ipc_comp_dev));
	if (!icd) {
		tr_err(&ipc_tr, "ipc_comp_new(): alloc failed");
		ctx->error = IPC4_OUT_OF_MEMORY;
		return -ENOMEM;
	}

	icd->cd = dev;
	icd->type = COMP_TYPE_COMPONENT;
	icd->core = dev->ipc_config.core;
	icd->id = dev->ipc_config.id;

	tr_dbg(&ipc_tr, "ipc4_add_comp_dev add comp %x", icd->id);
	/* add new component to the list */
	list_item_append(&icd->list, &ipc->comp_list);

	return 0;
};

static void ipc4_free_comp_dev(struct comp_dev *dev)
{
	struct ipc *ipc = ipc_get();
	struct ipc_comp_dev *icd = ipc_get_comp_by_id(ipc, dev->ipc_config.id);

	if (!icd)
		return;

	list_item_del(&icd->list);
	rfree(icd);
}

struct comp_dev *comp_new(struct sof_ipc_comp *comp)
{
	struct comp_ipc_config ipc_config;
	const struct comp_driver *drv;
	struct comp_dev *dev;
	int ret;

	drv = ipc4_get_comp_drv(IPC4_MOD_ID(comp->id));
	if (!drv)
		return NULL;

	if (ipc4_get_comp_dev(comp->id)) {
		tr_err(&ipc_tr, "comp %d exists", comp->id);
		return NULL;
	}

	if (comp->core >= CONFIG_CORE_COUNT) {
		tr_err(&ipc_tr, "ipc: comp->core = %u", comp->core);
		return NULL;
	}

	memset(&ipc_config, 0, sizeof(ipc_config));
	ipc_config.id = comp->id;
	ipc_config.pipeline_id = comp->pipeline_id;
	ipc_config.core = comp->core;

	dcache_invalidate_region((void *)(MAILBOX_HOSTBOX_BASE),
				 MAILBOX_HOSTBOX_SIZE);

	/* Also copies ipc_config to dev->ipc_config */
	dev = drv->ops.create(drv, &ipc_config, (void *)MAILBOX_HOSTBOX_BASE);
	if (!dev)
		return NULL;

	/* ipc4_add_comp_dev() sets IPC4 error */
	ret = ipc4_add_comp_dev(dev);
	if (ret < 0) {
		drv->ops.free(dev);
		return NULL;
	}

	list_init(&dev->bsource_list);
	list_init(&dev->bsink_list);

	return dev;
}

static int ipc4_create_pipeline(struct ipc *ipc, uint32_t pipeline_id, uint32_t priority,
				uint32_t memory_size)
{
	struct ipc_core_ctx *ctx = *arch_ipc_get();
	struct ipc_comp_dev *ipc_pipe;
	struct pipeline *pipe;

	/* check whether pipeline id is already taken or in use */
	ipc_pipe = ipc_get_comp_by_ppl_id(ipc, COMP_TYPE_PIPELINE,
					  pipeline_id);
	if (ipc_pipe) {
		tr_err(&ipc_tr, "ipc: pipeline id is already taken, pipe_desc->instance_id = %u",
		       pipeline_id);
		ctx->error = IPC4_INVALID_RESOURCE_ID;
		return -EBUSY;
	}

	/* create the pipeline. FIXME: is a component ID of 0 really suitable? */
	pipe = pipeline_new(pipeline_id, priority, 0);
	if (!pipe) {
		tr_err(&ipc_tr, "ipc: pipeline_new() failed");
		ctx->error = IPC4_OUT_OF_MEMORY;
		return -ENOMEM;
	}

	pipe->time_domain = SOF_TIME_DOMAIN_TIMER;
	/* 1ms
	 * TODO: add DP scheduler support. Now only the
	 * LL scheduler tasks is supported.
	 */
	pipe->period = 1000;

	/* sched_id is set in FW so initialize it to a invalid value */
	pipe->sched_id = 0xFFFFFFFF;

	/* allocate the IPC pipeline container */
	ipc_pipe = rzalloc(SOF_MEM_ZONE_RUNTIME_SHARED, 0, SOF_MEM_CAPS_RAM,
			   sizeof(struct ipc_comp_dev));
	if (!ipc_pipe) {
		pipeline_free(pipe);
		ctx->error = IPC4_OUT_OF_MEMORY;
		return -ENOMEM;
	}

	ipc_pipe->pipeline = pipe;
	ipc_pipe->type = COMP_TYPE_PIPELINE;
	ipc_pipe->id = pipeline_id;

	/* add new pipeline to the list */
	list_item_append(&ipc_pipe->list, &ipc->comp_list);

	return 0;
}

int ipc_pipeline_new(struct ipc *ipc, ipc_pipe_new *_pipe_desc)
{
	struct ipc4_pipeline_create *pipe_desc = ipc_from_pipe_new(_pipe_desc);

	tr_dbg(&ipc_tr, "ipc: pipeline id = %u", (uint32_t)pipe_desc->header.r.instance_id);

	/* ipc4_create_pipeline() sets IPC4 error */
	return ipc4_create_pipeline(ipc, pipe_desc->header.r.instance_id,
		pipe_desc->header.r.ppl_priority, pipe_desc->header.r.ppl_mem_size);
}

static int ipc_pipeline_module_free(uint32_t pipeline_id)
{
	struct ipc *ipc = ipc_get();
	struct ipc_comp_dev *icd;
	int ret;

	/* Find all components on the pipeline and free them */
	while ((icd = ipc_get_comp_by_ppl_id(ipc, COMP_TYPE_COMPONENT, pipeline_id))) {
		ret = ipc_comp_free(ipc, icd->id);
		if (ret)
			return ret;
	}

	return 0;
}

int ipc_pipeline_free(struct ipc *ipc, uint32_t comp_id)
{
	struct ipc_core_ctx *ctx = *arch_ipc_get();
	struct ipc_comp_dev *ipc_pipe;
	int ret;

	/* check whether pipeline exists */
	ipc_pipe = ipc_get_comp_by_id(ipc, comp_id);
	if (!ipc_pipe) {
		ctx->error = IPC4_INVALID_RESOURCE_ID;
		return -ENODEV;
	}

	/* check core */
	if (!cpu_is_me(ipc_pipe->core)) {
		ret = ipc_process_on_core(ipc_pipe->core, false);
		if (ret < 0)
			ctx->error = IPC4_INVALID_CORE_ID;
		return ret;
	}

	ret = ipc_pipeline_module_free(ipc_pipe->pipeline->pipeline_id);
	if (ret) {
		tr_err(&ipc_tr, "ipc_pipeline_free(): module free () failed");
		ctx->error = IPC4_INVALID_RESOURCE_STATE;
		return ret;
	}

	/* free buffer and remove from list */
	ret = pipeline_free(ipc_pipe->pipeline);
	if (ret < 0) {
		tr_err(&ipc_tr, "ipc_pipeline_free(): pipeline_free() failed");
		ctx->error = IPC4_INVALID_RESOURCE_STATE;
		return ret;
	}

	list_item_del(&ipc_pipe->list);
	rfree(ipc_pipe);

	return 0;
}

static struct comp_buffer *ipc4_create_buffer(struct comp_dev *src, struct comp_dev *sink,
					      uint32_t src_queue, uint32_t dst_queue)
{
	struct ipc4_base_module_cfg *src_cfg;
	struct sof_ipc_buffer ipc_buf;
	int buf_size;

	src_cfg = (struct ipc4_base_module_cfg *)comp_get_drvdata(src);

	/* double it since obs is single buffer size */
	buf_size = src_cfg->obs * 2;
	memset(&ipc_buf, 0, sizeof(ipc_buf));
	ipc_buf.size = buf_size;
	ipc_buf.comp.id = IPC4_COMP_ID(src_queue, dst_queue);
	ipc_buf.comp.pipeline_id = src->ipc_config.pipeline_id;
	ipc_buf.comp.core = src->ipc_config.core;

	return buffer_new(&ipc_buf);
}

int ipc_comp_connect(struct ipc *ipc, ipc_pipe_comp_connect *_connect)
{
	struct ipc_core_ctx *ctx = *arch_ipc_get();
	struct ipc4_module_bind_unbind *bu;
	struct comp_buffer *buffer = NULL;
	struct comp_dev *source;
	struct comp_dev *sink;
	int src_id, sink_id;
	int ret;

	bu = (struct ipc4_module_bind_unbind *)_connect;
	src_id = IPC4_COMP_ID(bu->header.r.module_id, bu->header.r.instance_id);
	sink_id = IPC4_COMP_ID(bu->data.r.dst_module_id, bu->data.r.dst_instance_id);
	source = ipc4_get_comp_dev(src_id);
	sink = ipc4_get_comp_dev(sink_id);

	if (!source || !sink) {
		tr_err(&ipc_tr, "failed to find src %x, or dst %x", src_id, sink_id);
		ctx->error = IPC4_INVALID_RESOURCE_ID;
		return -ENODEV;
	}

	buffer = ipc4_create_buffer(source, sink, bu->data.r.src_queue,
				    bu->data.r.dst_queue);
	if (!buffer) {
		tr_err(&ipc_tr, "failed to allocate buffer to bind %d to %d", src_id, sink_id);
		ctx->error = IPC4_OUT_OF_MEMORY;
		return -ENOMEM;
	}

	ret = comp_buffer_connect(source, source->ipc_config.core, buffer,
				  PPL_CONN_DIR_COMP_TO_BUFFER);
	if (ret < 0) {
		tr_err(&ipc_tr, "failed to connect src %d to internal buffer", src_id);
		goto err;
	}

	ret = comp_buffer_connect(sink, sink->ipc_config.core, buffer,
				  PPL_CONN_DIR_BUFFER_TO_COMP);
	if (ret < 0) {
		tr_err(&ipc_tr, "failed to connect internal buffer to sink %d", sink_id);
		goto err;
	}

	/*
	 * FIXME: is the buffer not freed for these errors because it is already
	 * connected and will be freed normally during tear-down?
	 */
	ret = comp_bind(source, bu);
	if (ret < 0) {
		ctx->error = IPC4_INVALID_RESOURCE_ID;
		return ret;
	}

	ret = comp_bind(sink, bu);
	if (ret < 0)
		ctx->error = IPC4_INVALID_RESOURCE_ID;

	return ret;

err:
	buffer_free(buffer);
	ctx->error = IPC4_INVALID_RESOURCE_STATE;
	return ret;
}

/* when both module instances are parts of the same pipeline Unbind IPC would
 * be ignored by FW since FW does not support changing internal topology of pipeline
 * during run-time. The only way to change pipeline topology is to delete the whole
 * pipeline and create it in modified form.
 */
int ipc_comp_disconnect(struct ipc *ipc, ipc_pipe_comp_connect *_connect)
{
	struct ipc_core_ctx *ctx = *arch_ipc_get();
	struct ipc4_module_bind_unbind *bu;
	struct comp_buffer *buffer = NULL;
	struct comp_dev *src, *sink;
	struct list_item *sink_list;
	uint32_t src_id, sink_id, buffer_id;
	uint32_t flags;
	int ret;

	bu = (struct ipc4_module_bind_unbind *)_connect;
	src_id = IPC4_COMP_ID(bu->header.r.module_id, bu->header.r.instance_id);
	sink_id = IPC4_COMP_ID(bu->data.r.dst_module_id, bu->data.r.dst_instance_id);
	src = ipc4_get_comp_dev(src_id);
	sink = ipc4_get_comp_dev(sink_id);
	if (!src || !sink) {
		tr_err(&ipc_tr, "failed to find src %x, or dst %x", src_id, sink_id);
		ctx->error = IPC4_INVALID_RESOURCE_ID;
		return -ENODEV;
	}

	if (src->pipeline == sink->pipeline) {
		ctx->error = IPC4_INVALID_REQUEST;
		return -EINVAL;
	}

	buffer_id = IPC4_COMP_ID(bu->data.r.src_queue, bu->data.r.dst_queue);
	list_for_item(sink_list, &src->bsink_list) {
		struct comp_buffer *buf;

		buf = container_of(sink_list, struct comp_buffer, source_list);
		if (buf->id == buffer_id) {
			buffer = buf;
			break;
		}
	}

	if (!buffer) {
		ctx->error = IPC4_INVALID_RESOURCE_ID;
		return -ENOENT;
	}

	irq_local_disable(flags);
	list_item_del(buffer_comp_list(buffer, PPL_CONN_DIR_COMP_TO_BUFFER));
	list_item_del(buffer_comp_list(buffer, PPL_CONN_DIR_BUFFER_TO_COMP));
	comp_writeback(src);
	comp_writeback(sink);
	irq_local_enable(flags);

	buffer_free(buffer);

	ret = comp_unbind(src, bu);
	if (ret < 0) {
		ctx->error = IPC4_INVALID_RESOURCE_ID;
		return ret;
	}

	ret = comp_unbind(sink, bu);
	if (ret < 0)
		ctx->error = IPC4_INVALID_RESOURCE_ID;

	return ret;
}

/* dma index may be for playback or capture. Current
 * hw supports PLATFORM_MAX_DMA_CHAN of playback
 * and the index more than this will be capture. This function
 * convert dma id to dma channel
 */
static int process_dma_index(uint32_t dma_id, uint32_t *dir, uint32_t *chan)
{
	struct ipc_core_ctx *ctx = *arch_ipc_get();

	if (dma_id > DAI_NUM_HDA_OUT + DAI_NUM_HDA_IN) {
		tr_err(&ipc_tr, "dma id %d is out of range", dma_id);
		ctx->error = IPC4_INVALID_NODE_ID;
		return -EINVAL;
	}

	if (dma_id >= PLATFORM_MAX_DMA_CHAN) {
		*dir = SOF_IPC_STREAM_CAPTURE;
		*chan = dma_id - PLATFORM_MAX_DMA_CHAN;
	} else {
		*dir = SOF_IPC_STREAM_PLAYBACK;
		*chan = dma_id;
	}

	return 0;
}

static struct comp_dev *ipc4_create_host(uint32_t pipeline_id, uint32_t id, uint32_t dir)
{
	struct sof_uuid uuid = {0x8b9d100c, 0x6d78, 0x418f, {0x90, 0xa3, 0xe0,
			0xe8, 0x05, 0xd0, 0x85, 0x2b}};
	struct ipc_config_host ipc_host;
	struct comp_ipc_config config;
	const struct comp_driver *drv;
	struct comp_dev *dev;

	drv = ipc4_get_drv((uint8_t *)&uuid);
	if (!drv)
		return NULL;

	memset(&config, 0, sizeof(config));
	config.type = SOF_COMP_HOST;
	config.pipeline_id = pipeline_id;
	config.core = 0;
	config.id = id;

	memset(&ipc_host, 0, sizeof(ipc_host));
	ipc_host.direction = dir;

	dev = drv->ops.create(drv, &config, &ipc_host);
	if (!dev)
		return NULL;

	dev->direction = dir;
	list_init(&dev->bsource_list);
	list_init(&dev->bsink_list);

	return dev;
}

static struct comp_dev *ipc4_create_dai(struct pipeline *pipe, uint32_t id, uint32_t dir,
					struct ipc4_copier_module_cfg *copier_cfg)
{
	struct sof_uuid uuid = {0xc2b00d27, 0xffbc, 0x4150, {0xa5, 0x1a, 0x24,
				0x5c, 0x79, 0xc5, 0xe5, 0x4b}};
	const struct comp_driver *drv;
	struct comp_ipc_config config;
	struct ipc_config_dai dai;
	struct comp_dev *dev;
	int ret;

	drv = ipc4_get_drv((uint8_t *)&uuid);
	if (!drv)
		return NULL;

	memset(&config, 0, sizeof(config));
	config.type = SOF_COMP_DAI;
	config.pipeline_id = pipe->pipeline_id;
	config.core = 0;
	config.id = id;

	memset(&dai, 0, sizeof(dai));
	dai.dai_index = DAI_NUM_HDA_OUT + DAI_NUM_HDA_IN - 1;
	dai.type = SOF_DAI_INTEL_HDA;
	dai.is_config_blob = true;
	dai.direction = dir;
	dev = drv->ops.create(drv, &config, &dai);
	if (!dev)
		return NULL;

	pipe->sched_id = id;

	dev->direction = dir;
	list_init(&dev->bsource_list);
	list_init(&dev->bsink_list);

	ret = comp_dai_config(dev, &dai, copier_cfg);
	if (ret < 0) {
		drv->ops.free(dev);
		return NULL;
	}

	return dev;
}

/* host does not send any params to FW since it expects simple copy
 * but sof needs hw params to feed pass-through pipeline. This
 * function rebuild the hw params based on fifo_size since only 48K
 * and 44.1K sample rate and 16 & 24bit are supported by chain dma.
 */
static void construct_config(struct ipc4_copier_module_cfg *copier_cfg, uint32_t fifo_size,
			     struct sof_ipc_stream_params *params)
{
	uint32_t frame_size;

	/* fifo_size = rate * channel * depth */
	if (fifo_size % 48 == 0)
		copier_cfg->base.audio_fmt.sampling_frequency = IPC4_FS_48000HZ;
	else
		copier_cfg->base.audio_fmt.sampling_frequency = IPC4_FS_44100HZ;
	params->rate = copier_cfg->base.audio_fmt.sampling_frequency;

	frame_size = fifo_size / (copier_cfg->base.audio_fmt.sampling_frequency / 1000);
	/* convert double buffer to single buffer */
	frame_size >>= 1;

	/* two cases: 16 bits or 24 bits sample size */
	if (frame_size % 3 == 0) {
		copier_cfg->base.audio_fmt.depth = 24;
		copier_cfg->base.audio_fmt.valid_bit_depth = 24;
		params->frame_fmt = SOF_IPC_FRAME_S24_3LE;
		params->sample_container_bytes = 3;
		params->sample_valid_bytes = 3;
	} else {
		copier_cfg->base.audio_fmt.depth = 16;
		copier_cfg->base.audio_fmt.valid_bit_depth = 16;
		params->frame_fmt = SOF_IPC_FRAME_S16_LE;
		params->sample_container_bytes = 2;
		params->sample_valid_bytes = 2;
	}

	copier_cfg->base.audio_fmt.channels_count = frame_size /
		(copier_cfg->base.audio_fmt.depth / 8);

	params->channels = copier_cfg->base.audio_fmt.channels_count;
	params->buffer.size = fifo_size;

	copier_cfg->out_fmt = copier_cfg->base.audio_fmt;
	copier_cfg->base.ibs = fifo_size;
	copier_cfg->base.obs = fifo_size;
}

int ipc4_create_chain_dma(struct ipc *ipc, struct ipc4_chain_dma *cdma)
{
	struct ipc_core_ctx *ctx = *arch_ipc_get();
	struct ipc4_copier_module_cfg copier_cfg;
	struct sof_ipc_stream_params params;
	struct ipc_comp_dev *ipc_pipe;
	struct sof_ipc_buffer ipc_buf;
	struct comp_dev *host;
	struct comp_dev *dai;
	struct comp_buffer *buf;
	struct comp_dev *src;
	struct comp_dev *sink;
	uint32_t pipeline_id;
	uint32_t host_id;
	uint32_t dai_id;
	uint32_t buf_id;
	uint32_t dir, host_chan, link_chan;
	int ret;

	/* process_dma_index() sets IPC4 error */
	ret = process_dma_index(cdma->header.r.host_dma_id, &dir, &host_chan);
	if (ret < 0)
		return ret;

	ret = process_dma_index(cdma->header.r.link_dma_id, &dir, &link_chan);
	if (ret < 0)
		return ret;

	/* build a pipeline id based on dma id */
	pipeline_id = IPC4_COMP_ID(cdma->header.r.host_dma_id + IPC4_MAX_MODULE_COUNT,
				   cdma->header.r.link_dma_id);
	/* ipc4_create_pipeline() sets IPC4 error */
	ret = ipc4_create_pipeline(ipc, pipeline_id, 0, cdma->data.r.fifo_size);
	if (ret < 0) {
		tr_err(&comp_tr, "failed to create pipeline for chain dma");
		return ret;
	}

	ipc_pipe = ipc_get_comp_by_id(ipc, pipeline_id);
	if (!ipc_pipe) {
		/*
		 * We just created this pipeline. If we cannot find it again, we
		 * either have a bug or a race. We won't be able to free it
		 * either.
		 */
		ctx->error = IPC4_FAILURE;
		return -EFAULT;
	}

	host_id = pipeline_id + 1;
	host = ipc4_create_host(pipeline_id, host_id, dir);
	if (!host) {
		tr_err(&comp_tr, "failed to create host for chain dma");
		ctx->error = IPC4_INVALID_REQUEST;
		ret = -ENOENT;
		goto e_pipe;
	}

	host->period = ipc_pipe->pipeline->period;

	/* ipc4_add_comp_dev() sets IPC4 error */
	ret = ipc4_add_comp_dev(host);
	if (ret < 0)
		goto e_host_create;

	memset(&params, 0, sizeof(params));
	memset(&copier_cfg, 0, sizeof(copier_cfg));
	construct_config(&copier_cfg, cdma->data.r.fifo_size, &params);
	params.direction = dir;
	params.stream_tag = host_chan + 1;

	dai_id = host_id + 1;
	dai = ipc4_create_dai(ipc_pipe->pipeline, dai_id, dir, &copier_cfg);
	if (!dai) {
		tr_err(&comp_tr, "failed to create dai for chain dma");
		ctx->error = IPC4_INVALID_REQUEST;
		ret = -ENOENT;
		goto e_host_add;
	}

	dai->period = ipc_pipe->pipeline->period;

	/* ipc4_add_comp_dev() sets IPC4 error */
	ret = ipc4_add_comp_dev(dai);
	if (ret < 0)
		goto e_dai_create;

	buf_id = dai_id + 1;
	if (dir == SOF_IPC_STREAM_PLAYBACK) {
		src = host;
		sink = dai;
	} else {
		src = dai;
		sink = host;
	}

	memset(&ipc_buf, 0, sizeof(ipc_buf));
	ipc_buf.size = cdma->data.r.fifo_size;
	ipc_buf.comp.id = buf_id;
	ipc_buf.comp.pipeline_id = pipeline_id;
	ipc_buf.comp.core = src->ipc_config.core;
	buf = buffer_new(&ipc_buf);
	if (!buf) {
		tr_err(&comp_tr, "failed to create buffer for chain dma");
		ctx->error = IPC4_OUT_OF_MEMORY;
		ret = -ENOMEM;
		goto e_dai_add;
	}

	comp_buffer_connect(src, src->ipc_config.core, buf, PPL_CONN_DIR_COMP_TO_BUFFER);
	comp_buffer_connect(sink, sink->ipc_config.core, buf, PPL_CONN_DIR_BUFFER_TO_COMP);

	ipc_pipe->pipeline->sched_comp = host;
	ipc_pipe->pipeline->source_comp = src;
	ipc_pipe->pipeline->sink_comp = sink;

	/* FIXME: shouldn't ipc_pipe->pipeline->core be set? */
	ret = ipc_pipeline_complete(ipc, pipeline_id);
	if (ret < 0) {
		ctx->error = IPC4_INVALID_RESOURCE_STATE;
		goto e_buf;
	}

	/* set up host & dai and start pipeline */
	if (cdma->header.r.enable) {
		buf->stream.channels = params.channels;
		buf->stream.frame_fmt = params.frame_fmt;
		buf->stream.valid_sample_fmt = params.frame_fmt;
		buf->stream.rate = params.rate;

		ret = host->drv->ops.params(host, &params);
		if (ret < 0) {
			tr_err(&ipc_tr, "failed to set host params %d", ret);
			ctx->error = IPC4_ERROR_INVALID_PARAM;
			goto e_buf;
		}

		ret = dai->drv->ops.params(dai, &params);
		if (ret < 0) {
			tr_err(&ipc_tr, "failed to set dai params %d", ret);
			ctx->error = IPC4_ERROR_INVALID_PARAM;
			goto e_buf;
		}

		ret = pipeline_prepare(ipc_pipe->pipeline, host);
		if (ret < 0) {
			tr_err(&ipc_tr, "failed to prepare for chain dma %d", ret);
			ctx->error = IPC4_INVALID_RESOURCE_STATE;
			goto e_buf;
		}
	}

	return 0;

e_buf:
	/* undo buffer_new() */
	buffer_free(buf);
e_dai_add:
	/* undo ipc4_add_comp_dev() */
	ipc4_free_comp_dev(dai);
e_dai_create:
	/* undo ipc4_create_dai() */
	dai->drv->ops.free(dai);
e_host_add:
	/* undo ipc4_add_comp_dev() */
	ipc4_free_comp_dev(host);
e_host_create:
	/* undo ipc4_create_host() */
	host->drv->ops.free(host);
e_pipe:
	/* undo ipc4_create_pipeline() */
	ipc_pipeline_free(ipc_get(), pipeline_id);

	return ret;
}

/* Can return 0, a negative error, or one of PPL_STATUS_* codes */
int ipc4_trigger_chain_dma(struct ipc *ipc, struct ipc4_chain_dma *cdma)
{
	struct ipc_core_ctx *ctx = *arch_ipc_get();
	struct ipc_comp_dev *ipc_pipe;
	uint32_t pipeline_id;
	int ret;

	pipeline_id = IPC4_COMP_ID(cdma->header.r.host_dma_id + IPC4_MAX_MODULE_COUNT,
				   cdma->header.r.link_dma_id);
	ipc_pipe = ipc_get_comp_by_id(ipc, pipeline_id);
	if (!ipc_pipe) {
		ctx->error = IPC4_INVALID_RESOURCE_ID;
		return -ENODEV;
	}

	/* FIXME: shouldn't this run on the pipeline core? */

	/* pause or release chain dma */
	if (!cdma->header.r.enable) {
		if (ipc_pipe->pipeline->status == COMP_STATE_ACTIVE) {
			ret = pipeline_trigger(ipc_pipe->pipeline, ipc_pipe->pipeline->sink_comp,
					       COMP_TRIGGER_PAUSE);
			if (ret < 0) {
				tr_err(&ipc_tr, "failed to disable chain dma %d", ret);
				ctx->error = IPC4_BAD_STATE;
				return ret;
			}
		}

		/* release chain dma */
		if (!cdma->header.r.allocate) {
			ret = pipeline_reset(ipc_pipe->pipeline, ipc_pipe->pipeline->sink_comp);
			if (ret < 0) {
				tr_err(&ipc_tr, "failed to reset chain dma %d", ret);
				ctx->error = IPC4_BAD_STATE;
				return ret;
			}

			/* ipc_pipeline_free() sets IPC4 error */
			ret = ipc_pipeline_free(ipc, pipeline_id);
			if (ret < 0) {
				tr_err(&ipc_tr, "failed to free chain dma %d", ret);
				return ret;
			}
		}

		return 0;
	}

	if (!cdma->header.r.allocate) {
		tr_err(&ipc_tr, "can't enable chain dma");
		ctx->error = IPC4_INVALID_REQUEST;
		return -EINVAL;
	}

	switch (ipc_pipe->pipeline->status) {
	case COMP_STATE_PAUSED:
		ret = pipeline_trigger(ipc_pipe->pipeline, ipc_pipe->pipeline->sink_comp,
				       COMP_TRIGGER_PRE_RELEASE);
		if (ret < 0) {
			tr_err(&ipc_tr, "failed to resume chain dma %d", ret);
			ctx->error = IPC4_BAD_STATE;
		}
		return ret;
	case COMP_STATE_READY:
	case COMP_STATE_PREPARE:
		ret = pipeline_trigger(ipc_pipe->pipeline, ipc_pipe->pipeline->sink_comp,
				       COMP_TRIGGER_PRE_START);
		if (ret < 0) {
			tr_err(&ipc_tr, "failed to start chain dma %d", ret);
			ctx->error = IPC4_BAD_STATE;
		}
		return ret;
	}

	return 0;
}

const struct comp_driver *ipc4_get_drv(uint8_t *uuid)
{
	struct comp_driver_list *drivers = comp_drivers_get();
	struct list_item *clist;
	const struct comp_driver *drv = NULL;
	struct comp_driver_info *info;
	uint32_t flags;

	irq_local_disable(flags);

	/* search driver list with UUID */
	list_for_item(clist, &drivers->list) {
		info = container_of(clist, struct comp_driver_info,
				    list);
		if (!memcmp(info->drv->uid, uuid, UUID_SIZE)) {
			tr_dbg(&comp_tr,
			       "found type %d, uuid %pU",
			       info->drv->type,
			       info->drv->tctx->uuid_p);
			drv = info->drv;
			goto out;
		}
	}

	tr_err(&comp_tr, "get_drv(): the provided UUID (%8x %8x %8x %8x) can't be found!",
	       *(uint32_t *)(&uuid[0]),
	       *(uint32_t *)(&uuid[4]),
	       *(uint32_t *)(&uuid[8]),
	       *(uint32_t *)(&uuid[12]));

out:
	irq_local_enable(flags);
	return drv;
}

const struct comp_driver *ipc4_get_comp_drv(int module_id)
{
	struct sof_man_fw_desc *desc = (struct sof_man_fw_desc *)IMR_BOOT_LDR_MANIFEST_BASE;
	struct sof_man_module *mod;
	int entry_index;

	/* module_id 0 is used for base fw which is in entry 1 */
	if (!module_id)
		entry_index = 1;
	else
		entry_index = module_id;

	mod = (struct sof_man_module *)((char *)desc + SOF_MAN_MODULE_OFFSET(entry_index));

	return ipc4_get_drv(mod->uuid);
}

struct comp_dev *ipc4_get_comp_dev(uint32_t comp_id)
{
	struct ipc *ipc = ipc_get();
	struct ipc_comp_dev *icd;

	icd = ipc_get_comp_by_id(ipc, comp_id);
	if (!icd)
		return NULL;

	return icd->cd;
}
