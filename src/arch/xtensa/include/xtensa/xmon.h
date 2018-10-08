/* xmon.h - XMON definitions
 *
 * $Id: //depot/rel/Foxhill/dot.8/Xtensa/OS/xmon/xmon.h#1 $
 *
 * Copyright (c) 2001-2013 Tensilica Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef __H_XMON
#define __H_XMON

#ifndef UCHAR
# define UCHAR unsigned char
#endif

#ifndef C_UCHAR
# define C_UCHAR const unsigned char
#endif

#ifndef UINT32
# define UINT32 unsigned int
#endif

/* Default GDB packet size */
#define GDB_PKT_SIZE 4096

/*XMON signals */
#define XMON_SIGINT    2   /*target was interrupted */
#define XMON_SIGILL    4   /*illegal instruction */
#define XMON_SIGTRAP   5   /*general exception */
#define XMON_SIGSEGV   11  /*page faults */


/* Type of log message from XMON to the application */
typedef enum {
   XMON_LOG,
   XMON_TRACE,
   XMON_ERR,
} xmon_log_t;


#ifdef  _cplusplus
extern "C" {
#endif

/*
 * THE FOLLOWING ROUTINES ARE USED BY THE USER
 */

/** 
 * Initialize XMON so GDB can attach.
 * gdbBuf     - pointer to a buffer XMON uses to comm. with GDB
 * gdbPktSize - Size of the allocated buffer for GDB communication.
 * xlog       - log handler for XMON produced errors/logs/traces
 
 */
extern int 
_xmon_init(char* gdbBuf, int gdbPktSize,
           void(*xlog)(xmon_log_t type, const char* str));

/** 
 * Detach from XMON. Can execute at any time
 */
extern void 
_xmon_close(void);

/** 
 * Print message to GDB 
 */
extern void 
_xmon_consoleString(const char* str);

/** 
 * XMON version
 */
extern const char* 
_xmon_version();

/** 
 * Enable disable various logging and tracing chains
 * app_log_en   - enable/disable logging to the app log handler.
 *                ENABLED BY DEFAULT.
 * app_trace_en - enable/disable tracing to the app log handler.
 *                DISABLED BY DEFAULT.
 * gdb_log_en   - enable/disable log notifications to the GDB.
 *                ENABLED BY DEFAULT.
 * gdb_trace_en - enable/disable tracing notifications to the GDB.
 *                DISABLED BY DEFAULT.
 */
extern void 
_xmon_log(char app_log_en, char app_trace_en, 
          char gdb_log_en, char gdb_trace_en);

//extern int
//_xmon_process_packet (int len, char* buf);

//extern int
//_xmon_process_packet2 (void);

/*
 * THE FOLLOWING ROUTINES NEED TO BE PROVIDED BY USER
 */
 
/* 
 * Receive remote packet bytes from GDB
 * wait:    If the function would block waiting for more 
 *          characters from gdb, wait=0 instructs it to 
 *          return 0 immediatelly. Otherwise, if wait=1, 
 *          the function may or may not wait for GDB. 
 *          NOTE: Current XMON version supports single char
 *          input only (return value is 1 always)
 * buf:     Pointer to the buffer for the received data.
 * Returns: 0  - no data avaiable, 
            >0 - length of received array in buf.
 */
extern int 
_xmon_in(int wait, UCHAR* buf);

/* 
 * Output an array of chars to GDB 
 * len - number of chars in the array
 */
extern void
_xmon_out(int len, UCHAR*);

/* 
 * Flush output characthers 
 * XMON invokes this one when a full response is ready
 */
extern int
_xmon_flush(void);  // flush output characters

#ifdef  _cplusplus
}
#endif


#endif
