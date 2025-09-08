// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
// Author: Rander Wang <rander.wang@linux.intel.com>
/*
 * IPC (InterProcessor Communication) provides a method of two way
 * communication between the host processor and the DSP. The IPC used here
 * utilises a shared mailbox and door bell between the host and DSP.
 *
 */

#include <sof/audio/buffer.h>
#include <sof/audio/component_ext.h>
#include <sof/audio/pipeline.h>
#include <sof/boot_test.h>
#include <sof/common.h>
#include <sof/ipc/topology.h>
#include <sof/ipc/common.h>
#include <sof/ipc/msg.h>
#include <sof/ipc/driver.h>
#include <sof/lib/mailbox.h>
#include <sof/lib/memory.h>
#include <sof/lib/pm_runtime.h>
#include <sof/llext_manager.h>
#include <sof/math/numbers.h>
#include <sof/tlv.h>
#include <sof/trace/trace.h>
#include <ipc4/error_status.h>
#include <ipc/header.h>
#include <ipc4/module.h>
#include <ipc4/pipeline.h>
#include <ipc4/notification.h>
#include <ipc/trace.h>
#include <user/trace.h>

#include <rtos/atomic.h>
#include <rtos/kernel.h>
#include <sof/lib_manager.h>

#if CONFIG_SOF_BOOT_TEST
/* CONFIG_SOF_BOOT_TEST depends on Zephyr */
#include <zephyr/ztest.h>
#endif

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../audio/copier/ipcgtw_copier.h"

/* Command format errors during fuzzing are reported for virtually all
 * commands, and the resulting flood of logging becomes a severe
 * performance penalty (i.e. we get a lot less fuzzing done per CPU
 * cycle).
 */
#ifdef CONFIG_ARCH_POSIX_LIBFUZZER
#define ipc_cmd_err(...)
#else
#define ipc_cmd_err(...) tr_err(__VA_ARGS__)
#endif

LOG_MODULE_DECLARE(ipc, CONFIG_SOF_LOG_LEVEL);

struct ipc4_msg_data {
	struct ipc_cmd_hdr msg_in; /* local copy of current message from host header */
	struct ipc_cmd_hdr msg_out; /* local copy of current message to host header */
	atomic_t delayed_reply;
	uint32_t delayed_error;
};

static struct ipc4_msg_data msg_data;

/* fw sends a fw ipc message to send the status of the last host ipc message */
static struct ipc_msg msg_reply = {0, 0, 0, 0, LIST_INIT(msg_reply.list)};

static struct ipc_msg msg_notify = {0, 0, 0, 0, LIST_INIT(msg_notify.list)};

#if CONFIG_LIBRARY
static inline struct ipc4_message_request *ipc4_get_message_request(void)
{
	struct ipc *ipc = ipc_get();

	return (struct ipc4_message_request *)ipc->comp_data;
}

static inline void ipc4_send_reply(struct ipc4_message_reply *reply)
{
	struct ipc *ipc = ipc_get();

	/* copy the extension from the message reply */
	reply->extension.dat = msg_reply.extension;
	memcpy((char *)ipc->comp_data, reply, sizeof(*reply));
}

static inline const struct ipc4_pipeline_set_state_data *ipc4_get_pipeline_data(void)
{
	const struct ipc4_pipeline_set_state_data *ppl_data;
	struct ipc *ipc = ipc_get();

	ppl_data = (const struct ipc4_pipeline_set_state_data *)ipc->comp_data;

	return ppl_data;
}
#else
static inline struct ipc4_message_request *ipc4_get_message_request(void)
{
	/* ignoring _hdr as it does not contain valid data in IPC4/IDC case */
	return ipc_from_hdr(&msg_data.msg_in);
}

static inline void ipc4_send_reply(struct ipc4_message_reply *reply)
{
	struct ipc *ipc = ipc_get();
	char *data = ipc->comp_data;

	ipc_msg_send(&msg_reply, data, true);
}

#endif

