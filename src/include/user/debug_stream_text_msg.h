/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2024 Intel Corporation.
 */

#ifndef __SOC_DEBUG_STREAM_TEXT_MSG_H__
#define __SOC_DEBUG_STREAM_TEXT_MSG_H__

#include <user/debug_stream_slot.h>

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

#endif /*  __SOC_DEBUG_STREAM_TEXT_MSG_H__ */
