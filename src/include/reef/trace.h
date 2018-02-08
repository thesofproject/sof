/*
 * Copyright (c) 2016, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the Intel Corporation nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 */

#ifndef __INCLUDE_TRACE__
#define __INCLUDE_TRACE__

#include <stdint.h>
#include <stdlib.h>
#include <reef/reef.h>
#include <reef/mailbox.h>
#include <reef/debug.h>
#include <reef/timer.h>
#include <platform/platform.h>
#include <platform/timer.h>

/* bootloader trace values */
#define TRACE_BOOT_LDR_ENTRY		0x100
#define TRACE_BOOT_LDR_HPSRAM		0x110
#define TRACE_BOOT_LDR_MANIFEST	0x120
#define TRACE_BOOT_LDR_JUMP		0x150

/* general trace init codes - only used at boot when main trace is not available */
#define TRACE_BOOT_START		0x1000
#define TRACE_BOOT_ARCH		0x2000
#define TRACE_BOOT_SYS			0x3000
#define TRACE_BOOT_PLATFORM		0x4000

/* system specific codes */
#define TRACE_BOOT_SYS_WORK		(TRACE_BOOT_SYS + 0x100)
#define TRACE_BOOT_SYS_CPU_FREQ		(TRACE_BOOT_SYS + 0x200)
#define TRACE_BOOT_SYS_HEAP		(TRACE_BOOT_SYS + 0x300)
#define TRACE_BOOT_SYS_NOTE		(TRACE_BOOT_SYS + 0x400)
#define TRACE_BOOT_SYS_SCHED		(TRACE_BOOT_SYS + 0x500)

/* platform/device specific codes */
#define TRACE_BOOT_PLATFORM_ENTRY	(TRACE_BOOT_PLATFORM + 0x100)
#define TRACE_BOOT_PLATFORM_MBOX	(TRACE_BOOT_PLATFORM + 0x110)
#define TRACE_BOOT_PLATFORM_SHIM	(TRACE_BOOT_PLATFORM + 0x120)
#define TRACE_BOOT_PLATFORM_PMC		(TRACE_BOOT_PLATFORM + 0x130)
#define TRACE_BOOT_PLATFORM_TIMER	(TRACE_BOOT_PLATFORM + 0x140)
#define TRACE_BOOT_PLATFORM_CLOCK	(TRACE_BOOT_PLATFORM + 0x150)
#define TRACE_BOOT_PLATFORM_SSP_FREQ	(TRACE_BOOT_PLATFORM + 0x160)
#define TRACE_BOOT_PLATFORM_IPC		(TRACE_BOOT_PLATFORM + 0x170)
#define TRACE_BOOT_PLATFORM_DMA		(TRACE_BOOT_PLATFORM + 0x180)
#define TRACE_BOOT_PLATFORM_SSP		(TRACE_BOOT_PLATFORM + 0x190)


/* trace event classes - high 8 bits*/
#define TRACE_CLASS_IRQ		(1 << 24)
#define TRACE_CLASS_IPC		(2 << 24)
#define TRACE_CLASS_PIPE	(3 << 24)
#define TRACE_CLASS_HOST	(4 << 24)
#define TRACE_CLASS_DAI		(5 << 24)
#define TRACE_CLASS_DMA		(6 << 24)
#define TRACE_CLASS_SSP		(7 << 24)
#define TRACE_CLASS_COMP	(8 << 24)
#define TRACE_CLASS_WAIT	(9 << 24)
#define TRACE_CLASS_LOCK	(10 << 24)
#define TRACE_CLASS_MEM		(11 << 24)
#define TRACE_CLASS_MIXER	(12 << 24)
#define TRACE_CLASS_BUFFER	(13 << 24)
#define TRACE_CLASS_VOLUME	(14 << 24)
#define TRACE_CLASS_SWITCH	(15 << 24)
#define TRACE_CLASS_MUX		(16 << 24)
#define TRACE_CLASS_SRC         (17 << 24)
#define TRACE_CLASS_TONE        (18 << 24)
#define TRACE_CLASS_EQ_FIR      (19 << 24)
#define TRACE_CLASS_EQ_IIR      (20 << 24)
#define TRACE_CLASS_SA		(21 << 24)

/* move to config.h */
#define TRACE	1
#define TRACEV	0
#define TRACEE	1

void _trace_event(uint32_t event);
void _trace_error(uint32_t event);
void _trace_event_atomic(uint32_t event);
void _trace_error_atomic(uint32_t event);
void trace_off(void);
void trace_init(struct reef * reef);

#if TRACE

#define trace_event(__c, __e) \
	_trace_event(__c | (__e[0] << 16) | (__e[1] <<8) | __e[2])
#define trace_event_atomic(__c, __e) \
	_trace_event_atomic(__c | (__e[0] << 16) | (__e[1] <<8) | __e[2])

#define trace_value(x)	_trace_event(x)
#define trace_value_atomic(x)	_trace_event_atomic(x)

#define trace_point(x) platform_trace_point(x)

/* verbose tracing */
#if TRACEV
#define tracev_event(__c, __e) trace_event(__c, __e)
#define tracev_value(x)	_trace_event(x)
#define tracev_event_atomic(__c, __e) trace_event_atomic(__c, __e)
#define tracev_value_atomic(x)	_trace_event_atomic(x)
#else
#define tracev_event(__c, __e)
#define tracev_value(x)
#define tracev_event_atomic(__c, __e)
#define tracev_value_atomic(x)
#endif

/* error tracing */
#if TRACEE
#define trace_error(__c, __e) \
	_trace_error_atomic(__c | (__e[0] << 16) | (__e[1] <<8) | __e[2])
#define trace_error_atomic(__c, __e) \
	_trace_error_atomic(__c | (__e[0] << 16) | (__e[1] <<8) | __e[2])
#else
#define trace_error(__c, __e)
#define trace_error_atomic(__c, __e)
#endif

#else

#define trace_event(x, e)
#define trace_error(c, e)
#define trace_value(x)
#define trace_point(x)
#define tracev_event(__c, __e)
#define tracev_value(x)

#endif

#endif
