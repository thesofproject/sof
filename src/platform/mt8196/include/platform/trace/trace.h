/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright(c) 2024 MediaTek. All rights reserved.
 *
 * Author: Hailong Fan <hailong.fan@mediatek.com>
 */

#ifdef __SOF_TRACE_TRACE_H__

#ifndef __PLATFORM_TRACE_TRACE_H__
#define __PLATFORM_TRACE_TRACE_H__

#include <sof/lib/mailbox.h>
#include <sof/lib/memory.h>
#include <sof/lib/io.h>
#include <platform/drivers/mt_reg_base.h>

#define platform_trace_point(__x)

#endif /* __PLATFORM_TRACE_TRACE_H__ */

#else

#error "This file shouldn't be included from outside of sof/trace/trace.h"

#endif /* __SOF_TRACE_TRACE_H__ */
