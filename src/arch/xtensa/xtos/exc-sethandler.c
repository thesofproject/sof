
/* exc-sethandler.c - register an exception handler in XTOS */

/*
 * Copyright (c) 1999-2017 Cadence Design Systems, Inc.
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

#include "xtos-internal.h"
#include <xtensa/config/core.h>


#if XCHAL_HAVE_EXCEPTIONS

extern void xtos_c_wrapper_handler(void *arg);   /* assembly-level wrapper for C handlers */
extern void xtos_unhandled_exception(void *arg); /* assembly-level handler for exceptions
                                                    with no registered handler */
extern void xtos_p_none(void *arg);              /* default/empty C handler */


extern _xtos_handler xtos_c_handler_table[XCHAL_EXCCAUSE_NUM];
extern _xtos_handler xtos_exc_handler_table[XCHAL_EXCCAUSE_NUM];

/*
 *  Register a C handler for the specified general exception
 *  (specified EXCCAUSE value).
 */
_xtos_handler _xtos_set_exception_handler( int n, _xtos_handler f )
{
    _xtos_handler ret = 0;

    if( n < XCHAL_EXCCAUSE_NUM ) {
        _xtos_handler func = f;

        if( func == 0 ) {
            func = &xtos_p_none;
        }
        ret = xtos_c_handler_table[n];
        xtos_exc_handler_table[n] = ( (func == &xtos_p_none)
                                    ? &xtos_unhandled_exception
                                    : &xtos_c_wrapper_handler );
        xtos_c_handler_table[n] = func;
        if( ret == &xtos_p_none ) {
            ret = 0;
        }
    }

    return ret;
}

#endif /* XCHAL_HAVE_EXCEPTIONS */

