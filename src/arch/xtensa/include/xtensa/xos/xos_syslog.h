
// xos_syslog.h - X/OS Event logging module.

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


#ifndef __XOS_SYSLOG_H__
#define __XOS_SYSLOG_H__

#include <stdint.h>

#ifdef _cplusplus
extern "C" {
#endif


//-----------------------------------------------------------------------------
//  The X/OS system log is an array of fixed size entries. The size of the log
//  is determined by the application, and memory for the log must be provided
//  at init time. Every time the log function is called, an entry is made in
//  the log and the next pointer advanced. When the log is full, it will wrap
//  around and start overwriting the oldest entries.
//  Logging can be done from C/C++ code as well as assembly code, and at any
//  interrupt level, even from high level interrupt handlers.
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
//  Defines.
//-----------------------------------------------------------------------------
#define XOS_SYSLOG_ENABLED              0x0001


//-----------------------------------------------------------------------------
//  Use this macro to compute how much memory to allocate for the syslog.
//-----------------------------------------------------------------------------
#define XOS_SYSLOG_SIZE(num_entries) \
    ( sizeof(XosSysLog) + ((num_entries - 1) * sizeof(XosSysLogEntry)) )


//-----------------------------------------------------------------------------
//  Event log entry structure. The fields are as follows:
//
//  timestamp           64-bit timestamp in clock cycles.
//
//  param1              User defined value.
//
//  param2              User defined value.
//
//  next                Link to next entry (reserved).
//-----------------------------------------------------------------------------
typedef struct XosSysLogEntry {
    uint64_t                timestamp;  // System cycle count
    uint32_t                param1;     // User defined value
    uint32_t                param2;     // User defined value
    struct XosSysLogEntry * next;       // Link to next entry
} XosSysLogEntry;


//-----------------------------------------------------------------------------
//  Event log struct.
//-----------------------------------------------------------------------------
typedef struct XosSysLog {
    uint16_t         flags;             // Flags
    uint16_t         size;              // Number of entries
    XosSysLogEntry * next;              // Next write position
    XosSysLogEntry   entries[1];        // First entry
} XosSysLog;


//-----------------------------------------------------------------------------
//  Pointer to syslog area.
//-----------------------------------------------------------------------------
extern XosSysLog * xos_syslog;


//-----------------------------------------------------------------------------
//  Initialize the syslog. Initializing the log also enables it. The system
//  log always wraps around when full and overwrites the oldest entries.
//
//  log_mem             Pointer to allocated memory for the log.
//
//  num_entries         The number of entries that the log can contain.
//
//  Returns : Nothing.
//-----------------------------------------------------------------------------
static inline void
xos_syslog_init(void * log_mem, uint16_t num_entries)
{
    uint16_t i;

    xos_syslog = (XosSysLog *) log_mem;
    xos_syslog->size = num_entries;
    xos_syslog->next = xos_syslog->entries;

    for (i = 0; i < num_entries - 1; i++) {
        xos_syslog->entries[i].next = &(xos_syslog->entries[i+1]);
        xos_syslog->entries[i].timestamp = 0;
    }
    xos_syslog->entries[i].next = xos_syslog->entries;
    xos_syslog->entries[i].timestamp = 0;

    xos_syslog->flags = XOS_SYSLOG_ENABLED;
}


//-----------------------------------------------------------------------------
//  Reset the log. All entries made up to now are abandoned.
//
//  No parameters.
//
//  Returns : Nothing.
//-----------------------------------------------------------------------------
static inline void
xos_syslog_clear()
{
    uint32_t ps = xos_set_intlevel(XCHAL_NUM_INTLEVELS);

    xos_syslog_init(xos_syslog, xos_syslog->size);
    xos_restore_intlevel(ps);
}


//-----------------------------------------------------------------------------
//  Enable logging to the syslog.
//
//  No parameters.
//
//  Returns : Nothing.
//-----------------------------------------------------------------------------
static inline void
xos_syslog_enable() 
{
    uint32_t ps = xos_set_intlevel(XCHAL_NUM_INTLEVELS);
 
    xos_syslog->flags |= XOS_SYSLOG_ENABLED;
    xos_restore_intlevel(ps);
}


//-----------------------------------------------------------------------------
//  Disable logging to the syslog. It is sometimes useful to disable logging
//  while the log is being examined or dumped.
//
//  No parameters.
//
//  Returns : Nothing.
//-----------------------------------------------------------------------------
static inline void
xos_syslog_disable()
{
    uint32_t ps = xos_set_intlevel(XCHAL_NUM_INTLEVELS);

    xos_syslog->flags &= ~XOS_SYSLOG_ENABLED;
    xos_restore_intlevel(ps);
}


//-----------------------------------------------------------------------------
//  Write an entry into the syslog. This function does disable all interrupts
//  since logging can be done from interrupt handlers as well.
//
//  param1              User defined value.
//
//  param2              User defined value.
//
//  Returns : Nothing.
//-----------------------------------------------------------------------------
static inline void
xos_syslog_write(uint32_t param1, uint32_t param2)
{
    if (xos_syslog) {
        uint32_t         ps   = xos_set_intlevel(XCHAL_NUM_INTLEVELS);
        XosSysLogEntry * next = xos_syslog->next;

        next->timestamp = xos_get_system_cycles();
        next->param1    = param1;
        next->param2    = param2;

        xos_syslog->next = next->next;
        xos_restore_intlevel(ps);
    }
}


//-----------------------------------------------------------------------------
//  Read the first (oldest) entry in the syslog. Will return an error if the
//  log has not been created or is empty. Storage to copy the entry must be
//  provided by the caller.
//
//  entry               Pointer to storage where the entry data will be copied.
//                      This pointer must be passed to xos_syslog_get_next().
//
//  Returns : XOS_OK on success, else error code.
//-----------------------------------------------------------------------------
static inline int32_t
xos_syslog_get_first(XosSysLogEntry * entry)
{
    if (!xos_syslog) {
        return XOS_ERR_NOT_FOUND;
    }

    if (entry) {
        uint32_t         ps   = xos_set_intlevel(XCHAL_NUM_INTLEVELS);
        XosSysLogEntry * next = xos_syslog->next;

        // 'next' should be pointing to the next entry to be overwritten, if we
        // have wrapped. This means it is the oldest entry. However if this entry
        // has a zero timestamp then we have not wrapped, in which case we must
        // look at the first entry in the list.
        if (next->timestamp == 0) {
            next = xos_syslog->entries;
        }

        *entry = *next;
        xos_restore_intlevel(ps);
        return entry->timestamp ? XOS_OK : XOS_ERR_NOT_FOUND;
    }

    return XOS_ERR_INVALID_PARAMETER;
}


//-----------------------------------------------------------------------------
//  Get the next sequential entry from the syslog. This function must be called
//  only after xos_syslog_get_first() has been called.
//
//  entry               Pointer to storage where the entry data will be copied.
//                      Must be the same pointer that was passed in the call to
//                      xos_syslog_get_first(), as it is used to keep track of
//                      the current position.
//
//  Returns : XOS_OK on success, else error code.
//-----------------------------------------------------------------------------
static inline int32_t
xos_syslog_get_next(XosSysLogEntry * entry)
{
    if (entry) {
        uint32_t         ps   = xos_set_intlevel(XCHAL_NUM_INTLEVELS);
        XosSysLogEntry * next = entry->next;
        int32_t          ret  = XOS_OK;

        // Make sure we're not pointing past the last entry.
        if (next && (next != xos_syslog->next) && next->timestamp) {
            *entry = *next;
        }
        else {
            ret = XOS_ERR_NOT_FOUND;
        }
        xos_restore_intlevel(ps);
        return ret;
    }

    return XOS_ERR_INVALID_PARAMETER;
}


#ifdef _cplusplus
}
#endif

#endif // __XOS_SYSLOG_H__

