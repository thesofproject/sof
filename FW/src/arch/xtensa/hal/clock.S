// 
// clock.S - assembly language clock routines
//
// $Id: //depot/rel/Eaglenest/Xtensa/OS/hal/clock.S#1 $

// Copyright (c) 2003-2010 Tensilica Inc.
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


//  A useful looping macro:
//  'iterate' invokes 'what' (an instruction, pseudo-op or other macro)
//  multiple times, passing it a numbered parameter from 'from' to 'to'
//  inclusively.  Does not invoke 'what' at all if from > to.
//  Maximum difference between 'from' and 'to' is 99 minus nesting depth
//  (GNU 'as' doesn't allow nesting deeper than 100).
//
	.macro	iterate		from, to, what
	.ifeq	((\to-\from) & ~0xFFF)
	\what	\from
	iterate	"(\from+1)", \to, \what
	.endif
	.endm	// iterate


//----------------------------------------------------------------------
// Read CCOUNT register
//----------------------------------------------------------------------

// unsigned xthal_get_ccount(void)
//
	.global	xthal_get_ccount
	.type	xthal_get_ccount,@function
	.align	4
xthal_get_ccount:
	abi_entry
#if XCHAL_HAVE_CCOUNT
	rsr	a2, CCOUNT
/*
 *  The following alternative (in absence of CCOUNT) doesn't work well,
 *  because ICOUNT is often used for debugging.  (And when it isn't,
 *  one would have to ensure that ICOUNTLEVEL is high enough and that
 *  ICOUNT is incremented to zero in the debug exception handler upon
 *  ICOUNT exceptions.)
 *
 *	#elif XCHAL_HAVE_DEBUG
 *		rsr	a2, ICOUNT	// no CCOUNT, return ICOUNT if available
 */
#else
	movi	a2, 0		// else no counter at all, just return zero
#endif
	abi_return
	.size	xthal_get_ccount, . - xthal_get_ccount


//----------------------------------------------------------------------
// Access CCOMPAREn registers
//----------------------------------------------------------------------

// void xthal_set_ccompare(int, unsigned)
//
	.global	xthal_set_ccompare
	.type	xthal_set_ccompare,@function
	.align	4
xthal_set_ccompare:
	abi_entry
#if XCHAL_NUM_TIMERS > 0
	bnez	a2, 1f
	wsr	a3, CCOMPARE_0

	.macro	set_ccompare_n	num
	j	2f
1:
	bgeui	a2, (\num + 1), 1f
	wsr	a3, CCOMPARE_0 + \num
	.endm
	iterate	1, XCHAL_NUM_TIMERS-1, set_ccompare_n

2:	isync
	isync_retw_nop
1:
#endif /* XCHAL_NUM_TIMERS > 0 */
	abi_return
	.size	xthal_set_ccompare, . - xthal_set_ccompare


// unsigned xthal_get_ccompare(int)
//
	.global	xthal_get_ccompare
	.type	xthal_get_ccompare,@function
	.align	4
xthal_get_ccompare:
	abi_entry
#if XCHAL_NUM_TIMERS > 0
	bnez	a2, 1f
	rsr	a2, CCOMPARE_0
	abi_return
1:

	.macro	get_ccompare_n	num
	bgeui	a2, (\num + 1), 1f
	rsr	a2, CCOMPARE_0 + \num
	abi_return
1:
	.endm
	iterate	1, XCHAL_NUM_TIMERS-1, get_ccompare_n

#endif /* XCHAL_NUM_TIMERS > 0 */
	movi	a2, 0		// non-existent CCOMPAREn, return 0
2:
	abi_return
	.size	xthal_get_ccompare, . - xthal_get_ccompare


