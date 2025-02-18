// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
// Author: Rander Wang <rander.wang@linux.intel.com>

#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/audio/audio_buffer.h>
#include <sof/audio/ring_buffer.h>
#include <sof/audio/component_ext.h>
#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/pipeline.h>
#include <module/module/base.h>
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
#include <rtos/symbol.h>
#include <rtos/wait.h>

/* TODO: Remove platform-specific code, see https://github.com/thesofproject/sof/issues/7549 */
#if defined(CONFIG_SOC_SERIES_INTEL_ADSP_ACE) || defined(CONFIG_INTEL_ADSP_CAVS)
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
#include <ipc/header.h>
#include <ipc4/notification.h>
#include <ipc4/pipeline.h>
#include <ipc4/module.h>
#include <ipc4/error_status.h>
#include <sof/lib_manager.h>
#include <sof/tlv.h>

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <sof/debug/telemetry/telemetry.h>
#include <sof/debug/telemetry/performance_monitor.h>

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

static const struct comp_driver *ipc4_library_get_comp_drv(char *data)
{
	return ipc4_get_drv(data);
}
#else
static inline char *ipc4_get_comp_new_data(void)
{
	return (char *)MAILBOX_HOSTBOX_BASE;
}
#endif

__cold struct comp_dev *comp_new_ipc4(struct ipc4_module_init_instance *module_init)
{
	struct comp_ipc_config ipc_config;
	const struct comp_driver *drv;
	struct comp_dev *dev;
	uint32_t comp_id;
	char *data;

	comp_id = IPC4_COMP_ID(module_init->primary.r.module_id,
			       module_init->primary.r.instance_id);

	if (ipc4_get_comp_dev(comp_id)) {
		tr_err(&ipc_tr, "comp 0x%x exists", comp_id);
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

	dcache_invalidate_region((__sparse_force void __sparse_cache *)MAILBOX_HOSTBOX_BASE,
				 MAILBOX_HOSTBOX_SIZE);

	data = ipc4_get_comp_new_data();

#if CONFIG_LIBRARY
	ipc_config.ipc_config_size -= sizeof(struct sof_uuid);
	drv = ipc4_library_get_comp_drv(data + ipc_config.ipc_config_size);
#else
	drv = ipc4_get_comp_drv(IPC4_MOD_ID(comp_id));
#endif
	if (!drv)
		return NULL;

#if CONFIG_ZEPHYR_DP_SCHEDULER
	if (module_init->extension.r.proc_domain)
		ipc_config.proc_domain = COMP_PROCESSING_DOMAIN_DP;
	else
		ipc_config.proc_domain = COMP_PROCESSING_DOMAIN_LL;
#else /* CONFIG_ZEPHYR_DP_SCHEDULER */
	if (module_init->extension.r.proc_domain) {
		tr_err(&ipc_tr, "ipc: DP scheduling is disabled, cannot create comp 0x%x", comp_id);
		return NULL;
	}
	ipc_config.proc_domain = COMP_PROCESSING_DOMAIN_LL;
#endif /* CONFIG_ZEPHYR_DP_SCHEDULER */

	if (drv->type == SOF_COMP_MODULE_ADAPTER) {
		const struct ipc_config_process spec = {
			.data = (const unsigned char *)data,
			.size = ipc_config.ipc_config_size,
		};

		dev = drv->ops.create(drv, &ipc_config, (const void *)&spec);
	} else {
		dev = drv->ops.create(drv, &ipc_config, (const void *)data);
	}
	if (!dev)
		return NULL;

	list_init(&dev->bsource_list);
	list_init(&dev->bsink_list);

#ifdef CONFIG_SOF_TELEMETRY_PERFORMANCE_MEASUREMENTS
	/* init global performance measurement */
	dev->perf_data.perf_data_item = perf_data_getnext();
	/* this can be null, just no performance measurements in this case */
	if (dev->perf_data.perf_data_item) {
		dev->perf_data.perf_data_item->item.resource_id = comp_id;
		if (perf_meas_get_state() != IPC4_PERF_MEASUREMENTS_DISABLED)
			comp_init_performance_data(dev);
	}
#endif

	ipc4_add_comp_dev(dev);

	comp_update_ibs_obs_cpc(dev);

	return dev;
}

struct ipc_comp_dev *ipc_get_comp_by_ppl_id(struct ipc *ipc, uint16_t type,
					    uint32_t ppl_id,
					    uint32_t ignore_remote)
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
			if ((!cpu_is_me(icd->core)) && ignore_remote)
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

static inline int ipc_comp_free_remote(struct comp_dev *dev)
{
	struct idc_msg msg = { IDC_MSG_FREE, IDC_MSG_FREE_EXT(dev->ipc_config.id),
		dev->ipc_config.core,};

	return idc_send_msg(&msg, IDC_BLOCKING);
}

static int ipc_pipeline_module_free(uint32_t pipeline_id)
{
	struct ipc *ipc = ipc_get();
	struct ipc_comp_dev *icd;
	int ret;

	icd = ipc_get_comp_by_ppl_id(ipc, COMP_TYPE_COMPONENT, pipeline_id, IPC_COMP_ALL);
	while (icd) {
		struct comp_buffer *buffer;
		struct comp_buffer *safe;

		/* free sink buffer allocated by current component in bind function */
		comp_dev_for_each_consumer_safe(icd->cd, buffer, safe) {
			struct comp_dev *sink = comp_buffer_get_sink_component(buffer);

			/* free the buffer only when the sink module has also been disconnected */
			if (!sink)
				buffer_free(buffer);
		}

		/* free source buffer allocated by current component in bind function */
		comp_dev_for_each_producer_safe(icd->cd, buffer, safe) {
			pipeline_disconnect(icd->cd, buffer, PPL_CONN_DIR_BUFFER_TO_COMP);
			struct comp_dev *source = comp_buffer_get_source_component(buffer);

			/* free the buffer only when the source module has also been disconnected */
			if (!source)
				buffer_free(buffer);
		}

		if (!cpu_is_me(icd->core))
			ret = ipc_comp_free_remote(icd->cd);
		else
			ret = ipc_comp_free(ipc, icd->id);

		if (ret)
			return IPC4_INVALID_RESOURCE_STATE;

		icd = ipc_get_comp_by_ppl_id(ipc, COMP_TYPE_COMPONENT, pipeline_id, IPC_COMP_ALL);
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
	ipc_buf.comp.core = cpu_get_id();
	return buffer_new(&ipc_buf, is_shared);
}

#if CONFIG_CROSS_CORE_STREAM
/*
 * Disabling interrupts to block next LL cycle works much faster comparing using
 * of condition variable and mutex. Since same core binding is the most typical
 * case, let's use slower cond_var blocking mechanism only for not so typical
 * cross-core binding.
 *
 * Note, disabling interrupts to block LL for cross-core binding case will not work
 * as .bind() handlers are called on corresponding cores using IDC tasks. IDC requires
 * interrupts to be enabled. Only disabling timer interrupt instead of all interrupts
 * might work. However, as CPU could go to some power down mode while waiting for
 * blocking IDC call response, it's not clear how safe is to assume CPU can wakeup
 * without timer interrupt. It depends on blocking IDC waiting implementation. That
 * is why additional cond_var mechanism to block LL was introduced which does not
 * disable any interrupts.
 */

#define ll_block(cross_core_bind, flags) \
	do { \
		if (cross_core_bind) \
			domain_block(sof_get()->platform_timer_domain); \
		else \
			irq_local_disable(flags); \
	} while (0)

#define ll_unblock(cross_core_bind, flags) \
	do { \
		if (cross_core_bind) \
			domain_unblock(sof_get()->platform_timer_domain); \
		else \
			irq_local_enable(flags); \
	} while (0)

/* Calling both ll_block() and ll_wait_finished_on_core() makes sure LL will not start its
 * next cycle and its current cycle on specified core has finished.
 */
static int ll_wait_finished_on_core(struct comp_dev *dev)
{
	/* To make sure (blocked) LL has finished its current cycle, it is
	 * enough to send any blocking IDC to the core. Since IDC task has lower
	 * priority then LL thread and cannot preempt it, execution of IDC task
	 * happens when LL thread is not active waiting for its next cycle.
	 */

	int ret;
	struct ipc4_base_module_cfg dummy;

	if (cpu_is_me(dev->ipc_config.core))
		return 0;

	/* Any blocking IDC that does not change component state could be utilized */
	ret = comp_ipc4_get_attribute_remote(dev, COMP_ATTR_BASE_CONFIG, &dummy);
	if (ret < 0) {
		tr_err(&ipc_tr, "comp_ipc4_get_attribute_remote() failed for module %#x",
		       dev_comp_id(dev));
		return ret;
	}

	return 0;
}

#else

#define ll_block(cross_core_bind, flags)	irq_local_disable(flags)
#define ll_unblock(cross_core_bind, flags)	irq_local_enable(flags)

#endif

int ipc_comp_connect(struct ipc *ipc, ipc_pipe_comp_connect *_connect)
{
	struct ipc4_module_bind_unbind *bu;
	struct comp_buffer *buffer;
	struct comp_dev *source;
	struct comp_dev *sink;
	struct ipc4_base_module_cfg source_src_cfg;
	struct ipc4_base_module_cfg sink_src_cfg;
	uint32_t flags = 0;
	uint32_t ibs = 0;
	uint32_t obs = 0;
	uint32_t buf_size;
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

#if CONFIG_ZEPHYR_DP_SCHEDULER
	if (source->ipc_config.proc_domain == COMP_PROCESSING_DOMAIN_DP &&
	    sink->ipc_config.proc_domain == COMP_PROCESSING_DOMAIN_DP) {
		tr_err(&ipc_tr, "DP to DP binding is not supported: can't bind %x to %x",
		       src_id, sink_id);
		return IPC4_INVALID_REQUEST;
	}
#endif /* CONFIG_ZEPHYR_DP_SCHEDULER */
	bool cross_core_bind = source->ipc_config.core != sink->ipc_config.core;

	/* If both components are on same core -- process IPC on that core,
	 * otherwise stay on core 0
	 */
	if (!cpu_is_me(source->ipc_config.core) && !cross_core_bind)
		return ipc4_process_on_core(source->ipc_config.core, false);

	if (source->drv->type == SOF_COMP_MODULE_ADAPTER) {
		struct processing_module *srcmod = comp_mod(source);
		struct module_config *srccfg = &srcmod->priv.cfg;

		/* get obs from the base config extension if the src queue ID is non-zero */
		if (bu->extension.r.src_queue && bu->extension.r.src_queue < srccfg->nb_output_pins)
			obs = srccfg->output_pins[bu->extension.r.src_queue].obs;
	}

	/* get obs from base config if src queue ID is 0 or if base config extn is missing */
	if (!obs) {
		/* these might call comp_ipc4_get_attribute_remote() if necessary */
		ret = comp_get_attribute(source, COMP_ATTR_BASE_CONFIG, &source_src_cfg);

		if (ret < 0) {
			tr_err(&ipc_tr, "failed to get base config for src module %#x",
			       dev_comp_id(source));
			return IPC4_FAILURE;
		}
		obs = source_src_cfg.obs;
	}

	if (sink->drv->type == SOF_COMP_MODULE_ADAPTER) {
		struct processing_module *dstmod = comp_mod(sink);
		struct module_config *dstcfg = &dstmod->priv.cfg;

		/* get ibs from the base config extension if the sink queue ID is non-zero */
		if (bu->extension.r.dst_queue && bu->extension.r.dst_queue < dstcfg->nb_input_pins)
			ibs = dstcfg->input_pins[bu->extension.r.dst_queue].ibs;
	}
	/* get ibs from base config if sink queue ID is 0 or if base config extn is missing */
	if (!ibs) {
		ret = comp_get_attribute(sink, COMP_ATTR_BASE_CONFIG, &sink_src_cfg);
		if (ret < 0) {
			tr_err(&ipc_tr, "failed to get base config for sink module %#x",
			       dev_comp_id(sink));
			return IPC4_FAILURE;
		}

		ibs = sink_src_cfg.ibs;
	}

	/* create a buffer
	 * in case of LL -> LL or LL->DP
	 *	size = 2*obs of source module (obs is single buffer size)
	 * in case of DP -> LL
	 *	size = 2*ibs of destination (LL) module. DP queue will handle obs of DP module
	 */
	if (source->ipc_config.proc_domain == COMP_PROCESSING_DOMAIN_LL)
		buf_size = obs * 2;
	else
		buf_size = ibs * 2;

	buffer = ipc4_create_buffer(source, cross_core_bind, buf_size, bu->extension.r.src_queue,
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
	sink_set_min_free_space(audio_buffer_get_sink(&buffer->audio_buffer), obs);
	source_set_min_available(audio_buffer_get_source(&buffer->audio_buffer), ibs);

#if CONFIG_ZEPHYR_DP_SCHEDULER
	struct ring_buffer *ring_buffer = NULL;

	if (sink->ipc_config.proc_domain == COMP_PROCESSING_DOMAIN_DP ||
	    source->ipc_config.proc_domain == COMP_PROCESSING_DOMAIN_DP) {
		struct sof_source *source = audio_buffer_get_source(&buffer->audio_buffer);
		struct sof_sink *sink = audio_buffer_get_sink(&buffer->audio_buffer);

		ring_buffer = ring_buffer_create(source_get_min_available(source),
						 sink_get_min_free_space(sink),
						 audio_buffer_is_shared(&buffer->audio_buffer),
						 buf_get_id(buffer));
		if (!ring_buffer)
			goto free;
	}

	if (sink->ipc_config.proc_domain == COMP_PROCESSING_DOMAIN_DP)
		/* data destination module needs to use ring_buffer */
		audio_buffer_attach_secondary_buffer(&buffer->audio_buffer, false, /* at_input */
						     &ring_buffer->audio_buffer);
	else if (source->ipc_config.proc_domain == COMP_PROCESSING_DOMAIN_DP)
		/* data source module needs to use ring_buffer */
		audio_buffer_attach_secondary_buffer(&buffer->audio_buffer, true, /* at_input */
						     &ring_buffer->audio_buffer);
#endif /* CONFIG_ZEPHYR_DP_SCHEDULER */
	/*
	 * Connect and bind the buffer to both source and sink components with LL processing been
	 * blocked on corresponding core(s) to prevent IPC or IDC task getting preempted which
	 * could result in buffers being only half connected when a pipeline task gets executed.
	 */
	ll_block(cross_core_bind, flags);

	if (cross_core_bind) {
#if CONFIG_CROSS_CORE_STREAM
		/* Make sure LL has finished on both cores */
		if (!cpu_is_me(source->ipc_config.core))
			if (ll_wait_finished_on_core(source) < 0)
				goto free;
		if (!cpu_is_me(sink->ipc_config.core))
			if (ll_wait_finished_on_core(sink) < 0)
				goto free;
#else
		tr_err(&ipc_tr, "Cross-core binding is disabled");
		goto free;
#endif
	}

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

	/* these might call comp_ipc4_bind_remote() if necessary */
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

	ll_unblock(cross_core_bind, flags);

	return IPC4_SUCCESS;

e_sink_bind:
	comp_unbind(source, bu);
e_src_bind:
	pipeline_disconnect(sink, buffer, PPL_CONN_DIR_BUFFER_TO_COMP);
e_sink_connect:
	pipeline_disconnect(source, buffer, PPL_CONN_DIR_COMP_TO_BUFFER);
free:
	ll_unblock(cross_core_bind, flags);
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
	struct comp_buffer *buf;
	struct comp_dev *src, *sink;
	uint32_t src_id, sink_id, buffer_id;
	uint32_t flags = 0;
	int ret, ret1;
	bool cross_core_unbind;

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

	cross_core_unbind = src->ipc_config.core != sink->ipc_config.core;

	/* Pass IPC to target core if both modules has the same target core,
	 * otherwise stay on core 0
	 */
	if (!cpu_is_me(src->ipc_config.core) && !cross_core_unbind)
		return ipc4_process_on_core(src->ipc_config.core, false);

	buffer_id = IPC4_COMP_ID(bu->extension.r.src_queue, bu->extension.r.dst_queue);
	comp_dev_for_each_consumer(src, buf) {
		bool found = buf_get_id(buf) == buffer_id;
		if (found) {
			buffer = buf;
			break;
		}
	}

	if (!buffer)
		return IPC4_INVALID_RESOURCE_ID;

	/*
	 * Disconnect and unbind buffer from source/sink components and continue to free the buffer
	 * even in case of errors. Block LL processing during disconnect and unbinding to prevent
	 * IPC or IDC task getting preempted which could result in buffers being only half connected
	 * when a pipeline task gets executed.
	 */
	ll_block(cross_core_unbind, flags);

	if (cross_core_unbind) {
#if CONFIG_CROSS_CORE_STREAM
		/* Make sure LL has finished on both cores */
		if (!cpu_is_me(src->ipc_config.core))
			if (ll_wait_finished_on_core(src) < 0) {
				ll_unblock(cross_core_unbind, flags);
				return IPC4_FAILURE;
			}
		if (!cpu_is_me(sink->ipc_config.core))
			if (ll_wait_finished_on_core(sink) < 0) {
				ll_unblock(cross_core_unbind, flags);
				return IPC4_FAILURE;
			}
#else
		tr_err(&ipc_tr, "Cross-core binding is disabled");
		ll_unblock(cross_core_unbind, flags);
		return IPC4_FAILURE;
#endif
	}

	pipeline_disconnect(src, buffer, PPL_CONN_DIR_COMP_TO_BUFFER);
	pipeline_disconnect(sink, buffer, PPL_CONN_DIR_BUFFER_TO_COMP);
	/* these might call comp_ipc4_bind_remote() if necessary */
	ret = comp_unbind(src, bu);
	ret1 = comp_unbind(sink, bu);

	ll_unblock(cross_core_unbind, flags);

	buffer_free(buffer);

	if (ret || ret1)
		return IPC4_INVALID_RESOURCE_ID;

	return IPC4_SUCCESS;
}

#if CONFIG_COMP_CHAIN_DMA
int ipc4_chain_manager_create(struct ipc4_chain_dma *cdma)
{
	const struct sof_uuid uuid = {0x6a0a274f, 0x27cc, 0x4afb, {0xa3, 0xe7, 0x34,
			0x44, 0x72, 0x3f, 0x43, 0x2e}};
	const struct comp_driver *drv;
	struct comp_dev *dev;

	drv = ipc4_get_drv(&uuid);
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
	struct ipc *ipc = ipc_get();
	struct ipc_comp_dev *icd;
	struct list_item *clist, *_tmp;
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

		list_for_item_safe(clist, _tmp, &ipc->comp_list) {
			icd = container_of(clist, struct ipc_comp_dev, list);
			if (icd->cd != dev)
				continue;
			list_item_del(&icd->list);
			rfree(icd);
			break;
		}
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

		if (list_is_empty(&icd->cd->bsource_list))
			continue;

		src_buf = comp_dev_get_first_data_producer(icd->cd);
		if (comp_buffer_get_source_component(src_buf)->direction_set) {
			icd->cd->direction = comp_buffer_get_source_component(src_buf)->direction;
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

const struct comp_driver *ipc4_get_drv(const void *uuid)
{
	const struct sof_uuid *const sof_uuid = (const struct sof_uuid *)uuid;
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

	tr_warn(&comp_tr,
		"get_drv(): the provided UUID (%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x) can't be found!",
		sof_uuid->a, sof_uuid->b, sof_uuid->c, sof_uuid->d[0], sof_uuid->d[1],
		sof_uuid->d[2], sof_uuid->d[3], sof_uuid->d[4], sof_uuid->d[5], sof_uuid->d[6],
		sof_uuid->d[7]);

out:
	irq_local_enable(flags);
	return drv;
}

const struct comp_driver *ipc4_get_comp_drv(uint32_t module_id)
{
	const struct sof_man_fw_desc *desc = NULL;
	const struct comp_driver *drv;
	const struct sof_man_module *mod;
	uint32_t entry_index;

#ifdef RIMAGE_MANIFEST
	desc = (const struct sof_man_fw_desc *)IMR_BOOT_LDR_MANIFEST_BASE;
#else
	/* Non-rimage platforms have no component facility yet.
	 * This needs to move to the platform layer.
	 */
	return NULL;
#endif

	uint32_t lib_idx = LIB_MANAGER_GET_LIB_ID(module_id);

	if (lib_idx == 0) {
		/* module_id 0 is used for base fw which is in entry 1 or 2 */
		if (!module_id)
			entry_index = 1 + IS_ENABLED(CONFIG_COLD_STORE_EXECUTE_DRAM);
		else
			entry_index = module_id;

		if (entry_index >= desc->header.num_module_entries) {
			tr_err(&comp_tr, "Error: entry index %d out of bounds.", entry_index);
			return NULL;
		}

		mod = (const struct sof_man_module *)((const char *)desc +
						      SOF_MAN_MODULE_OFFSET(entry_index));
	} else {
		/* Library index greater than 0 possible only when LIBRARY_MANAGER
		 * support enabled.
		 */
#if CONFIG_LIBRARY_MANAGER
		mod = lib_manager_get_module_manifest(module_id);

		if (!mod) {
			tr_err(&comp_tr, "Error: Couldn't find loadable module with id %d.",
			       module_id);
			return NULL;
		}
#else
		tr_err(&comp_tr, "Error: lib index:%d, while loadable libraries are not supported!!!",
		       lib_idx);
		return NULL;
#endif
	}
	/* Check already registered components */
	drv = ipc4_get_drv(&mod->uuid);

#if CONFIG_LIBRARY_MANAGER
	if (!drv) {
		/* New module not registered yet. */
		lib_manager_register_module(module_id);
		drv = ipc4_get_drv(&mod->uuid);
	}
#endif

	return drv;
}

struct comp_dev *ipc4_get_comp_dev(uint32_t comp_id)
{
	struct ipc_comp_dev *icd = ipc_get_comp_by_id(ipc_get(), comp_id);

	return icd ? icd->cd : NULL;
}
EXPORT_SYMBOL(ipc4_get_comp_dev);

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

	tr_dbg(&ipc_tr, "ipc4_add_comp_dev add comp 0x%x", icd->id);
	/* add new component to the list */
	list_item_append(&icd->list, &ipc->comp_list);

	return IPC4_SUCCESS;
};

int ipc4_find_dma_config(struct ipc_config_dai *dai, uint8_t *data_buffer, uint32_t size)
{
#if ACE_VERSION > ACE_VERSION_1_5
	uint32_t *dma_config_id = GET_IPC_DMA_CONFIG_ID(data_buffer, size);

	if (*dma_config_id != GTW_DMA_CONFIG_ID)
		return IPC4_INVALID_REQUEST;

	dai->host_dma_config[0] = GET_IPC_DMA_CONFIG(data_buffer, size);
#endif
	return IPC4_SUCCESS;
}

int ipc4_find_dma_config_multiple(struct ipc_config_dai *dai, uint8_t *data_buffer,
				  uint32_t size, uint32_t device_id, int dma_cfg_idx)
{
	uint32_t end_addr = (uint32_t)data_buffer + size;
	struct ipc_dma_config *dma_cfg;
	struct sof_tlv *tlvs;

	for (tlvs = (struct sof_tlv *)data_buffer; (uint32_t)tlvs < end_addr;
	     tlvs = tlv_next(tlvs)) {
		dma_cfg = tlv_value_ptr_get(tlvs, GTW_DMA_CONFIG_ID);
		if (!dma_cfg)
			continue;

		/* To be able to retrieve proper DMA config we need to check if
		 * device_id value (which is alh_id) is equal to device_address.
		 * They both contain SNDW master id and PDI. If they match then
		 * proper config is found.
		 */
		for (uint32_t i = 0; i < dma_cfg->channel_map.device_count; i++) {
			if (dma_cfg->channel_map.map[i].device_address == device_id) {
				dai->host_dma_config[dma_cfg_idx] = dma_cfg;
				return IPC4_SUCCESS;
			}
		}
	}

	return IPC4_INVALID_REQUEST;
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
	params->frame_fmt = frame_fmt;

	for (i = 0; i < SOF_IPC_MAX_CHANNELS; i++)
		params->chmap[i] = (base_cfg->audio_fmt.ch_map >> i * 4) & 0xf;
}
EXPORT_SYMBOL(ipc4_base_module_cfg_to_stream_params);

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
		audio_buffer_set_chmap(&buf_c->audio_buffer, i, (fmt->ch_map >> i * 4) & 0xf);

	audio_buffer_set_hw_params_configured(&buf_c->audio_buffer);
}

void ipc4_update_source_format(struct sof_source *source,
			       const struct ipc4_audio_format *fmt)
{
	enum sof_ipc_frame valid_fmt, frame_fmt;

	source_set_channels(source, fmt->channels_count);
	source_set_rate(source, fmt->sampling_frequency);
	audio_stream_fmt_conversion(fmt->depth,
				    fmt->valid_bit_depth,
				    &frame_fmt, &valid_fmt,
				    fmt->s_type);

	source_set_frm_fmt(source, frame_fmt);
	source_set_valid_fmt(source, valid_fmt);
	source_set_buffer_fmt(source, fmt->interleaving_style);
}

void ipc4_update_sink_format(struct sof_sink *sink,
			     const struct ipc4_audio_format *fmt)
{
	enum sof_ipc_frame valid_fmt, frame_fmt;

	sink_set_channels(sink, fmt->channels_count);
	sink_set_rate(sink, fmt->sampling_frequency);
	audio_stream_fmt_conversion(fmt->depth,
				    fmt->valid_bit_depth,
				    &frame_fmt, &valid_fmt,
				    fmt->s_type);

	sink_set_frm_fmt(sink, frame_fmt);
	sink_set_valid_fmt(sink, valid_fmt);
	sink_set_buffer_fmt(sink, fmt->interleaving_style);
}
EXPORT_SYMBOL(ipc4_update_buffer_format);
