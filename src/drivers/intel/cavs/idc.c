// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>

#include <sof/audio/component.h>
#include <sof/audio/pipeline.h>
#include <sof/debug/panic.h>
#include <sof/drivers/idc.h>
#include <sof/drivers/interrupt.h>
#include <sof/drivers/ipc.h>
#include <sof/drivers/timer.h>
#include <sof/lib/alloc.h>
#include <sof/lib/cache.h>
#include <sof/lib/clk.h>
#include <sof/lib/cpu.h>
#include <sof/lib/memory.h>
#include <sof/lib/notifier.h>
#include <sof/lib/shim.h>
#include <sof/platform.h>
#include <sof/schedule/edf_schedule.h>
#include <sof/schedule/ll_schedule.h>
#include <sof/schedule/schedule.h>
#include <sof/schedule/task.h>
#include <ipc/control.h>
#include <ipc/stream.h>
#include <ipc/topology.h>
#include <xtos-structs.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** \brief IDC message payload per core. */
static SHARED_DATA struct idc_payload payload[PLATFORM_CORE_COUNT];

/**
 * \brief Returns IDC data.
 * \return Pointer to pointer of IDC data.
 */
static struct idc **idc_get(void)
{
	struct core_context *ctx = (struct core_context *)cpu_read_threadptr();

	return &ctx->idc;
}

/**
 * \brief Enables IDC interrupts.
 * \param[in] target_core Target core id.
 * \param[in] source_core Source core id.
 */
void idc_enable_interrupts(int target_core, int source_core)
{
	struct idc *idc = *idc_get();

	idc_write(IPC_IDCCTL, target_core,
		  IPC_IDCCTL_IDCTBIE(source_core));
	interrupt_unmask(idc->irq, target_core);
}

/**
 * \brief IDC interrupt handler.
 * \param[in,out] arg Pointer to IDC data.
 */
static void idc_irq_handler(void *arg)
{
	struct idc *idc = arg;
	int core = cpu_get_id();
	uint32_t idctfc;
	uint32_t idctefc;
	uint32_t idcietc;
	uint32_t i;

	tracev_idc("idc_irq_handler()");

	for (i = 0; i < PLATFORM_CORE_COUNT; i++) {
		/* skip current core */
		if (core == i)
			continue;

		idctfc = idc_read(IPC_IDCTFC(i), core);

		if (idctfc & IPC_IDCTFC_BUSY) {
			trace_idc("idc_irq_handler(), IPC_IDCTFC_BUSY");

			/* disable BUSY interrupt */
			idc_write(IPC_IDCCTL, core, idc->done_bit_mask);

			idc->received_msg.core = i;
			idc->received_msg.header =
					idctfc & IPC_IDCTFC_MSG_MASK;

			idctefc = idc_read(IPC_IDCTEFC(i), core);
			idc->received_msg.extension =
					idctefc & IPC_IDCTEFC_MSG_MASK;

			schedule_task(&idc->idc_task, 0, IDC_DEADLINE);
		}
	}

	for (i = 0; i < PLATFORM_CORE_COUNT; i++) {
		/* skip current core */
		if (core == i)
			continue;

		idcietc = idc_read(IPC_IDCIETC(i), core);

		if (idcietc & IPC_IDCIETC_DONE) {
			tracev_idc("idc_irq_handler(), "
				   "IPC_IDCIETC_DONE");

			idc_write(IPC_IDCIETC(i), core,
				  idcietc | IPC_IDCIETC_DONE);

			idc->msg_processed[i] = true;
		}
	}
}

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

	platform_shared_commit(payload, sizeof(*payload));
}

/**
 * \brief Retrieves IDC command status after sending message.
 * \param[in] core Id of the core for this status.
 * \return Last IDC message status.
 */
static int idc_msg_status_get(uint32_t core)
{
	struct idc *idc = *idc_get();
	struct idc_payload *payload = idc_payload_get(idc, core);
	int status;

	status = *(uint32_t *)payload->data;

	platform_shared_commit(payload, sizeof(*payload));

	return status;
}

