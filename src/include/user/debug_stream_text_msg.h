/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2024 Intel Corporation.
 */

/**
 * \file include/user/debug_stream_text_msg.h
 * \brief Text message records transmitted over the debug stream.
 * \ingroup debug_stream_api
 */

#ifndef __SOC_DEBUG_STREAM_TEXT_MSG_H__
#define __SOC_DEBUG_STREAM_TEXT_MSG_H__

#include <user/debug_stream_slot.h>
#include <stdarg.h>

/**
 * \brief Formatted text message record within a debug stream.
 * \ingroup debug_stream_api
 *
 * The record payload is length-delimited by `hdr.size_words`. The text data in `msg[]`
 * is ASCII/UTF-8 encoded and padded with trailing NUL bytes to a 32-bit word boundary.
 * Receivers should calculate maximum payload capacity as
 * `(hdr.size_words * sizeof(uint32_t) - sizeof(struct debug_stream_record))` and stop
 * parsing at either the capacity limit or the first internal NUL terminator.
 */
struct debug_stream_text_msg {
	struct debug_stream_record hdr;	/**< Stream record header specifying size_words */
	char msg[];	/**< Length-delimited text message, padded to 32-bit boundary */
} __packed;

/**
 * \brief Format and transmit a text message record over the active debug stream slot.
 * \ingroup debug_stream_api
 * @param[in] format Printf-style format string.
 * @param[in] ... Variable arguments matching format.
 */
void ds_msg(const char *format, ...);

/**
 * \brief Transmit a text message record over debug stream using va_list arguments.
 * \ingroup debug_stream_api
 * @param[in] format Printf-style format string.
 * @param[in] ap Variable arguments list.
 */
void ds_vamsg(const char *format, va_list ap);

#endif /* __SOC_DEBUG_STREAM_TEXT_MSG_H__ */
