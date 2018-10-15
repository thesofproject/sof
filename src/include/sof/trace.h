/*
 * Copyright (c) 2016, Intel Corporation
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
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 *         Artur Kloniecki <arturx.kloniecki@linux.intel.com>
 */

#ifndef __INCLUDE_TRACE__
#define __INCLUDE_TRACE__

#include <stdint.h>
#include <stdlib.h>
#include <sof/sof.h>
#include <sof/mailbox.h>
#include <sof/debug.h>
#include <sof/timer.h>
#include <platform/platform.h>
#include <platform/timer.h>
#include <uapi/logging.h>

/* bootloader trace values */
#define TRACE_BOOT_LDR_ENTRY		0x100
#define TRACE_BOOT_LDR_HPSRAM		0x110
#define TRACE_BOOT_LDR_MANIFEST	0x120
#define TRACE_BOOT_LDR_JUMP		0x150

#define TRACE_BOOT_LDR_PARSE_MODULE	0x210
#define TRACE_BOOT_LDR_PARSE_SEGMENT	0x220

/* general trace init codes - only used at boot when main trace is not available */
#define TRACE_BOOT_START		0x1000
#define TRACE_BOOT_ARCH		0x2000
#define TRACE_BOOT_SYS			0x3000
#define TRACE_BOOT_PLATFORM		0x4000

/* system specific codes */
#define TRACE_BOOT_SYS_WORK		(TRACE_BOOT_SYS + 0x100)
#define TRACE_BOOT_SYS_CPU_FREQ		(TRACE_BOOT_SYS + 0x200)
#define TRACE_BOOT_SYS_HEAP		(TRACE_BOOT_SYS + 0x300)
#define TRACE_BOOT_SYS_NOTE		(TRACE_BOOT_SYS + 0x400)
#define TRACE_BOOT_SYS_SCHED		(TRACE_BOOT_SYS + 0x500)
#define TRACE_BOOT_SYS_POWER		(TRACE_BOOT_SYS + 0x600)

/* platform/device specific codes */
#define TRACE_BOOT_PLATFORM_ENTRY	(TRACE_BOOT_PLATFORM + 0x100)
#define TRACE_BOOT_PLATFORM_MBOX	(TRACE_BOOT_PLATFORM + 0x110)
#define TRACE_BOOT_PLATFORM_SHIM	(TRACE_BOOT_PLATFORM + 0x120)
#define TRACE_BOOT_PLATFORM_PMC		(TRACE_BOOT_PLATFORM + 0x130)
#define TRACE_BOOT_PLATFORM_TIMER	(TRACE_BOOT_PLATFORM + 0x140)
#define TRACE_BOOT_PLATFORM_CLOCK	(TRACE_BOOT_PLATFORM + 0x150)
#define TRACE_BOOT_PLATFORM_SSP_FREQ	(TRACE_BOOT_PLATFORM + 0x160)
#define TRACE_BOOT_PLATFORM_IPC		(TRACE_BOOT_PLATFORM + 0x170)
#define TRACE_BOOT_PLATFORM_DMA		(TRACE_BOOT_PLATFORM + 0x180)
#define TRACE_BOOT_PLATFORM_SSP		(TRACE_BOOT_PLATFORM + 0x190)
#define TRACE_BOOT_PLATFORM_DMIC	(TRACE_BOOT_PLATFORM + 0x1a0)
#define TRACE_BOOT_PLATFORM_IDC		(TRACE_BOOT_PLATFORM + 0x1b0)

/* move to config.h */
#define TRACE	1
#define TRACEV	0
#define TRACEE	1
#define TRACEM	0 /* send all trace messages to mbox and local trace buffer */

void _trace_event0(uint32_t log_entry);
void _trace_event_mbox0(uint32_t log_entry);
void _trace_event_atomic0(uint32_t log_entry);
void _trace_event_mbox_atomic0(uint32_t log_entry);

