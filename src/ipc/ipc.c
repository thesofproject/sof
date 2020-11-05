// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>

#include <sof/audio/buffer.h>
#include <sof/audio/component_ext.h>
#include <sof/audio/pipeline.h>
#include <sof/common.h>
#include <sof/drivers/idc.h>
#include <sof/drivers/ipc.h>
#include <sof/lib/alloc.h>
#include <sof/lib/cache.h>
#include <sof/lib/cpu.h>
#include <sof/lib/mailbox.h>
#include <sof/list.h>
#include <sof/platform.h>
#include <sof/sof.h>
#include <sof/spinlock.h>
#include <ipc/dai.h>
#include <ipc/header.h>
#include <ipc/stream.h>
#include <ipc/topology.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* be60f97d-78df-4796-a0ee-435cb56b720a */
DECLARE_SOF_UUID("ipc", ipc_uuid, 0xbe60f97d, 0x78df, 0x4796,
		 0xa0, 0xee, 0x43, 0x5c, 0xb5, 0x6b, 0x72, 0x0a);

DECLARE_TR_CTX(ipc_tr, SOF_UUID(ipc_uuid), LOG_LEVEL_INFO);

/* Returns pipeline source component */
#define ipc_get_ppl_src_comp(ipc, ppl_id) \
	ipc_get_ppl_comp(ipc, ppl_id, PPL_DIR_UPSTREAM)

/* Returns pipeline sink component */
#define ipc_get_ppl_sink_comp(ipc, ppl_id) \
	ipc_get_ppl_comp(ipc, ppl_id, PPL_DIR_DOWNSTREAM)

int ipc_process_on_core(uint32_t core)
{
	struct idc_msg msg = { .header = IDC_MSG_IPC, .core = core, };
	int ret;

	/* check if requested core is enabled */
	if (!cpu_is_core_enabled(core)) {
		tr_err(&ipc_tr, "ipc_process_on_core(): core #%d is disabled", core);
		return -EACCES;
	}

	/* send IDC message */
	ret = idc_send_msg(&msg, IDC_BLOCKING);
	if (ret < 0)
		return ret;

	/* reply sent by other core */
	return 1;
}

/*
 * Components, buffers and pipelines all use the same set of monotonic ID
 * numbers passed in by the host. They are stored in different lists, hence
 * more than 1 list may need to be searched for the corresponding component.
 */

struct ipc_comp_dev *ipc_get_comp_by_id(struct ipc *ipc, uint32_t id)
{
	struct ipc_comp_dev *icd;
	struct list_item *clist;

	list_for_item(clist, &ipc->comp_list) {
		icd = container_of(clist, struct ipc_comp_dev, list);
		if (icd->id == id)
			return icd;

		platform_shared_commit(icd, sizeof(*icd));
	}

	return NULL;
}

struct ipc_comp_dev *ipc_get_comp_by_ppl_id(struct ipc *ipc, uint16_t type,
					    uint32_t ppl_id)
{
	struct ipc_comp_dev *icd;
	struct list_item *clist;

	list_for_item(clist, &ipc->comp_list) {
		icd = container_of(clist, struct ipc_comp_dev, list);
		if (icd->type != type) {
			platform_shared_commit(icd, sizeof(*icd));
			continue;
		}

		if (!cpu_is_me(icd->core)) {
			platform_shared_commit(icd, sizeof(*icd));
			continue;
		}

		if (ipc_comp_pipe_id(icd) == ppl_id)
			return icd;

		platform_shared_commit(icd, sizeof(*icd));
	}

	return NULL;
}

