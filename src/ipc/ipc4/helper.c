// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
// Author: Rander Wang <rander.wang@linux.intel.com>

#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/audio/component_ext.h>
#include <sof/audio/pipeline.h>
#include <sof/common.h>
#include <rtos/interrupt.h>
#include <sof/ipc/topology.h>
#include <sof/ipc/common.h>
#include <ipc/dai.h>
#include <sof/ipc/msg.h>
#include <sof/lib/mailbox.h>
#include <sof/list.h>
#include <sof/platform.h>
#include <sof/schedule/ll_schedule_domain.h>
#include <rtos/wait.h>

/* TODO: Remove platform-specific code, see https://github.com/thesofproject/sof/issues/7549 */
#if defined(CONFIG_SOC_SERIES_INTEL_ACE) || defined(CONFIG_INTEL_ADSP_CAVS)
#define RIMAGE_MANIFEST 1
#endif

#ifdef RIMAGE_MANIFEST
#include <adsp_memory.h>
#endif

#include <rtos/sof.h>
#include <rtos/spinlock.h>
#include <rimage/cavs/cavs_ext_manifest.h>
#include <rimage/sof/user/manifest.h>
#include <ipc4/base-config.h>
#include <ipc4/copier.h>
#include <ipc/header.h>
#include <ipc4/notification.h>
#include <ipc4/pipeline.h>
#include <ipc4/module.h>
#include <ipc4/error_status.h>
#include <sof/lib_manager.h>

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

LOG_MODULE_DECLARE(ipc, CONFIG_SOF_LOG_LEVEL);

extern struct tr_ctx comp_tr;

void ipc_build_stream_posn(struct sof_ipc_stream_posn *posn, uint32_t type,
			   uint32_t id)
{
	memset(posn, 0, sizeof(*posn));
}

void ipc_build_comp_event(struct sof_ipc_comp_event *event, uint32_t type,
			  uint32_t id)
{
}

bool ipc_trigger_trace_xfer(uint32_t avail)
{
	return avail >= DMA_TRACE_LOCAL_SIZE / 2;
}

void ipc_build_trace_posn(struct sof_ipc_dma_trace_posn *posn)
{
	posn->rhdr.hdr.cmd =  SOF_IPC4_NOTIF_HEADER(SOF_IPC4_NOTIFY_LOG_BUFFER_STATUS);
	posn->rhdr.hdr.size = 0;
}

#if CONFIG_LIBRARY
static inline char *ipc4_get_comp_new_data(void)
{
	struct ipc *ipc = ipc_get();
	char *data = (char *)ipc->comp_data + sizeof(struct ipc4_module_init_instance);

	return data;
}
#else
static inline char *ipc4_get_comp_new_data(void)
{
	return (char *)MAILBOX_HOSTBOX_BASE;
}
#endif

struct comp_dev *comp_new_ipc4(struct ipc4_module_init_instance *module_init)
{
	struct comp_ipc_config ipc_config;
	const struct comp_driver *drv;
	struct comp_dev *dev;
	uint32_t comp_id;
	char *data;

	comp_id = IPC4_COMP_ID(module_init->primary.r.module_id,
			       module_init->primary.r.instance_id);
	drv = ipc4_get_comp_drv(IPC4_MOD_ID(comp_id));
	if (!drv)
		return NULL;

	if (ipc4_get_comp_dev(comp_id)) {
		tr_err(&ipc_tr, "comp %d exists", comp_id);
		return NULL;
	}

	if (module_init->extension.r.core_id >= CONFIG_CORE_COUNT) {
		tr_err(&ipc_tr, "ipc: comp->core = %u", (uint32_t)module_init->extension.r.core_id);
		return NULL;
	}

