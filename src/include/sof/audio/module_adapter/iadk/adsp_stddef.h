/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 */

#ifndef _ADSP_STDDEF_H_
#define _ADSP_STDDEF_H_

#include <stddef.h>
#include <stdint.h>
#include <sys/util.h>
#include <user/trace.h>
#include <rtos/string.h>

#ifdef __XTENSA__
  #define RESTRICT __restrict
#else
  #define RESTRICT
#endif

/*! Log level priority enumeration. */
typedef enum log_priority {
	/*! Critical message. */
	L_CRITICAL = LOG_LEVEL_CRITICAL,
	/*! Error message. */
	L_ERROR = LOG_LEVEL_ERROR,
	/*! High importance log level. */
	L_HIGH = LOG_LEVEL_ERROR,
	/*! Warning message. */
	L_WARNING = LOG_LEVEL_WARNING,
	/*! Medium importance log level. */
	L_MEDIUM = LOG_LEVEL_WARNING,
	/*! Low importance log level. */
	L_LOW = LOG_LEVEL_INFO,
	/*! Information. */
	L_INFO = LOG_LEVEL_INFO,
	/*! Verbose message. */
	L_VERBOSE = LOG_LEVEL_VERBOSE,
	L_DEBUG   = LOG_LEVEL_DEBUG,
	L_MAX     = LOG_LEVEL_DEBUG,
} AdspLogPriority,
	log_priority_e;
struct AdspLogHandle;
typedef struct AdspLogHandle AdspLogHandle;

#ifdef __cplusplus
namespace intel_adsp
{
struct ModulePlaceholder {};
}
inline void *operator new(size_t size, intel_adsp::ModulePlaceholder * placeholder) throw()
{
	(void)size;
	return placeholder;
}
#endif /* #ifdef __cplusplus */

#endif /*_ADSP_STDDEF_H_ */
