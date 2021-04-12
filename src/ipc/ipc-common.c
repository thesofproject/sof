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
			continue;
		}

		if (!cpu_is_me(icd->core)) {
			continue;
		}

		if (ipc_comp_pipe_id(icd) == ppl_id)
			return icd;

	}

	return NULL;
}

struct ipc_comp_dev *ipc_get_ppl_comp(struct ipc *ipc,
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
			continue;
		}

		if (!cpu_is_me(icd->core)) {
			continue;
		}

		if (dev_comp_pipe_id(icd->cd) == pipeline_id &&
		    list_is_empty(comp_buffer_list(icd->cd, dir)))
			return icd;

	}

	/* it's connected pipeline, so find the connected module */
	list_for_item(clist, &ipc->comp_list) {
		icd = container_of(clist, struct ipc_comp_dev, list);
		if (icd->type != COMP_TYPE_COMPONENT) {
			continue;
		}

		if (!cpu_is_me(icd->core)) {
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