	memset(&ipc_config, 0, sizeof(ipc_config));
	ipc_config.id = comp_id;
	ipc_config.pipeline_id = module_init->extension.r.ppl_instance_id;
	ipc_config.core = module_init->extension.r.core_id;
	ipc_config.ipc_config_size = module_init->extension.r.param_block_size * sizeof(uint32_t);

#if CONFIG_ZEPHYR_DP_SCHEDULER
	if (module_init->extension.r.proc_domain)
		ipc_config.proc_domain = COMP_PROCESSING_DOMAIN_DP;
	else
		ipc_config.proc_domain = COMP_PROCESSING_DOMAIN_LL;
#else /* CONFIG_ZEPHYR_DP_SCHEDULER */
	if (module_init->extension.r.proc_domain) {
		tr_err(&ipc_tr, "ipc: DP scheduling is disabled, cannot create comp %d", comp_id);
		return NULL;
	}
	ipc_config.proc_domain = COMP_PROCESSING_DOMAIN_LL;
#endif /* CONFIG_ZEPHYR_DP_SCHEDULER */

	dcache_invalidate_region((__sparse_force void __sparse_cache *)MAILBOX_HOSTBOX_BASE,
				 MAILBOX_HOSTBOX_SIZE);

	data = ipc4_get_comp_new_data();
	if (drv->type == SOF_COMP_MODULE_ADAPTER) {
		const struct ipc_config_process spec = {
			.data = (const unsigned char *)data,
			/* spec_size in IPC4 is in DW. Convert to bytes. */
			.size = module_init->extension.r.param_block_size * sizeof(uint32_t),
		};

		dev = drv->ops.create(drv, &ipc_config, (const void *)&spec);
	} else {
		dev = drv->ops.create(drv, &ipc_config, (const void *)data);
	}
	if (!dev)
		return NULL;

	list_init(&dev->bsource_list);
	list_init(&dev->bsink_list);

	ipc4_add_comp_dev(dev);

	return dev;
}

struct ipc_comp_dev *ipc_get_comp_by_ppl_id(struct ipc *ipc, uint16_t type, uint32_t ppl_id)
{
	struct ipc_comp_dev *icd;
	struct list_item *clist;

	list_for_item(clist, &ipc->comp_list) {
		icd = container_of(clist, struct ipc_comp_dev, list);
		if (icd->type != type)
			continue;

		/* For IPC4, ipc_comp_dev.id field is equal to Pipeline ID
		 * in case of type COMP_TYPE_PIPELINE - can check directly here
		 */
		if (type == COMP_TYPE_PIPELINE) {
			if (icd->id == ppl_id)
				return icd;
		} else {
			if (!cpu_is_me(icd->core))
				continue;
			if (ipc_comp_pipe_id(icd) == ppl_id)
				return icd;
		}
	}
	return NULL;
}

static int ipc4_create_pipeline(struct ipc4_pipeline_create *pipe_desc)
{
	struct ipc_comp_dev *ipc_pipe;
	struct pipeline *pipe;
	struct ipc *ipc = ipc_get();

	/* check whether pipeline id is already taken or in use */
	ipc_pipe = ipc_get_pipeline_by_id(ipc, pipe_desc->primary.r.instance_id);
	if (ipc_pipe) {
		tr_err(&ipc_tr, "ipc: comp id is already taken, pipe_desc->instance_id = %u",
		       (uint32_t)pipe_desc->primary.r.instance_id);
		return IPC4_INVALID_RESOURCE_ID;
	}

	/* create the pipeline */
	pipe = pipeline_new(pipe_desc->primary.r.instance_id, pipe_desc->primary.r.ppl_priority, 0);
	if (!pipe) {
		tr_err(&ipc_tr, "ipc: pipeline_new() failed");
		return IPC4_OUT_OF_MEMORY;
	}

	pipe->time_domain = SOF_TIME_DOMAIN_TIMER;
	pipe->period = LL_TIMER_PERIOD_US;

	/* sched_id is set in FW so initialize it to a invalid value */
	pipe->sched_id = 0xFFFFFFFF;

	pipe->core = pipe_desc->extension.r.core_id;

	/* allocate the IPC pipeline container */
	ipc_pipe = rzalloc(SOF_MEM_ZONE_RUNTIME_SHARED, 0, SOF_MEM_CAPS_RAM,
			   sizeof(struct ipc_comp_dev));
	if (!ipc_pipe) {
		pipeline_free(pipe);
		return IPC4_OUT_OF_MEMORY;
	}

	ipc_pipe->pipeline = pipe;
	ipc_pipe->type = COMP_TYPE_PIPELINE;
	ipc_pipe->id = pipe_desc->primary.r.instance_id;
	ipc_pipe->core = pipe_desc->extension.r.core_id;
	ipc_pipe->pipeline->attributes = pipe_desc->extension.r.attributes;

	/* add new pipeline to the list */
	list_item_append(&ipc_pipe->list, &ipc->comp_list);

	return IPC4_SUCCESS;
}

