// 
// mp_asm.S - multi-processor synchronization routines
//
// $Id$

// Copyright (c) 2003, 2005, 2010 Tensilica Inc.
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


/*
  int xthal_compare_and_set( int *address, int test_value, int set_value )

  Atomically sets *address to set_value if *address equals test_value.
  Returns the previous value of *address (the one compared with test_value).

  Uses the S32C1I instruction if available.
  S32C1I requires special support from the memory controller for
  memory accessed via the PIF interface.  For this and other reasons,
  S32C1I might not work on the entire 4GB address range.  This function
  does not test address validity.  That is the responsibility of the
  software invoking this function.
*/
	.text
	.align 4
	.global xthal_compare_and_set
	.type	xthal_compare_and_set,@function

xthal_compare_and_set:
	abi_entry
	// a2 == address
	// a3 == test value
	// a4 == set value

#if XCHAL_HAVE_S32C1I && XCHAL_HW_MIN_VERSION_MAJOR >= 2200
	wsr	a3, SCOMPARE1
	s32c1i	a4, a2, 0
	mov	a2, a4
#else
	mov	a7, a2		// a7 == address, a2 is return val
# if XCHAL_HAVE_INTERRUPTS
	rsil	a5, 15		// a5 == new ps
# endif
	l32i	a2, a7, 0	// a2 == value to test, return val
	bne	a3, a2, done	// test

	s32i	a4, a7, 0	// write the new value

done:
# if XCHAL_HAVE_INTERRUPTS
	wsr	a5, PS		// restore the PS
	rsync
# endif
#endif
	abi_return

	.size	xthal_compare_and_set, . - xthal_compare_and_set


/*
  unsigned  xthal_get_prid( void );

  Returns the value of the PRID register (processor ID),
  or 0 if not configured.
  (Note: this register, when present, cannot / must-not
  change value during runtime; on certain processors,
  its value may get sampled only at reset.
  It can never be written to, hence
  there is no xthal_set_prid() function.)
*/
	.align 4
	.global xthal_get_prid
	.type	xthal_get_prid,@function
xthal_get_prid:
	abi_entry
#if XCHAL_HAVE_PRID
	rsr	a2, PRID
#else
	movi	a2, 0
#endif
	abi_return
	.size	xthal_get_prid, . - xthal_get_prid