static struct ipc_comp_dev *ipc_get_ppl_comp(struct ipc *ipc,
					     uint32_t pipeline_id, int dir)
{
	struct ipc_comp_dev *icd;
	struct comp_buffer *buffer;
	struct comp_dev *buff_comp;
	struct list_item *clist;

	/* first try to find the module in the pipeline */
	list_for_item(clist, &ipc->comp_list) {
		icd = container_of(clist, struct ipc_comp_dev, list);
		if (icd->type != COMP_TYPE_COMPONENT) {
			platform_shared_commit(icd, sizeof(*icd));
			continue;
		}

		if (!cpu_is_me(icd->core)) {
			platform_shared_commit(icd, sizeof(*icd));
			continue;
		}

		if (dev_comp_pipe_id(icd->cd) == pipeline_id &&
		    list_is_empty(comp_buffer_list(icd->cd, dir)))
			return icd;

		platform_shared_commit(icd, sizeof(*icd));
	}

	/* it's connected pipeline, so find the connected module */
	list_for_item(clist, &ipc->comp_list) {
		icd = container_of(clist, struct ipc_comp_dev, list);
		if (icd->type != COMP_TYPE_COMPONENT) {
			platform_shared_commit(icd, sizeof(*icd));
			continue;
		}

		if (!cpu_is_me(icd->core)) {
			platform_shared_commit(icd, sizeof(*icd));
			continue;
		}

		if (dev_comp_pipe_id(icd->cd) == pipeline_id) {
			buffer = buffer_from_list
					(comp_buffer_list(icd->cd, dir)->next,
					 struct comp_buffer, dir);
			buff_comp = buffer_get_comp(buffer, dir);
			if (buff_comp &&
			    dev_comp_pipe_id(buff_comp) != pipeline_id)
				return icd;
		}

		platform_shared_commit(icd, sizeof(*icd));
	}

	return NULL;
}

int ipc_comp_new(struct ipc *ipc, struct sof_ipc_comp *comp)
{
	struct comp_dev *cd;
	struct ipc_comp_dev *icd;

	/* check whether component already exists */
	icd = ipc_get_comp_by_id(ipc, comp->id);
	if (icd != NULL) {
		tr_err(&ipc_tr, "ipc_comp_new(): comp->id = %u", comp->id);
		return -EINVAL;
	}

	/* create component */
	cd = comp_new(comp);
	if (!cd) {
		tr_err(&ipc_tr, "ipc_comp_new(): component cd = NULL");
		return -EINVAL;
	}

	/* allocate the IPC component container */
	icd = rzalloc(SOF_MEM_ZONE_RUNTIME_SHARED, 0, SOF_MEM_CAPS_RAM,
		      sizeof(struct ipc_comp_dev));
	if (!icd) {
		tr_err(&ipc_tr, "ipc_comp_new(): alloc failed");
		rfree(cd);
		return -ENOMEM;
	}
	icd->cd = cd;
	icd->type = COMP_TYPE_COMPONENT;
	icd->core = comp->core;
	icd->id = comp->id;

	/* add new component to the list */
	list_item_append(&icd->list, &ipc->comp_list);

	platform_shared_commit(icd, sizeof(*icd));

	return 0;
}

int ipc_comp_free(struct ipc *ipc, uint32_t comp_id)
{
	struct ipc_comp_dev *icd;

	/* check whether component exists */
	icd = ipc_get_comp_by_id(ipc, comp_id);
	if (!icd)
		return -ENODEV;

	/* check core */
	if (!cpu_is_me(icd->core))
		return ipc_process_on_core(icd->core);

	/* check state */
	if (icd->cd->state != COMP_STATE_READY)
		return -EINVAL;

	/* set pipeline sink/source/sched pointers to NULL if needed */
	if (icd->cd->pipeline) {
		if (icd->cd == icd->cd->pipeline->source_comp)
			icd->cd->pipeline->source_comp = NULL;
		if (icd->cd == icd->cd->pipeline->sink_comp)
			icd->cd->pipeline->sink_comp = NULL;
		if (icd->cd == icd->cd->pipeline->sched_comp)
			icd->cd->pipeline->sched_comp = NULL;
	}

	/* free component and remove from list */
	comp_free(icd->cd);

	icd->cd = NULL;

	list_item_del(&icd->list);
	rfree(icd);

	return 0;
}

