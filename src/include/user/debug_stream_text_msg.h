/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2024 Intel Corporation.
 */

#ifndef __SOC_DEBUG_STREAM_TEXT_MSG_H__
#define __SOC_DEBUG_STREAM_TEXT_MSG_H__

#include <user/debug_stream_slot.h>
#include <stdarg.h>

/*
 * Debug Stream text message.
 */
struct debug_stream_text_msg {
	struct debug_stream_record hdr;
	char msg[];
} __packed;

/*
 * To send debug messages over debug stream. Enable
 * CONFIG_SOF_DEBUG_STREAM_TEXT_MSG to enable this function.
 */
void ds_msg(const char *format, ...);
void ds_vamsg(const char *format, va_list ap);

#define DS_TEXT_MSG_MAX_LEN 128

#if defined(__ZEPHYR__) && defined(CONFIG_USERSPACE)
__syscall void ds_send_text_record(const char *text, size_t len);
#else
void ds_send_text_record(const char *text, size_t len);
#endif

#if defined(__ZEPHYR__) && defined(CONFIG_USERSPACE)
#include <zephyr/syscalls/debug_stream_text_msg.h>
#endif

#endif /*  __SOC_DEBUG_STREAM_TEXT_MSG_H__ */