void _trace_event1(uint32_t log_entry, uint32_t param);
void _trace_event_mbox1(uint32_t log_entry, uint32_t param);
void _trace_event_atomic1(uint32_t log_entry, uint32_t param);
void _trace_event_mbox_atomic1(uint32_t log_entry, uint32_t param);

void _trace_event2(uint32_t log_entry, uint32_t param1, uint32_t param2);
void _trace_event_mbox2(uint32_t log_entry, uint32_t param1, uint32_t param2);
void _trace_event_atomic2(uint32_t log_entry, uint32_t param1, uint32_t param2);
void _trace_event_mbox_atomic2(uint32_t log_entry, uint32_t param1,
	uint32_t param2);

void _trace_event3(uint32_t log_entry, uint32_t param1, uint32_t param2,
	uint32_t param3);
void _trace_event_mbox3(uint32_t log_entry, uint32_t param1, uint32_t param2,
	uint32_t param3);
void _trace_event_atomic3(uint32_t log_entry, uint32_t param1, uint32_t param2,
	uint32_t param3);
void _trace_event_mbox_atomic3(uint32_t log_entry, uint32_t param1,
	uint32_t param2, uint32_t param3);

void _trace_event4(uint32_t log_entry, uint32_t param1, uint32_t param2,
	uint32_t param3, uint32_t param4);
void _trace_event_mbox4(uint32_t log_entry, uint32_t param1, uint32_t param2,
	uint32_t param3, uint32_t param4);
void _trace_event_atomic4(uint32_t log_entry, uint32_t param1, uint32_t param2,
	uint32_t param3, uint32_t param4);
void _trace_event_mbox_atomic4(uint32_t log_entry, uint32_t param1,
	uint32_t param2, uint32_t param3, uint32_t param4);

void trace_flush(void);
void trace_off(void);
void trace_init(struct sof *sof);

#if TRACE

/*
 * trace_event macro definition
 *
 * trace_event() macro is used for logging events that occur at runtime.
 * It comes in 2 main flavours, atomic and non-atomic. Depending of definitions
 * above, it might also propagate log messages to mbox if desired.
 *
 * First argument is always class of event being logged, as defined in
 * uapi/logging.h.
 * Second argument is string literal in printf format, followed by up to 4
 * parameters (uint32_t), that are used to expand into string fromat when
 * parsing log data.
 *
 * All compile-time accessible data (verbosity, class, source file name, line
 * index and string literal) are linked into .static_log_entries section
 * of binary and then extracted by rimage, so they do not contribute to loadable
 * image size. This way more elaborate log messages are possible and encouraged,
 * for better debugging experience, without worrying about runtime performance.
 */
