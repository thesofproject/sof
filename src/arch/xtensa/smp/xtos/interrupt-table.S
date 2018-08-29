// interrupt-table.S - Interrupt handler table and default handler

// Copyright (c) 2004-2017 Cadence Design Systems, Inc.
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

#include <xtensa/coreasm.h>
#include "xtos-internal.h"


#if XCHAL_HAVE_INTERRUPTS

	//
	// void xtos_unhandled_interrupt( int n );
	// 
	//  Default/empty interrupt handler.
	//  This is used for interrupts with no registered handler.
	//  Parameter n is the interrupt number (0 thru 31).
	//

	.text 
	.align 4 
	.global xtos_unhandled_interrupt
	.type xtos_unhandled_interrupt,@function

xtos_unhandled_interrupt:
	abi_entry
# if XCHAL_HAVE_DEBUG
	//  Break into the debugger if one is present and active:
#  if XCHAL_NUM_INTLEVELS > XCHAL_DEBUGLEVEL
	rsil	a3, XCHAL_DEBUGLEVEL-1	// ensure break takes effect
#  endif
	break	1, 15		// unhandled (unregistered) interrupt $a2
# else
1:	j	1b		// unhandled interrupt - loop forever
# endif
	abi_return

	.size	xtos_unhandled_interrupt, . - xtos_unhandled_interrupt

#endif /* XCHAL_HAVE_INTERRUPTS */

