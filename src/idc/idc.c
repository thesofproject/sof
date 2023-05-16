// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Intel Corporation. All rights reserved.
//
// Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>

#include <sof/audio/component.h>
#include <sof/audio/component_ext.h>
#include <rtos/idc.h>
#include <sof/ipc/driver.h>
#include <sof/ipc/msg.h>
#include <sof/ipc/topology.h>
#include <sof/ipc/schedule.h>
#include <rtos/timer.h>
#include <rtos/alloc.h>
#include <rtos/clk.h>
#include <sof/lib/cpu.h>
#include <sof/lib/memory.h>
#include <sof/lib/notifier.h>
#include <sof/lib/pm_runtime.h>
#include <sof/lib/uuid.h>
#include <sof/lib/watchdog.h>
#include <sof/platform.h>
#include <rtos/wait.h>
#include <sof/schedule/edf_schedule.h>
#include <sof/schedule/ll_schedule.h>
#include <sof/schedule/schedule.h>
#include <rtos/task.h>
#include <sof/trace/trace.h>
#include <ipc/header.h>
#include <ipc/stream.h>
#include <ipc/topology.h>
#include <errno.h>
#include <stdint.h>
#include <sof/lib/ams.h>

LOG_MODULE_REGISTER(idc, CONFIG_SOF_LOG_LEVEL);

/** \brief IDC message payload per core. */
static SHARED_DATA struct idc_payload static_payload[CONFIG_CORE_COUNT];

/* 379a60ae-cedb-4777-aaf2-5659b0a85735 */
DECLARE_SOF_UUID("idc", idc_uuid, 0x379a60ae, 0xcedb, 0x4777,
		 0xaa, 0xf2, 0x56, 0x59, 0xb0, 0xa8, 0x57, 0x35);

DECLARE_TR_CTX(idc_tr, SOF_UUID(idc_uuid), LOG_LEVEL_INFO);

/* b90f5a4e-5537-4375-a1df-95485472ff9e */
DECLARE_SOF_UUID("comp-task", idc_comp_task_uuid, 0xb90f5a4e, 0x5537, 0x4375,
		 0xa1, 0xdf, 0x95, 0x48, 0x54, 0x72, 0xff, 0x9e);

#ifndef __ZEPHYR__
/* a5dacb0e-88dc-415c-a1b5-3e8df77f1976 */
DECLARE_SOF_UUID("idc-cmd-task", idc_cmd_task_uuid, 0xa5dacb0e, 0x88dc, 0x415c,
		 0xa1, 0xb5, 0x3e, 0x8d, 0xf7, 0x7f, 0x19, 0x76);
#endif

/**
 * \brief Sets IDC command status after execution.
 * \param[in] status Status to be set.
 * \param[in] core Id of the core for this status.
 */
static void idc_msg_status_set(int status, uint32_t core)
{
	struct idc *idc = *idc_get();
	struct idc_payload *payload = idc_payload_get(idc, core);

	*(uint32_t *)payload->data = status;

}

/**
 * \brief Retrieves IDC command status after sending message.
 * \param[in] core Id of the core for this status.
 * \return Last IDC message status.
 */
int idc_msg_status_get(uint32_t core)
{
	struct idc *idc = *idc_get();
	struct idc_payload *payload = idc_payload_get(idc, core);
	int status;

	status = *(uint32_t *)payload->data;


	return status;
}

/**
 * \brief Waits until status condition is true.
 * \param[in] target_core Id of the core receiving the message.
 * \param[in] cond Pointer to condition function.
 * \return Error code.
 */
int idc_wait_in_blocking_mode(uint32_t target_core, bool (*cond)(int))
{
	uint64_t deadline = sof_cycle_get_64() + k_us_to_cyc_ceil64(IDC_TIMEOUT);

	while (!cond(target_core)) {

		/* spin here so other core can access IO and timers freely */
		idelay(8192);

		if (deadline < sof_cycle_get_64())
			break;
	}

	/* safe check in case we've got preempted
	 * after read
	 */
	if (cond(target_core))
		return 0;

	tr_err(&idc_tr, "idc_wait_in_blocking_mode() error: timeout, target_core %u",
	       target_core);
	return -ETIME;
}

/**
 * \brief Executes IDC IPC processing message.
 */
static void idc_ipc(void)
{
	struct ipc *ipc = ipc_get();

	ipc_cmd(ipc->comp_data);
}

/**
 * \brief Executes IDC component params message.
 * \param[in] comp_id Component id to have params set.
 * \return Error code.
 */
