// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2024 Intel Corporation.

#include <sys/types.h>
#include <stdarg.h>
#include <stdio.h>
#include <soc.h>
#include <adsp_debug_window.h>
#include <sof/common.h>
#include <zephyr/logging/log.h>
#include <zephyr/arch/exception.h>

#include <user/debug_stream_text_msg.h>

LOG_MODULE_REGISTER(debug_stream_text_msg);

void ds_msg(const char *format, ...)
{
	va_list args;
	struct {
		struct debug_stream_text_msg msg;
		char text[128];
	} __packed buf = { 0 };
	ssize_t len;

	va_start(args, format);
	len = vsnprintf(buf.text, sizeof(buf.text), format, args);
	va_end(args);

	if (len < 0)
		return;
	len = MIN(len, sizeof(buf.text));

	buf.msg.hdr.id = DEBUG_STREAM_RECORD_ID_TEXT_MSG;
	buf.msg.hdr.size_words = SOF_DIV_ROUND_UP(sizeof(buf.msg) + len,
						  sizeof(buf.msg.hdr.data[0]));
	debug_stream_slot_send_record(&buf.msg.hdr);
}

#if defined(CONFIG_EXCEPTION_DUMP_HOOK)
/* The debug stream debug window slot is 4k, and when it is split
 * between the cores and the header/other overhead is removed, with 5
 * cores the size is 768 bytes. The dump record must be smaller than
 * that to get through to the host side.
 *
 * Also, because of the limited circular buffer size, we should only
 * send one exception record. On some situations the exceptions happen
 * in bursts, and sending more than one record in short time makes the
 * host-side decoder lose track of things.
 */
static struct {
	struct debug_stream_text_msg msg;
	char text[640];
} __packed ds_buf[CONFIG_MP_MAX_NUM_CPUS];
static int reports_sent_cpu[CONFIG_MP_MAX_NUM_CPUS];
static size_t ds_pos[CONFIG_MP_MAX_NUM_CPUS];

static void ds_exception_drain(bool flush)
{
	unsigned int cpu = arch_proc_id();

	if (flush) {
		ds_pos[cpu] = 0;
		reports_sent_cpu[cpu] = 0;
		return;
	}

	if (ds_pos[cpu] == 0)
		return;

	if (reports_sent_cpu[cpu] > 0)
		return;

	ds_buf[cpu].msg.hdr.id = DEBUG_STREAM_RECORD_ID_TEXT_MSG;
	ds_buf[cpu].msg.hdr.size_words =
		SOF_DIV_ROUND_UP(sizeof(ds_buf[cpu].msg) + ds_pos[cpu],
				 sizeof(ds_buf[cpu].msg.hdr.data[0]));
	/* Make sure the possible up to 3 extra bytes at end of msg are '\0' */
	memset(ds_buf[cpu].text + ds_pos[cpu], 0,
	       ds_buf[cpu].msg.hdr.size_words * sizeof(ds_buf[cpu].msg.hdr.data[0]) - ds_pos[cpu]);
	debug_stream_slot_send_record(&ds_buf[cpu].msg.hdr);
	reports_sent_cpu[cpu] = 1;
	ds_pos[cpu] = 0;
}

static void ds_exception_dump(const char *format, va_list args)
{
	ssize_t len;
	size_t avail;
	size_t written;
	unsigned int cpu = arch_proc_id();

	if (reports_sent_cpu[cpu] > 0)
		return;

	avail = sizeof(ds_buf[cpu].text) - ds_pos[cpu];
	if (avail == 0) {
		ds_exception_drain(false);
		return;
	}

	if (format[0] == '\0')
		return;

	len = vsnprintf(ds_buf[cpu].text + ds_pos[cpu], avail, format, args);
	if (len < 0) {
		ds_pos[cpu] = 0;
		return;
	}

	if (len == 0)
		return;

	if ((size_t)len >= avail)
		written = avail - 1;
	else
		written = (size_t)len;

	ds_pos[cpu] += written;

	if (ds_pos[cpu] >= sizeof(ds_buf[cpu].text))
		ds_exception_drain(false);
}

static int init_exception_dump_hook(void)
{
	set_exception_dump_hook(ds_exception_dump, ds_exception_drain);
	LOG_INF("exception_dump_hook set");
	return 0;
}

SYS_INIT(init_exception_dump_hook, APPLICATION, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);
#endif
