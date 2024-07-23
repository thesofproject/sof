/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright(c) 2024 Intel Corporation. All rights reserved.
 *
 * Author: Adrian Warecki <adrian.warecki@intel.com>
 */

#ifndef __MODULE_MODULE_LOGGER_H__
#define __MODULE_MODULE_LOGGER_H__

/* Log level priority enumeration. */
enum log_priority {
	L_CRITICAL,	/* Critical message. */
	L_ERROR,	/* Error message. */
	L_HIGH,		/* High importance log level. */
	L_WARNING,	/* Warning message. */
	L_MEDIUM,	/* Medium importance log level. */
	L_LOW,		/* Low importance log level. */
	L_INFO,		/* Information. */
	L_VERBOSE,	/* Verbose message. */
	L_DEBUG,
	L_MAX,
};

/* struct log_handle identifies the log message sender.
 *
 * struct log_handle instance is passed to the system_service::log_message function.
 * This struct should not be used directly.
 */
struct log_handle;

#endif /* __MODULE_MODULE_LOGGER_H__ */
