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
#include <rtos/idc.h>
#include <rtos/symbol.h>
#include <sof/ipc/topology.h>
#include <sof/ipc/common.h>
#include <sof/ipc/msg.h>
#include <sof/ipc/driver.h>
#include <sof/ipc/schedule.h>
#include <rtos/alloc.h>
#include <rtos/cache.h>
#include <sof/lib/cpu.h>
#include <sof/lib/mailbox.h>
#include <sof/lib/memory.h>
#include <sof/list.h>
#include <sof/platform.h>
#include <sof/schedule/ll_schedule.h>
#include <sof/schedule/ll_schedule_domain.h>
#include <rtos/sof.h>
#include <rtos/spinlock.h>
#include <ipc/dai.h>
#include <ipc/header.h>
#include <ipc/stream.h>
#include <ipc/topology.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __ZEPHYR__
#include <zephyr/kernel.h>
#endif

#ifdef CONFIG_SOF_USERSPACE_LL
#include <rtos/userspace_helper.h>
#include <sof/schedule/ll_schedule_domain.h>
#include <ipc4/pipeline.h>
#include <ipc4/module.h>
#include <ipc4/handler.h>
#endif

#include <sof/debug/telemetry/performance_monitor.h>

LOG_MODULE_REGISTER(ipc, CONFIG_SOF_LOG_LEVEL);

SOF_DEFINE_REG_UUID(ipc);

DECLARE_TR_CTX(ipc_tr, SOF_UUID(ipc_uuid), LOG_LEVEL_INFO);

#ifdef CONFIG_SOF_USERSPACE_LL
K_APPMEM_PARTITION_DEFINE(ipc_context_part);

K_APP_BMEM(ipc_context_part) static struct ipc ipc_context;

struct ipc *ipc_get(void)
{
	return &ipc_context;
}
EXPORT_SYMBOL(ipc_get);
#endif

int ipc_process_on_core(uint32_t core, bool blocking)
{
	struct ipc *ipc = ipc_get();
	struct idc_msg msg = { .header = IDC_MSG_IPC, .core = core, };
	int ret;

	/* check if requested core is enabled */
	if (!cpu_is_core_enabled(core)) {
		tr_err(&ipc_tr, "core #%d is disabled", core);
		return -EACCES;
	}

#if CONFIG_IPC_MAJOR_3
	/* The other core will write there its response */
	dcache_invalidate_region((__sparse_force void __sparse_cache *)MAILBOX_HOSTBOX_BASE,
				 ((struct sof_ipc_cmd_hdr *)ipc->comp_data)->size);
#endif

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

void ipc_send_queued_msg(void)
{
	struct ipc *ipc = ipc_get();
	struct ipc_msg *msg;
	k_spinlock_key_t key;

	key = k_spin_lock(&ipc->lock);

	if (ipc->pm_prepare_D3)
		goto out;

	/* any messages to send ? */
	if (list_is_empty(&ipc->msg_list))
		goto out;

	msg = list_first_item(&ipc->msg_list, struct ipc_msg,
			      list);

	if (ipc_platform_send_msg(msg) == 0) {
		/* Remove the message from the list if it has been successfully sent. */
		list_item_del(&msg->list);
		/* Invoke a callback to notify that the message has been sent. */
		if (msg->callback)
			msg->callback(msg);
#ifdef CONFIG_SOF_TELEMETRY_IO_PERFORMANCE_MEASUREMENTS
		/* Increment performance counters */
		io_perf_monitor_update_data(ipc->io_perf_out_msg_count, 1);
#endif
	}
out:
	k_spin_unlock(&ipc->lock, key);
}

#ifdef __ZEPHYR__
static K_THREAD_STACK_DEFINE(ipc_send_wq_stack, CONFIG_STACK_SIZE_IPC_TX);
#endif

static void schedule_ipc_worker(void)
{
	/*
	 * note: in XTOS builds, this is handled in
	 * task_main_primary_core()
	 */
#ifdef __ZEPHYR__
	struct ipc *ipc = ipc_get();

	k_work_schedule_for_queue(&ipc->ipc_send_wq, &ipc->z_delayed_work, K_USEC(IPC_PERIOD_USEC));
#endif
}

__cold void ipc_msg_send_direct(struct ipc_msg *msg, void *data)
{
	struct ipc *ipc = ipc_get();
	k_spinlock_key_t key;
	int ret;

	assert_can_be_cold();

	key = k_spin_lock(&ipc->lock);

	/* copy mailbox data to message if not already copied */
	if (msg->tx_size > 0 && msg->tx_size <= SOF_IPC_MSG_MAX_SIZE &&
	    msg->tx_data != data) {
		ret = memcpy_s(msg->tx_data, msg->tx_size, data, msg->tx_size);
		assert(!ret);
	}

	ipc_platform_send_msg_direct(msg);

	k_spin_unlock(&ipc->lock, key);
}

void ipc_msg_send(struct ipc_msg *msg, void *data, bool high_priority)
{
	struct ipc *ipc = ipc_get();
	k_spinlock_key_t key;
	int ret;

	key = k_spin_lock(&ipc->lock);

	/* copy mailbox data to message if not already copied */
	if ((msg->tx_size > 0 && msg->tx_size <= SOF_IPC_MSG_MAX_SIZE) &&
	    msg->tx_data != data) {
		ret = memcpy_s(msg->tx_data, msg->tx_size, data, msg->tx_size);
		assert(!ret);
	}

	/*
	 * note: This function can be executed in LL or EDF context, from any core.
	 * In Zephyr builds, there is IPC queue that is always handled by the primary core,
	 * whereas submitting to the queue is allowed from any core. Therefore disable option
	 * of sending IPC immediately by any context/core to secure IPC registers/mailbox
	 * access.
	 */
#ifndef __ZEPHYR__
	/* try to send critical notifications right away */
	if (high_priority) {
		ret = ipc_platform_send_msg(msg);
		if (!ret) {
			k_spin_unlock(&ipc->lock, key);
			return;
		}
	}
#endif
	/* add to queue unless already there */
	if (list_is_empty(&msg->list)) {
		if (high_priority)
			list_item_prepend(&msg->list, &ipc->msg_list);
		else
			list_item_append(&msg->list, &ipc->msg_list);
	}

#if 0 /*def CONFIG_SOF_USERSPACE_LL */
	LOG_WRN("Skipping IPC worker schedule. TODO to fix\n");
#else
	schedule_ipc_worker();
#endif

	k_spin_unlock(&ipc->lock, key);
}
EXPORT_SYMBOL(ipc_msg_send);

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
#if CONFIG_TWB_IPC_TASK
	schedule_task(ipc->ipc_task, 0, IPC_PERIOD_USEC);
#else
	schedule_task(&ipc->ipc_task, 0, IPC_PERIOD_USEC);
#endif
}

