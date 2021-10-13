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
#include <sof/ipc/msg.h>
#include <sof/lib/mailbox.h>
#include <sof/list.h>
#include <sof/platform.h>
#include <sof/sof.h>
#include <sof/spinlock.h>
#include <rimage/cavs/cavs_ext_manifest.h>
#include <rimage/sof/user/manifest.h>
#include <ipc4/base-config.h>
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

struct comp_dev *comp_new(struct sof_ipc_comp *comp)
{
	struct comp_ipc_config ipc_config;
	const struct comp_driver *drv;
	struct comp_dev *dev;

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

	dev = drv->ops.create(drv, &ipc_config, (void *)MAILBOX_HOSTBOX_BASE);
	if (!dev)
		return NULL;

	list_init(&dev->bsource_list);
	list_init(&dev->bsink_list);

	ipc4_add_comp_dev(dev);

	return dev;
}

int ipc_pipeline_new(struct ipc *ipc, ipc_pipe_new *_pipe_desc)
{
	struct ipc4_pipeline_create *pipe_desc = ipc_from_pipe_new(_pipe_desc);
	struct ipc_comp_dev *ipc_pipe;
	struct pipeline *pipe;

	tr_dbg(&ipc_tr, "ipc: pipeline id = %u", (uint32_t)pipe_desc->header.r.instance_id);

	/* check whether pipeline id is already taken or in use */
	ipc_pipe = ipc_get_comp_by_ppl_id(ipc, COMP_TYPE_PIPELINE,
					  pipe_desc->header.r.instance_id);
	if (ipc_pipe) {
		tr_err(&ipc_tr, "ipc: pipeline id is already taken, pipe_desc->instance_id = %u",
		       (uint32_t)pipe_desc->header.r.instance_id);
		return IPC4_INVALID_RESOURCE_ID;
	}

	/* create the pipeline */
	pipe = pipeline_new(pipe_desc->header.r.instance_id,
			    pipe_desc->header.r.ppl_priority, 0);
	if (!pipe) {
		tr_err(&ipc_tr, "ipc: pipeline_new() failed");
		return IPC4_OUT_OF_MEMORY;
	}

	pipe->time_domain = SOF_TIME_DOMAIN_TIMER;
	/* 1ms
	 * TODO: add DP scheduler support. Now only the
	 * LL scheduler tasks is supported.
	 */
	pipe->period = 1000;

	/* allocate the IPC pipeline container */
	ipc_pipe = rzalloc(SOF_MEM_ZONE_RUNTIME_SHARED, 0, SOF_MEM_CAPS_RAM,
			   sizeof(struct ipc_comp_dev));
	if (!ipc_pipe) {
		pipeline_free(pipe);
		return IPC4_OUT_OF_MEMORY;
	}

	ipc_pipe->pipeline = pipe;
	ipc_pipe->type = COMP_TYPE_PIPELINE;
	ipc_pipe->id = pipe_desc->header.r.instance_id;

	/* add new pipeline to the list */
	list_item_append(&ipc_pipe->list, &ipc->comp_list);

	return 0;
}

static int ipc_pipeline_module_free(uint32_t pipeline_id)
{
	struct ipc *ipc = ipc_get();
	struct ipc_comp_dev *icd;
	int ret;

	icd = ipc_get_comp_by_ppl_id(ipc, COMP_TYPE_COMPONENT, pipeline_id);
	while (icd) {
		ret = ipc_comp_free(ipc, icd->id);
		if (ret)
			return ret;

		icd = ipc_get_comp_by_ppl_id(ipc, COMP_TYPE_COMPONENT, pipeline_id);
	}

	return IPC4_SUCCESS;
}

int ipc_pipeline_free(struct ipc *ipc, uint32_t comp_id)
{
	struct ipc_comp_dev *ipc_pipe;
	int ret;

	/* check whether pipeline exists */
	ipc_pipe = ipc_get_comp_by_id(ipc, comp_id);
	if (!ipc_pipe)
		return -ENODEV;

	/* check core */
	if (!cpu_is_me(ipc_pipe->core))
		return ipc_process_on_core(ipc_pipe->core, false);

	ret = ipc_pipeline_module_free(ipc_pipe->pipeline->pipeline_id);
	if (ret) {
		tr_err(&ipc_tr, "ipc_pipeline_free(): module free () failed");
		return ret;
	}

	/* free buffer and remove from list */
	ret = pipeline_free(ipc_pipe->pipeline);
	if (ret < 0) {
		tr_err(&ipc_tr, "ipc_pipeline_free(): pipeline_free() failed");
		return IPC4_INVALID_RESOURCE_STATE;
	}

	ipc_pipe->pipeline = NULL;
	list_item_del(&ipc_pipe->list);
	rfree(ipc_pipe);

	return IPC4_SUCCESS;
}

