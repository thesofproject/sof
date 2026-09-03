/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright(c) 2022 - 2024 Intel Corporation. All rights reserved.
 */

#ifndef _ADSP_STDDEF_H_
#define _ADSP_STDDEF_H_

#include <stddef.h>
#include <stdint.h>
#include <rtos/string.h>

#ifdef __ZEPHYR__
#include <zephyr/sys/util.h>
#endif /* __ZEPHYR__ */

#include <module/module/logger.h>

/*! Log level priority enumeration. */
typedef enum log_priority AdspLogPriority, log_priority_e;
typedef struct log_handle AdspLogHandle;

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
