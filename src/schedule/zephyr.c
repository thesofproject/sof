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

#include <kernel.h>

#include <sys/p4wq.h>
#include <sof/drivers/idc.h>
#include <sof/init.h>
#include <sof/lib/alloc.h>
#include <ipc/topology.h>

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
	struct idc_msg *msg = &zmsg->msg;
	int payload = -1;

	SOC_DCACHE_INVALIDATE(msg, sizeof(*msg));

	if (msg->size == sizeof(int))
		assert(!memcpy_s(&payload, sizeof(payload), msg->payload, msg->size));

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

	assert(!memcpy_s(msg_cp, sizeof(*msg_cp), msg, sizeof(*msg)));
	/* TODO: verify .priority and .deadline */
	work->priority = K_HIGHEST_THREAD_PRIO + 1;
	work->deadline = 0;
	work->handler = idc_handler;
	work->sync = mode == IDC_BLOCKING;

	if (msg->payload) {
		assert(!memcpy_s(payload->data, sizeof(payload->data),
				 msg->payload, msg->size));
		SOC_DCACHE_FLUSH(payload->data, MIN(sizeof(payload->data), msg->size));
	}

	/* Temporarily store sender core ID */
	msg_cp->core = cpu_get_id();

	SOC_DCACHE_FLUSH(msg_cp, sizeof(*msg_cp));
	k_p4wq_submit(q_zephyr_idc + target_cpu, work);

	switch (mode) {
	case IDC_BLOCKING:
		ret = k_p4wq_wait(work, K_FOREVER);
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