__cold static bool is_any_ppl_active(void)
{
	struct ipc_comp_dev *icd;
	struct list_item *clist;

	assert_can_be_cold();

	list_for_item(clist, &ipc_get()->comp_list) {
		icd = container_of(clist, struct ipc_comp_dev, list);
		if (icd->type != COMP_TYPE_PIPELINE)
			continue;

		if (icd->pipeline->status == COMP_STATE_ACTIVE)
			return true;
	}

	return false;
}

void ipc_compound_pre_start(int msg_id)
{
	/* ipc thread will wait for all scheduled tasks to be complete
	 * Use a reference count to check status of these tasks.
	 */
	atomic_add(&msg_data.delayed_reply, 1);
}

void ipc_compound_post_start(uint32_t msg_id, int ret, bool delayed)
{
	if (ret) {
		ipc_cmd_err(&ipc_tr, "failed to process msg %d status %d", msg_id, ret);
		atomic_set(&msg_data.delayed_reply, 0);
		return;
	}

	/* decrease counter if it is not scheduled by another thread */
	if (!delayed)
		atomic_sub(&msg_data.delayed_reply, 1);
}

void ipc_compound_msg_done(uint32_t msg_id, int error)
{
	if (!atomic_read(&msg_data.delayed_reply)) {
		ipc_cmd_err(&ipc_tr, "unexpected delayed reply");
		return;
	}

	atomic_sub(&msg_data.delayed_reply, 1);

	/* error reported in delayed pipeline task */
	if (error < 0) {
		if (msg_id == SOF_IPC4_GLB_SET_PIPELINE_STATE)
			msg_data.delayed_error = IPC4_PIPELINE_STATE_NOT_SET;
	}
}

#if CONFIG_LIBRARY
/* There is no parallel execution in testbench for scheduler and pipelines, so the result would
 * be always IPC4_FAILURE. Therefore the compound messages handling is simplified. The pipeline
 * triggers will require an explicit scheduler call to get the components to desired state.
 */
int ipc_wait_for_compound_msg(void)
{
	atomic_set(&msg_data.delayed_reply, 0);
	return IPC4_SUCCESS;
}
#else
int ipc_wait_for_compound_msg(void)
{
	int try_count = 30;

	while (atomic_read(&msg_data.delayed_reply)) {
		k_sleep(Z_TIMEOUT_US(250));

		if (!try_count--) {
			atomic_set(&msg_data.delayed_reply, 0);
			ipc_cmd_err(&ipc_tr, "ipc4: failed to wait schedule thread");
			return IPC4_FAILURE;
		}
	}

	return IPC4_SUCCESS;
}
#endif

#if CONFIG_LIBRARY_MANAGER
__cold static int ipc4_load_library(struct ipc4_message_request *ipc4)
{
	struct ipc4_module_load_library library;
	int ret;

	assert_can_be_cold();

	library.header.dat = ipc4->primary.dat;

	ret = lib_manager_load_library(library.header.r.dma_id, library.header.r.lib_id,
				       ipc4->primary.r.type);
	if (ret != 0)
		return (ret == -EINVAL) ? IPC4_ERROR_INVALID_PARAM : IPC4_FAILURE;

	return IPC4_SUCCESS;
}
#endif

static int ipc4_process_glb_message(struct ipc4_message_request *ipc4)
{
	uint32_t type;
	int ret;

	type = ipc4->primary.r.type;

	switch (type) {

	/* Loads library (using Code Load or HD/A Host Output DMA) */
#ifdef CONFIG_LIBRARY_MANAGER
	case SOF_IPC4_GLB_LOAD_LIBRARY:
		ret = ipc4_load_library(ipc4);
		break;
	case SOF_IPC4_GLB_LOAD_LIBRARY_PREPARE:
		ret = ipc4_load_library(ipc4);
		break;
#endif
	/* not a kernel level IPC message */
	default:
		/* try and handle as a user IPC message */
		ret = ipc4_user_process_glb_message(ipc4, &msg_reply);
		break;
	}

	return ret;
}