int ipc_buffer_new(struct ipc *ipc, struct sof_ipc_buffer *desc)
{
	struct ipc_comp_dev *ibd;
	struct comp_buffer *buffer;
	int ret = 0;

	/* check whether buffer already exists */
	ibd = ipc_get_comp_by_id(ipc, desc->comp.id);
	if (ibd != NULL) {
		tr_err(&ipc_tr, "ipc_buffer_new(): buffer already exists, desc->comp.id = %u",
		       desc->comp.id);
		return -EINVAL;
	}

	/* register buffer with pipeline */
	buffer = buffer_new(desc);
	if (!buffer) {
		tr_err(&ipc_tr, "ipc_buffer_new(): buffer_new() failed");
		return -ENOMEM;
	}

	ibd = rzalloc(SOF_MEM_ZONE_RUNTIME_SHARED, 0, SOF_MEM_CAPS_RAM,
		      sizeof(struct ipc_comp_dev));
	if (!ibd) {
		buffer_free(buffer);
		return -ENOMEM;
	}
	ibd->cb = buffer;
	ibd->type = COMP_TYPE_BUFFER;
	ibd->core = desc->comp.core;
	ibd->id = desc->comp.id;

	/* add new buffer to the list */
	list_item_append(&ibd->list, &ipc->comp_list);

	platform_shared_commit(ibd, sizeof(*ibd));

	return ret;
}

int ipc_buffer_free(struct ipc *ipc, uint32_t buffer_id)
{
	struct ipc_comp_dev *ibd;
	struct ipc_comp_dev *icd;
	struct list_item *clist;
	bool sink_state_invalid = false;
	bool source_state_invalid = false;

	/* check whether buffer exists */
	ibd = ipc_get_comp_by_id(ipc, buffer_id);
	if (!ibd)
		return -ENODEV;

	/* check core */
	if (!cpu_is_me(ibd->core))
		return ipc_process_on_core(ibd->core);

	/* try to find sink/source components to check if they still exists */
	list_for_item(clist, &ipc->comp_list) {
		icd = container_of(clist, struct ipc_comp_dev, list);
		if (icd->type != COMP_TYPE_COMPONENT)
			continue;

		/* check comp state if sink and source are valid */
		if (ibd->cb->sink == icd->cd &&
		    ibd->cb->sink->state != COMP_STATE_READY)
			sink_state_invalid = true;
		if (ibd->cb->source == icd->cd &&
		    ibd->cb->source->state != COMP_STATE_READY)
			source_state_invalid = true;
	}

	/*
	 * A buffer should only be prevented from being freed when both the
	 * sink and the source widgets are in the active state. In the case
	 * of dynamic pipelines, a buffer belonging to a pipeline will need to
	 * be freed when that pipeline is no longer active but the other end
	 * of the buffer might still be active.
	 */
	if (sink_state_invalid && source_state_invalid)
		return -EINVAL;

	/* free buffer and remove from list */
	buffer_free(ibd->cb);
	list_item_del(&ibd->list);
	rfree(ibd);

	return 0;
}

static int ipc_comp_to_buffer_connect(struct ipc_comp_dev *comp,
				      struct ipc_comp_dev *buffer)
{
	int ret;

	if (!cpu_is_me(comp->core))
		return ipc_process_on_core(comp->core);

	tr_dbg(&ipc_tr, "ipc: comp sink %d, source %d  -> connect", buffer->id,
	       comp->id);

	/* check if it's a connection between cores */
	if (buffer->core != comp->core) {
		dcache_invalidate_region(buffer->cb, sizeof(*buffer->cb));

		buffer->cb->inter_core = true;

		if (!comp->cd->is_shared) {
			comp->cd = comp_make_shared(comp->cd);
			if (!comp->cd)
				return -ENOMEM;
		}
	}

	ret = pipeline_connect(comp->cd, buffer->cb,
			       PPL_CONN_DIR_COMP_TO_BUFFER);

	dcache_writeback_invalidate_region(buffer->cb, sizeof(*buffer->cb));

	platform_shared_commit(comp, sizeof(*comp));
	platform_shared_commit(buffer, sizeof(*buffer));

