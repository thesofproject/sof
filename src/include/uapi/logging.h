/*
 * Copyright (c) 2018, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the Intel Corporation nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Bartosz Kokoszko <bartoszx.kokoszko@linux.intel.com>
 *         Artur Kloniecki <arturx.kloniecki@linux.intel.com>
 */

#ifndef __INCLUDE_LOGGING__
#define __INCLUDE_LOGGING__

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
};

#define LOG_ENABLE		1  /* Enable logging */
#define LOG_DISABLE		0  /* Disable logging */

#define LOG_LEVEL_CRITICAL	1  /* (FDK fatal) */
#define LOG_LEVEL_HIGH		2  /* (FDK error) */
#define LOG_LEVEL_MEDIUM	3  /* (FDK warning) */
#define LOG_LEVEL_LOW		4
#define LOG_LEVEL_VERBOSE	5

/*
 * Logging configuration per single core.
 *
 * (cavs: LogState).
 */
struct log_state_core {
	uint32_t enabled; /* LOG_ENABLE / LOG_DISABLE */
	uint32_t level;   /* Logging level, one of LOG_LEVEL_. */
};

/*
 * Logging settings.
 *
 * Aging timer period pecifies how frequently FW sends Log Buffer Status
 * notification for new entries in case the usual notification sending
 * criteria are not met (half of the buffer is full).
 *
 * FIFO full timer period specifies the latency of logging 'dropped log
 * entries' information after the content is consumed by the driver but no
 * new log entry appears (which would trigger logging 'dropped entries' as
 * well).
 *
 * Core mask indicates which cores are targeted by the message.
 * Bit[i] set to 1 means that configuration for i-th core, specified in
 * logs_core[i] is valid.
 *
 * Logs core array specifies logging settings for each core. i-th entry is
 * processed by FW only if bit[i] of core_mask is set.
 *
 * (cavs: LogStateInfo).
 */
struct log_state {
	uint32_t aging_timer_period;        /* Aging timer period */
	uint32_t fifo_full_timer_period;    /* FIFO full timer period */
	uint32_t core_mask;                 /* Mask of cores */
	struct log_state_core logs_core[0]; /* Logging state per core */
};

/*
 * Layout of a log fifo.
 */
struct log_buffer_layout {
	uint32_t read_ptr;  /*read pointer */
	uint32_t write_ptr; /* write pointer */
	uint32_t buffer[0]; /* buffer */
};

/*
 * Log buffer status reported by FW.
 */
struct log_buffer_status {
	uint32_t core_id;  /* ID of core that logged to other half */
};

/*
 *  Log entry header.
 *
 * The header is followed by an array of arguments (uint32_t[]).
 * Number of arguments is specified by the params_num field of log_entry,
 * and is 0-based value (entry_len=0 means there is 1 argument).
 */
struct log_entry_header {
	uint32_t rsvd : 24;	/* Unused */
	uint32_t core_id : 8;	/* Reporting core's id */

	uint64_t timestamp;	/* Timestamp (in dsp ticks) */
} __attribute__((__packed__));

#endif //#ifndef __INCLUDE_LOGGING__