/**
 * \brief Sends IDC message.
 * \param[in,out] msg Pointer to IDC message.
 * \param[in] mode Is message blocking or not.
 * \return Error code.
 */
int idc_send_msg(struct idc_msg *msg, uint32_t mode)
{
	struct timer *timer = timer_get();
	struct idc *idc = *idc_get();
	struct idc_payload *payload = idc_payload_get(idc, msg->core);
	int core = cpu_get_id();
	uint64_t deadline;
	int ret = 0;

	tracev_idc("arch_idc_send_msg()");

	idc->msg_processed[msg->core] = false;

	/* copy payload if available */
	if (msg->payload) {
		ret = memcpy_s(payload->data, IDC_MAX_PAYLOAD_SIZE,
			       msg->payload, msg->size);
		assert(!ret);
		platform_shared_commit(payload, sizeof(*payload));
	}

	idc_write(IPC_IDCIETC(msg->core), core, msg->extension);
	idc_write(IPC_IDCITC(msg->core), core, msg->header | IPC_IDCITC_BUSY);

	if (mode == IDC_BLOCKING) {
		deadline = platform_timer_get(timer) +
			clock_ms_to_ticks(PLATFORM_DEFAULT_CLOCK, 1) *
			IDC_TIMEOUT / 1000;

		while (!idc->msg_processed[msg->core]) {
			if (deadline < platform_timer_get(timer)) {
				/* safe check in case we've got preempted
				 * after read
				 */
				if (idc->msg_processed[msg->core])
					break;

				trace_idc_error("arch_idc_send_msg() error: "
						"timeout");
				return -ETIME;
			}
		}

		ret = idc_msg_status_get(msg->core);
	}

	return ret;
}

/**
 * \brief Executes IDC IPC processing message.
 */
static void idc_ipc(void)
{
	struct ipc *ipc = ipc_get();
	struct sof_ipc_cmd_hdr *hdr = ipc->comp_data;

	ipc_cmd(hdr);
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

	platform_shared_commit(payload, sizeof(*payload));
	platform_shared_commit(ipc_dev, sizeof(*ipc_dev));
	platform_shared_commit(ipc, sizeof(*ipc));

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

		ret = schedule_task_init_ll(dev->task, SOF_SCHEDULE_LL_TIMER,
					    dev->priority, comp_task, dev,
					    dev->comp.core, 0);
		if (ret < 0) {
			rfree(dev->task);
			goto out;
		}
	}

	ret = comp_prepare(ipc_dev->cd);

out:
	platform_shared_commit(dev, sizeof(*dev));
	platform_shared_commit(ipc_dev, sizeof(*ipc_dev));
	platform_shared_commit(ipc, sizeof(*ipc));

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
	platform_shared_commit(payload, sizeof(*payload));
	platform_shared_commit(ipc_dev->cd, sizeof(*ipc_dev->cd));
	platform_shared_commit(ipc_dev, sizeof(*ipc_dev));
	platform_shared_commit(ipc, sizeof(*ipc));

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

	platform_shared_commit(ipc_dev, sizeof(*ipc_dev));
	platform_shared_commit(ipc, sizeof(*ipc));

	return ret;
}

/**
 * \brief Executes IDC message based on type.
 * \param[in,out] msg Pointer to IDC message.
 */
static void idc_cmd(struct idc_msg *msg)
{
	uint32_t type = iTS(msg->header);
	int ret = 0;

	switch (type) {
	case iTS(IDC_MSG_POWER_DOWN):
		cpu_power_down_core();
		break;
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
	default:
		trace_idc_error("idc_cmd() error: invalid msg->header = %u",
				msg->header);
	}

	idc_msg_status_set(ret, cpu_get_id());
}

/**
 * \brief Handles received IDC message.
 * \param[in,out] data Pointer to IDC data.
 */