	return ret;
}

static int ipc_buffer_to_comp_connect(struct ipc_comp_dev *buffer,
				      struct ipc_comp_dev *comp)
{
	int ret;

	if (!cpu_is_me(comp->core))
		return ipc_process_on_core(comp->core);

	tr_dbg(&ipc_tr, "ipc: comp sink %d, source %d  -> connect", comp->id,
	       buffer->id);

	/* check if it's a connection between cores */
	if (buffer->core != comp->core) {
		dcache_invalidate_region(buffer->cb, sizeof(*buffer->cb));

		buffer->cb->inter_core = true;

		if (!comp->cd->is_shared) {
			comp->cd = comp_make_shared(comp->cd);
			if (!comp->cd)
				return -ENOMEM;
		}
	}

	ret = pipeline_connect(comp->cd, buffer->cb,
			       PPL_CONN_DIR_BUFFER_TO_COMP);

	dcache_writeback_invalidate_region(buffer->cb, sizeof(*buffer->cb));

	platform_shared_commit(comp, sizeof(*comp));
	platform_shared_commit(buffer, sizeof(*buffer));

	return ret;
}

int ipc_comp_connect(struct ipc *ipc,
	struct sof_ipc_pipe_comp_connect *connect)
{
	struct ipc_comp_dev *icd_source;
	struct ipc_comp_dev *icd_sink;

	/* check whether the components already exist */
	icd_source = ipc_get_comp_by_id(ipc, connect->source_id);
	if (!icd_source) {
		tr_err(&ipc_tr, "ipc_comp_connect(): source component does not exist, source_id = %u sink_id = %u",
		       connect->source_id, connect->sink_id);
		return -EINVAL;
	}

	icd_sink = ipc_get_comp_by_id(ipc, connect->sink_id);
	if (!icd_sink) {
		tr_err(&ipc_tr, "ipc_comp_connect(): sink component does not exist, source_id = %d sink_id = %u",
		       connect->sink_id, connect->source_id);
		return -EINVAL;
	}

	/* check source and sink types */
	if (icd_source->type == COMP_TYPE_BUFFER &&
	    icd_sink->type == COMP_TYPE_COMPONENT)
		return ipc_buffer_to_comp_connect(icd_source, icd_sink);
	else if (icd_source->type == COMP_TYPE_COMPONENT &&
		 icd_sink->type == COMP_TYPE_BUFFER)
		return ipc_comp_to_buffer_connect(icd_source, icd_sink);
	else {
		tr_err(&ipc_tr, "ipc_comp_connect(): invalid source and sink types, connect->source_id = %u, connect->sink_id = %u",
		       connect->source_id, connect->sink_id);
		return -EINVAL;
	}
}


int ipc_pipeline_new(struct ipc *ipc,
	struct sof_ipc_pipe_new *pipe_desc)
{
	struct ipc_comp_dev *ipc_pipe;
	struct pipeline *pipe;
	struct ipc_comp_dev *icd;

	/* check whether the pipeline already exists */
	ipc_pipe = ipc_get_comp_by_id(ipc, pipe_desc->comp_id);
	if (ipc_pipe != NULL) {
		tr_err(&ipc_tr, "ipc_pipeline_new(): pipeline already exists, pipe_desc->comp_id = %u",
		       pipe_desc->comp_id);
		return -EINVAL;
	}

	/* check whether pipeline id is already taken */
	ipc_pipe = ipc_get_comp_by_ppl_id(ipc, COMP_TYPE_PIPELINE,
					  pipe_desc->pipeline_id);
	if (ipc_pipe) {
		tr_err(&ipc_tr, "ipc_pipeline_new(): pipeline id is already taken, pipe_desc->pipeline_id = %u",
		       pipe_desc->pipeline_id);
		return -EINVAL;
	}

	/* find the scheduling component */
	icd = ipc_get_comp_by_id(ipc, pipe_desc->sched_id);
	if (!icd) {
		tr_err(&ipc_tr, "ipc_pipeline_new(): cannot find the scheduling component, pipe_desc->sched_id = %u",
		       pipe_desc->sched_id);
		return -EINVAL;
	}

