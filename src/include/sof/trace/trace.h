/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 *         Artur Kloniecki <arturx.kloniecki@linux.intel.com>
 *         Karol Trzcinski <karolx.trzcinski@linux.intel.com>
 */

#ifndef __SOF_TRACE_TRACE_H__
#define __SOF_TRACE_TRACE_H__

#ifndef RELATIVE_FILE
#error "This file requires RELATIVE_FILE to be defined. " \
	"Add it to CMake's target with sof_append_relative_path_definitions."
#endif

#if !CONFIG_LIBRARY
#include <platform/trace/trace.h>
#endif
#include <sof/common.h>
#include <sof/sof.h>
#include <sof/trace/preproc.h>

#include <stdbool.h>
#include <stdint.h>
#if CONFIG_LIBRARY
#include <stdio.h>
#endif

struct sof;
struct trace;
struct tr_ctx;

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

#define _TRACE_EVENT_MAX_ARGUMENT_COUNT 4

static inline struct trace *trace_get(void)
{
	return sof_get()->trace;
}

/* Silences compiler warnings about unused variables */
#define trace_unused(class, ctx, id_1, id_2, format, ...) \
	UNUSED(ctx, id_1, id_2, ##__VA_ARGS__)

struct trace_filter {
	uint32_t uuid_id;	/**< type id, or 0 when not important */
	int32_t comp_id;	/**< component id or -1 when not important */
	int32_t pipe_id;	/**< pipeline id or -1 when not important */
	int32_t log_level;	/**< new log level value */
};

#if CONFIG_TRACE

/*
 * trace_event macro definition
 *
 * trace_event() macro is used for logging events that occur at runtime.
 * It comes in 2 main flavours, atomic and non-atomic. Depending of definitions
 * above, it might also propagate log messages to mbox if desired.
 *
 * First argument is always class of event being logged, as defined in
 * user/trace.h - TRACE_CLASS_* (deprecated - do not use).
 * Second argument is string literal in printf format, followed by up to 4
 * parameters (uint32_t), that are used to expand into string fromat when
 * parsing log data.
 *
 * All compile-time accessible data (verbosity, class, source file name, line
 * index and string literal) are linked into .static_log_entries section
 * of binary and then extracted by smex, so they do not contribute to loadable
 * image size. This way more elaborate log messages are possible and encouraged,
 * for better debugging experience, without worrying about runtime performance.
 */
/* Map the different trace_xxxx_with_ids(... ) levels to the
 * _trace_event_with_ids(level_xxxx, ...) macro shared across log
 * levels.
 */
#define trace_event_with_ids(class, ctx, id_1, id_2, format, ...)	\
	_trace_event_with_ids(LOG_LEVEL_INFO, class, ctx, id_1, id_2,	\
			      format, ##__VA_ARGS__)

#define trace_event_atomic_with_ids(class, ctx, id_1, id_2, format, ...)     \
	_trace_event_atomic_with_ids(LOG_LEVEL_INFO, class, ctx, id_1, id_2, \
				     format, ##__VA_ARGS__)

#define trace_warn_with_ids(class, ctx, id_1, id_2, format, ...)	 \
	_trace_event_with_ids(LOG_LEVEL_WARNING, class, ctx, id_1, id_2, \
			      format, ##__VA_ARGS__)

#define trace_warn_atomic_with_ids(class, ctx, id_1, id_2, format, ...)	\
	_trace_event_atomic_with_ids(LOG_LEVEL_WARNING, class,		\
				     ctx, id_1, id_2,			\
				     format, ##__VA_ARGS__)

void trace_flush(void);
void trace_on(void);
void trace_off(void);
void trace_init(struct sof *sof);

/* All tracing macros in this file end up calling these functions in the end. */
void trace_log_filtered(bool send_atomic, const void *log_entry, const struct tr_ctx *ctx,
			uint32_t lvl, uint32_t id_1, uint32_t id_2, int arg_count, ...);
void trace_log_unfiltered(bool send_atomic, const void *log_entry, const struct tr_ctx *ctx,
			  uint32_t lvl, uint32_t id_1, uint32_t id_2, int arg_count, ...);
struct sof_ipc_trace_filter_elem *trace_filter_fill(struct sof_ipc_trace_filter_elem *elem,
						    struct sof_ipc_trace_filter_elem *end,
						    struct trace_filter *filter);
int trace_filter_update(const struct trace_filter *elem);

#define _trace_event_with_ids(lvl, class, ctx, id_1, id_2, format, ...)	\
	_log_message(trace_log_filtered, false, lvl, class, ctx, id_1, id_2, format, ##__VA_ARGS__)

#define _trace_event_atomic_with_ids(lvl, class, ctx, id_1, id_2, format, ...)	\
	_log_message(trace_log_filtered, true, lvl, class, ctx, id_1, id_2, format, ##__VA_ARGS__)

#ifndef CONFIG_LIBRARY

#define trace_point(x) platform_trace_point(x)

/** The start of this linker output MUST match the 'ldc_entry_header'
 *  struct defined in the logger program running in user space.
 */
#define _DECLARE_LOG_ENTRY(lvl, format, comp_class, params)	\
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

#define CT_ASSERT(COND, MESSAGE) \
	((void)sizeof(char[1 - 2 * !(COND)]))

#define trace_check_size_uint32(a) \
	CT_ASSERT(sizeof(a) <= sizeof(uint32_t), "error: trace argument is bigger than a uint32_t");

#define STATIC_ASSERT_ARG_SIZE(...) \
	META_MAP(1, trace_check_size_uint32, __VA_ARGS__)

/** _log_message is where the memory-saving dictionary magic described
 * above happens: the "format" string argument is moved to a special
 * linker section and replaced by a &log_entry pointer to it.
 */
#define _log_message(log_func, atomic, lvl, comp_class, ctx, id_1, id_2, format, ...)	\
do {											\
	_DECLARE_LOG_ENTRY(lvl, format, comp_class,					\
			   META_COUNT_VARAGS_BEFORE_COMPILE(__VA_ARGS__));		\
	STATIC_ASSERT_ARG_SIZE(__VA_ARGS__);						\
	STATIC_ASSERT(_TRACE_EVENT_MAX_ARGUMENT_COUNT >=				\
			META_COUNT_VARAGS_BEFORE_COMPILE(__VA_ARGS__),			\
		BASE_LOG_ASSERT_FAIL_MSG						\
	);										\
	log_func(atomic, &log_entry, ctx, lvl, id_1, id_2,				\
		 META_COUNT_VARAGS_BEFORE_COMPILE(__VA_ARGS__), ##__VA_ARGS__);		\
} while (0)

#else /* CONFIG_LIBRARY */

extern int test_bench_trace;
char *get_trace_class(uint32_t trace_class);
#define _log_message(log_func, atomic, level, comp_class, ctx, id_1, id_2, format, ...)	\
do {											\
	(void)ctx;									\
	(void)id_1;									\
	(void)id_2;									\
	if (test_bench_trace) {								\
		char *msg = "%s " format;						\
		fprintf(stderr, msg, get_trace_class(comp_class), ##__VA_ARGS__);	\
		fprintf(stderr, "\n");							\
	}										\
} while (0)

#define trace_point(x)  do {} while (0)

#endif /* CONFIG_LIBRARY */

#else /* CONFIG_TRACE */

#define trace_event_with_ids(class, ctx, id_1, id_2, format, ...)	\
	trace_unused(class, ctx, id_1, id_2, format, ##__VA_ARGS__)
#define trace_event_atomic_with_ids(class, ctx, id_1, id_2, format, ...) \
	trace_unused(class, ctx, id_1, id_2, format, ##__VA_ARGS__)

#define trace_warn_with_ids(class, ctx, id_1, id_2, format, ...)	\
	trace_unused(class, ctx, id_1, id_2, format, ##__VA_ARGS__)
#define trace_warn_atomic_with_ids(class, ctx, id_1, id_2, format, ...)	\
	trace_unused(class, ctx, id_1, id_2, format, ##__VA_ARGS__)

#define trace_point(x)  do {} while (0)

static inline void trace_flush(void) { }
static inline void trace_on(void) { }
static inline void trace_off(void) { }
static inline void trace_init(struct sof *sof) { }
static inline int trace_filter_update(const struct trace_filter *filter)
	{ return 0; }

#endif /* CONFIG_TRACE */

#if CONFIG_TRACEV
/* Enable tr_dbg() statements by defining tracev_...() */
#define tracev_event_with_ids(class, ctx, id_1, id_2, format, ...)	\
	_trace_event_with_ids(LOG_LEVEL_VERBOSE, class,			\
			      ctx, id_1, id_2,				\
			      format, ##__VA_ARGS__)

#define tracev_event_atomic_with_ids(class, ctx, id_1, id_2, format, ...) \
	_trace_event_atomic_with_ids(LOG_LEVEL_VERBOSE, class,		  \
				     ctx, id_1, id_2,			  \
				     format, ##__VA_ARGS__)

#else /* CONFIG_TRACEV */
#define tracev_event_with_ids(class, ctx, id_1, id_2, format, ...)	\
	trace_unused(class, ctx, id_1, id_2, format, ##__VA_ARGS__)
#define tracev_event_atomic_with_ids(class, ctx, id_1, id_2, format, ...) \
	trace_unused(class, ctx, id_1, id_2, format, ##__VA_ARGS__)

#endif /* CONFIG_TRACEV */

/* The _error_ level has 2, 1 or 0 backends depending on Kconfig */
#if CONFIG_TRACEE
/* LOG_LEVEL_CRITICAL messages are duplicated to the mail box */
#define _trace_error_with_ids(class, ctx, id_1, id_2, format, ...)			\
	_log_message(trace_log_filtered, true, LOG_LEVEL_CRITICAL, class, ctx, id_1,	\
		     id_2, format, ##__VA_ARGS__)
#define trace_error_with_ids(class, ctx, id_1, id_2, format, ...)	\
	_trace_error_with_ids(class, ctx, id_1, id_2, format, ##__VA_ARGS__)
#define trace_error_atomic_with_ids(...) trace_error_with_ids(__VA_ARGS__)

#elif CONFIG_TRACE
/* Goes to trace_log_filtered() too but with a downgraded, LOG_INFO level */
#define trace_error_with_ids(...) trace_event_with_ids(__VA_ARGS__)
#define trace_error_atomic_with_ids(...) \
	trace_event_atomic_with_ids(__VA_ARGS__)

#else /* CONFIG_TRACEE, CONFIG_TRACE */
#define trace_error_with_ids(class, ctx, id_1, id_2, format, ...)	\
	trace_unused(class, ctx, id_1, id_2, format, ##__VA_ARGS__)
#define trace_error_atomic_with_ids(class, ctx, id_1, id_2, format, ...) \
	trace_unused(class, ctx, id_1, id_2, format, ##__VA_ARGS__)

#endif /* CONFIG_TRACEE, CONFIG_TRACE */


#define _TRACE_INV_CLASS	TRACE_CLASS_DEPRECATED
#define _TRACE_INV_ID		-1

/**
 * Trace context.
 */
struct tr_ctx {
	const struct sof_uuid_entry *uuid_p;	/**< UUID pointer, use SOF_UUID() to init */
	uint32_t level;				/**< Default log level */
};

#if defined(UNIT_TEST) || defined(__ZEPHYR__)
#define TRACE_CONTEXT_SECTION
#else
#define TRACE_CONTEXT_SECTION __section(".trace_ctx")
#endif

/**
 * Declares trace context.
 * @param ctx_name (Symbol) name.
 * @param uuid UUID pointer, use SOF_UUID() to inititalize.
 * @param default_log_level Default log level.
 */
#define DECLARE_TR_CTX(ctx_name, uuid, default_log_level)	\
	struct tr_ctx ctx_name TRACE_CONTEXT_SECTION = {	\
		.uuid_p = uuid,					\
		.level = default_log_level,			\
	}

/* tracing from device (component, pipeline, dai, ...) */

/** \brief Trace from a device on err level.
 *
 * @param get_ctx_m Macro that can retrieve trace context from dev
 * @param get_id_m Macro that can retrieve device's id0 from the dev
 * @param get_subid_m Macro that can retrieve device's id1 from the dev
 * @param dev Device
 * @param fmt Format followed by parameters
 * @param ... Parameters
 */
#define trace_dev_err(get_ctx_m, get_id_m, get_subid_m, dev, fmt, ...)	\
	trace_error_with_ids(_TRACE_INV_CLASS, get_ctx_m(dev),		\
			     get_id_m(dev), get_subid_m(dev),		\
			     fmt, ##__VA_ARGS__)

/** \brief Trace from a device on warning level. */
#define trace_dev_warn(get_ctx_m, get_id_m, get_subid_m, dev, fmt, ...)	\
	trace_warn_with_ids(_TRACE_INV_CLASS, get_ctx_m(dev),		\
			    get_id_m(dev), get_subid_m(dev),		\
			    fmt, ##__VA_ARGS__)

/** \brief Trace from a device on info level. */
#define trace_dev_info(get_ctx_m, get_id_m, get_subid_m, dev, fmt, ...)	\
	trace_event_with_ids(_TRACE_INV_CLASS, get_ctx_m(dev),		\
			     get_id_m(dev), get_subid_m(dev),		\
			     fmt, ##__VA_ARGS__)

/** \brief Trace from a device on dbg level. */
#define trace_dev_dbg(get_ctx_m, get_id_m, get_subid_m, dev, fmt, ...)	\
	tracev_event_with_ids(_TRACE_INV_CLASS,				\
			      get_ctx_m(dev), get_id_m(dev),		\
			      get_subid_m(dev), fmt, ##__VA_ARGS__)

/* tracing from infrastructure part */

#define tr_err(ctx, fmt, ...) \
	trace_error_with_ids(_TRACE_INV_CLASS, ctx, \
			     _TRACE_INV_ID, _TRACE_INV_ID, fmt, ##__VA_ARGS__)

#define tr_err_atomic(ctx, fmt, ...) \
	trace_error_atomic_with_ids(_TRACE_INV_CLASS, ctx, \
				    _TRACE_INV_ID, _TRACE_INV_ID, \
				    fmt, ##__VA_ARGS__)

#define tr_warn(ctx, fmt, ...) \
	trace_warn_with_ids(_TRACE_INV_CLASS, ctx, \
			    _TRACE_INV_ID, _TRACE_INV_ID, fmt, ##__VA_ARGS__)

#define tr_warn_atomic(ctx, fmt, ...) \
	trace_warn_atomic_with_ids(_TRACE_INV_CLASS, ctx, \
				   _TRACE_INV_ID, _TRACE_INV_ID, \
				   fmt, ##__VA_ARGS__)

#define tr_info(ctx, fmt, ...) \
	trace_event_with_ids(_TRACE_INV_CLASS, ctx, \
			     _TRACE_INV_ID, _TRACE_INV_ID, fmt, ##__VA_ARGS__)

#define tr_info_atomic(ctx, fmt, ...) \
	trace_event_atomic_with_ids(_TRACE_INV_CLASS, ctx, \
				    _TRACE_INV_ID, _TRACE_INV_ID, \
				    fmt, ##__VA_ARGS__)

/* tracev_ output depends on CONFIG_TRACEV=y */
#define tr_dbg(ctx, fmt, ...) \
	tracev_event_with_ids(_TRACE_INV_CLASS, ctx, \
			      _TRACE_INV_ID, _TRACE_INV_ID, fmt, ##__VA_ARGS__)

#define tr_dbg_atomic(ctx, fmt, ...) \
	tracev_event_atomic_with_ids(_TRACE_INV_CLASS, ctx, \
				     _TRACE_INV_ID, _TRACE_INV_ID, \
				     fmt, ##__VA_ARGS__)

#endif /* __SOF_TRACE_TRACE_H__ */