int ipc_pipeline_new(struct ipc *ipc, ipc_pipe_new *_pipe_desc)
{
	struct ipc4_pipeline_create *pipe_desc = ipc_from_pipe_new(_pipe_desc);

	tr_dbg(&ipc_tr, "ipc: pipeline id = %u", (uint32_t)pipe_desc->primary.r.instance_id);

	/* pass IPC to target core */
	if (!cpu_is_me(pipe_desc->extension.r.core_id))
		return ipc4_process_on_core(pipe_desc->extension.r.core_id, false);

	return ipc4_create_pipeline(pipe_desc);
}

static int ipc_pipeline_module_free(uint32_t pipeline_id)
{
	struct ipc *ipc = ipc_get();
	struct ipc_comp_dev *icd;
	int ret;

	icd = ipc_get_comp_by_ppl_id(ipc, COMP_TYPE_COMPONENT, pipeline_id);
	while (icd) {
		struct list_item *list, *_list;
		struct comp_buffer *buffer;

		/* free sink buffer allocated by current component in bind function */
		list_for_item_safe(list, _list, &icd->cd->bsink_list) {
			struct comp_dev *sink;

			buffer = container_of(list, struct comp_buffer, source_list);
			pipeline_disconnect(icd->cd, buffer, PPL_CONN_DIR_COMP_TO_BUFFER);
			sink = buffer->sink;

			/* free the buffer only when the sink module has also been disconnected */
			if (!sink)
				buffer_free(buffer);
		}

		/* free source buffer allocated by current component in bind function */
		list_for_item_safe(list, _list, &icd->cd->bsource_list) {
			struct comp_dev *source;

			buffer = container_of(list, struct comp_buffer, sink_list);
			pipeline_disconnect(icd->cd, buffer, PPL_CONN_DIR_BUFFER_TO_COMP);
			source = buffer->source;

			/* free the buffer only when the source module has also been disconnected */
			if (!source)
				buffer_free(buffer);
		}

		ret = ipc_comp_free(ipc, icd->id);
		if (ret)
			return IPC4_INVALID_RESOURCE_STATE;

		icd = ipc_get_comp_by_ppl_id(ipc, COMP_TYPE_COMPONENT, pipeline_id);
	}

	return IPC4_SUCCESS;
}

