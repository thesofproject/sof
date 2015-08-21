
// xos_cond.h - X/OS condition variables API interface and data structures.

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

#ifndef __XOS_COND_H__
#define __XOS_COND_H__

#include <stdint.h>

#ifdef _cplusplus
extern "C" {
#endif


//-----------------------------------------------------------------------------
// Function pointer type for condition callbacks.
//-----------------------------------------------------------------------------
typedef int32_t (XosCondFunc)(void * arg, int32_t sig_value);

//-----------------------------------------------------------------------------
// Condition object.
//-----------------------------------------------------------------------------
typedef struct XosCond {
  XosThreadQueue        queue;          // Queue of waiters.
#if XOS_COND_DEBUG
  uint32_t              sig;            // Valid signature indicates inited.
#endif
} XosCond;


//-----------------------------------------------------------------------------
//  Initialize a condition object before first use.
//
//  cond        Pointer to condition object.
//
//  Returns: nothing.
//-----------------------------------------------------------------------------
void
xos_cond_create(XosCond * cond);


//-----------------------------------------------------------------------------
//  Destroy a condition object. Must have been previously created by calling
//  xos_cond_create().
//
//  cond        Pointer to condition object.
//
//  Returns: nothing.
//-----------------------------------------------------------------------------
void
xos_cond_delete(XosCond * cond);


//-----------------------------------------------------------------------------
//  Wait on a condition: block until the condition is satisfied. The condition
//  is satisfied when xos_cond_signal() is called on this condition *and* the
//  condition callback function returns non-zero. If there is no callback
//  function, then the condition is automatically satisfied.
//
//  The condition structure must have been initialized before first use by
//  calling xos_cond_create().
//
//  cond         Pointer to condition object.
//
//  cond_fn      Pointer to a function, called by xos_cond_signal(), that
//               should return non-zero if this thread should be resumed.
//               Function is invoked as:  (*cond_fn)(cond_arg, sig_value).
//
//  cond_arg     Argument passed to \a cond_fn.
//
//  Returns: The value passed to \c xos_cond_signal().
//-----------------------------------------------------------------------------
int32_t
xos_cond_wait(XosCond * cond, XosCondFunc * cond_fn, void * cond_arg);


//-----------------------------------------------------------------------------
//  Trigger the condition: wake all threads waiting on the condition, if their
//  condition function evaluates to true (non-zero). If there is no condition
//  function for a thread then it is automatically awakened.
//
//  The condition structure must have been initialized before first use by
//  calling xos_cond_create().
//
//  cond        Pointer to condition object.
//  sig_value   Value passed to all waiters, returned by \c xos_cond_wait().
//
//  Returns:    The number of waiting threads that were resumed.
//
//  NOTE: Signaling a condition that has no waiters has no effect on it, and
//  the signal is not remembered. Any thread that waits on it later must be
//  woken by another call to xos_cond_signal().
//-----------------------------------------------------------------------------
int32_t
xos_cond_signal(XosCond * cond, int32_t sig_value);


#ifdef _cplusplus
}
#endif

#endif  //      __XOS_COND_H__

