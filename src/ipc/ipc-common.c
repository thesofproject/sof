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
#include <sof/debug/gdb/gdb.h>
#include <rtos/idc_msg.h>
#include <rtos/symbol.h>
#include <sof/ipc/topology.h>
#include <sof/ipc/common.h>
#include <sof/ipc/msg.h>
#include <sof/ipc/schedule.h>
#include <rtos/alloc.h>
#include <rtos/cache.h>
#include <sof/lib/cpu.h>
#include <sof/list.h>
#include <sof/schedule/ll_schedule.h>
#include <sof/schedule/ll_schedule_domain.h>
#include <rtos/sof.h>
#include <rtos/spinlock.h>
#include <ipc/dai.h>
#include <ipc/header.h>
#include <ipc/stream.h>
#include <ipc/topology.h>
#if CONFIG_IPC_MAJOR_4
#include <ipc4/module.h>
#endif
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __ZEPHYR__
#include <zephyr/kernel.h>
#include <zephyr/internal/syscall_handler.h>
#endif

#include <sof/debug/telemetry/performance_monitor.h>

/* The IPC code that runs in kernel mode is in src/kernel/ipc/ipc-common-kernel.c */

LOG_MODULE_DECLARE(ipc, CONFIG_SOF_LOG_LEVEL);

struct ipc_msg *ipc_msg_w_ext_init(struct k_heap *heap, uint32_t header,
				   uint32_t extension, uint32_t size)
{
	struct ipc_msg *msg;

	msg = sof_heap_alloc(heap, SOF_MEM_FLAG_USER | SOF_MEM_FLAG_COHERENT,
				     sizeof(*msg), 0);
	if (!msg)
		return NULL;

	memset(msg, 0, sizeof(*msg));

	if (size) {
		msg->tx_data = sof_heap_alloc(heap,
					      SOF_MEM_FLAG_USER | SOF_MEM_FLAG_COHERENT,
					      size, 0);
		if (!msg->tx_data) {
			sof_heap_free(heap, msg);
			return NULL;
		}
		memset(msg->tx_data, 0, size);
	}

	msg->header = header;
	msg->extension = extension;
	msg->tx_size = size;
	list_init(&msg->list);

	return msg;
}

/*
 * Components, buffers and pipelines are stored in the same lists, hence
 * type and ID have to be used for the identification.
 */
struct ipc_comp_dev *ipc_get_comp_dev(struct ipc *ipc, uint16_t type, uint32_t id)
{
	struct ipc_comp_dev *icd;
	struct list_item *clist;

	list_for_item(clist, &ipc->comp_list) {
		icd = container_of(clist, struct ipc_comp_dev, list);
		if (icd->id == id && (type == icd->type || type == COMP_TYPE_ANY))
			return icd;
	}

	return NULL;
}
EXPORT_SYMBOL(ipc_get_comp_dev);

/* Walks through the list of components looking for a sink/source endpoint component
 * of the given pipeline
 */
struct ipc_comp_dev *ipc_get_ppl_comp(struct ipc *ipc, uint32_t pipeline_id, int dir)
{
	struct ipc_comp_dev *icd;
	struct comp_buffer *buffer;
	struct comp_dev *buff_comp;
	struct list_item *clist, *blist;
	struct ipc_comp_dev *next_ppl_icd = NULL;

	list_for_item(clist, &ipc->comp_list) {
		icd = container_of(clist, struct ipc_comp_dev, list);
		if (icd->type != COMP_TYPE_COMPONENT)
			continue;

		/* first try to find the module in the pipeline */
		if (dev_comp_pipe_id(icd->cd) == pipeline_id) {
			struct list_item *buffer_list = comp_buffer_list(icd->cd, dir);
			bool last_in_pipeline = true;

			/* The component has no buffer in the given direction */
			if (list_is_empty(buffer_list))
				return icd;

			/* check all connected modules to see if they are on different pipelines */
			list_for_item(blist, buffer_list) {
				buffer = buffer_from_list(blist, dir);
				buff_comp = buffer_get_comp(buffer, dir);

				if (buff_comp && dev_comp_pipe_id(buff_comp) == pipeline_id)
					last_in_pipeline = false;
			}
			/* all connected components placed on another pipeline */
			if (last_in_pipeline)
				next_ppl_icd = icd;
		}
	}

	return next_ppl_icd;
}

#ifdef CONFIG_SOF_USERSPACE_LL
void ipc_user_xcore_reserve(uint32_t cores)
{
	struct ipc_user *ipc_user = ipc_get()->ipc_user_pdata;
	int core;

	/* in ascending core order, like the LL scheduler locks */
	for (core = 0; core < CONFIG_CORE_COUNT; core++) {
		if (!(cores & BIT(core)))
			continue;

		k_sem_take(ipc_user->xcore[core].lock, K_FOREVER);
		ipc_user->xcore[core].owner = k_current_get();
	}
}

void ipc_user_xcore_release(uint32_t cores)
{
	struct ipc_user *ipc_user = ipc_get()->ipc_user_pdata;
	int core;

	for (core = CONFIG_CORE_COUNT - 1; core >= 0; core--) {
		if (!(cores & BIT(core)))
			continue;

		ipc_user->xcore[core].owner = NULL;
		k_sem_give(ipc_user->xcore[core].lock);
	}
}

