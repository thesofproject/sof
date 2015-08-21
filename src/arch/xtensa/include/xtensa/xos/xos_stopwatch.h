
// xos_stopwatch.h - X/OS Stopwatch objects and related API.

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


#ifndef __XOS_STOPWATCH_H__
#define __XOS_STOPWATCH_H__

#ifdef _cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "xos_params.h"


//-----------------------------------------------------------------------------
//  A stopwatch object can be used to track elapsed time and accumulate total
//  elapsed time over multiple execution periods. The stopwatch records the
//  time whenever its start function is called, and stops recording the time
//  when the stop function is called and updates its cumulative time counter.
//  The stopwatch keeps time in cycles. This can be converted to seconds etc.
//  by using the XOS conversion calls such as xos_cycles_to_secs().
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
//  XosStopwatch object.
//-----------------------------------------------------------------------------
typedef struct XosStopwatch {
    uint64_t    total;
    uint64_t    start;
    uint16_t    active;
} XosStopwatch;


//-----------------------------------------------------------------------------
//  Initialize a stopwatch object.
//-----------------------------------------------------------------------------
static inline void
xos_stopwatch_init(XosStopwatch * sw)
{
    sw->total  = 0;
    sw->start  = 0;
    sw->active = 0;
}


//-----------------------------------------------------------------------------
//  Start a stopwatch. Starts cycle counting.
//  Note that this does not necessarily start counting from zero. The current
//  run (start-to-stop interval) will just get added to the accumulated count
//  in the stopwatch if any.
//  To reset the accumulated count, use xos_stopwatch_clear().
//-----------------------------------------------------------------------------
static inline void
xos_stopwatch_start(XosStopwatch * sw)
{
    XOS_ASSERT(!sw->active);
    sw->active = 1;
    sw->start  = xos_get_system_cycles();
}


//-----------------------------------------------------------------------------
//  Stop a stopwatch. Stops cycle counting and updates total.
//-----------------------------------------------------------------------------
static inline void
xos_stopwatch_stop(XosStopwatch * sw)
{
    XOS_ASSERT(sw->active);
    sw->active = 0;
    sw->total += xos_get_system_cycles() - sw->start;
}


//-----------------------------------------------------------------------------
//  Get stopwatch accumulated count.
//-----------------------------------------------------------------------------
static inline uint64_t
xos_stopwatch_count(XosStopwatch * sw)
{
    return sw->total;
}


//-----------------------------------------------------------------------------
//  Get elapsed time since stopwatch was started. If not started, returns zero.
//  Return value is in cycles.
//-----------------------------------------------------------------------------
static inline uint64_t
xos_stopwatch_elapsed(XosStopwatch * sw)
{
    return sw->active ? xos_get_system_cycles() - sw->start : 0;
}


//-----------------------------------------------------------------------------
//  Clears a stopwatch. Resets the accumulated count to zero, and deactivates
//  it if active.
//-----------------------------------------------------------------------------
static inline void
xos_stopwatch_clear(XosStopwatch * sw)
{
    xos_stopwatch_init(sw);
}


#ifdef _cplusplus
}
#endif

#endif // __XOS_STOPWATCH_H__

