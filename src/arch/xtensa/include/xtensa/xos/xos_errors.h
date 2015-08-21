
// xos_errors.h - X/OS error codes.

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


#ifndef __XOS_ERRORS_H__
#define __XOS_ERRORS_H__

#include <stdint.h>

//-----------------------------------------------------------------------------
//  List of XOS error codes. All error codes are negative integers, except for
//  XOS_OK which is zero.
//  XOS error codes occupy the range from -65536 up to -1.
//  The macro IS_XOS_ERRCODE() can be used to check if a value lies within the
//  error code range.
//-----------------------------------------------------------------------------

#define _XOS_ERR_FIRST          (-65536)
#define _XOS_ERR_LAST           (-1)

typedef enum xos_err_t {
    XOS_OK = 0,

    XOS_ERR_NOT_FOUND = _XOS_ERR_FIRST,
    XOS_ERR_INVALID_PARAMETER,
    XOS_ERR_LIMIT,
    XOS_ERR_NOT_OWNED,
    XOS_ERR_MUTEX_LOCKED,
    XOS_ERR_MUTEX_NOT_OWNED,
    XOS_ERR_MUTEX_ALREADY_OWNED,
    XOS_ERR_MUTEX_DELETE,
    XOS_ERR_COND_DELETE,
    XOS_ERR_SEM_DELETE,
    XOS_ERR_SEM_BUSY,
    XOS_ERR_EVENT_DELETE,
    XOS_ERR_MSGQ_FULL,
    XOS_ERR_MSGQ_EMPTY,
    XOS_ERR_MSGQ_DELETE,
    XOS_ERR_TIMER_DELETE,
    XOS_ERR_CONTAINER_NOT_RTC,
    XOS_ERR_CONTAINER_NOT_SAME_PRI,
    XOS_ERR_STACK_TOO_SMALL,
    XOS_ERR_CONTAINER_ILLEGAL,
    XOS_ERR_ILLEGAL_OPERATION,
    XOS_ERR_THREAD_EXITED,
    XOS_ERR_NO_TIMER,
    XOS_ERR_FEATURE_NOT_PRESENT,

    XOS_ERR_UNHANDLED_INTERRUPT,
    XOS_ERR_UNHANDLED_EXCEPTION,
    XOS_ERR_INTERRUPT_CONTEXT,
    XOS_ERR_THREAD_BLOCKED,
    XOS_ERR_ASSERT_FAILED,
    XOS_ERR_CLIB_ERR,
    XOS_ERR_INTERNAL_ERROR,

    XOS_ERR_LAST = _XOS_ERR_LAST,
} xos_err_t;


//-----------------------------------------------------------------------------
//  Return nonzero if 'val' is in the XOS error code range.
//-----------------------------------------------------------------------------
static inline int32_t
IS_XOS_ERRCODE(xos_err_t val)
{
    return ((val >= _XOS_ERR_FIRST) && (val <= _XOS_ERR_LAST));
}


#endif // __XOS_ERRORS_H__