static int idc_params(uint32_t comp_id)
{
	struct ipc *ipc = ipc_get();
	struct ipc_comp_dev *ipc_dev;
	struct idc *idc = *idc_get();
	struct idc_payload *payload = idc_payload_get(idc, cpu_get_id());
	struct sof_ipc_stream_params *params =
		(struct sof_ipc_stream_params *)payload;
	int ret;

	ipc_dev = ipc_get_comp_by_id(ipc, comp_id);
	if (!ipc_dev)
		return -ENODEV;

	ret = comp_params(ipc_dev->cd, params);


	return ret;
}

static enum task_state comp_task(void *data)
{
	if (comp_copy(data) < 0)
		return SOF_TASK_STATE_COMPLETED;

	return SOF_TASK_STATE_RESCHEDULE;
}

/**
 * \brief Executes IDC component prepare message.
 * \param[in] comp_id Component id to be prepared.
 * \return Error code.
 */
static int idc_prepare(uint32_t comp_id)
{
	struct ipc *ipc = ipc_get();
	struct ipc_comp_dev *ipc_dev;
	struct comp_dev *dev;
	int ret;

	ipc_dev = ipc_get_comp_by_id(ipc, comp_id);
	if (!ipc_dev)
		return -ENODEV;

	dev = ipc_dev->cd;

	/* we're running on different core, so allocate our own task */
	if (!dev->task) {
		/* allocate task for shared component */
		dev->task = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM,
				    sizeof(*dev->task));
		if (!dev->task) {
			ret = -ENOMEM;
			goto out;
		}

		ret = schedule_task_init_ll(dev->task,
					    SOF_UUID(idc_comp_task_uuid),
					    SOF_SCHEDULE_LL_TIMER,
					    dev->priority, comp_task, dev,
					    dev->ipc_config.core, 0);
		if (ret < 0) {
			rfree(dev->task);
			goto out;
		}
	}

	ret = comp_prepare(ipc_dev->cd);

out:

	return ret;
}

/**
 * \brief Executes IDC component trigger message.
 * \param[in] comp_id Component id to be triggered.
 * \return Error code.
 */
static int idc_trigger(uint32_t comp_id)
{
	struct ipc *ipc = ipc_get();
	struct ipc_comp_dev *ipc_dev;
	struct idc *idc = *idc_get();
	struct idc_payload *payload = idc_payload_get(idc, cpu_get_id());
	uint32_t cmd = *(uint32_t *)payload;
	int ret;

	ipc_dev = ipc_get_comp_by_id(ipc, comp_id);
	if (!ipc_dev)
		return -ENODEV;

	ret = comp_trigger(ipc_dev->cd, cmd);
	if (ret < 0)
		goto out;

	/* schedule or cancel task */
	switch (cmd) {
	case COMP_TRIGGER_START:
	case COMP_TRIGGER_RELEASE:
		schedule_task(ipc_dev->cd->task, 0, ipc_dev->cd->period);
		break;
	case COMP_TRIGGER_XRUN:
	case COMP_TRIGGER_PAUSE:
	case COMP_TRIGGER_STOP:
		schedule_task_cancel(ipc_dev->cd->task);
		break;
	}

out:

	return ret;
}

/**
 * \brief Executes IDC component reset message.
 * \param[in] comp_id Component id to be reset.
 * \return Error code.
 */
static int idc_reset(uint32_t comp_id)
{
	struct ipc *ipc = ipc_get();
	struct ipc_comp_dev *ipc_dev;
	int ret;

	ipc_dev = ipc_get_comp_by_id(ipc, comp_id);
	if (!ipc_dev)
		return -ENODEV;

	ret = comp_reset(ipc_dev->cd);

	return ret;
}

/**
 * \brief Executes IDC pipeline set state message.
 * \param[in] ppl_id Pipeline id to be triggered.
 * \return Error code.
 */
static int idc_ppl_state(uint32_t ppl_id)
{
	struct ipc *ipc = ipc_get();
	struct ipc_comp_dev *ipc_dev;
	struct idc *idc = *idc_get();
	struct idc_payload *payload = idc_payload_get(idc, cpu_get_id());
	struct ipc_comp_dev *ppl_icd;
	uint32_t cmd = *(uint32_t *)payload;
	bool delayed = false;
	int ret;

	ppl_icd = ipc_get_comp_by_ppl_id(ipc, COMP_TYPE_PIPELINE, ppl_id);
	if (!ppl_icd) {
		tr_err(&idc_tr, "idc: comp %d not found", ppl_id);
		return IPC4_INVALID_RESOURCE_ID;
	}

	ret = set_pipeline_state(ppl_icd, cmd, &delayed);

	return ret;
}

static void idc_prepare_d0ix(void)
{
	/* set prepare_d0ix flag, which indicates that in the next
	 * platform_wait_for_interrupt invocation(), core should get ready for
	 * d0ix power down - it is required by D0->D0ix flow, when primary
	 * core disables all secondary cores.
	 */
	platform_pm_runtime_prepare_d0ix_en(cpu_get_id());
}

