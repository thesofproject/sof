// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2024 Intel Corporation.

#include <sys/types.h>
#include <stdarg.h>
#include <stdio.h>
#include <soc.h>
#include <adsp_debug_window.h>
#include <sof/common.h>

#include <user/debug_stream_text_msg.h>

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
EXPORT_SYMBOL(ds_msg);

