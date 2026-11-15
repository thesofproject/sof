// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2024 Intel Corporation.

#include <sys/types.h>
#include <stdarg.h>
#include <stdio.h>
#include <soc.h>
#include <adsp_debug_window.h>
#include <sof/common.h>
#include <rtos/symbol.h>
#include <zephyr/logging/log.h>

#include <user/debug_stream_text_msg.h>

LOG_MODULE_REGISTER(debug_stream_text_msg);

/* The exception dump hook and assert_print(), which run in kernel mode, are in
 * src/kernel/debug/debug_stream/debug_stream_text_msg-kernel.c
 */

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
EXPORT_SYMBOL(ds_msg);
