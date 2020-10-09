
/* int-sethandler.c - register an interrupt handler in XTOS */

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
#include "xtos-structs.h"
#include <sof/lib/cpu.h>


#if XCHAL_HAVE_INTERRUPTS
#if CONFIG_MULTICORE
extern struct xtos_core_data *core_data_ptr[CONFIG_CORE_COUNT];
#else
/*
 *  Table of interrupt handlers.
 *  NOTE:  if the NSA/NSAU instructions are configured, then to save
 *  a few cycles in the interrupt dispatcher code, the
 *  xtos_interrupt_table[] array is filled in reverse.
 *  IMPORTANT:  Use the MAPINT() macro defined in xtos-internal.h to index entries in this array.
 */
extern XtosIntHandlerEntry      xtos_interrupt_table[XCHAL_NUM_INTERRUPTS];
#endif
#endif

_xtos_handler _xtos_set_interrupt_handler_arg( int n, _xtos_handler f, void *arg )
{
#if XCHAL_HAVE_INTERRUPTS
    XtosIntHandlerEntry *entry;
    _xtos_handler old;
    _xtos_handler ret;

    if( (n < 0) || (n >= XCHAL_NUM_INTERRUPTS) ) {
        ret = 0;     /* invalid interrupt number */
    }
    else if( (int) Xthal_intlevel[n] > XTOS_LOCKLEVEL ) {
        ret = 0;     /* priority level too high to safely handle in C */
    }
    else {
#if CONFIG_MULTICORE
        entry = &(core_data_ptr[cpu_get_id()]->xtos_int_data.xtos_interrupt_table.array[MAPINT(n)]);
#else
        entry = xtos_interrupt_table + MAPINT(n);
#endif
        old = entry->handler;
        if (f) {
            entry->handler = f;
            entry->u.varg  = arg;
        } else {
            entry->handler = &xtos_unhandled_interrupt;
            entry->u.narg  = n;
        }
        ret = (old == &xtos_unhandled_interrupt) ? 0 : old;
    }

    return ret;
#else
    return 0;
#endif
}


_xtos_handler _xtos_set_interrupt_handler( int n, _xtos_handler f )
{
    return _xtos_set_interrupt_handler_arg( n, f, (void *) n );
}

