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

/*
 * Host system time.
 *
 * This property is used by the driver to pass down information about
 * current system time. It is expressed in us.
 * FW translates timestamps (in log entries, probe pockets) to this time
 * domain.
 *
 * (cavs: SystemTime).
 */
struct system_time {
	uint32_t val_l;  /* Lower dword of current host time value */
	uint32_t val_u;  /* Upper dword of current host time value */
} __attribute__((packed));

/* trace event classes - high 8 bits*/
#define TRACE_CLASS_IRQ		(1 << 24)
#define TRACE_CLASS_IPC		(2 << 24)
#define TRACE_CLASS_PIPE	(3 << 24)
#define TRACE_CLASS_HOST	(4 << 24)
#define TRACE_CLASS_DAI		(5 << 24)
#define TRACE_CLASS_DMA		(6 << 24)
#define TRACE_CLASS_SSP		(7 << 24)
#define TRACE_CLASS_COMP	(8 << 24)
#define TRACE_CLASS_WAIT	(9 << 24)
#define TRACE_CLASS_LOCK	(10 << 24)
#define TRACE_CLASS_MEM		(11 << 24)
#define TRACE_CLASS_MIXER	(12 << 24)
#define TRACE_CLASS_BUFFER	(13 << 24)
#define TRACE_CLASS_VOLUME	(14 << 24)
#define TRACE_CLASS_SWITCH	(15 << 24)
#define TRACE_CLASS_MUX		(16 << 24)
#define TRACE_CLASS_SRC         (17 << 24)
#define TRACE_CLASS_TONE        (18 << 24)
#define TRACE_CLASS_EQ_FIR      (19 << 24)
#define TRACE_CLASS_EQ_IIR      (20 << 24)
#define TRACE_CLASS_SA		(21 << 24)
#define TRACE_CLASS_DMIC	(22 << 24)
#define TRACE_CLASS_POWER	(23 << 24)
#define TRACE_CLASS_IDC		(24 << 24)
#define TRACE_CLASS_CPU		(25 << 24)
#define TRACE_CLASS_CLK		(26 << 24)
#define TRACE_CLASS_EDF		(27 << 24)
#define TRACE_CLASS_KPB		(28 << 24)
#define TRACE_CLASS_SELECTOR	(29 << 24)
#define TRACE_CLASS_SCHEDULE	(30 << 24)
#define TRACE_CLASS_SCHEDULE_LL	(31 << 24)
#define TRACE_CLASS_ALH		(32 << 24)
#define TRACE_CLASS_KEYWORD	(33 << 24)

#define LOG_ENABLE		1  /* Enable logging */
#define LOG_DISABLE		0  /* Disable logging */

#define LOG_LEVEL_CRITICAL	1  /* (FDK fatal) */
#define LOG_LEVEL_VERBOSE	2

/*
 * Layout of a log fifo.
 */
struct log_buffer_layout {
	uint32_t read_ptr;  /*read pointer */
	uint32_t write_ptr; /* write pointer */
	uint32_t buffer[0]; /* buffer */
} __attribute__((packed));

/*
 * Log buffer status reported by FW.
 */
struct log_buffer_status {
	uint32_t core_id;  /* ID of core that logged to other half */
} __attribute__((packed));

#define TRACE_ID_LENGTH 12

/*
 *  Log entry header.
 *
 * The header is followed by an array of arguments (uint32_t[]).
 * Number of arguments is specified by the params_num field of log_entry
 */
struct log_entry_header {
	uint32_t id_0 : TRACE_ID_LENGTH;	/* e.g. Pipeline ID */
	uint32_t id_1 : TRACE_ID_LENGTH;	/* e.g. Component ID */
	uint32_t core_id : 8;		/* Reporting core's id */

	uint64_t timestamp;		/* Timestamp (in dsp ticks) */
	uint32_t log_entry_address;	/* Address of log entry in ELF */
} __attribute__((packed));

#endif /* __USER_TRACE_H__ */