/* disable power gating on core 0 */
__cold static int ipc4_module_process_d0ix(struct ipc4_message_request *ipc4)
{
	struct ipc4_module_set_d0ix d0ix;
	uint32_t module_id, instance_id;

	assert_can_be_cold();

	int ret = memcpy_s(&d0ix, sizeof(d0ix), ipc4, sizeof(*ipc4));

	if (ret < 0)
		return IPC4_FAILURE;

	module_id = d0ix.primary.r.module_id;
	instance_id = d0ix.primary.r.instance_id;

	tr_dbg(&ipc_tr, "ipc4_module_process_d0ix %x : %x", module_id, instance_id);

	/* only module 0 can be used to set d0ix state */
	if (d0ix.primary.r.module_id || d0ix.primary.r.instance_id) {
		ipc_cmd_err(&ipc_tr, "invalid resource id %x : %x", module_id, instance_id);
		return IPC4_INVALID_RESOURCE_ID;
	}

	if (d0ix.extension.r.prevent_power_gating)
		pm_runtime_disable(PM_RUNTIME_DSP, PLATFORM_PRIMARY_CORE_ID);
	else
		pm_runtime_enable(PM_RUNTIME_DSP, PLATFORM_PRIMARY_CORE_ID);

	return 0;
}

/* enable/disable cores according to the state mask */
__cold static int ipc4_module_process_dx(struct ipc4_message_request *ipc4)
{
	struct ipc4_module_set_dx dx;
	struct ipc4_dx_state_info dx_info;
	uint32_t module_id, instance_id;
	uint32_t core_id;

	assert_can_be_cold();

	int ret = memcpy_s(&dx, sizeof(dx), ipc4, sizeof(*ipc4));

	if (ret < 0)
		return IPC4_FAILURE;

	module_id = dx.primary.r.module_id;
	instance_id = dx.primary.r.instance_id;

	/* only module 0 can be used to set dx state */
	if (module_id || instance_id) {
		ipc_cmd_err(&ipc_tr, "invalid resource id %x : %x", module_id, instance_id);
		return IPC4_INVALID_RESOURCE_ID;
	}

	dcache_invalidate_region((__sparse_force void __sparse_cache *)MAILBOX_HOSTBOX_BASE,
				 sizeof(dx_info));
	ret = memcpy_s(&dx_info, sizeof(dx_info),
		       (const void *)MAILBOX_HOSTBOX_BASE, sizeof(dx_info));
	if (ret < 0)
		return IPC4_FAILURE;

	/* check if core enable mask is valid */
	if (dx_info.core_mask > MASK(CONFIG_CORE_COUNT - 1, 0)) {
		ipc_cmd_err(&ipc_tr, "ipc4_module_process_dx: CONFIG_CORE_COUNT: %d < core enable mask: %d",
			    CONFIG_CORE_COUNT, dx_info.core_mask);
		return IPC4_ERROR_INVALID_PARAM;
	}

	/* check primary core first */
	if ((dx_info.core_mask & BIT(PLATFORM_PRIMARY_CORE_ID)) &&
	    (dx_info.dx_mask & BIT(PLATFORM_PRIMARY_CORE_ID))) {
		/* core0 can't be activated more, it's already active since we got here */
		ipc_cmd_err(&ipc_tr, "Core0 is already active");
		return IPC4_BAD_STATE;
	}

	/* Activate/deactivate requested cores */
	for (core_id = 1; core_id < CONFIG_CORE_COUNT; core_id++) {
		if ((dx_info.core_mask & BIT(core_id)) == 0)
			continue;

		if (dx_info.dx_mask & BIT(core_id)) {
			ret = cpu_enable_core(core_id);
			if (ret != 0) {
				ipc_cmd_err(&ipc_tr, "failed to enable core %d", core_id);
				return IPC4_FAILURE;
			}
		} else {
			cpu_disable_core(core_id);
			if (cpu_is_core_enabled(core_id)) {
				ipc_cmd_err(&ipc_tr, "failed to disable core %d", core_id);
				return IPC4_FAILURE;
			}
		}
	}

	/* Deactivating primary core if requested.  */
	if (dx_info.core_mask & BIT(PLATFORM_PRIMARY_CORE_ID)) {
		if (cpu_enabled_cores() & ~BIT(PLATFORM_PRIMARY_CORE_ID)) {
			ipc_cmd_err(&ipc_tr, "secondary cores 0x%x still active",
				    cpu_enabled_cores());
			return IPC4_BUSY;
		}

		if (is_any_ppl_active()) {
			ipc_cmd_err(&ipc_tr, "some pipelines are still active");
			return IPC4_BUSY;
		}

#if !CONFIG_ADSP_IMR_CONTEXT_SAVE
		ret = llext_manager_store_to_dram();
		if (ret < 0)
			ipc_cmd_err(&ipc_tr, "Error %d saving LLEXT context. Resume might fail.",
				    ret);

#if CONFIG_L3_HEAP
		l3_heap_save();
#endif
#endif

#if defined(CONFIG_PM)
		ipc_get()->task_mask |= IPC_TASK_POWERDOWN;
#endif
		/* do platform specific suspending */
		platform_context_save(sof_get());

#if !defined(CONFIG_LIBRARY) && !defined(CONFIG_ZEPHYR_NATIVE_DRIVERS)
		arch_irq_lock();
		platform_timer_stop(timer_get());
#endif
		ipc_get()->pm_prepare_D3 = 1;
	}

	return IPC4_SUCCESS;
}

