/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2018 Intel Corporation. All rights reserved.
 *
 * Author: Bartosz Kokoszko <bartoszx.kokoszko@linux.intel.com>
 *         Artur Kloniecki <arturx.kloniecki@linux.intel.com>
 */

/**
 * \file include/user/trace.h
 * \brief User-space ABI log levels and binary trace packet header definitions.
 * \ingroup trace_logging_api
 */

#ifndef __USER_TRACE_H__
#define __USER_TRACE_H__

#include <stdint.h>

/** Legacy trace event class (deprecated) */
#define TRACE_CLASS_DEPRECATED	(0)

#define LOG_ENABLE		1  /**< Enable active logging */
#define LOG_DISABLE		0  /**< Disable logging */

/** \name Logging Severity Levels
 *  Severity ranking for runtime message filtering.
 *  @{
 */
#define LOG_LEVEL_CRITICAL	1  /**< Critical failure or unrecoverable error (fatal) */
#define LOG_LEVEL_ERROR		LOG_LEVEL_CRITICAL /**< Alias for critical errors */
#define LOG_LEVEL_WARNING	2  /**< Warning conditions requiring attention */
#define LOG_LEVEL_INFO		3  /**< Normal informational operational messages */
#define LOG_LEVEL_DEBUG		4  /**< Detailed debug logging */
#define LOG_LEVEL_VERBOSE	LOG_LEVEL_DEBUG    /**< Verbose debug logging */
/** @} */

/** Bitfield width of component and pipeline trace identifiers */
#define TRACE_ID_LENGTH 12

/**
 * \brief Binary log packet header preceding variable arguments in DMA trace stream.
 * \ingroup trace_logging_api
 *
 * Each dictionary log packet in the DMA buffer begins with this header, followed
 * by up to 4 parameters (uint32_t values) matching the format string referenced
 * by log_entry_address in the ELF dictionary (.ldc).
 */
struct log_entry_header {
	uint32_t uid;				/**< Unique log entry sequence number */
	uint32_t id_0 : TRACE_ID_LENGTH;	/**< Primary identifier (e.g. Pipeline ID) */
	uint32_t id_1 : TRACE_ID_LENGTH;	/**< Secondary identifier (e.g. Component ID) */
	uint32_t core_id : 8;			/**< Reporting DSP core identifier */

	uint64_t timestamp;			/**< High-resolution hardware clock timestamp in DSP ticks */
	uint32_t log_entry_address;		/**< Linker address of static log entry descriptor in firmware ELF */
} __attribute__((packed));

#endif /* __USER_TRACE_H__ */
