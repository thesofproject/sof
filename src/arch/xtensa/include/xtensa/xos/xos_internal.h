
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


#ifndef __XOS_INTERNAL_H__
#define __XOS_INTERNAL_H__

#if !defined(__XOS_H__) || !defined(_XOS_INCLUDE_INTERNAL_)
  #error "xos_internal.h must be included by defining _XOS_INCLUDE_INTERNAL_ before including xos.h"
#endif

#include <xtensa/config/core.h>
#include "xos_common.h"

#ifdef _cplusplus
extern "C" {
#endif


// Use this macro to suppress compiler warnings for unused variables.

#define UNUSED(x)       (void)(x)


#if XOS_DEBUG

#include <stdio.h>
#include <xtensa/xtutil.h>
# define DPRINTF        printf

#else

# define DPRINTF(x...)  do {} while(0)

#endif


//-----------------------------------------------------------------------------
// Internal flags for thread creation.
//-----------------------------------------------------------------------------
#define XOS_THREAD_FAKE   0x8000  // Don't allocate stack (init and idle threads).


//-----------------------------------------------------------------------------
// Interrupt handler table entry. This structure defines one entry in the XOS
// interrupt handler table.
//-----------------------------------------------------------------------------
typedef struct XosIntEntry {
  XosIntFunc *    handler;      // Pointer to handler function.
  void *          arg;          // Argument passed to handler function.
#if XOS_OPT_INTERRUPT_SWPRI
  unsigned char   level;        // Interrupt level.
  unsigned char   priority;     // Interrupt priority.
  short           reserved;     // Reserved.
  unsigned int    primask;      // Mask of interrupts at higher priority.
#else
  unsigned int    ps;           // Value of PS when running the handler.
#endif
} XosIntEntry;


//-----------------------------------------------------------------------------
// Extern variables.
//-----------------------------------------------------------------------------
extern unsigned       xos_intlevel_mask;
extern unsigned       xos_intenable_mask;
extern XosIntEntry    xos_interrupt_table[XCHAL_NUM_INTERRUPTS];

extern uint32_t xos_clock_freq;
extern uint32_t xos_tick_period;
extern uint64_t xos_system_ticks;
extern uint64_t xos_system_cycles;
extern uint32_t xos_num_ctx_switches;


#define xos_enable_ints(mask)   _xos_enable_ints(mask)
#define xos_disable_ints(mask)  _xos_disable_ints(mask)


static __inline__ unsigned _xos_enable_ints(unsigned mask)
{
#if XCHAL_HAVE_INTERRUPTS
    extern unsigned xos_globals[];
    unsigned ret;
    unsigned new_intenable;

    __asm__ __volatile__(
        "rsil    a15, " _XOS_STR(XOS_MAX_OS_INTLEVEL) "\n\t"
        "l32i    %0, %3, " _XOS_STR(XOS_INTENABLE_MASK) "\n\t"
        "l32i    %1, %3, " _XOS_STR(XOS_INTLEVEL_MASK) "\n\t"
        "or      %2, %0, %2    \n\t"
        "s32i    %2, %3, " _XOS_STR(XOS_INTENABLE_MASK) "\n\t"
        "and     %1, %2, %1    \n\t"
        "wsr     %1, intenable \n\t"
        "wsr     a15, ps       \n\t"
        "rsync                 \n\t"
        : "=&a" (ret), "=&a" (new_intenable)
        : "a" (mask), "a" (xos_globals)
        : "a15"
        );
    return ret;
#else
    return 0;
#endif
}

static __inline__ unsigned _xos_disable_ints(unsigned mask)
{
#if XCHAL_HAVE_INTERRUPTS
    extern unsigned xos_globals[];
    unsigned ret;
    unsigned new_intenable;

    __asm__ __volatile__( 
        "rsil    a15, " _XOS_STR(XOS_MAX_OS_INTLEVEL) "\n\t"
        "l32i    %0, %3, " _XOS_STR(XOS_INTENABLE_MASK) "\n\t"
        "l32i    %1, %3, " _XOS_STR(XOS_INTLEVEL_MASK) "\n\t"
        "and     %2, %0, %2    \n\t"
        "s32i    %2, %3, " _XOS_STR(XOS_INTENABLE_MASK) "\n\t"
        "and     %1, %2, %1    \n\t"
        "wsr     %1, intenable \n\t"
        "wsr     a15, ps       \n\t"
        "rsync                 \n\t"
        : "=&a" (ret), "=&a" (new_intenable)
        : "a" (~mask), "a" (xos_globals)
        : "a15"
        );
    return ret;
#else
    return 0;
#endif
}

/*

One thing I noticed is different between my initial idea of stack
assignments to RTC threads, when comparing to interrupts, is that I
expected each RTC thread priority to have its own stack, whereas
interrupts of different priorities share an interrupt stack.

It's not really a difference in memory usage, because when assigning
multiple priorities to a stack, you have to add-up worst-case for
all priorities.  One possible functional difference is that with
separate stacks per priority, it's possible to dynamically change
the priority of an RTC thread (while it's running).  Not sure how
valuable that might be -- changing priority is useful with priority
inheritance, to avoid priority inversion, but I don't know how often
an RTC thread might acquire a lock (it couldn't block on acquiring a
lock in the usual sense -- it could get queued waiting and be restarted
when it becomes available, or use try_lock instead of lock).

*/

#ifdef _cplusplus
}
#endif

#endif  /* __XOS_INTERNAL_H__ */

