/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 * Copyright 2021 NXP
 *
 * Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>
 * Author: Zhang Peng <peng.zhang_8@nxp.com>
 */

#ifdef __SOF_TRACE_TRACE_H__

#ifndef __PLATFORM_TRACE_TRACE_H__
#define __PLATFORM_TRACE_TRACE_H__

/* Platform defined trace code */
#define platform_trace_point(__x)

#endif /* __PLATFORM_TRACE_TRACE_H__ */

#else

#error "This file shouldn't be included from outside of sof/trace/trace.h"

#endif /* __SOF_TRACE_TRACE_H__ */
