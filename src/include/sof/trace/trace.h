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
#include <rtos/sof.h>
#include <sof/trace/preproc.h>

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#if CONFIG_LIBRARY
#include <stdio.h>
#endif

#ifdef __ZEPHYR__
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#endif

#include "trace_nonzephyr.h"

struct sof;
struct trace;
struct tr_ctx;

/* bootloader trace values */
#define TRACE_BOOT_LDR_ENTRY		0x100
#define TRACE_BOOT_LDR_HPSRAM		0x110
#define TRACE_BOOT_LDR_MANIFEST		0x120
#define TRACE_BOOT_LDR_LPSRAM		0x130
#define TRACE_BOOT_LDR_L1DRAM		0x140
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

static inline struct trace *trace_get(void)
{
	return sof_get()->trace;
}

struct trace_filter {
	uint32_t uuid_id;	/**< type id, or 0 when not important */
	int32_t comp_id;	/**< component id or -1 when not important */
	int32_t pipe_id;	/**< pipeline id or -1 when not important */
	int32_t log_level;	/**< new log level value */
};

/** The start of this linker output MUST match the 'ldc_entry_header'
 *  struct defined in the logger program running in user space.
 */
#define _DECLARE_LOG_ENTRY(lvl, format, comp_class, n_params)	\
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
		n_params,					\
		__LINE__,					\
		sizeof(RELATIVE_FILE),				\
		sizeof(format),					\
		RELATIVE_FILE,					\
		format						\
	}

#if CONFIG_TRACE

#include <stdarg.h>
#include <user/trace.h> /* LOG_LEVEL_... */

void trace_flush_dma_to_mbox(void);
void trace_on(void);
void trace_off(void);
void trace_init(struct sof *sof);

struct sof_ipc_trace_filter_elem *trace_filter_fill(struct sof_ipc_trace_filter_elem *elem,
						    struct sof_ipc_trace_filter_elem *end,
						    struct trace_filter *filter);
int trace_filter_update(const struct trace_filter *elem);

/**
 * Appends one SOF dictionary entry and log statement to the ring buffer
 * implementing the 'etrace' in shared memory.
 *
 * @param atomic_context Take the trace->lock if false.
 * @param log_entry_pointer dictionary index produced by the
 *        _DECLARE_LOG_ENTRY macro.
 * @param n_args number of va_args
 */
void mtrace_dict_entry(bool atomic_context, uint32_t log_entry_pointer, int n_args, ...);

/** Posts a fully prepared log header + log entry */
void mtrace_event(const char *complete_packet, uint32_t length);

/* _log_message() */

#ifdef CONFIG_LIBRARY

#include <sys/time.h>

/* trace level used on host configurations */
extern int host_trace_level;

char *get_trace_class(uint32_t trace_class);

#define trace_point(x)  do {} while (0)

#else  /* CONFIG_LIBRARY */

#define trace_point(x) platform_trace_point(x)

#endif /* CONFIG_LIBRARY */

#else /* CONFIG_TRACE */

#define trace_point(x)  do {} while (0)

static inline void trace_flush_dma_to_mbox(void) { }
static inline void trace_on(void) { }
static inline void trace_off(void) { }
static inline void trace_init(struct sof *sof) { }
static inline int trace_filter_update(const struct trace_filter *filter)
	{ return 0; }

#endif /* CONFIG_TRACE */

/** Default value when there is no specific pipeline, dev, dai, etc. */
#define _TRACE_INV_ID		-1

/** This has been replaced in commits 6ce635aa82 and earlier by the
 * DECLARE_TR_CTX, tr_ctx and component UUID system below
 */
#define _TRACE_INV_CLASS	TRACE_CLASS_DEPRECATED

/**
 * Trace context.
 */
struct tr_ctx {
	const struct sof_uuid_entry *uuid_p;	/**< UUID pointer, use SOF_UUID() to init */
	uint32_t level;				/**< Default log level */
};

#if defined(UNIT_TEST)
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

/* tracing from infrastructure part */

