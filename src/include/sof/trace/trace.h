/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 *         Artur Kloniecki <arturx.kloniecki@linux.intel.com>
 */

#ifndef __SOF_TRACE_TRACE_H__
#define __SOF_TRACE_TRACE_H__

#ifndef RELATIVE_FILE
#error "This file requires RELATIVE_FILE to be defined. " \
	"Add it to CMake's target with sof_append_relative_path_definitions."
#endif

#if !CONFIG_LIBRARY && CONFIG_TRACE
#include <platform/trace/trace.h>
#endif
#include <sof/common.h>
#include <sof/sof.h>
#include <sof/trace/preproc.h>
#include <config.h>
#include <stdint.h>
#if CONFIG_LIBRARY
#include <stdio.h>
#endif

struct sof;
struct trace;

/* bootloader trace values */
#define TRACE_BOOT_LDR_ENTRY		0x100
#define TRACE_BOOT_LDR_HPSRAM		0x110
#define TRACE_BOOT_LDR_MANIFEST		0x120
#define TRACE_BOOT_LDR_LPSRAM		0x130
#define TRACE_BOOT_LDR_JUMP		0x150

#define TRACE_BOOT_LDR_PARSE_MODULE	0x210
#define TRACE_BOOT_LDR_PARSE_SEGMENT	0x220

/* general trace init codes - only used at boot
 * when main trace is not available
 */
#define TRACE_BOOT_START		0x1000
#define TRACE_BOOT_ARCH			0x2000
#define TRACE_BOOT_SYS			0x3000
#define TRACE_BOOT_PLATFORM		0x4000

/* system specific codes */
#define TRACE_BOOT_SYS_HEAP		(TRACE_BOOT_SYS + 0x100)
#define TRACE_BOOT_SYS_TRACES		(TRACE_BOOT_SYS + 0x200)
#define TRACE_BOOT_SYS_NOTIFIER		(TRACE_BOOT_SYS + 0x300)
#define TRACE_BOOT_SYS_POWER		(TRACE_BOOT_SYS + 0x400)

/* platform/device specific codes */
#define TRACE_BOOT_PLATFORM_ENTRY	(TRACE_BOOT_PLATFORM + 0x100)
#define TRACE_BOOT_PLATFORM_IRQ		(TRACE_BOOT_PLATFORM + 0x110)
#define TRACE_BOOT_PLATFORM_MBOX	(TRACE_BOOT_PLATFORM + 0x120)
#define TRACE_BOOT_PLATFORM_SHIM	(TRACE_BOOT_PLATFORM + 0x130)
#define TRACE_BOOT_PLATFORM_PMC		(TRACE_BOOT_PLATFORM + 0x140)
#define TRACE_BOOT_PLATFORM_TIMER	(TRACE_BOOT_PLATFORM + 0x150)
#define TRACE_BOOT_PLATFORM_CLOCK	(TRACE_BOOT_PLATFORM + 0x160)
#define TRACE_BOOT_PLATFORM_SCHED	(TRACE_BOOT_PLATFORM + 0x170)
#define TRACE_BOOT_PLATFORM_AGENT	(TRACE_BOOT_PLATFORM + 0x180)
#define TRACE_BOOT_PLATFORM_CPU_FREQ	(TRACE_BOOT_PLATFORM + 0x190)
#define TRACE_BOOT_PLATFORM_SSP_FREQ	(TRACE_BOOT_PLATFORM + 0x1A0)
#define TRACE_BOOT_PLATFORM_DMA		(TRACE_BOOT_PLATFORM + 0x1B0)
#define TRACE_BOOT_PLATFORM_IPC		(TRACE_BOOT_PLATFORM + 0x1C0)
#define TRACE_BOOT_PLATFORM_IDC		(TRACE_BOOT_PLATFORM + 0x1D0)
#define TRACE_BOOT_PLATFORM_DAI		(TRACE_BOOT_PLATFORM + 0x1E0)
#define TRACE_BOOT_PLATFORM_SSP		(TRACE_BOOT_PLATFORM + 0x1F0)
#define TRACE_BOOT_PLATFORM_SPI		(TRACE_BOOT_PLATFORM + 0x200)
#define TRACE_BOOT_PLATFORM_DMA_TRACE	(TRACE_BOOT_PLATFORM + 0x210)