__cold static int ipc4_process_module_message(struct ipc4_message_request *ipc4)
{
	uint32_t type;
	int ret;

	assert_can_be_cold();

	type = ipc4->primary.r.type;

	switch (type) {
	case SOF_IPC4_MOD_SET_D0IX:
		ret = ipc4_module_process_d0ix(ipc4);
		break;
	case SOF_IPC4_MOD_SET_DX:
		ret = ipc4_module_process_dx(ipc4);
		break;
	default:
		/* try and handle as a user IPC message */
		ret = ipc4_user_process_module_message(ipc4, &msg_reply);
		break;
	}

	return ret;
}

__cold struct ipc_cmd_hdr *mailbox_validate(void)
{
	struct ipc_cmd_hdr *hdr = ipc_get()->comp_data;

	assert_can_be_cold();

	return hdr;
}

struct ipc_cmd_hdr *ipc_compact_read_msg(void)
{
	int words;

	words = ipc_platform_compact_read_msg(&msg_data.msg_in, 2);
	if (!words)
		return mailbox_validate();

	return &msg_data.msg_in;
}

struct ipc_cmd_hdr *ipc_prepare_to_send(const struct ipc_msg *msg)
{
	msg_data.msg_out.pri = msg->header;
	msg_data.msg_out.ext = msg->extension;

	if (msg->tx_size)
		mailbox_dspbox_write(0, (uint32_t *)msg->tx_data, msg->tx_size);

	return &msg_data.msg_out;
}

__cold void ipc_boot_complete_msg(struct ipc_cmd_hdr *header, uint32_t data)
{
	assert_can_be_cold();

	header->pri = SOF_IPC4_FW_READY;
	header->ext = 0;
}

#if defined(CONFIG_PM_DEVICE) && defined(CONFIG_INTEL_ADSP_IPC)
__cold void ipc_send_failed_power_transition_response(void)
{
	struct ipc4_message_request *request = ipc_from_hdr(&msg_data.msg_in);
	struct ipc4_message_reply response;

	assert_can_be_cold();

	response.primary.r.status = IPC4_POWER_TRANSITION_FAILED;
	response.primary.r.rsp = SOF_IPC4_MESSAGE_DIR_MSG_REPLY;
	response.primary.r.msg_tgt = request->primary.r.msg_tgt;
	response.primary.r.type = request->primary.r.type;

	msg_reply.header = response.primary.dat;
	list_init(&msg_reply.list);

	ipc_msg_send_direct(&msg_reply, NULL);
}
#endif /* defined(CONFIG_PM_DEVICE) && defined(CONFIG_INTEL_ADSP_IPC) */