#ifdef CONFIG_SOF_USERSPACE_LL
/* User-space thread for pipeline_two_components test */

#define IPC_USER_EVENT_CMD      BIT(0)
#define IPC_USER_EVENT_STOP     BIT(1)

static struct k_thread ipc_user_thread;
static K_THREAD_STACK_DEFINE(ipc_user_stack, CONFIG_SOF_IPC_USER_THREAD_STACK_SIZE);

/**
 * @brief Forward an IPC4 command to the user-space thread.
 *
 * Called from kernel context (IPC EDF task) to forward the IPC4
 * message to the user-space thread for processing. Sets
 * IPC_TASK_IN_THREAD in task_mask so the host is not signaled
 * until the user thread completes. Blocks until the user thread
 * finishes processing and returns the result.
 *
 * @param ipc4 Pointer to the IPC4 message request
 * @return Result from user thread processing
 */
int ipc_user_forward_cmd(struct ipc4_message_request *ipc4)
{
	struct ipc *ipc = ipc_get();
	struct ipc_user *pdata = ipc->ipc_user_pdata;
	k_spinlock_key_t key;
	int ret;

	LOG_DBG("IPC: forward cmd %08x", ipc4->primary.dat);

	/* Copy message words — original buffer may be reused */
	pdata->ipc_msg_pri = ipc4->primary.dat;
	pdata->ipc_msg_ext = ipc4->extension.dat;
	pdata->ipc = ipc;

	/* Prevent host completion until user thread finishes */
	key = k_spin_lock(&ipc->lock);
	ipc->task_mask |= IPC_TASK_IN_THREAD;
	k_spin_unlock(&ipc->lock, key);

	/* Wake the user thread */
	k_event_set(pdata->event, IPC_USER_EVENT_CMD);

	/* Wait for user thread to complete */
	ret = k_sem_take(pdata->sem, K_MSEC(10));
	if (ret) {
		LOG_ERR("IPC user: sem error %d\n", ret);
		return ret;
	}

	/* Clear the task mask bit and check for completion */
	key = k_spin_lock(&ipc->lock);
	ipc->task_mask &= ~IPC_TASK_IN_THREAD;
	ipc_complete_cmd(ipc);
	k_spin_unlock(&ipc->lock, key);

	return pdata->result;
}

/**
 * User-space thread entry point for pipeline_two_components test.
 * p1 points to the ppl_test_ctx shared with the kernel launcher.
 */