/**
 * @brief Pass an IDC message to the user-space IPC thread of another core.
 *
 * Called from user-space threads (IPC or LL) instead of idc_send_msg(),
 * which is kernel-only. Blocks until the target core has processed the
 * message and returns its result.
 *
 * @param msg IDC message
 * @return Result from the target core, or negative error code
 */
int ipc_user_xcore_send(const struct idc_msg *msg)
{
	struct ipc_user *ipc_user = ipc_get()->ipc_user_pdata;
	struct ipc_user_xcore *xc;
	unsigned int ll_depth = 0;
	bool reserved;
	int ll_core;
	int ret;

	if (msg->core >= CONFIG_CORE_COUNT || msg->size > sizeof(xc->payload) ||
	    !ipc_user->event[msg->core])
		return -EINVAL;

	xc = &ipc_user->xcore[msg->core];
	reserved = xc->owner == k_current_get();

	/*
	 * An LL thread holds the LL scheduler lock of its core for the whole
	 * LL cycle, e.g. when it triggers or resets a remote module. Release
	 * it while waiting: the target IPC thread may need it (e.g. for a
	 * bind, locking both cores), or need the LL lock of its own core,
	 * whose LL thread may in turn wait for a request to this core.
	 *
	 * Like with IDC in kernel mode, the pipeline graph, that the LL
	 * thread may be walking, isn't protected while waiting. The host
	 * resets a pipeline before unbinding or deleting its modules, and the
	 * reset waits for the LL thread to run the trigger. With IPC4 the
	 * walk doesn't wait in modules of other pipelines.
	 */
	for (ll_core = 0; ll_core < CONFIG_CORE_COUNT; ll_core++) {
		if (ipc_user->audio_thread[ll_core] == k_current_get()) {
			ll_depth = user_ll_unlock_sched_all(ll_core);
			break;
		}
	}

	if (!reserved)
		k_sem_take(xc->lock, K_FOREVER);

	xc->header = msg->header;
	xc->extension = msg->extension;
	if (msg->size) {
		ret = memcpy_s(xc->payload, sizeof(xc->payload), msg->payload, msg->size);
		if (ret < 0)
			goto out;
	}

#if CONFIG_IPC_MAJOR_4
	/*
	 * The bind / unbind message usually is on the sender's stack, which
	 * the target thread can't access. Pass a copy of it.
	 */
	if (iTS(msg->header) == iTS(IDC_MSG_BIND) || iTS(msg->header) == iTS(IDC_MSG_UNBIND)) {
		struct bind_info *bind_data = (struct bind_info *)xc->payload;

		if (bind_data->ipc4_data) {
			ret = memcpy_s(xc->ipc4_bind_data, sizeof(xc->ipc4_bind_data),
				       bind_data->ipc4_data, sizeof(*bind_data->ipc4_data));
			if (ret < 0)
				goto out;
			bind_data->ipc4_data =
				(const struct ipc4_module_bind_unbind *)xc->ipc4_bind_data;
		}
	}
#endif

	/* post, not set: don't clear a pending IPC_USER_EVENT_CMD */
	k_event_post(ipc_user->event[msg->core], IPC_USER_EVENT_XCORE);
	k_sem_take(xc->done, K_FOREVER);
	ret = xc->result;
out:
	if (!reserved)
		k_sem_give(xc->lock);

	if (ll_depth)
		user_ll_relock_sched(ll_core, ll_depth);

	return ret;
}

/**
 * @brief Protocol-specific dispatch of a forwarded IPC command.
 *
 * Weak default; the active IPC major overrides this. Runs in the user
 * thread context with the forwarded message words in @p ipc_user.
 */
__weak int ipc_user_thread_dispatch(struct ipc_user *ipc_user)
{
	ARG_UNUSED(ipc_user);
	return -ENOSYS;
}

/**
 * User-space IPC thread entry point. p1 points to the ipc_user context
 * shared with the kernel launcher.
 */
void ipc_user_thread_fn(void *p1, void *p2, void *p3)
{
	struct ipc_user *ipc_user = p1;
	unsigned int core = POINTER_TO_UINT(p2);

	ARG_UNUSED(p3);

	__ASSERT(k_is_user_context(), "expected user context");

	/* Signal startup complete — unblocks init waiting on semaphore */
	k_sem_give(ipc_user->sem);
	LOG_INF("IPC user-space thread started");

	for (;;) {
		uint32_t mask = k_event_wait_safe(ipc_user->event[core],
						  IPC_USER_EVENT_CMD | IPC_USER_EVENT_STOP |
						  IPC_USER_EVENT_XCORE,
						  false, K_FOREVER);

		LOG_INF("IPC user wake, mask %u", mask);

		/* idc_cmd_user() is only built with SMP, there are no requests without */
		if (IS_ENABLED(CONFIG_SMP) && (mask & IPC_USER_EVENT_XCORE)) {
			struct ipc_user_xcore *xc = &ipc_user->xcore[core];
			const struct idc_msg msg = {
				.header = xc->header,
				.extension = xc->extension,
				.core = core,
			};

			xc->result = idc_cmd_user(&msg, xc->payload);
			k_sem_give(xc->done);
		}

		if (mask & IPC_USER_EVENT_CMD) {
			ipc_user->result = ipc_user_thread_dispatch(ipc_user);

			/* Signal completion — kernel side will finish IPC */
			k_sem_give(ipc_user->sem);
		}

		if (mask & IPC_USER_EVENT_STOP)
			break;
	}
}
#endif /* CONFIG_SOF_USERSPACE_LL */
