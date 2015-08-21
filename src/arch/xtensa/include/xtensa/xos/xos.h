
// xos.h - X/OS API interface and data structures visible to user code.

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

// NOTE NOTE NOTE -- The name X/OS or XOS is already taken,
//      most notably XOS by http://openxos.org/
//      less notably X/OS by http://www.xoslinux.org/ (seems dead)
//      (if it helps, none of these have APIs with xos_ prefixes).
//      TaskOS also taken (popular Android app).
//      XTOS is not, really.
//
// This code is distantly derived from example code in the Xtensa Microprocessor
// Programmer's Guide.


#ifndef __XOS_H__
#define __XOS_H__

#ifdef _cplusplus
extern "C" {
#endif


#include <stdint.h>

#include <xtensa/config/core.h>

#include "xos_common.h"
#include "xos_errors.h"
#include "xos_regaccess.h"


//-----------------------------------------------------------------------------
// Convert x into a literal string.
//-----------------------------------------------------------------------------
#define _XOS_STR(x)             __XOS_STR(x)
#define __XOS_STR(x)            #x


//-----------------------------------------------------------------------------
// X/OS version.
//-----------------------------------------------------------------------------
#define XOS_VERSION_MAJOR       1
#define XOS_VERSION_MINOR       1
#define XOS_VERSION_STRING      _XOS_STR(XOS_VERSION_MAJOR) "." _XOS_STR(XOS_VERSION_MINOR)


//-----------------------------------------------------------------------------
// Runtime error handling.
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
//  Reports a fatal error and halts X/OS operation, i.e. halts the system. This
//  function will call a user-registered error handler (if one has been set) 
//  and then halt the system. The user handler may do system-specific things
//  such as record the error reason in nonvolatile memory etc.
//
//  errcode             Error code. May be any user defined value < 0.
//                      Values >=0 are reserved for use by the system.
//
//  errmsg              Optional text string describing the error.
//
//  Returns: This function does not return.
//-----------------------------------------------------------------------------
void
xos_fatal_error(int32_t errcode, const char * errmsg);


//-----------------------------------------------------------------------------
//  Assertion handling. In debug builds, an assertion failure will cause a 
//  fatal error to be reported. In non-debug builds, assertions are compiled
//  out.
//  NOTE: Remember that any code in XOS_ASSERT() statements gets compiled out
//  for non-debug builds.
//-----------------------------------------------------------------------------
#if XOS_DEBUG

void
xos_assert(const char * file, int32_t line);

#define XOS_ASSERT(expr)        if (!(expr)) xos_assert(__FILE__, __LINE__)

#else

#define XOS_ASSERT(expr)

#endif


//-----------------------------------------------------------------------------
// Function pointer types.
//-----------------------------------------------------------------------------
typedef void    (XosIntFunc)(void * arg);
typedef int32_t (XosPrintFunc)(void * arg, const char * fmt, ...);
typedef void    (XosFatalErrFunc)(int32_t errcode, const char * errmsg);
typedef void    (XosExcHandlerFunc)(XosExcFrame * frame);


//-----------------------------------------------------------------------------
//  Install a user defined exception handler for the specified exception type.
//  This will override the default X/OS exception handler. The handler is a C
//  function that is passed one parameter -- a pointer to the exception frame.
//  The exception frame is allocated on the stack of the thread that caused the
//  exception, and contains saved state and exception information. For details
//  of the exception frame see the structure XosExcFrame.
//
//  exc                 Exception type (number) to override. The exception
//                      numbers are enumerated in <xtensa/corebits.h>.
//
//  handler             Pointer to handler function to be installed. To revert
//                      to the default handler, pass NULL.
//
//  Returns: Pointer to previous handler installed, if any.
//-----------------------------------------------------------------------------
XosExcHandlerFunc *
xos_register_exception_handler(int32_t exc, XosExcHandlerFunc * handler);


//-----------------------------------------------------------------------------
//  Install a user defined fatal error handler. This function will be called if
//  a fatal error is reported either by user code or by X/OS itself. It will be
//  passed the same arguments that are passed to xos_fatal_error().
//
//  The handler need not return. It should make minimal assumptions about the
//  state of the system. In particular, it should not assume that further X/OS
//  system calls will succeed.
//
//  handler             Pointer to handler function to be installed.
//
//  Returns: Pointer to previous handler installed, if any.
//-----------------------------------------------------------------------------
XosFatalErrFunc *
xos_register_fatal_error_handler(XosFatalErrFunc * handler);


#ifdef _XOS_INCLUDE_INTERNAL_
# include "xos_internal.h"
#endif


#include "xos_thread.h"
#include "xos_timer.h"
#include "xos_cond.h"
#include "xos_event.h"
#include "xos_mutex.h"
#include "xos_msgq.h"
#include "xos_semaphore.h"
#include "xos_stopwatch.h"


#ifdef _XOS_INCLUDE_INTERNAL_

#else

//-----------------------------------------------------------------------------
//  Enable specific individual interrupt(s), by mask.
//  The state (enabled vs. disabled) of individual interrupts is global, i.e.
//  not associated with any specific thread. Depending on system options and
//  implementation, this state may be stored in one of two ways:
//  - directly in the INTENABLE register, or
//  - in a global variable (this is generally the case when INTENABLE is used
//    not just to control what interrupts are enabled globally, but also for
//    software interrupt prioritization within an interrupt level, effectively
//    providing finer grained levels; in this case X/OS takes care to update
//    INTENABLE whenever either the global enabled-state variable or the 
//    per-thread fine-grained-level variable change).
//  Thus it is best to never access the INTENABLE register directly.
//
//  To modify thread-specific interrupt level, use one of xos_set_intlevel(),
//  xos_disable_intlevel(), xos_enable_intlevel(), or xos_restore_intlevel().
//
//  mask                Mask of Xtensa core interrupts to enable. It is a
//                      bitmask where bits 0 (lsbit) thru 31 (msbit)
//                      correspond to core interrupts 0 to 31.
//
//  NOTE: To refer to a specific external interrupt number (BInterrupt pin),
//  use HAL macro XCHAL_EXTINT<ext>_NUM where <ext> is the external interrupt
//  number.  For example, to enable external interrupt 3 (BInterrupt[3]),
//  you can use:
//
//      xos_enable_ints( 1 << XCHAL_EXTINT3_NUM );
//
//  Returns: The previous set of enabled interrupts.
//-----------------------------------------------------------------------------
extern uint32_t
xos_enable_ints(uint32_t mask);


//-----------------------------------------------------------------------------
//  Disable specific individual interrupt(s), by mask.
//
//  This is the counterpart to xos_enable_ints(), where mask specifies the
//  interrupts to disable rather than interrupts to enable. See the description
//  of xos_enable_ints() for further comments and notes.
//
//  Returns: The previous set of enabled interrupts.
//-----------------------------------------------------------------------------
extern uint32_t
xos_disable_ints(uint32_t mask);

#endif // _XOS_INCLUDE_INTERNAL_


//-----------------------------------------------------------------------------
//  Register a handler function to call when interrupt "num" occurs.
//
//  For level-triggered and timer interrupts, the handler function will have
//  to clear the source of the interrupt before returning, to avoid infinitely
//  retaking the interrupt. Edge-triggered and software interrupts are 
//  automatically cleared by the OS interrupt dispatcher (see xos_handlers.S).
//
//  num                 Xtensa internal interrupt number (0..31). To refer to
//                      a specific external interrupt number (BInterrupt pin),
//                      use HAL macro XCHAL_EXTINT<ext>_NUM where <ext> is the
//                      external number.
//
//  handler             Pointer to handler function.
//
//  arg                 Argument passed to handler.
//
//  Returns: XOS_OK if successful, else error code.
//-----------------------------------------------------------------------------
int32_t
xos_register_interrupt_handler(int32_t num, XosIntFunc * handler, void * arg);


//-----------------------------------------------------------------------------
//  Unregister a handler function for interrupt "num". If no handler was 
//  installed, this function will have no effect.
//
//  num                 Xtensa internal interrupt number (0..31). To refer to
//                      a specific external interrupt number (BInterrupt pin),
//                      use HAL macro XCHAL_EXTINT<ext>_NUM where <ext> is the
//                      external number.
//
//  Returns: XOS_OK if successful, else error code.
//-----------------------------------------------------------------------------
int32_t
xos_unregister_interrupt_handler(int32_t num);


//-----------------------------------------------------------------------------
//  Register a high priority interrupt handler for interrupt level "level".
//
//  Unlike low and medium priority interrupt handlers, high priority handlers
//  are not installed for a specific interrupt number, but for an interrupt
//  level. The level must be above XCHAL_EXCM_LEVEL. The handler function must
//  be written in assembly since C handlers are not supported for levels above
//  XCHAL_EXCM_LEVEL. The handler function must preserve all registers except
//  a0, and must return to the dispatcher via a "ret" instruction, not "rfi".
//
//  NOTE: This method of dispatch takes a few cycles of overhead. If you wish
//  to save even these cycles, then you can define your own dispatch function
//  to override the built-in dispatcher. See xos_handlers.S for more details.
//
//  handler             Pointer to handler function.
//
//  Returns: XOS_OK if successful, else error code.
//-----------------------------------------------------------------------------
int32_t
xos_register_hp_interrupt_handler(int32_t level, void * handler);


//-----------------------------------------------------------------------------
//  Work in progress...
//-----------------------------------------------------------------------------
void
xos_display_threads(void * arg, XosPrintFunc * print_fn);


//-----------------------------------------------------------------------------
//  xos_set_intlevel
//
//  Set the processor interrupt level (eg. PS.INTLEVEL) to the specified value
//  (for the current thread or interrupt context). This interrupt level is part
//  of the thread context, so is saved and restored across context switches.
//  To enable and disable individual interrupts globally, see xos_enable_ints()
//  and xos_disable_ints() instead. Note those functions handle interrupts by
//  number, while these handle interrupts by level.
//
//  level               Desired interrupt level (0..15). Must be a constant, or
//                      a constant expression computable at compile time.
//                      Zero enables all interrupt levels.
//
//  Returns: A value that can be used to restore the previous interrupt level
//  by calling xos_restore_intlevel(). This is usually the value of the PS
//  register, but not guaranteed to be so.
//
//  xos_get_intlevel
//
//  Returns the current processor interrupt level.
//
//  xos_restore_intlevel
//
//  Restores the processor interrupt level to the given value. When setting
//  the interrupt level temporarily (such as in a critical section), call
//  xos_set_intlevel() or xos_disable_intlevel(), execute the critical code,
//  and then call xos_restore_intlevel() with the value returned from the
//  first call.
//
//  value               The value to restore, return value from a call to
//                      xos_set_intlevel() or xos_disable_intlevel().
//
//  Returns: Nothing.
//
//  NOTE: You usually don't want to disable interrupts at any level higher
//  than that of the highest priority interrupt that interacts with the OS
//  (i.e. calls into X/OS such that threads may be woken / blocked / 
//  reprioritized / switched, or otherwise access X/OS data structures).
//  In X/OS, that maximum level is XOS_MAX_OS_INTLEVEL, which defaults to
//  XCHAL_EXCM_LEVEL. This may be modified by editing xos_params.h and
//  rebuilding X/OS.
//
//  xos_disable_intlevel
//
//  Shortcut for xos_set_intlevel(XOS_MAX_OS_INTLEVEL);
//
//  xos_enable_intlevel
//
//  Shortcut for xos_set_intlevel(0);
//-----------------------------------------------------------------------------
#ifdef DOXYGEN

uint32_t xos_disable_intlevel(void)                 {}
uint32_t xos_enable_intlevel(void)                  {}
uint32_t xos_set_intlevel(const int32_t level)      {}
void     xos_restore_intlevel(const uint32_t oldps) {}

#endif

#if XCHAL_HAVE_INTERRUPTS

#define xos_set_intlevel(level)     (XT_RSIL(level))

static inline uint32_t
xos_get_intlevel()
{
    return XT_RSR_PS() & 0xF;
}

static inline uint32_t
xos_disable_intlevel(void)
{
    return xos_set_intlevel(XOS_MAX_OS_INTLEVEL);
}

static inline uint32_t
xos_enable_intlevel(void)
{
    return xos_set_intlevel(0);
}

static inline void
xos_restore_intlevel(const uint32_t oldps)
{
#pragma no_reorder
    XT_WSR_PS(oldps);
}

#if 0
#define xos_set_intlevel(level) \
                ({unsigned __ret; \
                 __asm__ __volatile__("rsil %0, " _XOS_STR(level) : "=&a" (__ret) :: "memory"); \
                 __ret;})
#define xos_restore_intlevel(ps) \
                __asm__ __volatile__("wsr.ps %0 ; rsync" :: "a" (ps) : "memory")
#endif

#else

#define xos_set_intlevel(level)         0
#define xos_get_intlevel()              0
#define xos_disable_intlevel()          0
#define xos_enable_intlevel()           0
#define xos_restore_intlevel(ps)        (ps = 0)

#endif


// This file uses things defined above
#include "xos_syslog.h"


#ifdef _cplusplus
}
#endif

#endif  // __XOS_H__

