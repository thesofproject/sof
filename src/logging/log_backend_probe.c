// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 Intel Corporation. All rights reserved.

#include <zephyr/logging/log_backend.h>
#include <zephyr/logging/log_core.h>
#include <zephyr/logging/log_output.h>
#include <zephyr/logging/log_backend_std.h>
#include <zephyr/kernel.h>

#include <sof/audio/buffer.h>
#include <sof/ipc/topology.h>
#include <sof/lib/memory.h>
#include <sof/lib/notifier.h>
#include <sof/probe/probe.h>

#ifdef PROBE_LOG_DEBUG
#include <stdio.h>
#endif

/*
 * A lock is needed as log_process() and log_panic() have no internal locks
 * to prevent concurrency. Meaning if log_process is called after
 * log_panic was called previously, log_process may happen from another
 * CPU and calling context than the log processing thread running in
 * the background. On an SMP system this is a race.
 *
 * This caused a race on the output trace such that the logging output
 * was garbled and useless.
 */
static struct k_spinlock probe_lock;

static uint32_t probe_log_format_current = CONFIG_LOG_BACKEND_SOF_PROBE_OUTPUT;

#define LOG_BUF_SIZE 80
static uint8_t log_buf[LOG_BUF_SIZE];

static probe_logging_hook_t probe_hook;

static void log_push(uint8_t *data, size_t length)
{
	size_t pos = 0;

	do {
		int ret = probe_hook(data + pos, length - pos);

		if (ret < 0)
			break;
		pos += ret;
	} while (pos < length);
}

#define PRE_BUFFER_SIZE 4096
static struct probe_pre_buffer {
	uint8_t *buf;
	size_t wpos;
	size_t len;
} prebuf;

static void pre_buffer_drain(void)
{
#ifdef PROBE_LOG_DEBUG
	/* NOTE: The debug code only works with ascii/text log output */
	uint64_t stamp = sof_cycle_get_64();
	char msg[80];
	int mlen;

	mlen = snprintf(msg, sizeof(msg), "[Drain %zu bytes of pre buffered logs]\n", prebuf.wpos);
	if (prebuf.len > prebuf.wpos && mlen < sizeof(msg))
		mlen += snprintf(msg + mlen, sizeof(msg) - mlen, "[%zu bytes dropped]\n",
			      prebuf.len - prebuf.wpos);
	log_push(msg, MIN(mlen, sizeof(msg)));
#endif

	log_push(prebuf.buf, prebuf.wpos);
	rfree(prebuf.buf);
	prebuf.buf = NULL;
	prebuf.len = 0;
	prebuf.wpos = 0;

#ifdef PROBE_LOG_DEBUG
	mlen = snprintf(msg, sizeof(msg), "[Buffer drained in %llu us]\n",
			k_cyc_to_us_near64(sof_cycle_get_64() - stamp));

	log_push(msg, MIN(mlen, sizeof(msg)));
#endif
}

static void pre_buffer(uint8_t *data, size_t length)
{
	int ret;

	prebuf.len += length;
	if (!prebuf.buf) {
		prebuf.buf = rzalloc(SOF_MEM_FLAG_USER, PRE_BUFFER_SIZE);
		if (!prebuf.buf)
			return;
	}
	/* Protection against buffer over flow relies on memcpy_s() */
	ret = memcpy_s(&prebuf.buf[prebuf.wpos], PRE_BUFFER_SIZE - prebuf.wpos, data, length);
	if (!ret)
		prebuf.wpos += length;
}

static int probe_char_out(uint8_t *data, size_t length, void *ctx)
{
	if (!probe_hook) {
		pre_buffer(data, length);
	} else {
		if (prebuf.wpos)
			pre_buffer_drain();

		log_push(data, length);
	}

	return length;
}

LOG_OUTPUT_DEFINE(log_output_adsp_probe, probe_char_out, log_buf, sizeof(log_buf));

static uint32_t format_flags(void)
{
	uint32_t flags = LOG_OUTPUT_FLAG_LEVEL | LOG_OUTPUT_FLAG_TIMESTAMP;

	if (IS_ENABLED(CONFIG_LOG_BACKEND_FORMAT_TIMESTAMP))
		flags |= LOG_OUTPUT_FLAG_FORMAT_TIMESTAMP;

	return flags;
}

static void probe_log_panic(struct log_backend const *const backend)
{
	k_spinlock_key_t key = k_spin_lock(&probe_lock);

	log_backend_std_panic(&log_output_adsp_probe);

	k_spin_unlock(&probe_lock, key);
}

static void probe_log_dropped(const struct log_backend *const backend,
			      uint32_t cnt)
{
	log_output_dropped_process(&log_output_adsp_probe, cnt);
}

static void probe_log_process(const struct log_backend *const backend,
			      union log_msg_generic *msg)
{
	log_format_func_t log_output_func = log_format_func_t_get(probe_log_format_current);

	k_spinlock_key_t key = k_spin_lock(&probe_lock);

	log_output_func(&log_output_adsp_probe, &msg->log, format_flags());

	k_spin_unlock(&probe_lock, key);
}

static int probe_log_format_set(const struct log_backend *const backend, uint32_t log_type)
{
	probe_log_format_current = log_type;
	return 0;
}

/**
 * Lazily initialized, while the DMA may not be setup we continue
 * to buffer log messages untilt he buffer is full.
 */
static void probe_log_init(const struct log_backend *const backend)
{
	ARG_UNUSED(backend);
}

const struct log_backend_api log_backend_adsp_probe_api = {
	.process = probe_log_process,
	.dropped = IS_ENABLED(CONFIG_LOG_MODE_IMMEDIATE) ? NULL : probe_log_dropped,
	.panic = probe_log_panic,
	.format_set = probe_log_format_set,
	.init = probe_log_init,
};

LOG_BACKEND_DEFINE(log_backend_adsp_probe, log_backend_adsp_probe_api, true);

void probe_logging_init(probe_logging_hook_t fn)
{
	probe_hook = fn;
}
EXPORT_SYMBOL(probe_logging_init);

const struct log_backend *log_backend_probe_get(void)
{
	return &log_backend_adsp_probe;
}

bool probe_is_backend_configured(void)
{
	return probe_hook != NULL;
}