static struct comp_buffer *ipc4_create_buffer(struct comp_dev *src, struct comp_dev *sink,
					      uint32_t src_queue, uint32_t dst_queue)
{
	struct ipc4_base_module_cfg *src_cfg, *sink_cfg;
	struct comp_buffer *buffer = NULL;
	struct sof_ipc_buffer ipc_buf;
	int buf_size;

	src_cfg = (struct ipc4_base_module_cfg *)comp_get_drvdata(src);
	sink_cfg = (struct ipc4_base_module_cfg *)comp_get_drvdata(sink);

	buf_size = MAX(src_cfg->obs, sink_cfg->ibs);
	memset(&ipc_buf, 0, sizeof(ipc_buf));
	ipc_buf.size = buf_size;
	ipc_buf.comp.id = IPC4_COMP_ID(src_queue, dst_queue);
	ipc_buf.comp.pipeline_id = src->ipc_config.pipeline_id;
	ipc_buf.comp.core = src->ipc_config.core;
	buffer = buffer_new(&ipc_buf);

	return buffer;
}

int ipc_comp_connect(struct ipc *ipc, ipc_pipe_comp_connect *_connect)
{
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
		return IPC4_INVALID_RESOURCE_ID;
	}

	buffer = ipc4_create_buffer(source, sink, bu->data.r.src_queue,
				    bu->data.r.dst_queue);
	if (!buffer) {
		tr_err(&ipc_tr, "failed to allocate buffer to bind %d to %d", src_id, sink_id);
		return IPC4_OUT_OF_MEMORY;
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

	ret = comp_bind(source, bu);
	if (ret < 0)
		return IPC4_INVALID_RESOURCE_ID;

	ret = comp_bind(sink, bu);
	if (ret < 0)
		return IPC4_INVALID_RESOURCE_ID;

	return 0;

err:
	buffer_free(buffer);
	return IPC4_INVALID_RESOURCE_STATE;
}

/* when both module instances are parts of the same pipeline Unbind IPC would
 * be ignored by FW since FW does not support changing internal topology of pipeline
 * during run-time. The only way to change pipeline topology is to delete the whole
 * pipeline and create it in modified form.
 */
int ipc_comp_disconnect(struct ipc *ipc, ipc_pipe_comp_connect *_connect)
{
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
		return IPC4_INVALID_RESOURCE_ID;
	}

	if (src->pipeline == sink->pipeline)
		return IPC4_INVALID_REQUEST;

	buffer_id = IPC4_COMP_ID(bu->data.r.src_queue, bu->data.r.dst_queue);
	list_for_item(sink_list, &src->bsink_list) {
		struct comp_buffer *buf;

		buf = container_of(sink_list, struct comp_buffer, source_list);
		if (buf->id == buffer_id) {
			buffer = buf;
			break;
		}
	}

	if (!buffer)
		return IPC4_INVALID_RESOURCE_ID;

	irq_local_disable(flags);
	list_item_del(buffer_comp_list(buffer, PPL_CONN_DIR_COMP_TO_BUFFER));
	list_item_del(buffer_comp_list(buffer, PPL_CONN_DIR_BUFFER_TO_COMP));
	irq_local_enable(flags);

	buffer_free(buffer);

	ret = comp_unbind(src, bu);
	if (ret < 0)
		return IPC4_INVALID_RESOURCE_ID;

	ret = comp_unbind(sink, bu);
	if (ret < 0)
		return IPC4_INVALID_RESOURCE_ID;

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
	const struct comp_driver *drv;

	/* skip basefw of module 0 in manifest */
	mod = (struct sof_man_module *)((char *)desc + SOF_MAN_MODULE_OFFSET(module_id));
	drv = ipc4_get_drv(mod->uuid);

	return drv;
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

int ipc4_add_comp_dev(struct comp_dev *dev)
{
	struct ipc *ipc = ipc_get();
	struct ipc_comp_dev *icd;

	/* allocate the IPC component container */
	icd = rzalloc(SOF_MEM_ZONE_RUNTIME_SHARED, 0, SOF_MEM_CAPS_RAM,
		      sizeof(struct ipc_comp_dev));
	if (!icd) {
		tr_err(&ipc_tr, "ipc_comp_new(): alloc failed");
		rfree(icd);
		return IPC4_OUT_OF_MEMORY;
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
