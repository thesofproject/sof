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

void ds_vamsg(const char *format, va_list args)
{
	struct {
		struct debug_stream_text_msg msg;
		char text[128];
	} __packed buf = { 0 };
	ssize_t len;

	len = vsnprintf(buf.text, sizeof(buf.text), format, args);

	if (len < 0)
		return;
	len = MIN(len, sizeof(buf.text));

	buf.msg.hdr.id = DEBUG_STREAM_RECORD_ID_TEXT_MSG;
	buf.msg.hdr.size_words = SOF_DIV_ROUND_UP(sizeof(buf.msg) + len,
						  sizeof(buf.msg.hdr.data[0]));
	debug_stream_slot_send_record(&buf.msg.hdr);
}

void ds_msg(const char *format, ...)
{
	va_list args;

	va_start(args, format);
	ds_vamsg(format, args);
	va_end(args);
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

/* Per-CPU state for exception dump and assert_print(). Static data is
 * currently placed in .bss and its ATM uncached so the ds_cpu table
 * elements do not need to be cache aligned, but if this changes we
 * need __aligned(CONFIG_DCACHE_LINE_SIZE) here.
 */
static struct ds_cpu_state {
	struct {
		struct debug_stream_text_msg msg;
		char text[640];
	} __packed buf;
	int reports_sent;
	size_t pos;
} ds_cpu[CONFIG_SOF_DEBUG_STREAM_SLOT_FORCE_MAX_CPUS];

static void ds_exception_drain(bool flush)
{
	unsigned int cpu = arch_proc_id();
	struct ds_cpu_state *cs = &ds_cpu[cpu];

	if (flush) {
		cs->pos = 0;
		return;
	}

	if (cs->pos == 0)
		return;

	if (cs->reports_sent > 0)
		return;

	cs->buf.msg.hdr.id = DEBUG_STREAM_RECORD_ID_TEXT_MSG;
	cs->buf.msg.hdr.size_words =
		SOF_DIV_ROUND_UP(sizeof(cs->buf.msg) + cs->pos,
				 sizeof(cs->buf.msg.hdr.data[0]));

	/* Make sure the possible up to 3 extra bytes at end of msg are '\0' */
	memset(cs->buf.text + cs->pos, 0,
	       cs->buf.msg.hdr.size_words * sizeof(cs->buf.msg.hdr.data[0]) - cs->pos);
	debug_stream_slot_send_record(&cs->buf.msg.hdr);
	cs->reports_sent = 1;
	cs->pos = 0;
}

static void ds_exception_dump(const char *format, va_list args)
{
	ssize_t len;
	size_t avail;
	size_t written;
	unsigned int cpu = arch_proc_id();
	struct ds_cpu_state *cs = &ds_cpu[cpu];

	if (cs->reports_sent > 0)
		return;

	avail = sizeof(cs->buf.text) - cs->pos;
	if (avail == 0) {
		ds_exception_drain(false);
		return;
	}

	if (format[0] == '\0')
		return;

	/* Skip useless " ** " prefix to save bytes */
	if (strlen(format) >= 4 &&
	    format[0] == ' ' && format[1] == '*' && format[2] == '*' && format[3] == ' ')
		format += 4;

	len = vsnprintf(cs->buf.text + cs->pos, avail, format, args);
	if (len < 0) {
		cs->pos = 0;
		return;
	}

	if (len == 0)
		return;

	if ((size_t)len >= avail)
		written = avail - 1;
	else
		written = (size_t)len;

	cs->pos += written;

	if (cs->pos >= sizeof(cs->buf.text))
		ds_exception_drain(false);
}

static int init_exception_dump_hook(void)
{
	arch_exception_set_dump_hook(ds_exception_dump, ds_exception_drain);
	LOG_DBG("exception_dump_hook set");
	return 0;
}

SYS_INIT(init_exception_dump_hook, APPLICATION, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);

#if defined(CONFIG_SOF_DEBUG_STREAM_TEXT_MSG_ASSERT_PRINT)
void assert_print(const char *fmt, ...)
{
	va_list ap;

	/* Do not print assert after exception has been dumped */
	if (ds_cpu[arch_proc_id()].reports_sent > 0)
		return;

	va_start(ap, fmt);
#if !defined(CONFIG_EXCEPTION_DUMP_HOOK_ONLY)
	{
		va_list ap2;

		va_copy(ap2, ap);
#endif
		ds_vamsg(fmt, ap);
#if !defined(CONFIG_EXCEPTION_DUMP_HOOK_ONLY)
		vprintk(fmt, ap2);
		va_end(ap2);
	}
#endif
	ds_vamsg(fmt, ap);
	va_end(ap);
}
EXPORT_SYMBOL(assert_print);
#endif
#endif
