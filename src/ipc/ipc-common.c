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
#include <sof/ipc/topology.h>
#include <sof/ipc/common.h>
#include <sof/ipc/msg.h>
#include <sof/ipc/driver.h>
#include <sof/ipc/schedule.h>
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

void ipc_msg_send(struct ipc_msg *msg, void *data, bool high_priority)
{
	struct ipc *ipc = ipc_get();
	uint32_t flags;
	int ret;

	spin_lock_irq(&ipc->lock, flags);

	/* copy mailbox data to message */
	if (msg->tx_size > 0 && msg->tx_size < SOF_IPC_MSG_MAX_SIZE) {
		ret = memcpy_s(msg->tx_data, msg->tx_size, data, msg->tx_size);
		assert(!ret);
	}

	/* try to send critical notifications right away */
	if (high_priority) {
		ret = ipc_platform_send_msg(msg);
		if (!ret)
			goto out;
	}

	/* add to queue unless already there */
	if (list_is_empty(&msg->list)) {
		if (high_priority)
			list_item_prepend(&msg->list, &ipc->msg_list);
		else
			list_item_append(&msg->list, &ipc->msg_list);
	}

out:
	spin_unlock_irq(&ipc->lock, flags);
}

void ipc_schedule_process(struct ipc *ipc)
{
	schedule_task(&ipc->ipc_task, 0, IPC_PERIOD_USEC);
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
