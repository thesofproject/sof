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
#include <rtos/alloc.h>
#include <rtos/cache.h>
#include <sof/lib/cpu.h>
#include <sof/lib/mailbox.h>
#include <sof/list.h>
#include <sof/platform.h>
#include <sof/sof.h>
#include <rtos/spinlock.h>
#include <ipc/dai.h>
#include <ipc/header.h>
#include <ipc/stream.h>
#include <ipc/topology.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

LOG_MODULE_REGISTER(ipc, CONFIG_SOF_LOG_LEVEL);

/* be60f97d-78df-4796-a0ee-435cb56b720a */
DECLARE_SOF_UUID("ipc", ipc_uuid, 0xbe60f97d, 0x78df, 0x4796,
		 0xa0, 0xee, 0x43, 0x5c, 0xb5, 0x6b, 0x72, 0x0a);

DECLARE_TR_CTX(ipc_tr, SOF_UUID(ipc_uuid), LOG_LEVEL_INFO);

int ipc_process_on_core(uint32_t core, bool blocking)
{
	struct ipc *ipc = ipc_get();
	struct idc_msg msg = { .header = IDC_MSG_IPC, .core = core, };
	int ret;

	/* check if requested core is enabled */
	if (!cpu_is_core_enabled(core)) {
		tr_err(&ipc_tr, "ipc_process_on_core(): core #%d is disabled", core);
		return -EACCES;
	}

	/* The other core will write there its response */
	dcache_invalidate_region((__sparse_force void __sparse_cache *)MAILBOX_HOSTBOX_BASE,
				 ((struct sof_ipc_cmd_hdr *)ipc->comp_data)->size);

	/*
	 * If the primary core is waiting for secondary cores to complete, it
	 * will also reply to the host
	 */
	if (!blocking) {
		k_spinlock_key_t key;

		ipc->core = core;
		key = k_spin_lock(&ipc->lock);
		ipc->task_mask |= IPC_TASK_SECONDARY_CORE;
		k_spin_unlock(&ipc->lock, key);
	}

	/* send IDC message */
	ret = idc_send_msg(&msg, blocking ? IDC_BLOCKING : IDC_NON_BLOCKING);
	if (ret < 0)
		return ret;

	/* reply written by other core */
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

struct ipc_comp_dev *ipc_get_comp_by_ppl_id(struct ipc *ipc, uint16_t type, uint32_t ppl_id)
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

/* Walks through the list of components looking for a sink/source endpoint component
 * of the given pipeline
 */
struct ipc_comp_dev *ipc_get_ppl_comp(struct ipc *ipc, uint32_t pipeline_id, int dir)
{
	struct ipc_comp_dev *icd;
	struct comp_buffer *buffer;
	struct comp_dev *buff_comp;
	struct list_item *clist;
	struct ipc_comp_dev *next_ppl_icd = NULL;

	list_for_item(clist, &ipc->comp_list) {
		icd = container_of(clist, struct ipc_comp_dev, list);
		if (icd->type != COMP_TYPE_COMPONENT)
			continue;

		/* first try to find the module in the pipeline */
		if (dev_comp_pipe_id(icd->cd) == pipeline_id) {
			struct list_item *buffer_list = comp_buffer_list(icd->cd, dir);

			/* The component has no buffer in the given direction */
			if (list_is_empty(buffer_list))
				return icd;

			/* it's connected pipeline, so find the connected module */
			buffer = buffer_from_list(buffer_list->next, struct comp_buffer, dir);
			buff_comp = buffer_get_comp(buffer, dir);

			/* Next component is placed on another pipeline */
			if (buff_comp && dev_comp_pipe_id(buff_comp) != pipeline_id)
				next_ppl_icd = icd;
		}

	}

	return next_ppl_icd;
}

void ipc_send_queued_msg(void)
{
	struct ipc *ipc = ipc_get();
	struct ipc_msg *msg;
	k_spinlock_key_t key;

	key = k_spin_lock(&ipc->lock);

	if (ipc_get()->pm_prepare_D3)
		goto out;

	/* any messages to send ? */
	if (list_is_empty(&ipc->msg_list))
		goto out;

	msg = list_first_item(&ipc->msg_list, struct ipc_msg,
			      list);

	if (ipc_platform_send_msg(msg) == 0)
		/* Remove the message from the list if it has been successfully sent. */
		list_item_del(&msg->list);
out:
	k_spin_unlock(&ipc->lock, key);
}

static void schedule_ipc_worker(void)
{
	/*
	 * note: in XTOS builds, this is handled in
	 * task_main_primary_core()
	 */
#ifdef __ZEPHYR__
	struct ipc *ipc = ipc_get();

	k_work_schedule(&ipc->z_delayed_work, K_USEC(IPC_PERIOD_USEC));
#endif
}

void ipc_msg_send(struct ipc_msg *msg, void *data, bool high_priority)
{
	struct ipc *ipc = ipc_get();
	k_spinlock_key_t key;
	int ret;

	key = k_spin_lock(&ipc->lock);

	/* copy mailbox data to message if not already copied */
	if ((msg->tx_size > 0 && msg->tx_size < SOF_IPC_MSG_MAX_SIZE) &&
	    msg->tx_data != data) {
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

	schedule_ipc_worker();

out:
	k_spin_unlock(&ipc->lock, key);
}

#ifdef __ZEPHYR__
static void ipc_work_handler(struct k_work *work)
{
	struct ipc *ipc = ipc_get();
	k_spinlock_key_t key;

	ipc_send_queued_msg();

	key = k_spin_lock(&ipc->lock);

	if (!list_is_empty(&ipc->msg_list) && !ipc->pm_prepare_D3)
		schedule_ipc_worker();

	k_spin_unlock(&ipc->lock, key);
}
#endif

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

	k_spinlock_init(&sof->ipc->lock);
	list_init(&sof->ipc->msg_list);
	list_init(&sof->ipc->comp_list);

#ifdef __ZEPHYR__
	k_work_init_delayable(&sof->ipc->z_delayed_work, ipc_work_handler);
#endif

	return platform_ipc_init(sof->ipc);
}

/* Locking: call with ipc->lock held and interrupts disabled */
void ipc_complete_cmd(struct ipc *ipc)
{
	/*
	 * We have up to three contexts, attempting to complete IPC processing:
	 * the original IPC EDF task, the IDC EDF task on a secondary core, or
	 * an LL pipeline thread, running either on the primary or one of
	 * secondary cores. All these three contexts execute asynchronously. It
	 * is important to only signal the host that the IPC processing has
	 * completed after *all* tasks have completed. Therefore only the last
	 * context should do that. We accomplish this by setting IPC_TASK_* bits
	 * in ipc->task_mask for each used IPC context and by clearing them when
	 * each of those contexts completes. Only when the mask is 0 we can
	 * signal the host.
	 */
	if (ipc->task_mask)
		return;

	ipc_platform_complete_cmd(ipc);
}

static void ipc_complete_task(void *data)
{
	struct ipc *ipc = data;
	k_spinlock_key_t key;

	key = k_spin_lock(&ipc->lock);
	ipc->task_mask &= ~IPC_TASK_INLINE;
	ipc_complete_cmd(ipc);
	k_spin_unlock(&ipc->lock, key);
}

static enum task_state ipc_do_cmd(void *data)
{
	struct ipc *ipc = data;

	/*
	 * 32-bit writes are atomic and at the moment no IPC processing is
	 * taking place, so, no need for a lock.
	 */
	ipc->task_mask = IPC_TASK_INLINE;

	return ipc_platform_do_cmd(ipc);
}

struct task_ops ipc_task_ops = {
	.run		= ipc_do_cmd,
	.complete	= ipc_complete_task,
	.get_deadline	= ipc_task_deadline,
};
