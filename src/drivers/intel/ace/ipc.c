// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>
//         Rander Wang <rander.wang@intel.com>
//         Serhiy Katsyuba <serhiy.katsyuba@intel.com>
//         Andrey Borisovich <andrey.borisovich@intel.com>
//         Adrian Warecki <adrian.warecki@intel.com>

#include <ace/version.h>

#include <autoconf.h>

#include <intel_adsp_ipc.h>
#include <ace_v1x-regs.h>
#include <sof/ipc/common.h>

#include <sof/ipc/schedule.h>
#include <sof/lib/mailbox.h>
#include <sof/lib/memory.h>
#include <sof/lib/pm_runtime.h>
#include <sof/lib/uuid.h>
#include <rtos/wait.h>
#include <sof/list.h>
#include <sof/platform.h>
#include <sof/schedule/edf_schedule.h>
#include <sof/schedule/schedule.h>
#include <sof/schedule/task.h>
#include <rtos/spinlock.h>
#include <ipc/header.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* 8fa1d42f-bc6f-464b-867f-547af08834da */
DECLARE_SOF_UUID("ipc-task", ipc_task_uuid, 0x8fa1d42f, 0xbc6f, 0x464b,
		 0x86, 0x7f, 0x54, 0x7a, 0xf0, 0x88, 0x34, 0xda);

/**
 * @brief Private data for IPC.
 *
 * Written in interrupt context to service incoming IPC message using registered
 * cAVS IPC Message Handler Callback (message_handler function).
 * Filled with content of TDR and TDD registers.
 * When IPC message is read fills ipc_cmd_hdr.
 */
static uint32_t g_last_data, g_last_ext_data;

/**
 * @brief cAVS IPC Message Handler Callback function.
 *
 * See @ref (*intel_adsp_ipc_handler_t) for function signature description.
 * @return false so BUSY on the other side will not be cleared immediately but
 * will remain set until message would have been processed by scheduled task, i.e.
 * until ipc_platform_complete_cmd() call.
 */
static bool message_handler(const struct device *dev, void *arg, uint32_t data, uint32_t ext_data)
{
	struct ipc *ipc = (struct ipc *)arg;

	k_spinlock_key_t key;

	key = k_spin_lock(&ipc->lock);

	g_last_data = data;
	g_last_ext_data = ext_data;

#if CONFIG_DEBUG_IPC_COUNTERS
	increment_ipc_received_counter();
#endif

	ipc_schedule_process(ipc);

	k_spin_unlock(&ipc->lock, key);

	return false;
}

#if CONFIG_DEBUG_IPC_COUNTERS

static inline void increment_ipc_received_counter(void)
{
	static uint32_t ipc_received_counter;

	mailbox_sw_reg_write(SRAM_REG_FW_IPC_RECEIVED_COUNT,
			     ipc_received_counter++);
}

static inline void increment_ipc_processed_counter(void)
{
	static uint32_t ipc_processed_counter;
	uint32_t *uncache_counter = cache_to_uncache(&ipc_processed_counter);

	mailbox_sw_reg_write(SRAM_REG_FW_IPC_PROCESSED_COUNT,
			     (*uncache_counter)++);
}

#endif /* CONFIG_DEBUG_IPC_COUNTERS */

int ipc_platform_compact_read_msg(struct ipc_cmd_hdr *hdr, int words)
{
	uint32_t *chdr = (uint32_t *)hdr;

	/* compact messages are 2 words on CAVS 1.8 onwards */
	if (words != 2)
		return 0;

	chdr[0] = g_last_data;
	chdr[1] = g_last_ext_data;

	return 2; /* number of words read */
}

int ipc_platform_compact_write_msg(struct ipc_cmd_hdr *hdr, int words)
{
	return 0; /* number of words read - not currently used on this platform */
}

enum task_state ipc_platform_do_cmd(struct ipc *ipc)
{
	struct ipc_cmd_hdr *hdr;

	hdr = ipc_compact_read_msg();

	/* perform command */
	ipc_cmd(hdr);

	return SOF_TASK_STATE_COMPLETED;
}

void ipc_platform_complete_cmd(struct ipc *ipc)
{
	ARG_UNUSED(ipc);
	intel_adsp_ipc_complete(INTEL_ADSP_IPC_HOST_DEV);

#if CONFIG_DEBUG_IPC_COUNTERS
	increment_ipc_processed_counter();
#endif
}

int ipc_platform_send_msg(const struct ipc_msg *msg)
{
	if (!intel_adsp_ipc_is_complete(INTEL_ADSP_IPC_HOST_DEV))
		return -EBUSY;

	/* prepare the message and copy to mailbox */
	struct ipc_cmd_hdr *hdr = ipc_prepare_to_send(msg);

	if (!intel_adsp_ipc_send_message(INTEL_ADSP_IPC_HOST_DEV, hdr->pri, hdr->ext))
		/* IPC device is busy with something else */
		return -EBUSY;

	return 0;
}

int platform_ipc_init(struct ipc *ipc)
{
	ipc_set_drvdata(ipc, NULL);

	/* schedule task */
	schedule_task_init_edf(&ipc->ipc_task, SOF_UUID(ipc_task_uuid),
			       &ipc_task_ops, ipc, 0, 0);

	/* configure interrupt - some work is done internally by Zephytr API */

	/* attach handlers */
	intel_adsp_ipc_set_message_handler(INTEL_ADSP_IPC_HOST_DEV, message_handler, ipc);

	return 0;
}
