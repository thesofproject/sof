// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Mediatek
//
// Author: Allen-KH Cheng <allen-kh.cheng@mediatek.com>

#ifdef __SOF_TRACE_TRACE_H__

#ifndef __PLATFORM_TRACE_TRACE_H__
#define __PLATFORM_TRACE_TRACE_H__

#include <sof/lib/mailbox.h>
#include <sof/lib/memory.h>
#include <sof/lib/io.h>
#include <platform/drivers/mt_reg_base.h>

#define ADSP_TRACE_POINT (0x2)

/* Platform defined trace code */
#define platform_trace_point(__x)                               \
	do {                                                        \
		io_reg_write(MTK_DSP_MBOX_OUT_CMD_MSG0(2), __x);        \
		io_reg_write(MTK_DSP_MBOX_OUT_CMD(2), ADSP_TRACE_POINT);\
	} while (0)

#endif /* __PLATFORM_TRACE_TRACE_H__ */

#else

#error "This file shouldn't be included from outside of sof/trace/trace.h"

#endif /* __SOF_TRACE_TRACE_H__ */
