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

#include <sof/debug/telemetry/performance_monitor.h>

LOG_MODULE_REGISTER(idc, CONFIG_SOF_LOG_LEVEL);

/** \brief IDC message payload per core. */
static SHARED_DATA struct idc_payload static_payload[CONFIG_CORE_COUNT];

SOF_DEFINE_REG_UUID(idc);

DECLARE_TR_CTX(idc_tr, SOF_UUID(idc_uuid), LOG_LEVEL_INFO);

SOF_DEFINE_REG_UUID(idc_task);

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

	return *(uint32_t *)payload->data;
}

/**
 * \brief Executes IDC IPC processing message.
 */
static void idc_ipc(void)
{
	struct ipc *ipc = ipc_get();

	ipc_cmd(ipc->comp_data);
}

static int idc_ipc4_bind(uint32_t comp_id)
{
	struct ipc_comp_dev *ipc_dev;
	struct idc_payload *payload;
	struct bind_info *bu;

	ipc_dev = ipc_get_comp_by_id(ipc_get(), comp_id);
	if (!ipc_dev)
		return -ENODEV;

	payload = idc_payload_get(*idc_get(), cpu_get_id());
	bu = (struct bind_info *)payload;

	return comp_bind(ipc_dev->cd, bu);
}

static int idc_ipc4_unbind(uint32_t comp_id)
{
	struct ipc_comp_dev *ipc_dev;
	struct idc_payload *payload;
	struct bind_info *bu;

	ipc_dev = ipc_get_comp_by_id(ipc_get(), comp_id);
	if (!ipc_dev)
		return -ENODEV;

	payload = idc_payload_get(*idc_get(), cpu_get_id());
	bu = (struct bind_info *)payload;

	return comp_unbind(ipc_dev->cd, bu);
}

static int idc_get_attribute(uint32_t comp_id)
{
	struct ipc_comp_dev *ipc_dev;
	struct idc_payload *idc_payload;
	struct get_attribute_remote_payload *get_attr_payload;

	ipc_dev = ipc_get_comp_by_id(ipc_get(), comp_id);
	if (!ipc_dev)
		return -ENODEV;

	idc_payload = idc_payload_get(*idc_get(), cpu_get_id());
	get_attr_payload = (struct get_attribute_remote_payload *)idc_payload;

	return comp_get_attribute(ipc_dev->cd, get_attr_payload->type, get_attr_payload->value);
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

	/* we're running LL on different core, so allocate our own task */
	if (!dev->task && dev->ipc_config.proc_domain == COMP_PROCESSING_DOMAIN_LL) {
		/* allocate task for shared component */
		dev->task = rzalloc(SOF_MEM_FLAG_USER,
				    sizeof(*dev->task));
		if (!dev->task) {
			ret = -ENOMEM;
			goto out;
		}

		ret = schedule_task_init_ll(dev->task,
					    SOF_UUID(idc_task_uuid),
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
 * \brief Executes IDC component reset message.
 * \param[in] comp_id Component id to be reset.
 * \return Error code.
 */
static int idc_comp_free(uint32_t comp_id)
{
	struct ipc *ipc = ipc_get();
	struct ipc_comp_dev *icd;
	int ret;

	icd = ipc_get_comp_by_id(ipc, comp_id);
	if (!icd)
		return -ENODEV;

	ret = ipc_comp_free(ipc, comp_id);
	return ret;
}

/**
 * \brief Executes IDC pipeline set state message.
 * \param[in] ppl_id Pipeline id to be triggered.
 * \return Error code.
 */
static int idc_ppl_state(uint32_t ppl_id, uint32_t phase)
{
	struct ipc *ipc = ipc_get();
	struct idc *idc = *idc_get();
	struct idc_payload *payload = idc_payload_get(idc, cpu_get_id());
	struct ipc_comp_dev *ppl_icd;
	uint32_t cmd = *(uint32_t *)payload;

	ppl_icd = ipc_get_comp_by_ppl_id(ipc, COMP_TYPE_PIPELINE, ppl_id, IPC_COMP_IGNORE_REMOTE);
	if (!ppl_icd) {
		tr_err(&idc_tr, "idc: comp %d not found", ppl_id);
		return IPC4_INVALID_RESOURCE_ID;
	}

	/* if no phase specified, correct it to be a ONESHOT */
	if (!phase)
		phase = IDC_PPL_STATE_PHASE_ONESHOT;

	if (phase & IDC_PPL_STATE_PHASE_PREPARE) {
		int ret;

		ret = ipc4_pipeline_prepare(ppl_icd, cmd);
		if (ret)
			return ret;
	}

	if (phase & IDC_PPL_STATE_PHASE_TRIGGER) {
		bool delayed = false;

		return ipc4_pipeline_trigger(ppl_icd, cmd, &delayed);
	}

	return 0;
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
	tr_err(&idc_tr, "AMS not enabled");
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
		dbg_path_hot_start_watching();
		idc_ipc();
		dbg_path_hot_stop_watching();
		break;
	case iTS(IDC_MSG_BIND):
		ret = idc_ipc4_bind(msg->extension);
		break;
	case iTS(IDC_MSG_UNBIND):
		ret = idc_ipc4_unbind(msg->extension);
		break;
	case iTS(IDC_MSG_GET_ATTRIBUTE):
		ret = idc_get_attribute(msg->extension);
		break;
	case iTS(IDC_MSG_FREE):
		ret = idc_comp_free(msg->extension);
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
		ret = idc_ppl_state(msg->extension & IDC_PPL_STATE_PPL_ID_MASK,
				    IDC_PPL_STATE_PHASE_GET(msg->extension));
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
		tr_err(&idc_tr, "invalid msg->header = %u",
		       msg->header);
	}

	idc_msg_status_set(ret, cpu_get_id());
}

/* Runs on each CPU */
int idc_init(void)
{
	struct idc **idc = idc_get();

	tr_dbg(&idc_tr, "entry");

	/* initialize idc data */
	(*idc)->payload = platform_shared_get(static_payload, sizeof(static_payload));

#ifdef CONFIG_SOF_TELEMETRY_IO_PERFORMANCE_MEASUREMENTS
	struct io_perf_data_item init_data = {IO_PERF_IDC_ID,
					      cpu_get_id(),
					      IO_PERF_INPUT_DIRECTION,
					      IO_PERF_POWERED_UP_ENABLED,
					      IO_PERF_D0IX_POWER_MODE,
					      0, 0, 0 };
	io_perf_monitor_init_data(&(*idc)->io_perf_in_msg_count, &init_data);
	init_data.direction = IO_PERF_OUTPUT_DIRECTION;
	io_perf_monitor_init_data(&(*idc)->io_perf_out_msg_count, &init_data);
#endif

	/* process task */
	idc_init_thread();

	return 0;
}

int idc_restore(void)
{
	struct idc **idc __unused = idc_get();

	tr_info(&idc_tr, "entry");

	/* idc_restore() is invoked during D0->D0ix/D0ix->D0 flow. In that
	 * case basic core structures e.g. idc struct should be already
	 * allocated (in D0->D0ix primary core disables all secondary cores, but
	 * memory has not been powered off).
	 */
	assert(*idc);

	return 0;
}
