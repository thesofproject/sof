// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>

#include <sof/debug/panic.h>
#include <sof/drivers/idc.h>
#include <sof/drivers/interrupt.h>
#include <sof/lib/cpu.h>
#include <sof/lib/mailbox.h>
#include <sof/lib/shim.h>
#include <sof/platform.h>
#include <sof/schedule/schedule.h>
#include <sof/schedule/task.h>
#include <sof/string.h>
#include <sof/trace/trace.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

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
	uint32_t i;

	tr_dbg(&idc_tr, "idc_irq_handler()");

	for (i = 0; i < CONFIG_CORE_COUNT; i++) {
		/* skip current core */
		if (core == i)
			continue;

		idctfc = idc_read(IPC_IDCTFC(i), core);

		if (idctfc & IPC_IDCTFC_BUSY) {
			tr_info(&idc_tr, "idc_irq_handler(), IPC_IDCTFC_BUSY");

			/* disable BUSY interrupt */
			idc_write(IPC_IDCCTL, core, 0);

			idc->received_msg.core = i;
			idc->received_msg.header =
					idctfc & IPC_IDCTFC_MSG_MASK;

			idctefc = idc_read(IPC_IDCTEFC(i), core);
			idc->received_msg.extension =
					idctefc & IPC_IDCTEFC_MSG_MASK;

			schedule_task(&idc->idc_task, 0, IDC_DEADLINE);
		}
	}
}

/**
 * \brief Checks IDC registers whether message has been received.
 * \param[in] target_core Id of the core receiving the message.
 * \return True if message received, false otherwise.
 */
static bool idc_is_received(int target_core)
{
	return idc_read(IPC_IDCIETC(target_core), cpu_get_id()) &
		IPC_IDCIETC_DONE;
}

/**
 * \brief Checks core status register.
 * \param[in] target_core Id of the core powering up.
 * \return True if core powered up, false otherwise.
 */
static bool idc_is_powered_up(int target_core)
{
	return mailbox_sw_reg_read(PLATFORM_TRACEP_SECONDARY_CORE(target_core)) ==
		TRACE_BOOT_PLATFORM;
}

/**
 * \brief Sends IDC message.
 * \param[in,out] msg Pointer to IDC message.
 * \param[in] mode Is message blocking or not.
 * \return Error code.
 */
int idc_send_msg(struct idc_msg *msg, uint32_t mode)
{
	struct idc *idc = *idc_get();
	struct idc_payload *payload = idc_payload_get(idc, msg->core);
	int core = cpu_get_id();
	uint32_t idcietc;
	int ret = 0;

	tr_dbg(&idc_tr, "arch_idc_send_msg()");

	/* clear any previous messages */
	idcietc = idc_read(IPC_IDCIETC(msg->core), core);
	if (idcietc & IPC_IDCIETC_DONE)
		idc_write(IPC_IDCIETC(msg->core), core, idcietc);

	/* copy payload if available */
	if (msg->payload) {
		ret = memcpy_s(payload->data, IDC_MAX_PAYLOAD_SIZE,
			       msg->payload, msg->size);
		assert(!ret);
	}

	idc_write(IPC_IDCIETC(msg->core), core, msg->extension);
	idc_write(IPC_IDCITC(msg->core), core, msg->header | IPC_IDCITC_BUSY);

	switch (mode) {
	case IDC_BLOCKING:
		ret = idc_wait_in_blocking_mode(msg->core, idc_is_received);
		if (ret < 0)
			return ret;

		idc_write(IPC_IDCIETC(msg->core), core,
			  idc_read(IPC_IDCIETC(msg->core), core) |
			  IPC_IDCIETC_DONE);

		ret = idc_msg_status_get(msg->core);
		break;

	case IDC_POWER_UP:
		ret = idc_wait_in_blocking_mode(msg->core, idc_is_powered_up);
		if (ret < 0) {
			tr_err(&idc_tr, "idc_send_msg(), power up core %d failed, reason 0x%x",
			       msg->core,
			       mailbox_sw_reg_read(PLATFORM_TRACEP_SECONDARY_CORE(msg->core)));
		}
		break;
	}

	return ret;
}

/**
 * \brief Handles received IDC message.
 * \param[in,out] data Pointer to IDC data.
 */
enum task_state idc_do_cmd(void *data)
{
	struct idc *idc = data;
	int core = cpu_get_id();
	int initiator = idc->received_msg.core;

	tr_info(&idc_tr, "idc_do_cmd()");

	idc_cmd(&idc->received_msg);

	/* clear BUSY bit */
	idc_write(IPC_IDCTFC(initiator), core,
		  idc_read(IPC_IDCTFC(initiator), core) | IPC_IDCTFC_BUSY);

	/* enable BUSY interrupt */
	idc_write(IPC_IDCCTL, core, idc->busy_bit_mask);

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

	for (i = 0; i < CONFIG_CORE_COUNT; i++) {
		if (i != core)
			busy_mask |= IPC_IDCCTL_IDCTBIE(i);
	}

	return busy_mask;
}

/**
 * \brief Initializes IDC data and registers for interrupt.
 */
int platform_idc_init(void)
{
	struct idc *idc = *idc_get();
	int core = cpu_get_id();
	int ret;

	/* initialize idc data */
	idc->busy_bit_mask = idc_get_busy_bit_mask(core);

	/* configure interrupt */
	idc->irq = interrupt_get_irq(PLATFORM_IDC_INTERRUPT,
				     PLATFORM_IDC_INTERRUPT_NAME);
	if (idc->irq < 0)
		return idc->irq;
	ret = interrupt_register(idc->irq, idc_irq_handler, idc);
	if (ret < 0)
		return ret;
	interrupt_enable(idc->irq, idc);

	/* enable BUSY interrupt */
	idc_write(IPC_IDCCTL, core, idc->busy_bit_mask);

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

	tr_info(&idc_tr, "idc_free()");

	/* disable and unregister interrupt */
	interrupt_disable(idc->irq, idc);
	interrupt_unregister(idc->irq, idc);

	/* clear BUSY bits */
	for (i = 0; i < CONFIG_CORE_COUNT; i++) {
		idctfc = idc_read(IPC_IDCTFC(i), core);
		if (idctfc & IPC_IDCTFC_BUSY)
			idc_write(IPC_IDCTFC(i), core, idctfc);
	}

	schedule_task_free(&idc->idc_task);
}
