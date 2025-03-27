// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 Intel Corporation. All rights reserved.
//
// Author: Kai Vehmanen <kai.vehmanen@linux.intel.com>

#if CONFIG_LOG_BACKEND_SOF_PROBE && CONFIG_LOG_BACKEND_ADSP_MTRACE
#error Cannot have both backends enabled
#endif

#include <stdint.h>
#include <sof/lib/uuid.h>
#include <sof/ipc/common.h>
#include <ipc4/base_fw.h>
#include <ipc4/error_status.h>
#include <ipc4/logging.h>
#include <rtos/kernel.h>
#if !CONFIG_LIBRARY
#include <zephyr/logging/log_backend.h>
#include <zephyr/logging/log.h>
#endif
#if CONFIG_LOG_BACKEND_SOF_PROBE
#include <sof/probe/probe.h>
#endif

#ifdef CONFIG_LOG_BACKEND_ADSP_MTRACE

#include <zephyr/logging/log_backend_adsp_mtrace.h>

#include <sof/schedule/edf_schedule.h>
#include <sof/schedule/schedule.h>
#include <rtos/task.h>

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

static uint64_t mtrace_notify_last_sent;

static uint32_t mtrace_bytes_pending;

static uint32_t mtrace_aging_timer = IPC4_MTRACE_NOTIFY_AGING_TIMER_MS;

#define MTRACE_IPC_CORE	PLATFORM_PRIMARY_CORE_ID

static struct k_mutex log_mutex;
static struct k_work_delayable log_work;

static void mtrace_log_hook_unlocked(size_t written, size_t space_left)
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

	if (space_left < NOTIFY_BUFFER_STATUS_THRESHOLD ||
	    delta >= mtrace_aging_timer) {
		ipc_send_buffer_status_notify();
		mtrace_notify_last_sent = k_uptime_get();
		mtrace_bytes_pending = 0;
	} else if (mtrace_bytes_pending) {
		k_work_schedule_for_queue(&ipc_get()->ipc_send_wq, &log_work,
					  K_MSEC(mtrace_aging_timer - delta));
	}
}

static void mtrace_log_hook(size_t written, size_t space_left)
{
	k_mutex_lock(&log_mutex, K_FOREVER);

	mtrace_log_hook_unlocked(written, space_left);

	k_mutex_unlock(&log_mutex);
}

static void log_work_handler(struct k_work *work)
{
	k_mutex_lock(&log_mutex, K_FOREVER);

	if (k_uptime_get() - mtrace_notify_last_sent >= mtrace_aging_timer &&
	    mtrace_bytes_pending)
		mtrace_log_hook_unlocked(0, 0);

	k_mutex_unlock(&log_mutex);
}

static struct k_work_sync ipc4_log_work_sync;

int ipc4_logging_enable_logs(bool first_block,
			     bool last_block,
			     uint32_t data_offset_or_size,
			     const char *data)
{
	const struct log_backend *log_backend = log_backend_adsp_mtrace_get();
	const struct ipc4_log_state_info *log_state;

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
	log_state = (const struct ipc4_log_state_info *)data;

	if (log_state->enable) {
		adsp_mtrace_log_init(mtrace_log_hook);

		k_mutex_init(&log_mutex);
		k_work_init_delayable(&log_work, log_work_handler);

		log_backend_activate(log_backend, mtrace_log_hook);

		mtrace_aging_timer = log_state->aging_timer_period;
		if (mtrace_aging_timer < IPC4_MTRACE_AGING_TIMER_MIN_MS) {
			mtrace_aging_timer = IPC4_MTRACE_AGING_TIMER_MIN_MS;
			LOG_WRN("Too small aging timer value, limiting to %u\n",
				mtrace_aging_timer);
		}
	} else  {
		k_work_flush_delayable(&log_work, &ipc4_log_work_sync);
		adsp_mtrace_log_init(NULL);
		log_backend_deactivate(log_backend);
	}

	return 0;
}

#elif CONFIG_LOG_BACKEND_SOF_PROBE

int ipc4_logging_enable_logs(bool first_block,
			     bool last_block,
			     uint32_t data_offset_or_size,
			     const char *data)
{
	const struct log_backend *log_backend = log_backend_probe_get();
	const struct ipc4_log_state_info *log_state;

	if (!(first_block && last_block))
		return -EINVAL;

	if (data_offset_or_size < sizeof(struct ipc4_log_state_info))
		return -EINVAL;

	/* Make sure we work on correct ipc data by invalidating cache
	 * data may be taken from different core to the one we are working
	 * on right now.
	 */
	dcache_invalidate_region((__sparse_force void __sparse_cache *)data, data_offset_or_size);

	log_state = (const struct ipc4_log_state_info *)data;

	if (log_state->enable) {
		if (!probe_is_backend_configured())
			return -EINVAL;

		log_backend_activate(log_backend, NULL);

	} else  {
		log_backend_deactivate(log_backend);
	}

	return 0;
}

#else /* unsupported logging method */

int ipc4_logging_enable_logs(bool first_block,
			     bool last_block,
			     uint32_t data_offset_or_size,
			     const char *data)
{
	return IPC4_UNKNOWN_MESSAGE_TYPE;
}

#endif

int ipc4_logging_shutdown(void)
{
	struct ipc4_log_state_info log_state = { .enable = 0, };

	return ipc4_logging_enable_logs(true, true, sizeof(log_state), (char *)&log_state);
}
