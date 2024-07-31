/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2024 Intel Corporation.
 */

#ifndef __SOC_DEBUG_STREAM_THREAD_INFO_H__
#define __SOC_DEBUG_STREAM_THREAD_INFO_H__

#include <user/debug_stream_slot.h>

/*
 * Debug Stream Thread Info record header, including the cpu load for the
 * core, and followed by thread_count. Immediately after the header
 * follows the thread fields. The thread_count indicates the number of
 * thread fields.
 */
struct thread_info_record_hdr {
	struct debug_stream_record hdr;
	uint8_t load;		/* Core's load U(0,8) fixed point value */
	uint8_t thread_count;
} __packed;

/*
 * Debug Stream Thread Info field for a single thread.
 */
struct thread_info {
	uint8_t stack_usage;	/* Relative stack usage U(0,8) fixed point value */
	uint8_t cpu_usage;	/* Relative cpu usage U(0,8) fixed point value */
	uint8_t name_len;	/* Length of name string */
	char name[];
} __packed;

#endif /*  __SOC_DEBUG_STREAM_THREAD_INFO_H__ */