#if TRACEM
/* send all trace to mbox and local trace buffer */
#define trace_event(__c, __e, ...) \
	_log_message(_mbox,, LOG_LEVEL_VERBOSE, __c, __e, ##__VA_ARGS__)
#define trace_event_atomic(__c, __e, ...) \
	_log_message(_mbox, _atomic, LOG_LEVEL_VERBOSE, __c, __e, ##__VA_ARGS__)
#else
/* send trace events only to the local trace buffer */
#define trace_event(__c, __e, ...) \
	_log_message(,, LOG_LEVEL_VERBOSE, __c, __e, ##__VA_ARGS__)
#define trace_event_atomic(__c, __e, ...) \
	_log_message(, _atomic, LOG_LEVEL_VERBOSE, __c, __e, ##__VA_ARGS__)
#endif
#define trace_value(x)		trace_event(0, "value %u", x)
#define trace_value_atomic(x)	trace_event_atomic(0, "value %u", x)

#define trace_point(x) platform_trace_point(x)

/* verbose tracing */
#if TRACEV
#define tracev_event(__c, __e, ...) trace_event(__c, __e, ##__VA_ARGS__)
#define tracev_value(x)	trace_value(x)
#define tracev_event_atomic(__c, __e, ...) \
	trace_event_atomic(__c, __e, ##__VA_ARGS__)
#define tracev_value_atomic(x)	trace_value_atomic(x)
#else
#define tracev_event(__c, __e, ...)
#define tracev_event_atomic(__c, __e, ...)

#define tracev_value(x)
#define tracev_value_atomic(x)
#endif

/* error tracing */
#if TRACEE
#define trace_error(__c, __e, ...) _log_message(_mbox, _atomic, \
	LOG_LEVEL_CRITICAL, __c, __e, ##__VA_ARGS__)
#define trace_error_atomic(__c, __e, ...) \
	trace_error(__c, __e, ##__VA_ARGS__)
/* write back error value to mbox */
#define trace_error_value(x) \
	trace_error(0, "value %u", x)
#define trace_error_value_atomic(x) \
	trace_error_atomic(0, "value %u", x)
#else
#define trace_error(__c, __e, ...)
#define trace_error_atomic(__c, __e, ...)
#define trace_error_value(x)
#define trace_error_value_atomic(x)
#endif

typedef void(*log_func)();

/*
 * Log entry declaration
 *
 * Each log_entry defines anonymous type, with all compile-time data
 * associated with it, to save on transmission. It is then placed by the linker
 * in .static_log_entries section of ELF.
 */
#define _DECLARE_LOG_ENTRY(lvl, format, comp_id, params)\
	__attribute__((section(".static_log." #lvl)))	\
	static const struct {				\
		uint32_t level;				\
		uint32_t component_id;			\
		uint32_t params_num;			\
		uint32_t line_idx;			\
		uint32_t file_name_len;			\
		const char file_name[sizeof(__FILE__)];	\
		uint32_t text_len;			\
		const char text[sizeof(format)];	\
	} log_entry = {					\
		lvl,					\
		comp_id,				\
		params,					\
		__LINE__,				\
		sizeof(__FILE__),			\
		__FILE__,				\
		sizeof(format),				\
		format					\
	}

#define BASE_LOG(function_name, entry, ...)				\
{									\
	log_func log_function = NULL;					\
	if (PP_NARG(__VA_ARGS__) == 0) {				\
		log_function = (log_func)&function_name##0;		\
		log_function(entry, ##__VA_ARGS__);			\
	} else if (PP_NARG(__VA_ARGS__) == 1) {				\
		log_function = (log_func)&function_name##1;		\
		log_function(entry, ##__VA_ARGS__);			\
	} else if (PP_NARG(__VA_ARGS__) == 2) {				\
		log_function = (log_func)&function_name##2;		\
		log_function(entry, ##__VA_ARGS__);			\
	} else if (PP_NARG(__VA_ARGS__) == 3) {				\
		log_function = (log_func)&function_name##3;		\
		log_function(entry, ##__VA_ARGS__);			\
	} else if (PP_NARG(__VA_ARGS__) == 4) {				\
		log_function = (log_func)&function_name##4;		\
		log_function(entry, ##__VA_ARGS__);			\
	} else {							\
		STATIC_ASSERT(PP_NARG(__VA_ARGS__) <= 4,		\
			unsupported_amount_of_params_in_trace_event);	\
	}								\
}

#define __log_message(func_name, lvl, comp_id, format, ...)		\
{									\
	_DECLARE_LOG_ENTRY(lvl, format, comp_id, PP_NARG(__VA_ARGS__));	\
	BASE_LOG(func_name, &log_entry, ##__VA_ARGS__)	\
}

#define _log_message(mbox, atomic, level, comp_id, format, ...)	\
	__log_message(_trace_event##mbox##atomic, level,	\
		comp_id, format, ##__VA_ARGS__)


#else

#define trace_event(__c, __e, ...)
#define trace_event_atomic(__c, __e, ...)

#define trace_error(__c, __e, ...)
#define trace_error_atomic(__c, __e, ...)

#define trace_error_value(x)
#define trace_error_value_atomic(x)

#define trace_value(x)
#define trace_value_atomic(x)

#define trace_point(x)

#define tracev_event(__c, __e, ...)
#define tracev_event_atomic(__c, __e, ...)
#define tracev_value(x)
#define tracev_value_atomic(x)

#endif

#endif
