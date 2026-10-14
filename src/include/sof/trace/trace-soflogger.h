/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016,2024 Intel Corporation.
 */

/**
 * \file include/sof/trace/trace-soflogger.h
 * \brief Dictionary-based trace logging engine, log filtering, and mailbox trace fallback.
 * \ingroup trace_logging_api
 */

#ifndef __SOF_TRACE_TRACE_SOFLOGGER_H__
#define __SOF_TRACE_TRACE_SOFLOGGER_H__

#if !CONFIG_LIBRARY
#include <platform/trace/trace.h>
#endif

#if CONFIG_LIBRARY
#include <stdio.h>
#endif

struct sof;
struct trace;
struct tr_ctx;

/**
 * \brief Runtime trace filter criteria for components and pipelines.
 * \ingroup trace_logging_api
 */
struct trace_filter {
	uint32_t uuid_id;	/**< Component UUID type identifier, or 0 for wildcard */
	int32_t comp_id;	/**< Component ID, or -1 for wildcard */
	int32_t pipe_id;	/**< Pipeline ID, or -1 for wildcard */
	int32_t log_level;	/**< Target log level threshold (LOG_LEVEL_*) */
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

#define _TRACE_EVENT_MAX_ARGUMENT_COUNT 4

/**
 * \brief Retrieve global trace subsystem context from SOF runtime.
 * \ingroup trace_logging_api
 * @return Pointer to struct trace.
 */
static inline struct trace *trace_get(void)
{
	return sof_get()->trace;
}

#include <stdarg.h>
#include <user/trace.h> /* LOG_LEVEL_... */

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

/**
 * \brief Flush buffered trace data from DMA buffer to Mailbox Window.
 * \ingroup trace_logging_api
 */
void trace_flush_dma_to_mbox(void);

/**
 * \brief Globally resume trace event logging.
 * \ingroup trace_logging_api
 */
void trace_on(void);

/**
 * \brief Globally pause trace event logging.
 * \ingroup trace_logging_api
 */
void trace_off(void);

/**
 * \brief Initialize the trace subsystem and allocate global trace buffers.
 * \ingroup trace_logging_api
 * @param[in,out] sof Global SOF runtime context.
 */
void trace_init(struct sof *sof);

/**
 * \brief Function pointer type for formatted trace logger backends.
 * \ingroup trace_logging_api
 * @param[in] send_atomic True if logging from atomic or interrupt context.
 * @param[in] log_entry Pointer to static log entry descriptor.
 * @param[in] ctx Pointer to caller trace context.
 * @param[in] lvl Severity log level (LOG_LEVEL_*).
 * @param[in] id_1 Primary component/pipe identifier.
 * @param[in] id_2 Secondary component/subid identifier.
 * @param[in] arg_count Number of variable arguments.
 * @param[in] args Variable arguments list.
 */
typedef void (*log_func_t)(bool send_atomic, const void *log_entry, const struct tr_ctx *ctx,
			   uint32_t lvl, uint32_t id_1, uint32_t id_2, int arg_count, va_list args);

/**
 * \brief Emit a filtered dictionary trace message.
 * \ingroup trace_logging_api
 * @param[in] send_atomic True if logging from atomic context.
 * @param[in] log_entry Pointer to static log entry.
 * @param[in] ctx Trace context.
 * @param[in] lvl Log level.
 * @param[in] id_1 Primary identifier.
 * @param[in] id_2 Secondary identifier.
 * @param[in] arg_count Argument count.
 * @param[in] args Argument list.
 */
void trace_log_filtered(bool send_atomic, const void *log_entry, const struct tr_ctx *ctx,
			uint32_t lvl, uint32_t id_1, uint32_t id_2, int arg_count, va_list args);

/**
 * \brief Emit an unfiltered dictionary trace message directly to buffers.
 * \ingroup trace_logging_api
 * @param[in] send_atomic True if logging from atomic context.
 * @param[in] log_entry Pointer to static log entry.
 * @param[in] ctx Trace context.
 * @param[in] lvl Log level.
 * @param[in] id_1 Primary identifier.
 * @param[in] id_2 Secondary identifier.
 * @param[in] arg_count Argument count.
 * @param[in] args Argument list.
 */
void trace_log_unfiltered(bool send_atomic, const void *log_entry, const struct tr_ctx *ctx,
			  uint32_t lvl, uint32_t id_1, uint32_t id_2, int arg_count, va_list args);

/**
 * \brief Populate an IPC trace filter element array.
 * \ingroup trace_logging_api
 * @param[in,out] elem Pointer to current output filter element.
 * @param[in] end Boundary pointer of element array.
 * @param[in] filter Source trace filter configuration.
 * @return Pointer to next available filter element slot.
 */
struct sof_ipc_trace_filter_elem *trace_filter_fill(struct sof_ipc_trace_filter_elem *elem,
						    struct sof_ipc_trace_filter_elem *end,
						    struct trace_filter *filter);

/**
 * \brief Update runtime trace filtering configuration from IPC payload.
 * \ingroup trace_logging_api
 * @param[in] elem Trace filter descriptor to apply.
 * @return 0 on success, or negative error code on failure.
 */
int trace_filter_update(const struct trace_filter *elem);

#define _trace_event_with_ids(lvl, class, ctx, id_1, id_2, format, ...)	\
	_log_message(trace_log_filtered, false, lvl, class, ctx, id_1, id_2, format, ##__VA_ARGS__)

#define _trace_event_atomic_with_ids(lvl, class, ctx, id_1, id_2, format, ...)	\
	_log_message(trace_log_filtered, true, lvl, class, ctx, id_1, id_2, format, ##__VA_ARGS__)

/**
 * \brief Appends one SOF dictionary entry and log statement to shared memory mtrace.
 * \ingroup trace_logging_api
 * @param atomic_context Take the trace->lock if false.
 * @param log_entry_pointer Dictionary index produced by _DECLARE_LOG_ENTRY macro.
 * @param n_args Number of variable arguments.
 */
void mtrace_dict_entry(bool atomic_context, uint32_t log_entry_pointer, int n_args, ...);

/**
 * \brief Posts a fully prepared log header and log entry packet to mtrace buffer.
 * \ingroup trace_logging_api
 * @param complete_packet Pointer to prepared log packet buffer.
 * @param length Packet size in bytes.
 */
void mtrace_event(const char *complete_packet, uint32_t length);

#ifdef CONFIG_TRACEM /* Send everything to shared memory too */
#  ifdef __ZEPHYR__
/* We don't use Zephyr's dictionary yet so there's not enough space for
 * DEBUG messages
 */
#    define MTRACE_DUPLICATION_LEVEL LOG_LEVEL_INFO
#  else
#    define MTRACE_DUPLICATION_LEVEL LOG_LEVEL_DEBUG
#  endif
#else /* copy only ERRORS */
#  define MTRACE_DUPLICATION_LEVEL LOG_LEVEL_ERROR
#endif /* CONFIG_TRACEM */

/**
 * \brief Internal dictionary logger formatting and dispatch helper.
 * \ingroup trace_logging_api
 * @param sofdict_logf Backend log emission callback function.
 * @param atomic True if executed in atomic context.
 * @param log_entry Pointer to static log entry.
 * @param ctx Trace context.
 * @param lvl Log severity level.
 * @param id_1 Primary identifier.
 * @param id_2 Secondary identifier.
 * @param arg_count Number of format arguments.
 */
void _log_sofdict(log_func_t sofdict_logf, bool atomic, const void *log_entry,
		  const struct tr_ctx *ctx, const uint32_t lvl,
		  uint32_t id_1, uint32_t id_2, int arg_count, ...);

/* _log_message() */

#ifdef CONFIG_LIBRARY

#include <sys/time.h>

/* trace level used on host configurations */
extern int host_trace_level;

char *get_trace_class(uint32_t trace_class);
#define _log_message(ignored_log_func, atomic, level, comp_class, ctx, id_1, id_2, format, ...)	\
do {								\
	(void)ctx;						\
	(void)id_1;						\
	(void)id_2;						\
	struct timeval tv;					\
	char slevel[5][4] = {"dis", "err", "wrn", "inf", "dbg"}; \
	char *msg = "(%s:%d) " format;				\
	if (level <= host_trace_level) {			\
		gettimeofday(&tv, NULL);				\
		fprintf(stderr, "trace: [%ld.%6.6ld] <%s> ", tv.tv_sec, tv.tv_usec, \
			slevel[level]);					\
		fprintf(stderr, msg, strrchr(__FILE__, '/') + 1,	\
			__LINE__, ##__VA_ARGS__);			\
		fprintf(stderr, "\n");					\
	}							\
} while (0)

#define trace_point(x)  do {} while (0)

#else  /* CONFIG_LIBRARY */

#define trace_point(x) platform_trace_point(x)

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
 * linker section and replaced by a &log_entry pointer to it. This must
 * be a macro for the source location to be meaningful.
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
	_log_sofdict(log_func, atomic, &log_entry, ctx, lvl, id_1, id_2, \
		     META_COUNT_VARAGS_BEFORE_COMPILE(__VA_ARGS__), ##__VA_ARGS__); \
	_log_nodict(atomic, META_COUNT_VARAGS_BEFORE_COMPILE(__VA_ARGS__), \
		    lvl, format, ##__VA_ARGS__);			\
} while (0)

#ifdef __ZEPHYR__
/* Just like XTOS, only the most urgent messages go to limited
 * shared memory.
 */
#define _log_nodict(atomic, arg_count, lvl, format, ...)		\
do {									\
	if ((lvl) <= MTRACE_DUPLICATION_LEVEL)				\
		printk("%llu " format "\n", k_cycle_get_64(),		\
		       ##__VA_ARGS__);					\
} while (0)
#else
#define _log_nodict(atomic, n_args, lvl, format, ...)
#endif

#endif /* CONFIG_LIBRARY */

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

#endif /* CONFIG_TRACEE, CONFIG_TRACE */

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

#define tr_err_atomic(ctx, fmt, ...) \
	trace_error_atomic_with_ids(_TRACE_INV_CLASS, ctx, \
				    _TRACE_INV_ID, _TRACE_INV_ID, \
				    fmt, ##__VA_ARGS__)

#define tr_warn_atomic(ctx, fmt, ...) \
	trace_warn_atomic_with_ids(_TRACE_INV_CLASS, ctx, \
				   _TRACE_INV_ID, _TRACE_INV_ID, \
				   fmt, ##__VA_ARGS__)

#define tr_info_atomic(ctx, fmt, ...) \
	trace_event_atomic_with_ids(_TRACE_INV_CLASS, ctx, \
				    _TRACE_INV_ID, _TRACE_INV_ID, \
				    fmt, ##__VA_ARGS__)

#define tr_dbg_atomic(ctx, fmt, ...) \
	tracev_event_atomic_with_ids(_TRACE_INV_CLASS, ctx, \
				     _TRACE_INV_ID, _TRACE_INV_ID, \
				     fmt, ##__VA_ARGS__)

#define tr_err(ctx, fmt, ...) \
	trace_error_with_ids(_TRACE_INV_CLASS, ctx, \
			     _TRACE_INV_ID, _TRACE_INV_ID, fmt, ##__VA_ARGS__)

#define tr_warn(ctx, fmt, ...) \
	trace_warn_with_ids(_TRACE_INV_CLASS, ctx, \
			    _TRACE_INV_ID, _TRACE_INV_ID, fmt, ##__VA_ARGS__)

#define tr_info(ctx, fmt, ...) \
	trace_event_with_ids(_TRACE_INV_CLASS, ctx, \
			     _TRACE_INV_ID, _TRACE_INV_ID, fmt, ##__VA_ARGS__)

/* tracev_ output depends on CONFIG_TRACEV=y */
#define tr_dbg(ctx, fmt, ...) \
	tracev_event_with_ids(_TRACE_INV_CLASS, ctx, \
			      _TRACE_INV_ID, _TRACE_INV_ID, fmt, ##__VA_ARGS__)

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

#endif