#define tr_err_atomic(fmt, ...) \
	trace_error_atomic_with_ids_nonzephyr(_TRACE_INV_CLASS,	\
				    _TRACE_INV_ID, _TRACE_INV_ID, \
				    fmt, ##__VA_ARGS__)

#define tr_warn_atomic(fmt, ...) \
	trace_warn_atomic_with_ids_nonzephyr(_TRACE_INV_CLASS,	\
				   _TRACE_INV_ID, _TRACE_INV_ID, \
				   fmt, ##__VA_ARGS__)

#define tr_info_atomic(fmt, ...) \
	trace_event_atomic_with_ids_nonzephyr(_TRACE_INV_CLASS,	\
				    _TRACE_INV_ID, _TRACE_INV_ID, \
				    fmt, ##__VA_ARGS__)

#define tr_dbg_atomic(fmt, ...) \
	tracev_event_atomic_with_ids_nonzephyr(_TRACE_INV_CLASS,	\
				     _TRACE_INV_ID, _TRACE_INV_ID, \
				     fmt, ##__VA_ARGS__)

#if defined(__ZEPHYR__) && defined(CONFIG_ZEPHYR_LOG)

#define tr_err(ctx, fmt, ...) LOG_ERR(fmt, ##__VA_ARGS__)

#define tr_warn(ctx, fmt, ...) LOG_WRN(fmt, ##__VA_ARGS__)

#define tr_info(ctx, fmt, ...) LOG_INF(fmt, ##__VA_ARGS__)

#define tr_dbg(ctx, fmt, ...) LOG_DBG(fmt, ##__VA_ARGS__)

#else

/* Only define these two macros for XTOS to avoid the collision with
 * zephyr/include/zephyr/logging/log.h
 */
#ifndef __ZEPHYR__
#define LOG_MODULE_REGISTER(ctx, level)
#define LOG_MODULE_DECLARE(ctx, level)
#endif

#define tr_err(ctx, fmt, ...) \
	trace_error_with_ids_nonzephyr(_TRACE_INV_CLASS,	\
			     _TRACE_INV_ID, _TRACE_INV_ID, fmt, ##__VA_ARGS__)

#define tr_warn(ctx, fmt, ...) \
	trace_warn_with_ids_nonzephyr(_TRACE_INV_CLASS,	\
			    _TRACE_INV_ID, _TRACE_INV_ID, fmt, ##__VA_ARGS__)

#define tr_info(ctx, fmt, ...) \
	trace_event_with_ids_nonzephyr(_TRACE_INV_CLASS,	\
			     _TRACE_INV_ID, _TRACE_INV_ID, fmt, ##__VA_ARGS__)

/* tracev_ output depends on CONFIG_TRACEV=y */
#define tr_dbg(ctx, fmt, ...) \
	tracev_event_with_ids_nonzephyr(_TRACE_INV_CLASS,	\
			      _TRACE_INV_ID, _TRACE_INV_ID, fmt, ##__VA_ARGS__)

#endif

#if CONFIG_TRACE

/** Direct, low-level access to mbox / shared memory logging when DMA
 * tracing is either not initialized yet or disabled or found broken for
 * any reason.
 * To keep it simpler than and with minimal dependencies on
 * the huge number of lines above, this does not check arguments at compile
 * time.
 * There is neither log level filtering, throttling or any other
 * advanced feature.
 */
#define mtrace_printf(log_level, format_str, ...)			\
	do {								\
		STATIC_ASSERT(META_COUNT_VARAGS_BEFORE_COMPILE(__VA_ARGS__)  \
			      <= _TRACE_EVENT_MAX_ARGUMENT_COUNT,	\
			      too_many_mtrace_printf_arguments);	\
		_DECLARE_LOG_ENTRY(log_level, format_str, _TRACE_INV_CLASS, \
				   META_COUNT_VARAGS_BEFORE_COMPILE(__VA_ARGS__)); \
		mtrace_dict_entry(true, (uint32_t)&log_entry,			\
				  META_COUNT_VARAGS_BEFORE_COMPILE(__VA_ARGS__), \
				  ##__VA_ARGS__);			\
	} while (0)


#else

static inline void mtrace_printf(int log_level, const char *format_str, ...)
{
};

#endif /* CONFIG_TRACE */

#endif /* __SOF_TRACE_TRACE_H__ */