__cold void ipc_send_panic_notification(void)
{
	assert_can_be_cold();

	msg_notify.header = SOF_IPC4_NOTIF_HEADER(SOF_IPC4_EXCEPTION_CAUGHT);
	msg_notify.extension = cpu_get_id();
	msg_notify.tx_size = 0;
	msg_notify.tx_data = NULL;
	list_init(&msg_notify.list);

	ipc_msg_send_direct(&msg_notify, NULL);
}

#ifdef CONFIG_LOG_BACKEND_ADSP_MTRACE

static bool is_notification_queued(struct ipc_msg *msg)
{
	struct ipc *ipc = ipc_get();
	k_spinlock_key_t key;
	bool queued = false;

	key = k_spin_lock(&ipc->lock);
	if (!list_is_empty(&msg->list))
		queued = true;
	k_spin_unlock(&ipc->lock, key);

	return queued;
}

/* Called from ipc_send_buffer_status_notify(), which is currently "hot" */
void ipc_send_buffer_status_notify(void)
{
	/* a single msg_notify object is used */
	if (is_notification_queued(&msg_notify))
		return;

	msg_notify.header = SOF_IPC4_NOTIF_HEADER(SOF_IPC4_NOTIFY_LOG_BUFFER_STATUS);
	msg_notify.extension = 0;
	msg_notify.tx_size = 0;

	tr_dbg(&ipc_tr, "tx-notify\t: %#x|%#x", msg_notify.header, msg_notify.extension);

	ipc_msg_send(&msg_notify, NULL, true);
}
#endif

void ipc_msg_reply(struct sof_ipc_reply *reply)
{
	struct ipc4_message_request in;

	in.primary.dat = msg_data.msg_in.pri;
	ipc_compound_msg_done(in.primary.r.type, reply->error);
}

