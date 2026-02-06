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

static struct {
	struct debug_stream_text_msg msg;
	char text[768];
} __packed buf = { 0 };
static int reports_sent_cpu[CONFIG_MP_MAX_NUM_CPUS] = { 0 };
static size_t pos;

static void ds_exception_drain(bool flush)
{
	if (flush) {
		pos = 0;
		return;
	}

	if (reports_sent_cpu[arch_proc_id()]++ > 0)
		return;

	buf.msg.hdr.id = DEBUG_STREAM_RECORD_ID_TEXT_MSG;
	buf.msg.hdr.size_words = SOF_DIV_ROUND_UP(sizeof(buf.msg) + pos,
						  sizeof(buf.msg.hdr.data[0]));
	memset(buf.text + pos, 0, buf.msg.hdr.size_words * sizeof(uint32_t) - pos);
	debug_stream_slot_send_record(&buf.msg.hdr);
	pos = 0;
}

static void ds_exception_dump(const char *format, va_list args)
{
	ssize_t len;

	if (reports_sent_cpu[arch_proc_id()] > 0)
		return;

	len = vsnprintf(buf.text + pos, sizeof(buf.text) - pos, format, args);
	if (len < 0) {
		pos = 0;
		return;
	}
	pos += MIN(len, sizeof(buf.text));

	if (pos >= sizeof(buf.text))
		ds_exception_drain(false);
}

#if defined(CONFIG_EXCEPTION_DUMP_HOOK)
static int init_exception_dump_hook(void)
{
	set_exception_dump_hook(ds_exception_dump, ds_exception_drain);
	LOG_INF("exception_dump_hook set");
	return 0;
}

SYS_INIT(init_exception_dump_hook, APPLICATION, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);
#endif