static void ipc_user_thread_fn(void *p1, void *p2, void *p3)
{
	struct ipc_user *ipc_user = p1;

	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	__ASSERT(k_is_user_context(), "expected user context");

	/* Signal startup complete — unblocks init waiting on semaphore */
	k_sem_give(ipc_user->sem);
	LOG_INF("IPC user-space thread started");

	for (;;) {
		uint32_t mask = k_event_wait_safe(ipc_user->event,
						  IPC_USER_EVENT_CMD | IPC_USER_EVENT_STOP,
						  false, K_MSEC(5000));

		LOG_DBG("IPC user wake, mask %u", mask);

		if (mask & IPC_USER_EVENT_CMD) {
			struct ipc4_message_request msg;

			/* Reconstruct the IPC4 message from copied words */
			msg.primary.dat = ipc_user->ipc_msg_pri;
			msg.extension.dat = ipc_user->ipc_msg_ext;

			ipc_user->reply_ext = 0;

			if (msg.primary.r.msg_tgt ==
			    SOF_IPC4_MESSAGE_TARGET_MODULE_MSG) {
				/* Module message dispatch */
				switch (msg.primary.r.type) {
				case SOF_IPC4_MOD_CONFIG_GET:
					ipc_user->result =
						ipc4_process_module_config(
							&msg, false,
							&ipc_user->reply_ext);
					break;
				case SOF_IPC4_MOD_CONFIG_SET:
					ipc_user->result =
						ipc4_process_module_config(
							&msg, true, NULL);
					break;
				default:
					LOG_ERR("IPC user: unsupported module cmd type %d",
						msg.primary.r.type);
					ipc_user->result = -EINVAL;
					break;
				}
			} else {
				/* Global message dispatch */
				switch (msg.primary.r.type) {
				case SOF_IPC4_GLB_CREATE_PIPELINE:
					ipc_user->result =
						ipc_pipeline_new(ipc_user->ipc,
								 (ipc_pipe_new *)&msg);
					break;
				case SOF_IPC4_GLB_DELETE_PIPELINE: {
					struct ipc4_pipeline_delete *pipe =
						(struct ipc4_pipeline_delete *)&msg;

					ipc_user->result =
						ipc_pipeline_free(
							ipc_user->ipc,
							pipe->primary.r.instance_id);
					break;
				}
				default:
					LOG_ERR("IPC user: unsupported glb cmd type %d",
						msg.primary.r.type);
					ipc_user->result = -EINVAL;
					break;
				}
			}

			/* Signal completion — kernel side will finish IPC */
			k_sem_give(ipc_user->sem);
		}

		if (mask & IPC_USER_EVENT_STOP)
			break;
	}
}

__cold int ipc_user_init(void)
{
	struct ipc *ipc = ipc_get();
	struct ipc_user *ipc_user = sof_heap_alloc(sof_sys_user_heap_get(), SOF_MEM_FLAG_USER,
						   sizeof(*ipc_user), 0);
	int ret;

	ipc_user->sem = k_object_alloc(K_OBJ_SEM);
	if (!ipc_user->sem) {
		LOG_ERR("user IPC sem alloc failed");
		k_panic();
	}

	ret = k_mem_domain_add_partition(zephyr_ll_mem_domain(), &ipc_context_part);

	k_sem_init(ipc_user->sem, 0, 1);

	/* Allocate kernel objects for the user-space thread */
	ipc_user->event = k_object_alloc(K_OBJ_EVENT);
	if (!ipc_user->event) {
		LOG_ERR("user IPC event alloc failed");
		k_panic();
	}
	k_event_init(ipc_user->event);

	k_thread_create(&ipc_user_thread, ipc_user_stack,
			CONFIG_SOF_IPC_USER_THREAD_STACK_SIZE,
			ipc_user_thread_fn, ipc_user, NULL, NULL,
			-1, K_USER, K_FOREVER);

	ipc_user->thread = &ipc_user_thread;
	k_thread_access_grant(&ipc_user_thread, ipc_user->sem, ipc_user->event);
	user_grant_dai_access_all(&ipc_user_thread);
	user_grant_dma_access_all(&ipc_user_thread);
	user_access_to_mailbox(zephyr_ll_mem_domain(), &ipc_user_thread);
	zephyr_ll_grant_access(&ipc_user_thread);
	k_mem_domain_add_thread(zephyr_ll_mem_domain(), &ipc_user_thread);

	k_thread_cpu_pin(&ipc_user_thread, PLATFORM_PRIMARY_CORE_ID);
	k_thread_name_set(&ipc_user_thread, "ipc_user");

	/* Store references in ipc struct so kernel handler can forward commands */
	ipc->ipc_user_pdata = ipc_user;

	k_thread_start(&ipc_user_thread);

	struct task *task = zephyr_ll_task_alloc();
	schedule_task_init_ll(task, SOF_UUID(ipc_uuid), SOF_SCHEDULE_LL_TIMER,
			0, NULL, NULL, cpu_get_id(), 0);
	ipc_user->audio_thread = scheduler_init_context(task);

	/* Wait for user thread startup — consumes the initial k_sem_give from thread */
	k_sem_take(ipc->ipc_user_pdata->sem, K_FOREVER);

	return 0;
}
#else
static int ipc_user_init(void)
{
	return 0;
}
#endif /* CONFIG_SOF_USERSPACE_LL */