void ipc_cmd(struct ipc_cmd_hdr *_hdr)
{
	struct ipc4_message_request *in = ipc4_get_message_request();
	enum ipc4_message_target target;
#ifdef CONFIG_DEBUG_IPC_TIMINGS
	struct ipc4_message_request req;
	uint64_t tstamp;
#endif
	int err;

#ifdef CONFIG_DEBUG_IPC_TIMINGS
	req = *in;
	tstamp = sof_cycle_get_64();
#else
	if (cpu_is_primary(cpu_get_id()))
		tr_info(&ipc_tr, "rx\t: %#x|%#x", in->primary.dat, in->extension.dat);
#endif
	/* no process on scheduled thread */
	atomic_set(&msg_data.delayed_reply, 0);
	msg_data.delayed_error = 0;
	msg_reply.tx_data = NULL;
	msg_reply.tx_size = 0;
	msg_reply.header = in->primary.dat;
	msg_reply.extension = in->extension.dat;

	target = in->primary.r.msg_tgt;

	switch (target) {
	case SOF_IPC4_MESSAGE_TARGET_FW_GEN_MSG:
		err = ipc4_process_glb_message(in);
		if (err)
			ipc_cmd_err(&ipc_tr, "ipc4: FW_GEN_MSG failed with err %d", err);
		break;
	case SOF_IPC4_MESSAGE_TARGET_MODULE_MSG:
		err = ipc4_process_module_message(in);
		if (err)
			ipc_cmd_err(&ipc_tr, "ipc4: MODULE_MSG failed with err %d", err);
		break;
	default:
		/* should not reach here as we only have 2 message types */
		ipc_cmd_err(&ipc_tr, "ipc4: invalid target %d", target);
		err = IPC4_UNKNOWN_MESSAGE_TYPE;
	}

	/* FW sends an ipc message to host if request bit is clear */
	if (in->primary.r.rsp == SOF_IPC4_MESSAGE_DIR_MSG_REQUEST) {
		struct ipc *ipc = ipc_get();
		struct ipc4_message_reply reply = {{0}};

		/* Process flow and time stamp for IPC4 msg processed on secondary core :
		 * core 0 (primary core)				core x (secondary core)
		 * # IPC msg thread		#IPC delayed worker     #core x idc thread
		 * ipc_task_ops.run()
		 * ipc_do_cmd()
		 * msg_reply.header = in->primary.dat
		 * ipc4_process_on_core(x)
		 * mask |= SECONDARY_CORE
		 * idc_send_message()
		 * Case 1:
		 * // Ipc msg processed by secondary core		idc_ipc()
		 * if ((mask & SECONDARY_CORE))				ipc_cmd()
		 *	return;						ipc_msg_send()
		 *							mask &= ~SECONDARY_CORE
		 *
		 *				ipc_platform_send_msg
		 * ----------------------------------------------------------------------------
		 * Case 2:
		 *                                                      idc_ipc()
		 *                                                      ipc_cmd()
		 *							//Prepare reply msg
		 *                                                      msg_reply.header =
		 *                                                      reply.primary.dat;
		 *                                                      ipc_msg_send()
		 *                                                      mask &= ~SECONDARY_CORE
		 *
		 * if ((mask & IPC_TASK_SECONDARY_CORE))
		 *	return;
		 * // Ipc reply msg was prepared, so return
		 * if (msg_reply.header != in->primary.dat)
		 *	return;
		 *				ipc_platform_send_msg
		 * ----------------------------------------------------------------------------
		 * Case 3:
		 *                                                      idc_ipc()
		 *                                                      ipc_cmd()
		 *							//Prepare reply msg
		 *                                                      msg_reply.header =
		 *                                                      reply.primary.dat;
		 *                                                      ipc_msg_send()
		 *                                                      mask &= ~SECONDARY_CORE
		 *
		 *                              ipc_platform_send_msg
		 *
		 * if ((mask & IPC_TASK_SECONDARY_CORE))
		 *      return;
		 * // Ipc reply msg was prepared, so return
		 * if (msg_reply.header != in->primary.dat)
		 *	return;
		 */

		/* Reply prepared by secondary core */
		if ((ipc->task_mask & IPC_TASK_SECONDARY_CORE) && cpu_is_primary(cpu_get_id()))
			return;
		/* Reply has been prepared by secondary core */
		if (msg_reply.header != in->primary.dat)
			return;

		/* Do not send reply for SET_DX if we are going to enter D3
		 * The reply is going to be sent as part of the power down
		 * sequence
		 */
		if (ipc->task_mask & IPC_TASK_POWERDOWN)
			return;

		if (ipc_wait_for_compound_msg() != 0) {
			ipc_cmd_err(&ipc_tr, "ipc4: failed to send delayed reply");
			err = IPC4_FAILURE;
		}

		/* copy contents of message received */
		reply.primary.r.rsp = SOF_IPC4_MESSAGE_DIR_MSG_REPLY;
		reply.primary.r.msg_tgt = in->primary.r.msg_tgt;
		reply.primary.r.type = in->primary.r.type;
		if (msg_data.delayed_error)
			reply.primary.r.status = msg_data.delayed_error;
		else
			reply.primary.r.status = err;

		msg_reply.header = reply.primary.dat;

#ifdef CONFIG_DEBUG_IPC_TIMINGS
		tr_info(&ipc_tr, "tx-reply\t: %#x|%#x to %#x|%#x in %llu us", msg_reply.header,
			msg_reply.extension, req.primary.dat, req.extension.dat,
			k_cyc_to_us_near64(sof_cycle_get_64() - tstamp));
#else
		tr_dbg(&ipc_tr, "tx-reply\t: %#x|%#x", msg_reply.header,
		       msg_reply.extension);
#endif
		ipc4_send_reply(&reply);
	}
}
