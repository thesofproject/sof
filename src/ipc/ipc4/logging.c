// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 Intel Corporation. All rights reserved.
//
// Author: Kai Vehmanen <kai.vehmanen@linux.intel.com>

#include <stdint.h>
#include <sof/lib/uuid.h>
#include <sof/ipc/common.h>
#include <ipc4/base_fw.h>
#include <ipc4/error_status.h>
#include <ipc4/logging.h>

#ifdef CONFIG_LOG_BACKEND_ADSP_MTRACE

#undef panic // SOF headers may redefine panic
#include <zephyr/logging/log_backend.h>
#include <zephyr/logging/log_backend_adsp_mtrace.h>
#include <zephyr/logging/log.h>

#include <sof/schedule/edf_schedule.h>
#include <sof/schedule/schedule.h>
#include <sof/schedule/task.h>

LOG_MODULE_REGISTER(mtrace, CONFIG_SOF_LOG_LEVEL);

/**
 * If mtrace log buffer has less space than
 * the threshold, notify host with BUFFER_STATUS IPC4.
 */
#define NOTIFY_BUFFER_STATUS_THRESHOLD 2048

/**
 * Default aging timer value. This time defines the maximum time to
 * block IPC BUFFER STATUS notify messages. The notification is sent
 * either due to amount of data available in the buffer, or by timeout
 * in case logs are sent at slow rate.
 */
#define IPC4_MTRACE_NOTIFY_AGING_TIMER_MS 1000

/**
 * Smallest possible aging timer value
 */
#define IPC4_MTRACE_AGING_TIMER_MIN_MS 100

/**
 * Shortest time between IPC notifications sent to host. This
 * is used as protection against a flood of log messages.
 */
#define IPC4_MTRACE_NOTIFY_MIN_DELTA_MS 10

/* bb2aa22e-1ab6-4650-8501-6e67fcc04f4e */
DECLARE_SOF_UUID("mtrace-task", mtrace_task_uuid, 0xbb2aa22e, 0x1ab6, 0x4650,
		 0x85, 0x01, 0x6e, 0x67, 0xfc, 0xc0, 0x4f, 0x4e);

static uint64_t mtrace_notify_last_sent;

static uint32_t mtrace_bytes_pending;

static uint32_t mtrace_aging_timer = IPC4_MTRACE_NOTIFY_AGING_TIMER_MS;

struct task mtrace_task;

#define MTRACE_IPC_CORE	PLATFORM_PRIMARY_CORE_ID

static void mtrace_log_hook(size_t written, size_t space_left)
{
	uint64_t delta;

	mtrace_bytes_pending += written;

	/* TODO: if hook is called on non-zero core, logs
	 * maybe lost with too slow aging timer. need to
	 * figure out a safe way to wake up the mtrace task
	 * from another core.
	 */
	if (arch_proc_id() != MTRACE_IPC_CORE)
		return;

	delta = k_uptime_get() - mtrace_notify_last_sent;

	if (delta < IPC4_MTRACE_NOTIFY_MIN_DELTA_MS)
		return;

	if (space_left < NOTIFY_BUFFER_STATUS_THRESHOLD ||
	    delta >= mtrace_aging_timer) {
		ipc_send_buffer_status_notify();
		mtrace_notify_last_sent = k_uptime_get();
		mtrace_bytes_pending = 0;
	}
}

static enum task_state mtrace_task_run(void *data)
{
	if (k_uptime_get() - mtrace_notify_last_sent >= mtrace_aging_timer &&
	    mtrace_bytes_pending)
		mtrace_log_hook(0, 0);

	/* task will be re-run based on mtrace_task_deadline() */
	return SOF_TASK_STATE_RESCHEDULE;
}

static uint64_t mtrace_task_deadline(void *data)
{
	return k_uptime_ticks() + k_ms_to_ticks_ceil64(mtrace_aging_timer);
}

int ipc4_logging_enable_logs(bool first_block,
			     bool last_block,
			     uint32_t data_offset_or_size,
			     char *data)
{
	const struct log_backend *log_backend = log_backend_adsp_mtrace_get();
	struct ipc4_log_state_info *log_state;
	struct task_ops ops = {
		.run = mtrace_task_run,
		.get_deadline = mtrace_task_deadline,
		.complete = NULL,
	};

	if (!(first_block && last_block)) {
		LOG_ERR("log_state data is expected to be sent as one chunk");
		return -EINVAL;
	}

	if (data_offset_or_size < sizeof(struct ipc4_log_state_info)) {
		LOG_ERR("log_state too small data size: %u", data_offset_or_size);
		return -EINVAL;
	}

	dcache_invalidate_region((__sparse_force void __sparse_cache *)data, data_offset_or_size);

	/*
	 * TODO: support is missing for extended log state info that
	 *       allows to configure logging type
	 */
	log_state = (struct ipc4_log_state_info *)data;

	if (log_state->enable) {
		adsp_mtrace_log_init(mtrace_log_hook);

		log_backend_activate(log_backend, mtrace_log_hook);

		mtrace_aging_timer = log_state->aging_timer_period;
		if (mtrace_aging_timer < IPC4_MTRACE_AGING_TIMER_MIN_MS) {
			mtrace_aging_timer = IPC4_MTRACE_AGING_TIMER_MIN_MS;
			LOG_WRN("Too small aging timer value, limiting to %u\n",
				mtrace_aging_timer);
		}

		schedule_task_init_edf(&mtrace_task, SOF_UUID(mtrace_task_uuid),
				       &ops, NULL, MTRACE_IPC_CORE, 0);
		schedule_task(&mtrace_task, mtrace_aging_timer * 1000, 0);
	} else  {
		adsp_mtrace_log_init(NULL);
		log_backend_deactivate(log_backend);
		schedule_task_cancel(&mtrace_task);
	}

	return 0;
}

int ipc4_logging_shutdown(void)
{
	struct ipc4_log_state_info log_state = { 0 };

	/* log_state.enable set to 0 above */

	return ipc4_logging_enable_logs(true, true, sizeof(log_state), (char *)&log_state);
}

#else
int ipc4_logging_enable_logs(bool first_block,
			     bool last_block,
			     uint32_t data_offset_or_size,
			     char *data)
{
	return IPC4_UNKNOWN_MESSAGE_TYPE;
}

int ipc4_logging_shutdown(void)
{
	return 0;
}

#endif