int ipc_pipeline_free(struct ipc *ipc, uint32_t comp_id)
{
	struct ipc_comp_dev *ipc_pipe;
	int ret;

	/* check whether pipeline exists */
	ipc_pipe = ipc_get_pipeline_by_id(ipc, comp_id);
	if (!ipc_pipe)
		return IPC4_INVALID_RESOURCE_ID;

	/* Pass IPC to target core */
	if (!cpu_is_me(ipc_pipe->core))
		return ipc4_process_on_core(ipc_pipe->core, false);

	ret = ipc_pipeline_module_free(ipc_pipe->pipeline->pipeline_id);
	if (ret != IPC4_SUCCESS) {
		tr_err(&ipc_tr, "ipc_pipeline_free(): module free () failed");
		return ret;
	}

	/* free buffer, delete all tasks and remove from list */
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

static struct comp_buffer *ipc4_create_buffer(struct comp_dev *src, bool is_shared,
					      uint32_t buf_size, uint32_t src_queue,
					      uint32_t dst_queue)
{
	struct sof_ipc_buffer ipc_buf;

	memset(&ipc_buf, 0, sizeof(ipc_buf));
	ipc_buf.size = buf_size;
	ipc_buf.comp.id = IPC4_COMP_ID(src_queue, dst_queue);
	ipc_buf.comp.pipeline_id = src->ipc_config.pipeline_id;
	ipc_buf.comp.core = src->ipc_config.core;
	return buffer_new(&ipc_buf, is_shared);
}

int ipc_comp_connect(struct ipc *ipc, ipc_pipe_comp_connect *_connect)
{
	struct ipc4_module_bind_unbind *bu;
	struct comp_buffer *buffer;
	struct comp_dev *source;
	struct comp_dev *sink;
	struct ipc4_base_module_cfg source_src_cfg;
	struct ipc4_base_module_cfg sink_src_cfg;
	uint32_t flags;
	int src_id, sink_id;
	int ret;

	bu = (struct ipc4_module_bind_unbind *)_connect;
	src_id = IPC4_COMP_ID(bu->primary.r.module_id, bu->primary.r.instance_id);
	sink_id = IPC4_COMP_ID(bu->extension.r.dst_module_id, bu->extension.r.dst_instance_id);
	source = ipc4_get_comp_dev(src_id);
	sink = ipc4_get_comp_dev(sink_id);

	if (!source || !sink) {
		tr_err(&ipc_tr, "failed to find src %x, or dst %x", src_id, sink_id);
		return IPC4_INVALID_RESOURCE_ID;
	}

	bool is_shared = source->ipc_config.core != sink->ipc_config.core;

	/* Pass IPC to target core if the buffer won't be shared and will be used
	 * on different core
	 */
	if (!cpu_is_me(source->ipc_config.core) && !is_shared)
		return ipc4_process_on_core(source->ipc_config.core, false);

	ret = comp_get_attribute(source, COMP_ATTR_BASE_CONFIG, &source_src_cfg);
	if (ret < 0) {
		tr_err(&ipc_tr, "failed to get base config for module %#x", dev_comp_id(source));
		return IPC4_FAILURE;
	}

	ret = comp_get_attribute(sink, COMP_ATTR_BASE_CONFIG, &sink_src_cfg);
	if (ret < 0) {
		tr_err(&ipc_tr, "failed to get base config for module %#x", dev_comp_id(sink));
		return IPC4_FAILURE;
	}

	/* create a buffer
	 * in case of LL -> LL or LL->DP
	 *	size = 2*obs of source module (obs is single buffer size)
	 * in case of DP -> LL
	 *	size = 2*ibs of destination (LL) module. DP queue will handle obs of DP module
	 */
	uint32_t buf_size;

	if (source->ipc_config.proc_domain == COMP_PROCESSING_DOMAIN_LL)
		buf_size = source_src_cfg.obs * 2;
	else
		buf_size = sink_src_cfg.ibs * 2;

	buffer = ipc4_create_buffer(source, is_shared, buf_size, bu->extension.r.src_queue,
				    bu->extension.r.dst_queue);
	if (!buffer) {
		tr_err(&ipc_tr, "failed to allocate buffer to bind %d to %d", src_id, sink_id);
		return IPC4_OUT_OF_MEMORY;
	}

	/*
	 * set min_free_space and min_available in sink/src api of created buffer.
	 * buffer is connected like:
	 *	source_module -> (sink_ifc) BUFFER (source_ifc) -> sink_module
	 *
	 *	source_module needs to set its OBS (out buffer size)
	 *		as min_free_space in buffer's sink ifc
	 *	sink_module needs to set its IBS (input buffer size)
	 *		as min_available in buffer's source ifc
	 */
	sink_set_min_free_space(audio_stream_get_sink(&buffer->stream), source_src_cfg.obs);
	source_set_min_available(audio_stream_get_source(&buffer->stream), sink_src_cfg.ibs);

	/*
	 * Connect and bind the buffer to both source and sink components with the interrupts
	 * disabled to prevent the IPC task getting preempted which could result in buffers being
	 * only half connected when a pipeline task gets executed. A spinlock isn't required
	 * because all connected pipelines need to be on the same core.
	 */
	irq_local_disable(flags);

	ret = comp_buffer_connect(source, source->ipc_config.core, buffer,
				  PPL_CONN_DIR_COMP_TO_BUFFER);
	if (ret < 0) {
		tr_err(&ipc_tr, "failed to connect src %d to internal buffer", src_id);
		goto free;
	}


	ret = comp_buffer_connect(sink, sink->ipc_config.core, buffer,
				  PPL_CONN_DIR_BUFFER_TO_COMP);
	if (ret < 0) {
		tr_err(&ipc_tr, "failed to connect internal buffer to sink %d", sink_id);
		goto e_sink_connect;
	}


	ret = comp_bind(source, bu);
	if (ret < 0)
		goto e_src_bind;

	ret = comp_bind(sink, bu);
	if (ret < 0)
		goto e_sink_bind;

	/* update direction for sink component if it is not set already */
	if (!sink->direction_set && source->direction_set) {
		sink->direction = source->direction;
		sink->direction_set = true;
	}

	/* update direction for source component if it is not set already */
	if (!source->direction_set && sink->direction_set) {
		source->direction = sink->direction;
		source->direction_set = true;
	}

	irq_local_enable(flags);

	return IPC4_SUCCESS;

e_sink_bind:
	comp_unbind(source, bu);
e_src_bind:
	pipeline_disconnect(sink, buffer, PPL_CONN_DIR_BUFFER_TO_COMP);
e_sink_connect:
	pipeline_disconnect(source, buffer, PPL_CONN_DIR_COMP_TO_BUFFER);
free:
	irq_local_enable(flags);
	buffer_free(buffer);
	return IPC4_INVALID_RESOURCE_STATE;
}

/* when both module instances are part of the same pipeline Unbind IPC would
 * be ignored since FW does not support changing internal topology of pipeline
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
	int ret, ret1;

	bu = (struct ipc4_module_bind_unbind *)_connect;
	src_id = IPC4_COMP_ID(bu->primary.r.module_id, bu->primary.r.instance_id);
	sink_id = IPC4_COMP_ID(bu->extension.r.dst_module_id, bu->extension.r.dst_instance_id);
	src = ipc4_get_comp_dev(src_id);
	sink = ipc4_get_comp_dev(sink_id);
	if (!src || !sink) {
		tr_err(&ipc_tr, "failed to find src %x, or dst %x", src_id, sink_id);
		return IPC4_INVALID_RESOURCE_ID;
	}

	if (src->pipeline == sink->pipeline) {
		tr_warn(&ipc_tr, "ignoring unbinding of src %x and dst %x", src_id, sink_id);
		return 0;
	}

	/* Pass IPC to target core if both modules has the same target core */
	if (!cpu_is_me(src->ipc_config.core) && src->ipc_config.core == sink->ipc_config.core)
		return ipc4_process_on_core(src->ipc_config.core, false);

	buffer_id = IPC4_COMP_ID(bu->extension.r.src_queue, bu->extension.r.dst_queue);
	list_for_item(sink_list, &src->bsink_list) {
		struct comp_buffer *buf = container_of(sink_list, struct comp_buffer, source_list);
		bool found = buf->id == buffer_id;

		if (found) {
			buffer = buf;
			break;
		}
	}

	if (!buffer)
		return IPC4_INVALID_RESOURCE_ID;

	/*
	 * Disconnect and unbind buffer from source/sink components and continue to free the buffer
	 * even in case of errors. Disable interrupts during disconnect and unbinding to prevent
	 * the IPC task getting preempted which could result in buffers being only half connected
	 * when a pipeline task gets executed. A spinlock isn't required because all connected
	 * pipelines need to be on the same core.
	 */
	irq_local_disable(flags);
	pipeline_disconnect(src, buffer, PPL_CONN_DIR_COMP_TO_BUFFER);
	pipeline_disconnect(sink, buffer, PPL_CONN_DIR_BUFFER_TO_COMP);
	ret = comp_unbind(src, bu);
	ret1 = comp_unbind(sink, bu);
	irq_local_enable(flags);

	buffer_free(buffer);

	if (ret || ret1)
		return IPC4_INVALID_RESOURCE_ID;

	return IPC4_SUCCESS;
}