	if (icd->type != COMP_TYPE_COMPONENT) {
		tr_err(&ipc_tr, "ipc_pipeline_new(): icd->type (%d) != COMP_TYPE_COMPONENT for pipeline scheduling component icd->id %d",
		       icd->type, icd->id);
		return -EINVAL;
	}

	if (icd->core != pipe_desc->core) {
		tr_err(&ipc_tr, "ipc_pipeline_new(): icd->core (%d) != pipe_desc->core (%d) for pipeline scheduling component icd->id %d",
		       icd->core, pipe_desc->core, icd->id);
		return -EINVAL;
	}

	/* create the pipeline */
	pipe = pipeline_new(pipe_desc, icd->cd);
	if (!pipe) {
		tr_err(&ipc_tr, "ipc_pipeline_new(): pipeline_new() failed");
		return -ENOMEM;
	}

	/* allocate the IPC pipeline container */
	ipc_pipe = rzalloc(SOF_MEM_ZONE_RUNTIME_SHARED, 0, SOF_MEM_CAPS_RAM,
			   sizeof(struct ipc_comp_dev));
	if (!ipc_pipe) {
		pipeline_free(pipe);
		return -ENOMEM;
	}

	ipc_pipe->pipeline = pipe;
	ipc_pipe->type = COMP_TYPE_PIPELINE;
	ipc_pipe->core = pipe_desc->core;
	ipc_pipe->id = pipe_desc->comp_id;

	/* add new pipeline to the list */
	list_item_append(&ipc_pipe->list, &ipc->comp_list);

	platform_shared_commit(ipc_pipe, sizeof(*ipc_pipe));

	return 0;
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
		return ipc_process_on_core(ipc_pipe->core);

	/* free buffer and remove from list */
	ret = pipeline_free(ipc_pipe->pipeline);
	if (ret < 0) {
		tr_err(&ipc_tr, "ipc_pipeline_free(): pipeline_free() failed");
		return ret;
	}
	ipc_pipe->pipeline = NULL;
	list_item_del(&ipc_pipe->list);
	rfree(ipc_pipe);

	return 0;
}

int ipc_pipeline_complete(struct ipc *ipc, uint32_t comp_id)
{
	struct ipc_comp_dev *ipc_pipe;
	uint32_t pipeline_id;
	struct ipc_comp_dev *ipc_ppl_source;
	struct ipc_comp_dev *ipc_ppl_sink;
	int ret;

	/* check whether pipeline exists */
	ipc_pipe = ipc_get_comp_by_id(ipc, comp_id);
	if (!ipc_pipe) {
		tr_err(&ipc_tr, "ipc: ipc_pipeline_complete looking for pipe component id %d failed",
		       comp_id);
		return -EINVAL;
	}

	/* check core */
	if (!cpu_is_me(ipc_pipe->core))
		return ipc_process_on_core(ipc_pipe->core);

	pipeline_id = ipc_pipe->pipeline->ipc_pipe.pipeline_id;

	tr_dbg(&ipc_tr, "ipc: pipe %d -> complete on comp %d", pipeline_id,
	       comp_id);

	/* get pipeline source component */
	ipc_ppl_source = ipc_get_ppl_src_comp(ipc, pipeline_id);
	if (!ipc_ppl_source) {
		tr_err(&ipc_tr, "ipc: ipc_pipeline_complete looking for pipeline source failed");
		return -EINVAL;
	}

	/* get pipeline sink component */
	ipc_ppl_sink = ipc_get_ppl_sink_comp(ipc, pipeline_id);
	if (!ipc_ppl_sink) {
		tr_err(&ipc_tr, "ipc: ipc_pipeline_complete looking for pipeline sink failed");
		return -EINVAL;
	}

	ret = pipeline_complete(ipc_pipe->pipeline, ipc_ppl_source->cd,
				ipc_ppl_sink->cd);

	platform_shared_commit(ipc_pipe, sizeof(*ipc_pipe));
	platform_shared_commit(ipc_ppl_source, sizeof(*ipc_ppl_source));
	platform_shared_commit(ipc_ppl_sink, sizeof(*ipc_ppl_sink));

	return ret;
}

