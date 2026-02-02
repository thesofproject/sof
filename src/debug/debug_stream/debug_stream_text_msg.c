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
static struct {
	struct debug_stream_text_msg msg;
	char text[512];
} __packed ds_buf;
static int reports_sent_cpu[CONFIG_MP_MAX_NUM_CPUS];
static size_t ds_pos;

static void ds_exception_drain(bool flush)
{
	if (flush) {
		ds_pos = 0;
		return;
	}

	if (reports_sent_cpu[arch_proc_id()]++ > 0)
		return;

	ds_buf.msg.hdr.id = DEBUG_STREAM_RECORD_ID_TEXT_MSG;
	ds_buf.msg.hdr.size_words = SOF_DIV_ROUND_UP(sizeof(ds_buf.msg) + ds_pos,
						     sizeof(ds_buf.msg.hdr.data[0]));
	/* Make sure the possible upto 3 extra bytes at end of msg are '\0' */
	memset(ds_buf.text + ds_pos, 0, ds_buf.msg.hdr.size_words *
	       sizeof(ds_buf.msg.hdr.data[0]) - ds_pos);
	debug_stream_slot_send_record(&ds_buf.msg.hdr);
	ds_pos = 0;
}

static void ds_exception_dump(const char *format, va_list args)
{
	ssize_t len;

	if (reports_sent_cpu[arch_proc_id()] > 0)
		return;

	len = vsnprintf(ds_buf.text + ds_pos, sizeof(ds_buf.text) - ds_pos, format, args);
	if (len < 0) {
		ds_pos = 0;
		return;
	}
	ds_pos += MIN(len, sizeof(ds_buf.text) - ds_pos);

	if (ds_pos >= sizeof(ds_buf.text))
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