__cold int ipc_init(struct sof *sof)
{
	struct k_heap *heap;
	struct ipc *ipc;

	assert_can_be_cold();

	tr_dbg(&ipc_tr, "entry");

#ifdef CONFIG_SOF_USERSPACE_LL
	heap = zephyr_ll_user_heap();

	ipc = ipc_get();
	memset(ipc, 0, sizeof(*ipc));
#else
	heap = NULL;

	/* init ipc data */
	ipc = rzalloc(SOF_MEM_FLAG_USER | SOF_MEM_FLAG_COHERENT, sizeof(*ipc));
	if (!ipc) {
		tr_err(&ipc_tr, "Unable to allocate IPC data");
		return -ENOMEM;
	}
	sof->ipc = ipc;
#endif

	ipc->comp_data = sof_heap_alloc(heap, SOF_MEM_FLAG_USER | SOF_MEM_FLAG_COHERENT,
					SOF_IPC_MSG_MAX_SIZE, 0);
	if (!ipc->comp_data) {
		tr_err(&ipc_tr, "Unable to allocate IPC component data");
		sof_heap_free(heap, ipc);
		return -ENOMEM;
	}
	memset(ipc->comp_data, 0, SOF_IPC_MSG_MAX_SIZE);

	k_spinlock_init(&ipc->lock);
	list_init(&ipc->msg_list);
	list_init(&ipc->comp_list);

#ifdef CONFIG_SOF_TELEMETRY_IO_PERFORMANCE_MEASUREMENTS
	struct io_perf_data_item init_data = {IO_PERF_IPC_ID,
					      cpu_get_id(),
					      IO_PERF_INPUT_DIRECTION,
					      IO_PERF_POWERED_UP_ENABLED,
					      IO_PERF_D0IX_POWER_MODE,
					      0, 0, 0 };
	io_perf_monitor_init_data(&ipc->io_perf_in_msg_count, &init_data);
	init_data.direction = IO_PERF_OUTPUT_DIRECTION;
	io_perf_monitor_init_data(&ipc->io_perf_out_msg_count, &init_data);
#endif


#ifdef __ZEPHYR__
	struct k_thread *thread = &ipc->ipc_send_wq.thread;

	k_work_queue_init(&ipc->ipc_send_wq);
	k_work_queue_start(&ipc->ipc_send_wq, ipc_send_wq_stack,
			   K_THREAD_STACK_SIZEOF(ipc_send_wq_stack), 1, NULL);

	k_thread_suspend(thread);

	k_thread_cpu_pin(thread, PLATFORM_PRIMARY_CORE_ID);
	k_thread_name_set(thread, "ipc_send_wq");

	k_thread_resume(thread);

	k_work_init_delayable(&ipc->z_delayed_work, ipc_work_handler);
#endif

	ipc_user_init();

#if CONFIG_SOF_BOOT_TEST_STANDALONE
	LOG_INF("SOF_BOOT_TEST_STANDALONE, skipping platform IPC init.");
	return 0;
#endif

	return platform_ipc_init(ipc);
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

bool ipc_enter_gdb;

__attribute__((weak)) void ipc_platform_wait_ack(struct ipc *ipc)
{
	k_msleep(1);
}

static void ipc_complete_task(void *data)
{
	struct ipc *ipc = data;
	k_spinlock_key_t key;

	key = k_spin_lock(&ipc->lock);
	ipc->task_mask &= ~IPC_TASK_INLINE;
	ipc_complete_cmd(ipc);
	k_spin_unlock(&ipc->lock, key);
#if CONFIG_GDBSTUB
	if (ipc_enter_gdb) {
		ipc_enter_gdb = false;
		ipc_platform_wait_ack(ipc);
		gdb_init();
	}
#endif
}

static enum task_state ipc_do_cmd(void *data)
{
	struct ipc *ipc = data;

#ifdef CONFIG_SOF_TELEMETRY_IO_PERFORMANCE_MEASUREMENTS
	/* Increment performance counters */
	io_perf_monitor_update_data(ipc->io_perf_in_msg_count, 1);
#endif

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