static void idc_process_async_msg(uint32_t slot)
{
#if CONFIG_AMS
	process_incoming_message(slot);
#else
	tr_err(&idc_tr, "idc_cmd(): AMS not enabled");
#endif
}

/**
 * \brief Handle IDC secondary core crashed message.
 * \param[in] header IDC message header
 */
static void idc_secondary_core_crashed(const uint32_t header)
{
	const uint32_t core = (header >> IDC_SCC_CORE_SHIFT) & IDC_SCC_CORE_MASK;
	const uint32_t reason = (header >> IDC_SCC_REASON_SHIFT) & IDC_SCC_REASON_MASK;

	(void)core;
	switch (reason) {
#if IS_ENABLED(CONFIG_LL_WATCHDOG)
	case IDC_SCC_REASON_WATCHDOG:
		watchdog_secondary_core_timeout(core);
		break;
#endif
	}
}

/**
 * \brief Executes IDC message based on type.
 * \param[in,out] msg Pointer to IDC message.
 */
void idc_cmd(struct idc_msg *msg)
{
	uint32_t type = iTS(msg->header);
	int ret = 0;

	switch (type) {
#ifndef CONFIG_PM
/* In flow with Zephyr PM this IDC is not used.
 * Primary core is forcing OFF state directly via power manager.
 */
	case iTS(IDC_MSG_POWER_DOWN):
		cpu_power_down_core(0);
		break;
#endif
	case iTS(IDC_MSG_NOTIFY):
		notifier_notify_remote();
		break;
	case iTS(IDC_MSG_IPC):
		idc_ipc();
		break;
	case iTS(IDC_MSG_PARAMS):
		ret = idc_params(msg->extension);
		break;
	case iTS(IDC_MSG_PREPARE):
		ret = idc_prepare(msg->extension);
		break;
	case iTS(IDC_MSG_TRIGGER):
		ret = idc_trigger(msg->extension);
		break;
	case iTS(IDC_MSG_RESET):
		ret = idc_reset(msg->extension);
		break;
	case iTS(IDC_MSG_PPL_STATE):
		ret = idc_ppl_state(msg->extension);
		break;
	case iTS(IDC_MSG_PREPARE_D0ix):
		idc_prepare_d0ix();
		break;
	case iTS(IDC_MSG_SECONDARY_CORE_CRASHED):
		idc_secondary_core_crashed(msg->header);
		break;
	case iTS(IDC_MSG_AMS):
		idc_process_async_msg(IDC_HEADER_TO_AMS_SLOT_MASK(msg->header));
		break;
	default:
		tr_err(&idc_tr, "idc_cmd(): invalid msg->header = %u",
		       msg->header);
	}

	idc_msg_status_set(ret, cpu_get_id());
}

#ifndef __ZEPHYR__
static void idc_complete(void *data)
{
	struct ipc *ipc = ipc_get();
	struct idc *idc = data;
	uint32_t type = iTS(idc->received_msg.header);
	k_spinlock_key_t key;

	switch (type) {
	case iTS(IDC_MSG_IPC):
		/* Signal the host */
		key = k_spin_lock(&ipc->lock);
		ipc->task_mask &= ~IPC_TASK_SECONDARY_CORE;
		ipc_complete_cmd(ipc);
		k_spin_unlock(&ipc->lock, key);
	}
}
#endif

/* Runs on each CPU */
int idc_init(void)
{
	struct idc **idc = idc_get();
#ifndef __ZEPHYR__
	struct task_ops ops = {
		.run = idc_do_cmd,
		.get_deadline = ipc_task_deadline,
		.complete = idc_complete,
	};

	*idc = rzalloc(SOF_MEM_ZONE_SYS, 0, SOF_MEM_CAPS_RAM, sizeof(**idc));
#endif

	tr_dbg(&idc_tr, "idc_init()");

	/* initialize idc data */
	(*idc)->payload = platform_shared_get(static_payload, sizeof(static_payload));

	/* process task */
#ifndef __ZEPHYR__
	schedule_task_init_edf(&(*idc)->idc_task, SOF_UUID(idc_cmd_task_uuid),
			       &ops, *idc, cpu_get_id(), 0);

	return platform_idc_init();
#else
	idc_init_thread();

	return 0;
#endif
}

int idc_restore(void)
{
	struct idc **idc __unused = idc_get();

	tr_info(&idc_tr, "idc_restore()");

	/* idc_restore() is invoked during D0->D0ix/D0ix->D0 flow. In that
	 * case basic core structures e.g. idc struct should be already
	 * allocated (in D0->D0ix primary core disables all secondary cores, but
	 * memory has not been powered off).
	 */
	assert(*idc);

#ifndef __ZEPHYR__
	return platform_idc_restore();
#endif

	return 0;
}
