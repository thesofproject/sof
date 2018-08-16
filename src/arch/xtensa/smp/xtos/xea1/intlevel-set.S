// xea1/intlevel-set.S - Interrupt related assembler code - _xtos_set_intlevel

// Copyright (c) 2004-2016 Tensilica Inc.
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
#include <xtensa/config/specreg.h>
#include "../xtos-internal.h"

#if XCHAL_HAVE_XEA1


/***************************************************************************
 *  unsigned  _xtos_set_intlevel(int intlevel);
 *
 *  _xtos_set_intlevel() is used to set the current priority from C code;
 *  it can be called from the application or from a C interrupt handler.
 *
 *  NOTE:  This version allows the 'intlevel' parameter to be computed
 *  at run-time, and thus is longer.  It is much more efficient, and
 *  highly recommented, to use the XTOS_SET_INTLEVEL(intlevel) macro instead
 *  (which requires a constant intlevel).
 */

	.text
	.align 4
	.global	_xtos_set_intlevel
	.type	_xtos_set_intlevel,@function
_xtos_set_intlevel:
	abi_entry
#if XCHAL_HAVE_INTERRUPTS
	/*  In XEA1, we have to rely on INTENABLE register virtualization:  */
	movi	a4, Xthal_intlevel_andbelow_mask
	extui	a3, a2, 0, 4	// keep only INTLEVEL bits of parameter
	addx4	a5, a3, a4	// index mask to use
	l32i	a3, a5, 0	// get mask of interrupts at requested intlevel and below
	movi	a5, -1		// all 1's
	xtos_addr_percore	a4, xtos_intstruct
	xor	a3, a3, a5	// mask of interrupts at intlevels above the requested one (to enable)
	xtos_lock	a7	// MUST USE highest address register of function to avoid window overflows in critical section
	l32i	a2, a4, XTOS_VPRI_ENABLED_OFS	// return old xtos_vpri_enabled (current vpri)
	l32i	a5, a4, XTOS_ENABLED_OFS	// a5 = xtos_enabled
	s32i	a3, a4, XTOS_VPRI_ENABLED_OFS	// set new xtos_vpri_enabled (current vpri)
	and	a5, a5, a3			// a5 = xtos_enabled & xtos_vpri_enabled
	wsr.intenable	a5
	xtos_unlock	a7
#endif /*XCHAL_HAVE_INTERRUPTS*/
	abi_return

	.size	_xtos_set_intlevel, . - _xtos_set_intlevel

#endif /* XEA1 */

