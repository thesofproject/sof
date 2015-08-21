
// xos_timer.h - X/OS Timer API interface and data structures.

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

// NOTE: Do not include this file directly in your application. Including
// xos.h will automatically include this file.


#ifndef __XOS_TIMER_H__
#define __XOS_TIMER_H__

#include <stdint.h>

#ifdef _cplusplus
extern "C" {
#endif


//-----------------------------------------------------------------------------
// Function pointer type for timer callbacks.
//-----------------------------------------------------------------------------
typedef void (XosTimerFunc)(void * arg);


//-----------------------------------------------------------------------------
// Timer event structure. Used to track pending timer events.
//-----------------------------------------------------------------------------
typedef struct XosTimer {
    struct XosTimer *   next;   // Pointer to next event in list.
    uint64_t            when;   // Time (clock cycles) at which to trigger.
    uint64_t            delta;  // Delta for next re-trigger, 0 if none.
    XosTimerFunc *      fn;     // Function to call when timer expires.
    void *              arg;    // Argument to pass to called function.
    int32_t             active; // Set if active (in some list of events).
#if XOS_OPT_TIMER_WAIT_ENABLE
    XosThreadQueue      waitq;  // Queue of threads waiting on this timer.
#endif
#if XOS_TIMER_DEBUG
    uint32_t            signature;
#endif
} XosTimer;


//-----------------------------------------------------------------------------
// Extern declarations.
//-----------------------------------------------------------------------------

// System clock frequency in cycles per second.
extern uint32_t xos_clock_freq;


//-----------------------------------------------------------------------------
// Functions to convert from clock cycles to time units and vice versa.
//
// Note that these are integer conversions so for example a cycle count of less
// than one second will convert to zero seconds.
//
// NOTE: Revisit whether we need to handle overflows in the math.
//-----------------------------------------------------------------------------


static inline uint64_t
xos_cycles_to_secs(uint64_t cycles)
{
    return cycles / xos_clock_freq;
}

static inline uint64_t
xos_cycles_to_msecs(uint64_t cycles)
{
    return (cycles * 1000) / xos_clock_freq;
}

static inline uint64_t
xos_cycles_to_usecs(uint64_t cycles)
{
    return (cycles * 1000000) / xos_clock_freq;
}

static inline uint64_t
xos_secs_to_cycles(uint64_t secs)
{
    return secs * xos_clock_freq;
}

static inline uint64_t
xos_msecs_to_cycles(uint64_t msecs)
{
    return (msecs * xos_clock_freq) / 1000;
}

static inline uint64_t
xos_usecs_to_cycles(uint64_t usecs)
{
    return (usecs * xos_clock_freq) / 1000000;
}


//-----------------------------------------------------------------------------
//  Set system clock frequency. Once dynamic clocking is supported, this call 
//  may take a nontrivial amount of time to update active timers etc.
//
//  freq                Frequency in cycles per second.
//
//  Returns: nothing.
//-----------------------------------------------------------------------------
static inline void
xos_set_clock_freq(uint32_t freq)
{
    xos_clock_freq = freq;
}


//-----------------------------------------------------------------------------
//  Get current system clock frequency.
//
//  Returns: Current system clock frequency in cycles per second.
//-----------------------------------------------------------------------------
static inline uint32_t
xos_get_clock_freq()
{
    return xos_clock_freq;
}


//-----------------------------------------------------------------------------
//  Initialize timer support and start the system timer.
//
//  timer_num           Which Xtensa timer to use (0..2). This timer must exist
//                      and be configured at level 1 or at a medium-priority
//                      interrupt level (<=EXCM_LEVEL).
//                      If 'timer_num' is -1, the function will automatically
//                      choose the highest priority timer that is suitable for
//                      use.
//
//  tick_period         Tick period - number of clock (CCOUNT) cycles between
//                      ticks. Must range between 0 and UINT32_MAX.
//                      Zero is used to specify dynamic tick (tickless) mode.
//
//  This function must be called before calling any other timer function.
//
//  NOTE: The smaller the tick period, the more precisely delays can be
//  specified using timers.  However, we also need to make the tick period
//  large enough to allow time both to execute the tick timer interrupt handler
//  and for the application to make reasonable forward progress.  If tick_period
//  is too small, the timer interrupt may re-trigger before the timer interrupt
//  handler has returned to the application, thus keeping the processor busy in
//  constantly executing the timer interrupt handler without leaving any cycles
//  for the application. Or, the application might get some cycles but only a 
//  fraction of what is spent in the timer interrupt handler, thus severely
//  impacting application performance.
//
//  The exact number of cycles needed to execute the timer interrupt handler
//  is not specified here.  It depends on many factors (e.g. use of caches, 
//  various processor configuration options, etc) and can vary by orders of
//  magnitude.  Also note that the time to execute this handler is variable:
//  when timers expire upon a given tick timer interrupt, their respective
//  timer handler functions are called from within the interrupt handler.
//-----------------------------------------------------------------------------
int32_t
xos_start_system_timer(int32_t timer_num, uint32_t tick_period);


//-----------------------------------------------------------------------------
//  Initialize timer structure.
//
//  timer               Pointer to timer event structure.
//
//  Returns: nothing.
//
//  NOTE: This function should not be called on a timer object once it has 
//  been activated.
//-----------------------------------------------------------------------------
void xos_timer_init(XosTimer * timer);


//-----------------------------------------------------------------------------
//  Flags for xos_timer_start() (see below).
//-----------------------------------------------------------------------------
#define XOS_TIMER_DELTA         0x0000
#define XOS_TIMER_RECURRING     0x0001
#define XOS_TIMER_ABSOLUTE      0x0002
#define XOS_TIMER_FROM_NOW      0x0000
#define XOS_TIMER_FROM_LAST     0x0010


//-----------------------------------------------------------------------------
//  Start the timer, and when the timer expires, call the specified function
//  (invoke (*fn)(arg)). If the timer is periodic, it will be automatically
//  restarted when it expires.
//
//  The specified timer event structure must have been initialized before
//  first use by calling xos_timer_init().
//
//  The callback function will be called in an interrupt context. Hence it is
//  NOT safe to use any coprocessors in the function, including the FPU. If a
//  coprocessor must be used, then its state must be saved and restored across
//  its use.
//
//  NOTE: If you are using the timer only to wait on (via xos_timer_wait())
//  then it is not necessary to specify a callback function.
//
//  timer               Pointer to timer event structure. Must have been
//                      initialized. May be active or not.
//
//  when                When to call the function (see flags).
//
//  flags               Set of option flags XOS_TIMER_***
//                      The following flags are mutually exclusive:
//                      - XOS_TIMER_DELTA       when is number of cycles from
//                                              [see below] (default)
//                      - XOS_TIMER_RECURRING   when is number of cycles from
//                                              [see below], and timer continually
//                                              re-triggers at that interval
//                      - XOS_TIMER_ABSOLUTE    when is absolute value of cycle count
//
//                      The following flags are mutually exclusive:
//                      - XOS_TIMER_FROM_NOW    *DELTA and *RECURRING are relative
//                                              to now (default)
//                      - XOS_TIMER_FROM_LAST   *DELTA and *RECURRING are relative
//                                              to the timer
//                                              event's last specified expiry time
//                                              (usually in the future if active,
//                                              in the past if not, absolute 0 if
//                                              was never activated).
//
//  fn                  Function to call (called in timer interrupt context).
//                      This argument is optional. Specify NULL if no function
//                      is to be called.
//
//  arg                 Argument passed to callback function. Only relevant if
//                      "fn" is not NULL.
//
//  Returns: XOS_OK on success, else error code.
//-----------------------------------------------------------------------------
int32_t
xos_timer_start(XosTimer *     timer,
                uint64_t       when,
                uint32_t       flags,
                XosTimerFunc * func,
                void *         arg);


//-----------------------------------------------------------------------------
//  Stop the timer and remove it from the list of active timers. Has no effect
//  if the timer is not active. Any waiting threads are woken up.
// 
//  The timer structure must have been initialized at least once, else its
//  contents are undefined and can lead to unpredictable results.
//
//  timer               Pointer to timer object.
//
//  Returns : XOS_OK on success, else error code.
//-----------------------------------------------------------------------------
int32_t
xos_timer_stop(XosTimer * timer);


//-----------------------------------------------------------------------------
//  Reset and restart the timer.
//
//  The timer is reset to go off at time "when" from now. If the timer was not
//  active, it will be activated. If the timer was active, it will be restarted.
//  If the timer is periodic, the period will be set to "when".
//  The timer object must have been initialized at some point before this call.
//
//  timer               Pointer to timer object.
//
//  when                Number of cycles from now that the timer will expire.
//
//  Returns : XOS_OK on success, else error code.
//-----------------------------------------------------------------------------
int32_t
xos_timer_reset(XosTimer * timer, uint64_t when);


//-----------------------------------------------------------------------------
//  Check if the timer is active. The timer is active if it has been started
//  and not yet expired or canceled.
//
//  timer               Pointer to timer object.
//
//  Returns : Non-zero if the timer is active, else zero.
//-----------------------------------------------------------------------------
static inline int32_t
xos_timer_is_active(XosTimer * timer)
{
    return timer ? timer->active : 0;
}


//-----------------------------------------------------------------------------
//  Get the repeat period for a periodic timer. For a one-shot timer this will
//  return zero. The period is reported in system clock cycles.
//
//  timer               Pointer to timer object.
//
//  Returns : Period in cycles, or zero for non-periodic timers.
//-----------------------------------------------------------------------------
static inline uint64_t
xos_timer_get_period(XosTimer * timer)
{
    return timer ? timer->delta : 0;
}


//-----------------------------------------------------------------------------
//  Set the repeat period for a periodic timer. The period must be specified
//  in system clock cycles.
//
//  If the timer is active, the change in period does not take effect until 
//  the timer expires at least once after this call.
//  Note that setting a period of zero will effectively turn a periodic timer
//  into a one-shot timer. Similarly, a one-shot timer can be turned into a
//  periodic timer.
//
//  timer               Pointer to timer object.
//
//  period              Repeat period in system clock cycles.
//
//  Returns : XOS_OK on success, else error code.
//-----------------------------------------------------------------------------
int32_t
xos_timer_set_period(XosTimer * timer, uint64_t period);


//-----------------------------------------------------------------------------
//  Get the current system cycle count. This accounts for the periodic rollover
//  of the 32-bit CCOUNT cycle counter and returns a 64-bit value.
//
//  Returns: Current system cycle count.
//-----------------------------------------------------------------------------
static inline uint64_t
xos_get_system_cycles(void)
{
    extern uint64_t xos_system_cycles;
    return ((xos_system_cycles >> 32) << 32) | xos_get_ccount();
}


//-----------------------------------------------------------------------------
//  Put calling thread to sleep for at least the specified number of cycles.
//  The actual number of cycles spent sleeping may be larger depending upon
//  the granularity of the system timer. Once the specified time has elapsed
//  the thread will be woken and made ready.
//
//  cycles              Number of system clock cycles to sleep.
//
//  Returns: XOS_OK on success, else error code.
//-----------------------------------------------------------------------------
int32_t
xos_thread_sleep(uint64_t cycles);


//-----------------------------------------------------------------------------
//  Put calling thread to sleep for at least the specified number of msec.
//  The actual amount of time spent sleeping may be larger depending upon
//  the granularity of the system timer. Once the specified time has elapsed
//  the thread will be woken and made ready.
//
//  msecs               The number of milliseconds to sleep.
//
//  Returns: XOS_OK on success, else error code.
//-----------------------------------------------------------------------------
static inline int32_t
xos_thread_sleep_msec(uint64_t msecs)
{
    return xos_thread_sleep(xos_msecs_to_cycles(msecs));
}


//-----------------------------------------------------------------------------
//  Put calling thread to sleep for at least the specified number of usec.
//  The actual amount of time spent sleeping may be larger depending upon
//  the granularity of the system timer. Once the specified time has elapsed
//  the thread will be woken and made ready.
//
//  usecs               The number of microseconds to sleep.
//
//  Returns: XOS_OK on success, else error code.
//-----------------------------------------------------------------------------
static inline int32_t
xos_thread_sleep_usec(uint64_t usecs)
{
    return xos_thread_sleep(xos_usecs_to_cycles(usecs));
}


//-----------------------------------------------------------------------------
//  Wait on a timer until it expires or is cancelled. The calling thread will
//  be blocked. The timer must be active.
//  NOTE: This operation is only available if XOS_OPT_TIMER_WAIT_ENABLE is set
//  to 1 in the configuration options.
//
//  timer               Pointer to timer object.
//
//  Returns : XOS_OK on normal timeout, else an error code.
//-----------------------------------------------------------------------------
int32_t
xos_timer_wait(XosTimer * timer);


#ifdef _cplusplus
}
#endif

#endif  // __XOS_TIMER_H__

