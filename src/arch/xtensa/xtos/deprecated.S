// deprecated.S -- Deprecated assembler functions

// Copyright (c) 2003-2015 Tensilica Inc.
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


#if XCHAL_HAVE_XEA1 || XCHAL_HAVE_XEA2

	.text 

/*
	void _xtos_timer_<N>_delta(int cycles)

	Set the timer from the current ccount.
	DEPRECATED.  PLEASE USE xthal_set_ccompare() AND xthal_get_ccount()
	INSTEAD, OR USE DIFFERENT ALGORITHM THAT UPDATES CCOMPAREn RELATIVE TO
	LAST CCOMPAREn FOR DRIFT-FREE PERIODIC TIMER INTERRUPTS.
*/
	.macro	define_timer_delta	num, numtimers
	.if ((\num-\numtimers) & ~0xFFF)	// num < numtimers ?
	.align 4 
	.global _xtos_timer_&num&_delta
	.type _xtos_timer_&num&_delta,@function
_xtos_timer_&num&_delta:
	abi_entry
	rsr.ccount	a3
	add	a3, a3, a2
	writesr	ccompare \num a3
	abi_return
	.size _xtos_timer_&num&_delta, . - _xtos_timer_&num&_delta
	.endif
	.endm



#if   defined(__SPLIT__t0_delta)

	define_timer_delta	0, XCHAL_NUM_TIMERS

#elif defined(__SPLIT__t1_delta)

	define_timer_delta	1, XCHAL_NUM_TIMERS

#elif defined(__SPLIT__t2_delta)

	define_timer_delta	2, XCHAL_NUM_TIMERS

#elif defined(__SPLIT__t3_delta)

	define_timer_delta	3, XCHAL_NUM_TIMERS


#elif defined(__SPLIT__read_ints)

	//
	// u32 _xtos_read_ints( void )
	// 
	// _xtos_read_ints() reads the INTERRUPT register and returns it.
	// DEPRECATED.
	// [Kept temporarily because it was documented in T1050 System SW Ref Manual.]
	//
	.text 
	.align 4 
	.global _xtos_read_ints
	.type _xtos_read_ints,@function
_xtos_read_ints:
	abi_entry
#if XCHAL_HAVE_INTERRUPTS
	rsr.interrupt	a2
#else /*XCHAL_HAVE_INTERRUPTS*/
	movi	a2, 0
#endif /*XCHAL_HAVE_INTERRUPTS*/
	abi_return
	.size	_xtos_read_ints, . - _xtos_read_ints


#elif defined(__SPLIT__clear_ints)

	//
	// void _xtos_clear_ints( u32 mask )
	//
	// _xtos_clear_ints() clears selected bits of the INTERRUPT register.
	// DEPRECATED.
	// [Kept temporarily because it was documented in T1050 System SW Ref Manual.]
	//
	.text 
	.align 4 
	.global _xtos_clear_ints
	.type _xtos_clear_ints,@function
_xtos_clear_ints:
	abi_entry
#if XCHAL_HAVE_INTERRUPTS
	wsr.intclear	a2
#endif /*XCHAL_HAVE_INTERRUPTS*/
	abi_return
	.size	_xtos_clear_ints, . - _xtos_clear_ints

#endif /* splitting */

#endif /* XEA1 or XEA2 */

