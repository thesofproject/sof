/*
 * Copyright (c) 2019,2022 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log_backend.h>
#include <zephyr/logging/log_core.h>
#include <zephyr/logging/log_output.h>
#include <zephyr/logging/log_backend_std.h>
#include <zephyr/zephyr.h>
#include <zephyr/sys/winstream.h>
#include <soc.h>
#include <adsp_memory.h>

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
static struct k_spinlock lock;

static uint32_t log_format_current = CONFIG_LOG_BACKEND_ADSP_OUTPUT_DEFAULT;

static struct sys_winstream *winstream;

#define SOF_MTRACE_DESCRIPTOR_SIZE	12 /* 3 x u32 */
#define SOF_MTRACE_PAGE_SIZE		0x1000
#define SOF_MTRACE_SLOT_SIZE		SOF_MTRACE_PAGE_SIZE

static void mtrace_init(void)
{
	uint8_t *buf2 = z_soc_uncached_ptr((void *)HP_SRAM_WIN2_BASE);
	uint8_t *toffset = (uint8_t*)buf2 + 4;
	unsigned int boffset = SOF_MTRACE_SLOT_SIZE + 8;
	volatile uint32_t *type = (uint32_t*) toffset;

	/* FIXME: something zeros window2 after soc_trace_init(), so
	 * this has to be redone here ondemand until issue rootcaused */
	if (*type != 0x474f4c00) {
		*type = 0x474f4c00;

		winstream = sys_winstream_init(buf2 + boffset, HP_SRAM_WIN2_SIZE - boffset);
	}
}

static void mtrace_update_dsp_writeptr(void)
{
	uint8_t *buf = z_soc_uncached_ptr((void *)HP_SRAM_WIN2_BASE);
	unsigned int winstr_offset = SOF_MTRACE_SLOT_SIZE + 2 * sizeof(uint32_t);
	unsigned int dspptr_offset = SOF_MTRACE_SLOT_SIZE + sizeof(uint32_t);
	struct sys_winstream *winstream = (struct sys_winstream *)(buf + winstr_offset);
	uint32_t pos = winstream->end + 4 * sizeof(uint32_t);
	volatile uint32_t *dsp_ptr = (uint32_t*)(buf + dspptr_offset);

	/* copy write pointer maintained by winstream to correct
	 * place (as expected by driver) */
	*dsp_ptr = pos;
}

static void mtrace_out(int8_t *str, size_t len)
{
	/* FIXME: the WIN2 area gets overwritten/zeroed
	 *        some time after soc_trace_init(), so as
	 *        a stopgap, keep reinitializing the WIN2
	 *        headers */
	mtrace_init();

	if (len == 0) {
		return;
	}

	sys_winstream_write(winstream, str, len);
	mtrace_update_dsp_writeptr();
}

static int char_out(uint8_t *data, size_t length, void *ctx)
{
	mtrace_out(data, length);

	return length;
}

/**
 * 80 bytes seems to catch most sensibly sized log message lines
 * in one go letting the trace out call output whole complete
 * lines. This avoids the overhead of a spin lock in the trace_out
 * more often as well as avoiding entwined characters from printk if
 * LOG_PRINTK=n.
 */
#define LOG_BUF_SIZE 80
static uint8_t log_buf[LOG_BUF_SIZE];

LOG_OUTPUT_DEFINE(log_output_adsp_mtrace, char_out, log_buf, sizeof(log_buf));

static uint32_t format_flags(void)
{
	uint32_t flags = LOG_OUTPUT_FLAG_LEVEL | LOG_OUTPUT_FLAG_TIMESTAMP;

	if (IS_ENABLED(CONFIG_LOG_BACKEND_FORMAT_TIMESTAMP)) {
		flags |= LOG_OUTPUT_FLAG_FORMAT_TIMESTAMP;
	}

	return flags;
}

static void panic(struct log_backend const *const backend)
{
	k_spinlock_key_t key = k_spin_lock(&lock);

	log_backend_std_panic(&log_output_adsp_mtrace);

	k_spin_unlock(&lock, key);
}

static inline void dropped(const struct log_backend *const backend,
			   uint32_t cnt)
{
	log_output_dropped_process(&log_output_adsp_mtrace, cnt);
}

static void process(const struct log_backend *const backend,
		union log_msg_generic *msg)
{
	log_format_func_t log_output_func = log_format_func_t_get(log_format_current);

	k_spinlock_key_t key = k_spin_lock(&lock);

	log_output_func(&log_output_adsp_mtrace, &msg->log, format_flags());

	k_spin_unlock(&lock, key);
}

static int format_set(const struct log_backend *const backend, uint32_t log_type)
{
	log_format_current = log_type;
	return 0;
}

/**
 * Lazily initialized, while the DMA may not be setup we continue
 * to buffer log messages untilt he buffer is full.
 */
static void init(const struct log_backend *const backend)
{
	ARG_UNUSED(backend);

	/* TODO: remove, this just to debug the logging backend itself */
	const char dbg[] = "mtrace log init\n";
	intel_adsp_trace_out(dbg, sizeof(dbg));
}

const struct log_backend_api log_backend_adsp_mtrace_api = {
	.process = process,
	.dropped = IS_ENABLED(CONFIG_LOG_MODE_IMMEDIATE) ? NULL : dropped,
	.panic = panic,
	.format_set = format_set,
	.init = init,
};

LOG_BACKEND_DEFINE(log_backend_adsp_mtrace, log_backend_adsp_mtrace_api, true);