int ipc_comp_dai_config(struct ipc *ipc, struct sof_ipc_dai_config *config)
{
	bool comp_on_core[CONFIG_CORE_COUNT] = { false };
	struct sof_ipc_comp_dai *dai;
	struct sof_ipc_reply reply;
	struct ipc_comp_dev *icd;
	struct list_item *clist;
	int ret = -ENODEV;
	int i;

	/* for each component */
	list_for_item(clist, &ipc->comp_list) {
		icd = container_of(clist, struct ipc_comp_dev, list);
		/* make sure we only config DAI comps */
		if (icd->type != COMP_TYPE_COMPONENT) {
			platform_shared_commit(icd, sizeof(*icd));
			continue;
		}

		if (!cpu_is_me(icd->core)) {
			comp_on_core[icd->core] = true;
			ret = 0;
			platform_shared_commit(icd, sizeof(*icd));
			continue;
		}

		if (dev_comp_type(icd->cd) == SOF_COMP_DAI ||
		    dev_comp_type(icd->cd) == SOF_COMP_SG_DAI) {
			dai = COMP_GET_IPC(icd->cd, sof_ipc_comp_dai);
			platform_shared_commit(icd, sizeof(*icd));
			/*
			 * set config if comp dai_index matches
			 * config dai_index.
			 */
			if (dai->dai_index == config->dai_index &&
			    dai->type == config->type) {
				ret = comp_dai_config(icd->cd, config);
				platform_shared_commit(icd, sizeof(*icd));
				if (ret < 0)
					break;
			}
		}
	}

	if (ret < 0) {
		tr_err(&ipc_tr, "ipc_comp_dai_config(): comp_dai_config() failed");
		return ret;
	}

	/* message forwarded only by primary core */
	if (!cpu_is_secondary(cpu_get_id())) {
		for (i = 0; i < CONFIG_CORE_COUNT; ++i) {
			if (!comp_on_core[i])
				continue;

			ret = ipc_process_on_core(i);
			if (ret < 0)
				return ret;

			/* check whether IPC failed on secondary core */
			mailbox_hostbox_read(&reply, sizeof(reply), 0,
					     sizeof(reply));
			if (reply.error < 0)
				/* error reply already written */
				return 1;
		}
	}

	return ret;
}

void ipc_send_queued_msg(void)
{
	struct ipc *ipc = ipc_get();
	struct ipc_msg *msg;
	uint32_t flags;

	spin_lock_irq(&ipc->lock, flags);

	/* any messages to send ? */
	if (list_is_empty(&ipc->msg_list))
		goto out;

	msg = list_first_item(&ipc->msg_list, struct ipc_msg,
			      list);

	ipc_platform_send_msg(msg);

out:
	platform_shared_commit(ipc, sizeof(*ipc));

	spin_unlock_irq(&ipc->lock, flags);
}

int ipc_init(struct sof *sof)
{
	tr_info(&ipc_tr, "ipc_init()");

	/* init ipc data */
	sof->ipc = rzalloc(SOF_MEM_ZONE_SYS_SHARED, 0, SOF_MEM_CAPS_RAM, sizeof(*sof->ipc));
	sof->ipc->comp_data = rzalloc(SOF_MEM_ZONE_SYS_SHARED, 0,
				      SOF_MEM_CAPS_RAM, SOF_IPC_MSG_MAX_SIZE);

	spinlock_init(&sof->ipc->lock);
	list_init(&sof->ipc->msg_list);
	list_init(&sof->ipc->comp_list);

	return platform_ipc_init(sof->ipc);
}

struct task_ops ipc_task_ops = {
	.run		= ipc_platform_do_cmd,
	.complete	= ipc_platform_complete_cmd,
	.get_deadline	= ipc_task_deadline,
};