/* dma index may be for playback or capture. Current hw supports PLATFORM_MAX_DMA_CHAN playback
 * channels and the rest are for capture. This function converts DMA ID to DMA channel.
 */
static inline int process_dma_index(uint32_t dma_id, uint32_t *dir, uint32_t *chan)
{
	if (dma_id > DAI_NUM_HDA_OUT + DAI_NUM_HDA_IN) {
		tr_err(&ipc_tr, "dma id %d is out of range", dma_id);
		return IPC4_INVALID_NODE_ID;
	}

	if (dma_id >= PLATFORM_MAX_DMA_CHAN) {
		*dir = SOF_IPC_STREAM_CAPTURE;
		*chan = dma_id - PLATFORM_MAX_DMA_CHAN;
	} else {
		*dir = SOF_IPC_STREAM_PLAYBACK;
		*chan = dma_id;
	}

	return IPC4_SUCCESS;
}

#if CONFIG_COMP_CHAIN_DMA
int ipc4_chain_manager_create(struct ipc4_chain_dma *cdma)
{
	const struct sof_uuid uuid = {0x6a0a274f, 0x27cc, 0x4afb, {0xa3, 0xe7, 0x34,
			0x44, 0x72, 0x3f, 0x43, 0x2e}};
	const struct comp_driver *drv;
	struct comp_dev *dev;

	drv = ipc4_get_drv((uint8_t *)&uuid);
	if (!drv)
		return -EINVAL;

	dev = drv->ops.create(drv, NULL, cdma);
	if (!dev)
		return -EINVAL;

	/* differentiate instance by unique ids assignment */
	const uint32_t comp_id = IPC4_COMP_ID(cdma->primary.r.host_dma_id
					      + IPC4_MAX_MODULE_COUNT, 0);
	dev->ipc_config.id = comp_id;
	dev->ipc_config.pipeline_id = cdma->primary.r.host_dma_id
				      + IPC4_MAX_MODULE_COUNT;

	return ipc4_add_comp_dev(dev);
}

