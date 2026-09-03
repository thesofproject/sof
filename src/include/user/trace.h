/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2018 Intel Corporation. All rights reserved.
 *
 * Author: Bartosz Kokoszko <bartoszx.kokoszko@linux.intel.com>
 *         Artur Kloniecki <arturx.kloniecki@linux.intel.com>
 */

#ifndef __USER_TRACE_H__
#define __USER_TRACE_H__

#include <stdint.h>

/* trace event classes - high 8 bits*/
#define TRACE_CLASS_DEPRECATED	(0)

#define LOG_ENABLE		1  /* Enable logging */
#define LOG_DISABLE		0  /* Disable logging */

#define LOG_LEVEL_CRITICAL	1  /* (FDK fatal) */
#define LOG_LEVEL_ERROR		LOG_LEVEL_CRITICAL
#define LOG_LEVEL_WARNING	2
#define LOG_LEVEL_INFO		3
#define LOG_LEVEL_DEBUG		4
#define LOG_LEVEL_VERBOSE	LOG_LEVEL_DEBUG

#define TRACE_ID_LENGTH 12 /* bit field length */

/*
 *  Log entry protocol header.
 *
 * The header is followed by an array of arguments (uint32_t[]).
 * Number of arguments is specified by the params_num field of log_entry
 */
struct log_entry_header {
	uint32_t uid;
	uint32_t id_0 : TRACE_ID_LENGTH; /* e.g. Pipeline ID */
	uint32_t id_1 : TRACE_ID_LENGTH; /* e.g. Component ID */
	uint32_t core_id : 8;		 /* Reporting core's id */

	uint64_t timestamp;		 /* Timestamp (in dsp ticks) */
	uint32_t log_entry_address;	 /* Address of log entry in ELF */
} __attribute__((packed));

#endif /* __USER_TRACE_H__ */