static enum task_state idc_do_cmd(void *data)
{
	struct idc *idc = data;
	int core = cpu_get_id();
	int initiator = idc->received_msg.core;

	trace_idc("idc_do_cmd()");

	idc_cmd(&idc->received_msg);

	/* clear BUSY bit */
	idc_write(IPC_IDCTFC(initiator), core,
		  idc_read(IPC_IDCTFC(initiator), core) | IPC_IDCTFC_BUSY);

	/* enable BUSY interrupt */
	idc_write(IPC_IDCCTL, core, idc->busy_bit_mask | idc->done_bit_mask);

	return SOF_TASK_STATE_COMPLETED;
}

/**
 * \brief Returns BUSY interrupt mask based on core id.
 * \param[in] core Core id.
 * \return BUSY interrupt mask.
 */
static uint32_t idc_get_busy_bit_mask(int core)
{
	uint32_t busy_mask = 0;
	int i;

	for (i = 0; i < PLATFORM_CORE_COUNT; i++) {
		if (i != core)
			busy_mask |= IPC_IDCCTL_IDCTBIE(i);
	}

	return busy_mask;
}

/**
 * \brief Returns DONE interrupt mask based on core id.
 * \param[in] core Core id.
 * \return DONE interrupt mask.
 */
static uint32_t idc_get_done_bit_mask(int core)
{
	uint32_t done_mask = 0;
	int i;

	for (i = 0; i < PLATFORM_CORE_COUNT; i++) {
		if (i != core)
			done_mask |= IPC_IDCCTL_IDCIDIE(i);
	}

	return done_mask;
}

/**
 * \brief Initializes IDC data and registers for interrupt.
 */
int idc_init(void)
{
	int core = cpu_get_id();
	int ret;
	struct task_ops ops = {
		.run = idc_do_cmd,
		.get_deadline = ipc_task_deadline,
	};

	trace_idc("arch_idc_init()");

	/* initialize idc data */
	struct idc **idc = idc_get();
	*idc = rzalloc(SOF_MEM_ZONE_SYS, 0, SOF_MEM_CAPS_RAM, sizeof(**idc));
	(*idc)->busy_bit_mask = idc_get_busy_bit_mask(core);
	(*idc)->done_bit_mask = idc_get_done_bit_mask(core);
	(*idc)->payload = cache_to_uncache((struct idc_payload *)payload);

	/* process task */
	schedule_task_init_edf(&(*idc)->idc_task, &ops, *idc, core, 0);

	/* configure interrupt */
	(*idc)->irq = interrupt_get_irq(PLATFORM_IDC_INTERRUPT,
					PLATFORM_IDC_INTERRUPT_NAME);
	if ((*idc)->irq < 0)
		return (*idc)->irq;
	ret = interrupt_register((*idc)->irq, idc_irq_handler, *idc);
	if (ret < 0)
		return ret;
	interrupt_enable((*idc)->irq, *idc);

	/* enable BUSY and DONE interrupts */
	idc_write(IPC_IDCCTL, core,
		  (*idc)->busy_bit_mask | (*idc)->done_bit_mask);

	return 0;
}

/**
 * \brief Frees IDC data and unregisters interrupt.
 */
void idc_free(void)
{
	struct idc *idc = *idc_get();
	int core = cpu_get_id();
	int i = 0;
	uint32_t idctfc;

	trace_idc("idc_free()");

	/* disable and unregister interrupt */
	interrupt_disable(idc->irq, idc);
	interrupt_unregister(idc->irq, idc);

	/* clear BUSY bits */
	for (i = 0; i < PLATFORM_CORE_COUNT; i++) {
		idctfc = idc_read(IPC_IDCTFC(i), core);
		if (idctfc & IPC_IDCTFC_BUSY)
			idc_write(IPC_IDCTFC(i), core, idctfc);
	}

	schedule_task_free(&idc->idc_task);
}