int ipc4_chain_dma_state(struct comp_dev *dev, struct ipc4_chain_dma *cdma)
{
	const bool allocate = cdma->primary.r.allocate;
	const bool enable = cdma->primary.r.enable;
	int ret;

	if (!dev)
		return -EINVAL;

	if (allocate) {
		if (enable)
			ret = comp_trigger(dev, COMP_TRIGGER_START);
		else
			ret = comp_trigger(dev, COMP_TRIGGER_PAUSE);
	} else {
		if (enable)
			return -EINVAL;

		/* remove chain part */
		ret = comp_trigger(dev, COMP_TRIGGER_PAUSE);
		if (ret < 0)
			return ret;
		comp_free(dev);
	}
	return ret;
}
#endif

static int ipc4_update_comps_direction(struct ipc *ipc, uint32_t ppl_id)
{
	struct ipc_comp_dev *icd;
	struct list_item *clist;
	struct comp_buffer *src_buf;

	list_for_item(clist, &ipc->comp_list) {
		icd = container_of(clist, struct ipc_comp_dev, list);
		if (icd->type != COMP_TYPE_COMPONENT)
			continue;

		if (dev_comp_pipe_id(icd->cd) != ppl_id)
			continue;

		if (icd->cd->direction_set)
			continue;

		src_buf = list_first_item(&icd->cd->bsource_list, struct comp_buffer, sink_list);
		if (src_buf && src_buf->source->direction_set) {
			icd->cd->direction = src_buf->source->direction;
			icd->cd->direction_set = true;
			continue;
		}

		return -EINVAL;
	}
	return 0;
}

int ipc4_pipeline_complete(struct ipc *ipc, uint32_t comp_id, uint32_t cmd)
{
	struct ipc_comp_dev *ipc_pipe;
	int ret;

	ipc_pipe = ipc_get_pipeline_by_id(ipc, comp_id);
	if (!ipc_pipe)
		return -IPC4_INVALID_RESOURCE_ID;

	/* Pass IPC to target core */
	if (!cpu_is_me(ipc_pipe->core))
		return ipc_process_on_core(ipc_pipe->core, false);

	/* Note: SOF driver cares to bind modules one by one from input to output gateway, so
	 * direction is always assigned in bind phase. We do not expect this call change anything.
	 * OED driver does not guarantee this approach, hence some module may be bound inside
	 * pipeline w/o connection to gateway, so direction is not configured in binding phase.
	 * Need to update direction for such modules when pipeline is completed.
	 */
	if (cmd != SOF_IPC4_PIPELINE_STATE_RESET) {
		ret = ipc4_update_comps_direction(ipc, comp_id);
		if (ret < 0)
			return ret;
	}

	return ipc_pipeline_complete(ipc, comp_id);
}

