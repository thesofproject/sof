
// xos_params.h - user-settable compile time parameters for X/OS.

// Copyright (c) 2003-2014 Cadence Design Systems, Inc.
//
// Permission is hereby granted, free of charge, to any person obtaining
// a copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
// CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
// TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
// SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


#ifndef __XOS_PARAMS_H__
#define __XOS_PARAMS_H__

#ifdef _cplusplus
extern "C" {
#endif


//-----------------------------------------------------------------------------
// Number of thread priority levels. At this time XOS supports a maximum of
// 32 priority levels (0 - 31).
//-----------------------------------------------------------------------------
#define XOS_NUM_PRIORITY        8       // Default is 8


//-----------------------------------------------------------------------------
// Maximum number of threads that can be connected to an event at one time.
// Increasing this value will cause an increase in the size of event objects.
// Each thread connection takes 4 bytes.
//-----------------------------------------------------------------------------
#define XOS_MAX_CONN_PER_EVENT  8


//-----------------------------------------------------------------------------
// Debug flags - Set to 1 for more verbose operation.
// WARNING: Enabling one or more of these flags will affect system performance
// and timing.
//-----------------------------------------------------------------------------
#if defined XOS_DEBUG_ALL

#define XOS_DEBUG               1       // Generic OS debug
#define XOS_THREAD_DEBUG        1       // Debug thread module
#define XOS_TIMER_DEBUG         1       // Debug timer module
#define XOS_COND_DEBUG          1       // Debug condition objects
#define XOS_MUTEX_DEBUG         1       // Debug mutex module
#define XOS_SEM_DEBUG           1       // Debug semaphore module
#define XOS_EVENT_DEBUG         1       // Debug event module
#define XOS_MSGQ_DEBUG          1       // Debug message queue module

#else

#define XOS_DEBUG               0       // Generic OS debug
#define XOS_THREAD_DEBUG        0       // Debug thread module
#define XOS_TIMER_DEBUG         0       // Debug timer module
#define XOS_COND_DEBUG          0       // Debug condition objects
#define XOS_MUTEX_DEBUG         0       // Debug mutex module
#define XOS_SEM_DEBUG           0       // Debug semaphore module
#define XOS_EVENT_DEBUG         0       // Debug event module
#define XOS_MSGQ_DEBUG          0       // Debug message queue module

#endif


//-----------------------------------------------------------------------------
// Set this option to 1 to enable runtime statistics collection for the core
// module.
// WARNING: Enabling this option will have some impact on runtime performance
// and OS footprint.
//-----------------------------------------------------------------------------
#if (defined XOS_DEBUG_ALL) || XOS_DEBUG

#define XOS_ENABLE_STATS                1       // OS runtime statistics

#else

#ifndef XOS_ENABLE_STATS
#define XOS_ENABLE_STATS                0       // OS runtime statistics
#endif

#endif


//-----------------------------------------------------------------------------
// Set this option to 1 to enable statistics tracking for message queues.
// enabling this will cause message queue objects to increase in size, and add
// some overhead to message queue processing.
//-----------------------------------------------------------------------------
#ifndef XOS_MSGQ_ENABLE_STATS
#define XOS_MSGQ_ENABLE_STATS           0
#endif


//-----------------------------------------------------------------------------
// Size of interrupt stack in bytes. Shared by all interrupt handlers. Must be
// sized to handle worst case nested interrupts.
//-----------------------------------------------------------------------------
#ifndef XOS_INT_STACK_SIZE
#define XOS_INT_STACK_SIZE              8192
#endif


//-----------------------------------------------------------------------------
// Default maximum interrupt level at which X/OS primitives may be called.
// It is the level at which interrupts are disabled by default.
// See also description of xos_set_intlevel().
//-----------------------------------------------------------------------------
#define XOS_MAX_OS_INTLEVEL             XCHAL_EXCM_LEVEL


//-----------------------------------------------------------------------------
// Set this to 1 to enable stack checking. The stack is filled with a pattern
// on thread creation, and the stack is checked at certain times during system
// operation.
// WARNING: Enabling this option can have some impact on runtime performance.
// FIXME: Does this work well for shared stacks (e.g. for RTC threads) ? Maybe
// should check at all thread transitions ?
//-----------------------------------------------------------------------------
#ifndef XOS_STACK_CHECK
#if XOS_DEBUG
#define XOS_STACK_CHECK                 1
#else
#define XOS_STACK_CHECK                 0
#endif
#endif


//-----------------------------------------------------------------------------
// Set XOS_CLOCK_FREQ to the system clock frequency if this is known ahead of
// time. Otherwise, call xos_set_clock_freq() to set it at run time.
//-----------------------------------------------------------------------------
#ifndef XOS_CLOCK_FREQ
#define XOS_CLOCK_FREQ                  1000000
#endif
#define XOS_DEFAULT_CLOCK_FREQ          XOS_CLOCK_FREQ


//-----------------------------------------------------------------------------
// Set this option to 1 to enable software prioritization of interrupts. The
// priority scheme applied is that a higher interrupt number at the same level
// will have higher priority.
//-----------------------------------------------------------------------------
#ifndef XOS_OPT_INTERRUPT_SWPRI
#define XOS_OPT_INTERRUPT_SWPRI         1
#endif


//-----------------------------------------------------------------------------
// Set this option to 1 to use the thread-safe version of the C runtime library.
// You may need to enable this if you call C library functions from multiple
// threads -- see the documentation for the relevant C library to determine if
// this is necessary. This option increases the size of the TCB.
// NOTE: At this time only the newlib and xclib libraries are supported for
// thread safety.
//-----------------------------------------------------------------------------
#include <xtensa/config/system.h>

#ifndef XOS_OPT_THREAD_SAFE_CLIB

#if XSHAL_CLIB == XTHAL_CLIB_XCLIB
#define XOS_OPT_THREAD_SAFE_CLIB        1
#elif XSHAL_CLIB == XTHAL_CLIB_NEWLIB
#define XOS_OPT_THREAD_SAFE_CLIB        1
#else
#define XOS_OPT_THREAD_SAFE_CLIB        0
#endif

#endif


//-----------------------------------------------------------------------------
// Set this option to 1 to enable the thread abort feature. If this feature is
// not needed, turning it off will save a small amount of code and data space.
//-----------------------------------------------------------------------------
#ifndef XOS_OPT_THREAD_ABORT
#define XOS_OPT_THREAD_ABORT            1
#endif


//-----------------------------------------------------------------------------
// Set this option to 1 to enable threads waiting on timer objects. If this
// feature is not used, turning it off will make timer objects smaller, and
// reduce the time taken by timer expiry processing (by a small amount).
//-----------------------------------------------------------------------------
#ifndef XOS_OPT_TIMER_WAIT_ENABLE
#define XOS_OPT_TIMER_WAIT_ENABLE       1
#endif


#ifdef _cplusplus
}
#endif

#endif  // __XOS_PARAMS_H__