#if CONFIG_LIBRARY

extern int test_bench_trace;
char *get_trace_class(uint32_t trace_class);
#define _log_message(mbox, atomic, level, comp_class, id_0, id_1, id_2,	\
		     format, ...)					\
do {									\
	(void)id_0;							\
	(void)id_1;							\
	(void)id_2;							\
	if (test_bench_trace) {						\
		char *msg = "%s " format;				\
		fprintf(stderr, msg, get_trace_class(comp_class),	\
			##__VA_ARGS__);					\
		fprintf(stderr, "\n");					\
	}								\
} while (0)

#endif

#define _TRACE_EVENT_NTH_PARAMS(id_count, param_count)			\
	uintptr_t log_entry						\
	META_SEQ_FROM_0_TO(id_count   , META_SEQ_STEP_id_uint32_t)	\
	META_SEQ_FROM_0_TO(param_count, META_SEQ_STEP_param_uint32_t)

#define _TRACE_EVENT_NTH(postfix, param_count)			\
	META_FUNC_WITH_VARARGS(					\
		_trace_event, META_CONCAT(postfix, param_count),\
		void, _TRACE_EVENT_NTH_PARAMS(3, param_count)	\
	)

#define _TRACE_EVENT_NTH_DECLARE_GROUP(arg_count)	\
	_TRACE_EVENT_NTH(		, arg_count);	\
	_TRACE_EVENT_NTH(_mbox		, arg_count);	\
	_TRACE_EVENT_NTH(_atomic	, arg_count);	\
	_TRACE_EVENT_NTH(_mbox_atomic	, arg_count);

/* Declaration of
 * void _trace_event0            (uint32_t log_entry, uint32_t ids...);
 * void _trace_event_mbox0       (uint32_t log_entry, uint32_t ids...);
 * void _trace_event_atomic0     (uint32_t log_entry, uint32_t ids...);
 * void _trace_event_mbox_atomic0(uint32_t log_entry, uint32_t ids...);
 */
_TRACE_EVENT_NTH_DECLARE_GROUP(0)

/* Declaration of
 * void _trace_event1            (uint32_t log_entry, uint32_t ids...,
 *                                uint32_t params...);
 * void _trace_event_mbox1       (uint32_t log_entry, uint32_t ids...,
 *                                uint32_t params...);
 * void _trace_event_atomic1     (uint32_t log_entry, uint32_t ids...,
 *                                uint32_t params...);
 * void _trace_event_mbox_atomic1(uint32_t log_entry, uint32_t ids...,
 *                                uint32_t params...);
 */
_TRACE_EVENT_NTH_DECLARE_GROUP(1)

/* Declaration of
 * void _trace_event2            (uint32_t log_entry, uint32_t ids...,
 *                                uint32_t params...);
 * void _trace_event_mbox2       (uint32_t log_entry, uint32_t ids...,
 *                                uint32_t params...);
 * void _trace_event_atomic2     (uint32_t log_entry, uint32_t ids...,
 *                                uint32_t params...);
 * void _trace_event_mbox_atomic2(uint32_t log_entry, uint32_t ids...,
 *                                uint32_t params...);
 */
_TRACE_EVENT_NTH_DECLARE_GROUP(2)

/* Declaration of
 * void _trace_event3            (uint32_t log_entry, uint32_t ids...,
 *                                uint32_t params...);
 * void _trace_event_mbox3       (uint32_t log_entry, uint32_t ids...,
 *                                uint32_t params...);
 * void _trace_event_atomic3     (uint32_t log_entry, uint32_t ids...,
 *                                uint32_t params...);
 * void _trace_event_mbox_atomic3(uint32_t log_entry, uint32_t ids...,
 *                                uint32_t params...);
 */
_TRACE_EVENT_NTH_DECLARE_GROUP(3)

/* Declaration of
 * void _trace_event4            (uint32_t log_entry, uint32_t ids...,
 *                                uint32_t params...);
 * void _trace_event_mbox4       (uint32_t log_entry, uint32_t ids...,
 *                                uint32_t params...);
 * void _trace_event_atomic4     (uint32_t log_entry, uint32_t ids...,
 *                                uint32_t params...);
 * void _trace_event_mbox_atomic4(uint32_t log_entry, uint32_t ids...,
 *                                uint32_t params...);
 */
_TRACE_EVENT_NTH_DECLARE_GROUP(4)

#define _TRACE_EVENT_MAX_ARGUMENT_COUNT 4

void trace_flush(void);
void trace_on(void);
void trace_off(void);
void trace_init(struct sof *sof);

static inline struct trace *trace_get(void)
{
	return sof_get()->trace;
}

#define trace_unused(class, id_0, id_1, id_2, format, ...) \
	UNUSED(id_0, id_1, id_2, ##__VA_ARGS__)

#if CONFIG_TRACE

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
#define trace_event(class, format, ...)					\
	_trace_event_with_ids(LOG_LEVEL_INFO, class, 0, -1, -1,		\
			      format, ##__VA_ARGS__)

#define trace_event_atomic(class, format, ...)				\
	_trace_event_atomic_with_ids(LOG_LEVEL_INFO, class, 0, -1, -1,	\
				     format, ##__VA_ARGS__)

#define trace_event_with_ids(class, id_0, id_1, id_2, format, ...)	\
	_trace_event_with_ids(LOG_LEVEL_INFO, class, id_0, id_1, id_2,	\
			      format, ##__VA_ARGS__)

#define trace_event_atomic_with_ids(class, id_0, id_1, id_2, format, ...)     \
	_trace_event_atomic_with_ids(LOG_LEVEL_INFO, class, id_0, id_1, id_2, \
				     format, ##__VA_ARGS__)

#define trace_warn(class, format, ...)					\
	_trace_event_with_ids(LOG_LEVEL_WARNING, class, 0, -1, -1,	\
			      format, ##__VA_ARGS__)

#define trace_warn_atomic(class, format, ...)				   \
	_trace_event_atomic_with_ids(LOG_LEVEL_WARNING, class, 0, -1, -1,  \
				     format, ##__VA_ARGS__)

#define trace_warn_with_ids(class, id_0, id_1, id_2, format, ...)	   \
	_trace_event_with_ids(LOG_LEVEL_WARNING, class, id_0, id_1, id_2,  \
			      format, ##__VA_ARGS__)

#define trace_warn_atomic_with_ids(class, id_0, id_1, id_2, format, ...)   \
	_trace_event_atomic_with_ids(LOG_LEVEL_WARNING, class,		   \
				     id_0, id_1, id_2,			   \
				     format, ##__VA_ARGS__)

#if CONFIG_TRACEM
/* send all trace to mbox and local trace buffer */
#define __mbox _mbox
#else
/* send trace events only to the local trace buffer */
#define __mbox
#endif

#define _trace_event_with_ids(lvl, class, id_0, id_1, id_2, format, ...)       \
	_log_message(__mbox,, lvl, class, id_0, id_1, id_2,		       \
		     format, ##__VA_ARGS__)

#define _trace_event_atomic_with_ids(lvl, class, id_0, id_1, id_2, format, ...)\
	_log_message(__mbox, _atomic, lvl, class, id_0, id_1,		       \
		     id_2, format, ##__VA_ARGS__)

#define trace_point(x) platform_trace_point(x)

#ifndef CONFIG_LIBRARY

#define _DECLARE_LOG_ENTRY(lvl, format, comp_class, params)     \
	__section(".static_log." #lvl)				\
	static const struct {					\
		uint32_t level;					\
		uint32_t component_class;			\
		uint32_t params_num;				\
		uint32_t line_idx;				\
		uint32_t file_name_len;				\
		uint32_t text_len;				\
		const char file_name[sizeof(RELATIVE_FILE)];	\
		const char text[sizeof(format)];		\
	} log_entry = {						\
		lvl,						\
		comp_class,					\
		params,						\
		__LINE__,					\
		sizeof(RELATIVE_FILE),				\
		sizeof(format),					\
		RELATIVE_FILE,					\
		format						\
	}

#define BASE_LOG_ASSERT_FAIL_MSG \
unsupported_amount_of_params_in_trace_event\
_thrown_from_macro_BASE_LOG_in_trace_h

#define BASE_LOG(function_name, id_0, id_1, id_2, entry, ...)	            \
{									    \
	STATIC_ASSERT(							    \
		_TRACE_EVENT_MAX_ARGUMENT_COUNT >=			    \
			META_COUNT_VARAGS_BEFORE_COMPILE(__VA_ARGS__),	    \
		BASE_LOG_ASSERT_FAIL_MSG				    \
	);								    \
	META_CONCAT(function_name,					    \
		    META_COUNT_VARAGS_BEFORE_COMPILE(__VA_ARGS__))	    \
			((uint32_t)entry, id_0, id_1, id_2, ##__VA_ARGS__); \
}

#define __log_message(func_name, lvl, comp_class, id_0, id_1, id_2,	    \
		      format, ...)					    \
do {									    \
	_DECLARE_LOG_ENTRY(lvl, format, comp_class,			    \
			   PP_NARG(__VA_ARGS__));			    \
	BASE_LOG(func_name, id_0, id_1, id_2, &log_entry, ##__VA_ARGS__)    \
} while (0)

#define _log_message(mbox, atomic, level, comp_class, id_0, id_1, id_2,	    \
		     format, ...)					    \
	__log_message(META_CONCAT_SEQ(_trace_event, mbox, atomic),	    \
		      level, comp_class, id_0, id_1, id_2, format,	    \
		      ##__VA_ARGS__)
#else
#define _DECLARE_LOG_ENTRY(lvl, format, comp_class, params)	\
	static const struct {					\
		uint32_t level;					\
		uint32_t component_class;			\
		uint32_t params_num;				\
		uint32_t line_idx;				\
		uint32_t file_name_len;				\
		uint32_t text_len;				\
		const char file_name[sizeof(RELATIVE_FILE)];	\
		const char text[sizeof(format)];		\
	} log_entry = {						\
		lvl,						\
		comp_class,					\
		params,						\
		__LINE__,					\
		sizeof(RELATIVE_FILE),				\
		sizeof(format),					\
		RELATIVE_FILE,					\
		format						\
	}
#endif
#else

#define	trace_event(class, format, ...) \
	trace_unused(class, 0, -1, -1, format, ##__VA_ARGS__)
#define trace_event_with_ids(class, id_0, id_1, id_2, format, ...) \
	trace_unused(class, id_0, id_1, id_2, format, ##__VA_ARGS__)
#define	trace_event_atomic(class, format, ...) \
	trace_unused(class, 0, -1, -1, format, ##__VA_ARGS__)
#define trace_event_atomic_with_ids(class, id_0, id_1, id_2, format, ...) \
	trace_unused(class, id_0, id_1, id_2, format, ##__VA_ARGS__)

#define trace_warn(class, format, ...) \
	trace_unused(class, 0, -1, -1, format, ##__VA_ARGS__)
#define trace_warn_with_ids(class, id_0, id_1, id_2, format, ...) \
	trace_unused(class, id_0, id_1, id_2, format, ##__VA_ARGS__)
#define trace_warn_atomic(class, format, ...) \
	trace_unused(class, 0, -1, -1, format, ##__VA_ARGS__)
#define trace_warn_atomic_with_ids(class, id_0, id_1, id_2, format, ...) \
	trace_unused(class, id_0, id_1, id_2, format, ##__VA_ARGS__)

#define trace_point(x)  do {} while (0)

#endif

/* verbose tracing */
#if CONFIG_TRACEV
#define tracev_event(class, format, ...)				\
	_trace_event_with_ids(LOG_LEVEL_VERBOSE, class, 0, -1, -1,	\
			      format, ##__VA_ARGS__)

#define tracev_event_atomic(class, format, ...)				   \
	_trace_event_atomic_with_ids(LOG_LEVEL_VERBOSE, class, 0, -1, -1,  \
				     format, ##__VA_ARGS__)

#define tracev_event_with_ids(class, id_0, id_1, id_2, format, ...)	   \
	_trace_event_with_ids(LOG_LEVEL_VERBOSE, class, id_0, id_1, id_2,  \
			      format, ##__VA_ARGS__)

#define tracev_event_atomic_with_ids(class, id_0, id_1, id_2, format, ...) \
	_trace_event_atomic_with_ids(LOG_LEVEL_VERBOSE, class,		   \
				     id_0, id_1, id_2,			   \
				     format, ##__VA_ARGS__)

#else
#define tracev_event(class, format, ...) \
	trace_unused(class, 0, -1, -1, format, ##__VA_ARGS__)
#define tracev_event_with_ids(class, id_0, id_1, id_2, format, ...) \
	trace_unused(class, id_0, id_1, id_2, format, ##__VA_ARGS__)
#define tracev_event_atomic(class, format, ...) \
	trace_unused(class, 0, -1, -1, format, ##__VA_ARGS__)
#define tracev_event_atomic_with_ids(class, id_0, id_1, id_2, format, ...) \
	trace_unused(class, id_0, id_1, id_2, format, ##__VA_ARGS__)

#endif

/* error tracing */
#if CONFIG_TRACEE
#define _trace_error_with_ids(class, id_0, id_1, id_2, format, ...)	     \
	_log_message(_mbox, _atomic, LOG_LEVEL_CRITICAL, class, id_0, id_1,  \
		     id_2, format, ##__VA_ARGS__)
#define trace_error(class, format, ...)					\
	_trace_error_with_ids(class, 0, -1, -1, format, ##__VA_ARGS__)
#define trace_error_with_ids(class, id_0, id_1, id_2, format, ...)	\
	_trace_error_with_ids(class, id_0, id_1, id_2, format, ##__VA_ARGS__)
#define trace_error_atomic(...) trace_error(__VA_ARGS__)
#define trace_error_atomic_with_ids(...) trace_error_with_ids(__VA_ARGS__)
#elif CONFIG_TRACE
#define trace_error(...) trace_event(__VA_ARGS__)
#define trace_error_with_ids(...) trace_event_with_ids(__VA_ARGS__)
#define trace_error_atomic(...) trace_event_atomic(__VA_ARGS__)
#define trace_error_atomic_with_ids(...) \
	trace_event_atomic_with_ids(__VA_ARGS__)
#else
#define trace_error(class, format, ...) \
	trace_unused(class, 0, -1, -1, format, ##__VA_ARGS__)
#define trace_error_with_ids(class, id_0, id_1, id_2, format, ...) \
	trace_unused(class, id_0, id_1, id_2, format, ##__VA_ARGS__)
#define trace_error_atomic(class, format, ...) \
	trace_unused(class, 0, -1, -1, format, ##__VA_ARGS__)
#define trace_error_atomic_with_ids(class, id_0, id_1, id_2, format, ...) \
	trace_unused(class, id_0, id_1, id_2, format, ##__VA_ARGS__)
#endif

/* tracing from device (component, pipeline, dai, ...) */

/** \brief Trace from a device on err level.
 *
 * @param class Trace class, one of TRACE_CLASS_...
 * @param get_uid_m Macro that can retrieve device's uid from the dev
 * @param get_id_m Macro that can retrieve device's id0 from the dev
 * @param get_subid_m Macro that can retrieve device's id1 from the dev
 * @param dev Device
 * @param fmt Format followed by parameters
 * @param ... Parameters
 */
#define trace_dev_err(class, get_uid_m, get_id_m, get_subid_m, dev, fmt, ...) \
	trace_error_with_ids(class, get_uid_m(dev), get_id_m(dev),	      \
			     get_subid_m(dev), fmt, ##__VA_ARGS__)

/** \brief Trace from a device on warning level. */
#define trace_dev_warn(class, get_uid_m, get_id_m, get_subid_m, dev, fmt, ...) \
	trace_warn_with_ids(class, get_uid_m(dev), get_id_m(dev),	       \
			     get_subid_m(dev), fmt, ##__VA_ARGS__)

/** \brief Trace from a device on info level. */
#define trace_dev_info(class, get_uid_m, get_id_m, get_subid_m, dev, fmt, ...) \
	trace_event_with_ids(class, get_uid_m(dev), get_id_m(dev),	       \
			     get_subid_m(dev), fmt, ##__VA_ARGS__)

/** \brief Trace from a device on dbg level. */
#define trace_dev_dbg(class, get_uid_m, get_id_m, get_subid_m, dev, fmt, ...) \
	tracev_event_with_ids(class, get_uid_m(dev), get_id_m(dev),	      \
			      get_subid_m(dev), fmt, ##__VA_ARGS__)

#endif /* __SOF_TRACE_TRACE_H__ */
