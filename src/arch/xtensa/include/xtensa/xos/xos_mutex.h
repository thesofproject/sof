
// xos_mutex.h - X/OS Mutex API interface and data structures.

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

#ifndef __XOS_MUTEX_H__
#define __XOS_MUTEX_H__

#include <stdint.h>

#ifdef _cplusplus
extern "C" {
#endif


//-----------------------------------------------------------------------------
// Mutex flags.
//-----------------------------------------------------------------------------
#define XOS_MUTEX_WAIT_PRIORITY 0x0000  // Wake waiters in priority order (default)
#define XOS_MUTEX_WAIT_FIFO     0x0001  // Wake waiters in FIFO order
#define XOS_MUTEX_PRIORITY_CLG  0x0004  // Use priority ceiling
#define XOS_MUTEX_PRIORITY_INV  0x0008  // Protect against priority inversion


//-----------------------------------------------------------------------------
// Mutex object.
//-----------------------------------------------------------------------------
typedef struct XosMutex {
  XosThread *           owner;          // Owning thread (null if unlocked).
  XosThreadQueue        waitq;          // Queue of waiters.
  uint32_t              flags;          // Properties.
  uint32_t              priority;
#if XOS_MUTEX_DEBUG
  uint32_t              sig;            // Valid signature indicates inited.
#endif
} XosMutex;


//-----------------------------------------------------------------------------
//  Initialize a mutex object before first use.
//
//  mutex        Pointer to mutex object.
//
//  flags        Creation flags:
//               XOS_MUTEX_WAIT_FIFO -- Queue waiting threads in fifo order.
//               XOS_MUTEX_WAIT_PRIORITY -- Queue waiting threads by priority.
//                   This is the default.
//               XOS_MUTEX_PRIORITY_CLG -- Use specified priority value as
//                   the mutex's priority ceiling. If the owning thread has a
//                   priority lower than the mutex's priority, then the thread
//                   will have its priority raised to the higher value as long
//                   as it owns the mutex.
//               XOS_MUTEX_PRIORITY_INV -- Protect against priority inversion.
//                   If there is a waiting thread with a higher priority than
//                   the current owner thread, then the owner thread's priority
//                   is raised to the higher value for as long as it owns the
//                   mutex.
//
//  priority     Mutex's priority ceiling. This is used only if the 
//               XOS_MUTEX_PRIORITY_CLG flag is set.
//
//  Returns: XOS_OK on success, else error code.
//-----------------------------------------------------------------------------
int32_t
xos_mutex_create(XosMutex * mutex, uint32_t flags, uint8_t priority);


//-----------------------------------------------------------------------------
//  Destroy a mutex object. Must have been previously created by calling
//  xos_mutex_create().
//
//  mutex        Pointer to mutex object.
//
//  Returns: XOS_OK on success, else error code.
//-----------------------------------------------------------------------------
int32_t
xos_mutex_delete(XosMutex * mutex);


//-----------------------------------------------------------------------------
//  Take ownership of the mutex: block until the mutex is owned.
//
//  The mutex structure must have been initialized before first use by
//  calling xos_mutex_create().
//
//  mutex         Pointer to mutex object.
//
//  Returns: XOS_OK on success, else error code.
//-----------------------------------------------------------------------------
int32_t
xos_mutex_lock(XosMutex * mutex);


//-----------------------------------------------------------------------------
//  Release ownership of the mutex. The mutex must have been inited previously
//  and must be owned by the calling thread.
//
//  mutex        Pointer to mutex object.
//
//  Returns: XOS_OK on success, else error code.   
//-----------------------------------------------------------------------------
int32_t
xos_mutex_unlock(XosMutex * mutex);


//-----------------------------------------------------------------------------
//  Try to take ownership of the mutex, but do not block if the mutex is taken.
//  Return immediately. The mutex object must have been initialized previously.
//
//  mutex        Pointer to mutex object.
//
//  Returns: XOS_OK on success (mutex owned), else error code.
//-----------------------------------------------------------------------------
int32_t
xos_mutex_trylock(XosMutex * mutex);


//-----------------------------------------------------------------------------
//  Return the state of the mutex (locked or unlocked) but do not attempt to
//  take ownership. The mutex object must have been initialized previously.
//
//  mutex        Pointer to mutex object.
//
//  Returns: 0 if the mutex is unlocked, 1 if it is locked, -1 on error.
//-----------------------------------------------------------------------------
static inline int32_t
xos_mutex_test(XosMutex * mutex)
{
    XOS_ASSERT(mutex);

    if (mutex) {
        return mutex->owner ? 1 : 0;
    }
    return -1;
}


#ifdef _cplusplus
}
#endif

#endif  //      __XOS_MUTEX_H__