int ipc4_process_on_core(uint32_t core, bool blocking)
{
	int ret;

	ret = ipc_process_on_core(core, blocking);
	switch (ret) {
	case 0:
	case 1:
		return IPC4_SUCCESS;
	case -EACCES:
		return IPC4_INVALID_CORE_ID;
	case -ETIME:
	case -EBUSY:
		return IPC4_BUSY;
	default:
		return IPC4_FAILURE;
	}

	return IPC4_SUCCESS;
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

#if CONFIG_LIBRARY
struct ipc4_module_uuid {
	int module_id;
	struct sof_uuid uuid;
};

/* Hardcoded table mapping UUIDs with module ID's. TODO: replace this with a scalable solution */
static const struct ipc4_module_uuid uuid_map[] = {
	{0x6, {.a = 0x61bca9a8,	.b = 0x18d0, .c = 0x4a18,
	       .d = { 0x8e, 0x7b, 0x26, 0x39, 0x21, 0x98, 0x04, 0xb7 }}}, /* gain */
	{0x2, {.a = 0x39656eb2, .b = 0x3b71, .c = 0x4049,
	       .d = { 0x8d, 0x3f, 0xf9, 0x2c, 0xd5, 0xc4, 0x3c, 0x09 }}}, /* mixin */
	{0x3, {.a = 0x3c56505a, .b = 0x24d7, .c = 0x418f,
		.d = { 0xbd, 0xdc, 0xc1, 0xf5, 0xa3, 0xac, 0x2a, 0xe0 }}}, /* mixout */
	{0x96, {.a = 0xe2b6031c, .b = 0x47e8, .c = 0x11ed,
		.d = { 0x07, 0xa9, 0x7f, 0x80, 0x1b, 0x6e, 0xfa, 0x6c }}}, /* host SHM write */
	{0x98, {.a = 0xdabe8814, .b = 0x47e8, .c = 0x11ed,
		.d = { 0xa5, 0x8b, 0xb3, 0x09, 0x97, 0x4f, 0xec, 0xce }}}, /* host SHM read */
	{0x97, {.a = 0x72cee996, .b = 0x39f2, .c = 0x11ed,
		.d = { 0xa0, 0x8f, 0x97, 0xfc, 0xc4, 0x2e, 0xaa, 0xeb }}}, /* ALSA aplay */
	{0x99, {.a = 0x66def9f0, .b = 0x39f2, .c = 0x11ed,
		.d = { 0xf7, 0x89, 0xaf, 0x98, 0xa6, 0x44, 0x0c, 0xc4 }}}, /* ALSA arecord */
};

static const struct comp_driver *ipc4_library_get_drv(int module_id)
{
	const struct ipc4_module_uuid *mod_uuid;
	int i;

	for (i = 0; i < ARRAY_SIZE(uuid_map); i++) {
		mod_uuid = &uuid_map[i];

		if (mod_uuid->module_id == module_id)
			return ipc4_get_drv((uint8_t *)&mod_uuid->uuid);
	}

	tr_err(&comp_tr, "ipc4_library_get_drv(): Unsupported module ID %#x\n", module_id);
	return NULL;
}
#endif

const struct comp_driver *ipc4_get_comp_drv(int module_id)
{
	struct sof_man_fw_desc *desc = NULL;
	const struct comp_driver *drv;
	struct sof_man_module *mod;
	int entry_index;

#if CONFIG_LIBRARY
	return ipc4_library_get_drv(module_id);
#endif

#ifdef RIMAGE_MANIFEST
	desc = (struct sof_man_fw_desc *)IMR_BOOT_LDR_MANIFEST_BASE;
#else
	/* Non-rimage platforms have no component facility yet.  This
	 * needs to move to the platform layer.
	 */
	return NULL;
#endif

	uint32_t lib_idx = LIB_MANAGER_GET_LIB_ID(module_id);

	if (lib_idx == 0) {
		/* module_id 0 is used for base fw which is in entry 1 */
		if (!module_id)
			entry_index = 1;
		else
			entry_index = module_id;
	} else {
		/* Library index greater than 0 possible only when LIBRARY_MANAGER
		 * support enabled.
		 */
#if CONFIG_LIBRARY_MANAGER
		desc = lib_manager_get_library_module_desc(module_id);
		entry_index = LIB_MANAGER_GET_MODULE_INDEX(module_id);
#else
		tr_err(&comp_tr, "Error: lib index:%d, while loadable libraries are not supported!!!",
		       lib_idx);
		return NULL;
#endif
	}
	/* Check already registered components */
	mod = (struct sof_man_module *)((char *)desc + SOF_MAN_MODULE_OFFSET(entry_index));
	drv = ipc4_get_drv(mod->uuid);

#if CONFIG_LIBRARY_MANAGER
	if (!drv) {
		/* New module not registered yet. */
		lib_manager_register_module(desc, module_id);
		drv = ipc4_get_drv(mod->uuid);
	}
#endif

	return drv;
}

struct comp_dev *ipc4_get_comp_dev(uint32_t comp_id)
{
	struct ipc_comp_dev *icd = ipc_get_comp_by_id(ipc_get(), comp_id);

	return icd ? icd->cd : NULL;
}

int ipc4_add_comp_dev(struct comp_dev *dev)
{
	struct ipc *ipc = ipc_get();
	struct ipc_comp_dev *icd;

	/* check id for duplicates */
	icd = ipc_get_comp_by_id(ipc, dev->ipc_config.id);
	if (icd) {
		tr_err(&ipc_tr, "ipc: duplicate component ID");
		return IPC4_INVALID_RESOURCE_ID;
	}

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

	return IPC4_SUCCESS;
};

int ipc4_find_dma_config(struct ipc_config_dai *dai, uint8_t *data_buffer, uint32_t size)
{
#if defined(CONFIG_ACE_VERSION_2_0)
	uint32_t *dma_config_id = GET_IPC_DMA_CONFIG_ID(data_buffer, size);

	if (*dma_config_id != GTW_DMA_CONFIG_ID)
		return IPC4_INVALID_REQUEST;

	dai->host_dma_config = GET_IPC_DMA_CONFIG(data_buffer, size);
#endif
	return IPC4_SUCCESS;
}

void ipc4_base_module_cfg_to_stream_params(const struct ipc4_base_module_cfg *base_cfg,
					   struct sof_ipc_stream_params *params)
{
	enum sof_ipc_frame frame_fmt, valid_fmt;
	int i;

	memset(params, 0, sizeof(struct sof_ipc_stream_params));
	params->channels = base_cfg->audio_fmt.channels_count;
	params->rate = base_cfg->audio_fmt.sampling_frequency;
	params->sample_container_bytes = base_cfg->audio_fmt.depth / 8;
	params->sample_valid_bytes = base_cfg->audio_fmt.valid_bit_depth / 8;
	params->buffer_fmt = base_cfg->audio_fmt.interleaving_style;
	params->buffer.size = base_cfg->obs * 2;

	audio_stream_fmt_conversion(base_cfg->audio_fmt.depth,
				    base_cfg->audio_fmt.valid_bit_depth,
				    &frame_fmt, &valid_fmt,
				    base_cfg->audio_fmt.s_type);
	params->frame_fmt = valid_fmt;

	for (i = 0; i < SOF_IPC_MAX_CHANNELS; i++)
		params->chmap[i] = (base_cfg->audio_fmt.ch_map >> i * 4) & 0xf;
}

void ipc4_update_buffer_format(struct comp_buffer *buf_c,
			       const struct ipc4_audio_format *fmt)
{
	enum sof_ipc_frame valid_fmt, frame_fmt;
	int i;

	audio_stream_set_channels(&buf_c->stream, fmt->channels_count);
	audio_stream_set_rate(&buf_c->stream, fmt->sampling_frequency);
	audio_stream_fmt_conversion(fmt->depth,
				    fmt->valid_bit_depth,
				    &frame_fmt, &valid_fmt,
				    fmt->s_type);

	audio_stream_set_frm_fmt(&buf_c->stream, frame_fmt);
	audio_stream_set_valid_fmt(&buf_c->stream, valid_fmt);
	audio_stream_set_buffer_fmt(&buf_c->stream, fmt->interleaving_style);

	for (i = 0; i < SOF_IPC_MAX_CHANNELS; i++)
		buf_c->chmap[i] = (fmt->ch_map >> i * 4) & 0xf;

	buf_c->hw_params_configured = true;
}
