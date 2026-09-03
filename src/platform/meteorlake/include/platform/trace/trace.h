/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 */

#ifdef __SOF_TRACE_TRACE_H__

#ifndef __PLATFORM_TRACE_TRACE_H__
#define __PLATFORM_TRACE_TRACE_H__

#include <sof/lib/cpu.h>
#include <sof/lib/mailbox.h>
#include <sof/lib/memory.h>
#include <stdint.h>

/* Platform defined trace code */
static inline void platform_trace_point(uint32_t x)
{ }

#endif /* __PLATFORM_TRACE_TRACE_H__ */

#else

#error "This file shouldn't be included from outside of sof/trace/trace.h"

#endif /* __SOF_TRACE_TRACE_H__ */
