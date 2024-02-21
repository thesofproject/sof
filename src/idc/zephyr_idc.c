// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Intel Corporation. All rights reserved.

/*
 * Use P4WQ to implement IDC for SOF. We create a P4 work queue per core and
 * when the primary core sends a message to a secondary core, a work item from
 * a static per-core array is queued accordingly. The secondary core is then
 * woken up, it executes irc_handler(), which eventually calls idc_cmd() just
 * like in the native SOF case. One work item per secondary core is enough
 * because IDC on SOF is always synchronous, the primary core always waits for
 * secondary cores to complete operation, so no races can occur.
 *
 * Design:
 * - use K_P4WQ_ARRAY_DEFINE() to statically create one queue with one thread
 *	per DSP core.
 * - k_p4wq_submit()
 *	runs on primary CPU
 *	send tasks to other CPUs.
 */

#include <zephyr/kernel.h>
#include <zephyr/cache.h>

#include <zephyr/sys/p4wq.h>
#include <rtos/idc.h>
#include <sof/init.h>
#include <sof/ipc/common.h>
#include <sof/schedule/edf_schedule.h>
#include <rtos/alloc.h>
#include <rtos/spinlock.h>
#include <ipc/topology.h>
#include <sof/trace/trace.h>
#include <sof/lib/uuid.h>

LOG_MODULE_REGISTER(zephyr_idc, CONFIG_SOF_LOG_LEVEL);

/* 5f1ec3f8-faaf-4099-903c-cee98351f169 */
DECLARE_SOF_UUID("zephyr-idc", zephyr_idc_uuid, 0x5f1ec3f8, 0xfaaf, 0x4099,
		 0x90, 0x3c, 0xce, 0xe9, 0x83, 0x51, 0xf1, 0x69);

DECLARE_TR_CTX(zephyr_idc_tr, SOF_UUID(zephyr_idc_uuid), LOG_LEVEL_INFO);

/*
 * Inter-CPU communication is only used in
 * - IPC
 * - Notifier
 * - Power management (IDC_MSG_POWER_UP, IDC_MSG_POWER_DOWN)
 */

#if !CONFIG_MULTICORE || !defined(CONFIG_SMP)

void idc_init_thread(void)
{
}

#else

K_P4WQ_ARRAY_DEFINE(q_zephyr_idc, CONFIG_CORE_COUNT, SOF_STACK_SIZE,
		    K_P4WQ_USER_CPU_MASK);

struct zephyr_idc_msg {
	struct k_p4wq_work work;
	struct idc_msg msg;
};

static void idc_handler(struct k_p4wq_work *work)
{
	struct zephyr_idc_msg *zmsg = container_of(work, struct zephyr_idc_msg, work);
	struct idc *idc = *idc_get();
	struct ipc *ipc = ipc_get();
	struct idc_msg *msg = &zmsg->msg;
	int payload = -1;
	k_spinlock_key_t key;

	__ASSERT_NO_MSG(!is_cached(msg));

	if (msg->size == sizeof(int)) {
		const int idc_handler_memcpy_err __unused =
			memcpy_s(&payload, sizeof(payload), msg->payload, msg->size);
		assert(!idc_handler_memcpy_err);
	}

	idc->received_msg.core = msg->core;
	idc->received_msg.header = msg->header;
	idc->received_msg.extension = msg->extension;

	switch (msg->header) {
	case IDC_MSG_POWER_UP:
		/* Run the core initialisation? */
		secondary_core_init(sof_get());
		break;
	default:
		idc_cmd(&idc->received_msg);
		break;
	case IDC_MSG_IPC:
		idc_cmd(&idc->received_msg);
		/* Signal the host */
		key = k_spin_lock(&ipc->lock);
		ipc->task_mask &= ~IPC_TASK_SECONDARY_CORE;
		ipc_complete_cmd(ipc);
		k_spin_unlock(&ipc->lock, key);
	}
}

/*
 * Used for *target* CPUs, since the initiator (usually core 0) can launch
 * several IDC messages at once
 */
static struct zephyr_idc_msg idc_work[CONFIG_CORE_COUNT];

int idc_send_msg(struct idc_msg *msg, uint32_t mode)
{
	struct idc *idc = *idc_get();
	struct idc_payload *payload = idc_payload_get(idc, msg->core);
	unsigned int target_cpu = msg->core;
	struct zephyr_idc_msg *zmsg = idc_work + target_cpu;
	struct idc_msg *msg_cp = &zmsg->msg;
	struct k_p4wq_work *work = &zmsg->work;
	int ret;
	int idc_send_memcpy_err __unused;

	idc_send_memcpy_err = memcpy_s(msg_cp, sizeof(*msg_cp), msg, sizeof(*msg));
	assert(!idc_send_memcpy_err);
	/* Same priority as the IPC thread which is an EDF task and under Zephyr */
	work->priority = EDF_ZEPHYR_PRIORITY;
	work->deadline = 0;
	work->handler = idc_handler;
	work->sync = mode == IDC_BLOCKING;

	if (!cpu_is_core_enabled(target_cpu)) {
		tr_err(&zephyr_idc_tr, "Core %u is down, cannot sent IDC message", target_cpu);
		return -EACCES;
	}
	if (msg->payload) {
		idc_send_memcpy_err = memcpy_s(payload->data, sizeof(payload->data),
					       msg->payload, msg->size);
		assert(!idc_send_memcpy_err);

		/* Sending a message to another core, write back local payload cache */
		sys_cache_data_flush_range(payload->data, MIN(sizeof(payload->data), msg->size));
	}

	/* Temporarily store sender core ID */
	msg_cp->core = cpu_get_id();

	__ASSERT_NO_MSG(!is_cached(msg_cp));

	k_p4wq_submit(q_zephyr_idc + target_cpu, work);

	switch (mode) {
	case IDC_BLOCKING:
		ret = k_p4wq_wait(work, K_USEC(IDC_TIMEOUT));
		if (!ret)
			/* message was sent and executed successfully, get status code */
			ret = idc_msg_status_get(msg->core);
		break;
	case IDC_POWER_UP:
	case IDC_NON_BLOCKING:
	default:
		ret = 0;
	}

	return ret;
}

void idc_init_thread(void)
{
	int cpu = cpu_get_id();

	k_p4wq_enable_static_thread(q_zephyr_idc + cpu,
				    _p4threads_q_zephyr_idc + cpu, BIT(cpu));
}

#endif /* CONFIG_MULTICORE */
