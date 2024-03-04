/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2024 Intel Corporation. All rights reserved.
 *
 * Author: Baofeng Tian <baofeng.tian@intel.com>
 */

#ifndef __SOF_TRACE_NONZEPHYR_H__
#define __SOF_TRACE_NONZEPHYR_H__

#include <sof/trace/preproc.h>

#define _TRACE_EVENT_MAX_ARGUMENT_COUNT 4

/* Silences compiler warnings about unused variables */
#define trace_unused_nonzephyr(class, id_1, id_2, format, ...) \
	SOF_TRACE_UNUSED(id_1, id_2, ##__VA_ARGS__)

#if CONFIG_TRACE

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
#define trace_event_with_ids_nonzephyr(class, id_1, id_2, format, ...)	\
	_trace_event_with_ids_nonzephyr(LOG_LEVEL_INFO, class, id_1, id_2,	\
					format, ##__VA_ARGS__)

#define trace_event_atomic_with_ids_nonzephyr(class, id_1, id_2, format, ...)     \
	_trace_event_atomic_with_ids_nonzephyr(LOG_LEVEL_INFO, class, id_1, id_2, \
					       format, ##__VA_ARGS__)

#define trace_warn_with_ids_nonzephyr(class, id_1, id_2, format, ...)	 \
	_trace_event_with_ids_nonzephyr(LOG_LEVEL_WARNING, class, id_1, id_2, \
					format, ##__VA_ARGS__)

#define trace_warn_atomic_with_ids_nonzephyr(class, id_1, id_2, format, ...)	\
	_trace_event_atomic_with_ids_nonzephyr(LOG_LEVEL_WARNING, class,		\
					       id_1, id_2,			\
					       format, ##__VA_ARGS__)

/* All tracing macros in this file end up calling these functions in the end. */
typedef void (*log_func_t_nonzephyr)(bool send_atomic, const void *log_entry,
				     uint32_t lvl, uint32_t id_1, uint32_t id_2,
				     int arg_count, va_list args);

void trace_log_filtered_nonzephyr(bool send_atomic, const void *log_entry,
				  uint32_t lvl, uint32_t id_1, uint32_t id_2,
				  int arg_count, va_list args);
void trace_log_unfiltered_nonzephyr(bool send_atomic, const void *log_entry,
				    uint32_t lvl, uint32_t id_1, uint32_t id_2,
				    int arg_count, va_list args);

#define _trace_event_with_ids_nonzephyr(lvl, class, id_1, id_2, format, ...)	\
	_log_message_nonzephyr(trace_log_filtered_nonzephyr, false, lvl, class, id_1,	\
			       id_2, format, ##__VA_ARGS__)

#define _trace_event_atomic_with_ids_nonzephyr(lvl, class, id_1, id_2, format, ...)	\
	_log_message_nonzephyr(trace_log_filtered_nonzephyr, true, lvl, class, id_1,	\
			       id_2, format, ##__VA_ARGS__)

/* This function is _not_ passed the format string to save space */
void _log_sofdict_nonzephyr(log_func_t_nonzephyr sofdict_logf, bool atomic, const void *log_entry,
			    const uint32_t lvl,
			    uint32_t id_1, uint32_t id_2, int arg_count, ...);

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

/* _log_message() */

#ifdef CONFIG_LIBRARY

#include <sys/time.h>

/* trace level used on host configurations */
extern int host_trace_level;

#define _log_message_nonzephyr(ignored_log_func, atomic, level, comp_class, id_1, id_2,	\
			       format, ...)	\
do {								\
	(void)id_1;						\
	(void)id_2;						\
	struct timeval tv;					\
	char *msg = "(%s:%d) " format;				\
	if (level >= host_trace_level) {			\
		gettimeofday(&tv, NULL);				\
		fprintf(stderr, "%ld.%6.6ld:", tv.tv_sec, tv.tv_usec);	\
		fprintf(stderr, msg, strrchr(__FILE__, '/') + 1,	\
			__LINE__, ##__VA_ARGS__);			\
		fprintf(stderr, "\n");					\
	}							\
} while (0)

#else  /* CONFIG_LIBRARY */

#define BASE_LOG_ASSERT_FAIL_MSG \
unsupported_amount_of_params_in_trace_event\
_thrown_from_macro_BASE_LOG_in_trace_h

#define CT_ASSERT(COND, MESSAGE) \
	((void)sizeof(char[1 - 2 * !(COND)]))

#define trace_check_size_uint32(a) \
	CT_ASSERT(sizeof(a) <= sizeof(uint32_t), "error: trace argument is bigger than a uint32_t");

#define STATIC_ASSERT_ARG_SIZE(...) \
	META_MAP(1, trace_check_size_uint32, __VA_ARGS__)

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

/** _log_message is where the memory-saving dictionary magic described
 * above happens: the "format" string argument is moved to a special
 * linker section and replaced by a &log_entry pointer to it. This must
 * be a macro for the source location to be meaningful.
 */
#define _log_message_nonzephyr(log_func, atomic, lvl, comp_class, id_1, id_2, format, ...)	\
do {											\
	_DECLARE_LOG_ENTRY(lvl, format, comp_class,					\
			   META_COUNT_VARAGS_BEFORE_COMPILE(__VA_ARGS__));		\
	STATIC_ASSERT_ARG_SIZE(__VA_ARGS__);						\
	STATIC_ASSERT(_TRACE_EVENT_MAX_ARGUMENT_COUNT >=				\
			META_COUNT_VARAGS_BEFORE_COMPILE(__VA_ARGS__),			\
		BASE_LOG_ASSERT_FAIL_MSG						\
	);										\
	_log_sofdict_nonzephyr(log_func, atomic, &log_entry, lvl, id_1, id_2, \
		     META_COUNT_VARAGS_BEFORE_COMPILE(__VA_ARGS__), ##__VA_ARGS__); \
	_log_nodict(atomic, META_COUNT_VARAGS_BEFORE_COMPILE(__VA_ARGS__), \
		    lvl, format, ##__VA_ARGS__);			\
} while (0)

#endif /* CONFIG_LIBRARY */

#else /* CONFIG_TRACE */

#define trace_event_with_ids_nonzephyr(class, id_1, id_2, format, ...)	\
	trace_unused_nonzephyr(class, id_1, id_2, format, ##__VA_ARGS__)
#define trace_event_atomic_with_ids_nonzephyr(class, id_1, id_2, format, ...) \
	trace_unused_nonzephyr(class, id_1, id_2, format, ##__VA_ARGS__)

#define trace_warn_with_ids_nonzephyr(class, id_1, id_2, format, ...)	\
	trace_unused_nonzephyr(class, id_1, id_2, format, ##__VA_ARGS__)
#define trace_warn_atomic_with_ids_nonzephyr(class, id_1, id_2, format, ...)	\
	trace_unused_nonzephyr(class, id_1, id_2, format, ##__VA_ARGS__)

#endif /* CONFIG_TRACE */

#if CONFIG_TRACEV
/* Enable tr_dbg() statements by defining tracev_...() */
#define tracev_event_with_ids_nonzephyr(class, id_1, id_2, format, ...)	\
	_trace_event_with_ids_nonzephyr(LOG_LEVEL_VERBOSE, class,			\
					id_1, id_2,				\
					format, ##__VA_ARGS__)

#define tracev_event_atomic_with_ids_nonzephyr(class, id_1, id_2, format, ...) \
	_trace_event_atomic_with_ids_nonzephyr(LOG_LEVEL_VERBOSE, class,		  \
					       id_1, id_2,			  \
					       format, ##__VA_ARGS__)

#else /* CONFIG_TRACEV */
#define tracev_event_with_ids_nonzephyr(class, id_1, id_2, format, ...)	\
	trace_unused_nonzephyr(class, id_1, id_2, format, ##__VA_ARGS__)
#define tracev_event_atomic_with_ids_nonzephyr(class, id_1, id_2, format, ...) \
	trace_unused_nonzephyr(class, id_1, id_2, format, ##__VA_ARGS__)

#endif /* CONFIG_TRACEV */

/* The _error_ level has 2, 1 or 0 backends depending on Kconfig */
#if CONFIG_TRACEE
/* LOG_LEVEL_CRITICAL messages are duplicated to the mail box */
#define _trace_error_with_ids_nonzephyr(class, id_1, id_2, format, ...)			\
	_log_message_nonzephyr(trace_log_filtered_nonzephyr, true, LOG_LEVEL_CRITICAL, class,	\
			       id_1, id_2, format, ##__VA_ARGS__)
#define trace_error_with_ids_nonzephyr(class, id_1, id_2, format, ...)	\
	_trace_error_with_ids_nonzephyr(class, id_1, id_2, format, ##__VA_ARGS__)
#define trace_error_atomic_with_ids_nonzephyr(...) trace_error_with_ids_nonzephyr(__VA_ARGS__)

#elif CONFIG_TRACE
/* Goes to trace_log_filtered() too but with a downgraded, LOG_INFO level */
#define trace_error_with_ids_nonzephyr(...) trace_event_with_ids_nonzephyr(__VA_ARGS__)
#define trace_error_atomic_with_ids_nonzephyr(...) \
	trace_event_atomic_with_ids_nonzephyr(__VA_ARGS__)

#else /* CONFIG_TRACEE, CONFIG_TRACE */
#define trace_error_with_ids_nonzephyr(class, id_1, id_2, format, ...)	\
	trace_unused_nonzephyr(class, id_1, id_2, format, ##__VA_ARGS__)
#define trace_error_atomic_with_ids_nonzephyr(class, id_1, id_2, format, ...) \
	trace_unused_nonzephyr(class, id_1, id_2, format, ##__VA_ARGS__)

#endif /* CONFIG_TRACEE, CONFIG_TRACE */

/* tracing from device (component, pipeline, dai, ...) */

/** \brief Trace from a device on err level.
 *
 * @param get_id_m Macro that can retrieve device's id0 from the dev
 * @param get_subid_m Macro that can retrieve device's id1 from the dev
 * @param dev Device
 * @param fmt Format followed by parameters
 * @param ... Parameters
 */
#define trace_dev_err_nonzephyr(get_id_m, get_subid_m, dev, fmt, ...)	\
	trace_error_with_ids_nonzephyr(_TRACE_INV_CLASS,		\
				       get_id_m(dev), get_subid_m(dev),		\
				       fmt, ##__VA_ARGS__)

/** \brief Trace from a device on warning level. */
#define trace_dev_warn_nonzephyr(get_id_m, get_subid_m, dev, fmt, ...)	\
	trace_warn_with_ids_nonzephyr(_TRACE_INV_CLASS,			\
				      get_id_m(dev), get_subid_m(dev),		\
				      fmt, ##__VA_ARGS__)

/** \brief Trace from a device on info level. */
#define trace_dev_info_nonzephyr(get_id_m, get_subid_m, dev, fmt, ...)	\
	trace_event_with_ids_nonzephyr(_TRACE_INV_CLASS,		\
				       get_id_m(dev), get_subid_m(dev),		\
				       fmt, ##__VA_ARGS__)

/** \brief Trace from a device on dbg level. */
#define trace_dev_dbg_nonzephyr(get_id_m, get_subid_m, dev, fmt, ...)	\
	tracev_event_with_ids_nonzephyr(_TRACE_INV_CLASS,				\
					get_id_m(dev),		\
					get_subid_m(dev), fmt, ##__VA_ARGS__)
#endif /* __SOF_TRACE_NONZEPHYR_H__ */
