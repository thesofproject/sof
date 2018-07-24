// 
// debug_hndlr.S -- default Xtensa debug exception handler
//
// $Id: //depot/rel/Eaglenest/Xtensa/OS/hal/debug_hndlr.S#1 $

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
#include <xtensa/config/specreg.h>
#include <xtensa/config/system.h>

#if XCHAL_HAVE_DEBUG && XCHAL_HAVE_EXCEPTIONS

	/*
	 *  Default debug exception handler.
	 *
	 *  Note that the debug exception vector must save a3
	 *  in EXCSAVE+XCHAL_DEBUGLEVEL before jumping here.
	 *
	 *  This handler is used when no debugger is present.
	 *  The end result of executing this default handler
	 *  is as if no debug exception had occurred, eg. as if
	 *  the core was running at PS.INTLEVEL >= DEBUGLEVEL.
	 *
	 *  Because the debug exception vector might get
	 *  placed in ROM, and be expected to work regardless
	 *  of what executable image or OS is running in RAM,
	 *  we're very careful not to use any RAM here.
	 *  We don't know what RAM we can safely use.
	 *  This tricky part to accomplishing this feat
	 *  is to use only *one* register (a3, which was
	 *  saved in EXCSAVE+XCHAL_DEBUGLEVEL), because we don't
	 *  have RAM in which to safely store other regs.
	 *
	 *  A real debugger application would normally
	 *  have some kind of conventions, or special
	 *  hardware support, to have its own RAM workspace
	 *  in which to save context and do real work
	 *  in this handler.
	 */


#if XSHAL_DEBUG_VECTOR_ISROM
	//  Debug exception vector is in ROM, so place the handler
	//  in ROM also.  Otherwise running different executables
	//  with that ROM will not work because the handler would
	//  likely not be there or be at the wrong address.
	//
	.section	.srom.text, "ax"
#else
	//  Debug exception vector is in RAM, so we can safely
	//  place the handler in RAM as well.
	//
	.text
#endif

	.global xthal_debugexc_defhndlr_nw
	.align 4
xthal_debugexc_defhndlr_nw:
	rsr	a3, DEBUGCAUSE		// get cause of debug exception

	//  Check for possible debug causes, in priority order.
	//  We only handle the highest priority condition present.
	//  (If there are multiple conditions, the lower priority
	//   condition(s) will normally trigger upon return from
	//   this exception handler.)

	bbci.l	a3, DEBUGCAUSE_ICOUNT_SHIFT, 1f	// ICOUNT trap?
	movi	a3, 0
	wsr	a3, ICOUNT		// clear ICOUNT
	j	3f

/*
 *  Ensure that we have IBREAKs, otherwise the IBREAKENABLE
 *  special register is not there:
 */
#if XCHAL_NUM_IBREAK > 0
1:	bbci.l	a3, DEBUGCAUSE_IBREAK_SHIFT, 1f	// IBREAK match?
	movi	a3, 0
	wsr	a3, IBREAKENABLE	// disable IBREAK traps
	j	3f
#endif

/*  Also check for DBREAK registers:  */
#if XCHAL_NUM_DBREAK > 0
1:	bbci.l	a3, DEBUGCAUSE_DBREAK_SHIFT, 1f		// DBREAK match?
	movi	a3, 0
	wsr	a3, DBREAKC_0			// disable DBREAK register 0
# if XCHAL_NUM_DBREAK > 1
	wsr	a3, DBREAKC_1			// disable DBREAK register 1
# endif
	j	3f
#endif

1:	bbci.l	a3, DEBUGCAUSE_BREAK_SHIFT, 1f		// BREAK instruction?
	//rsr	a3, EPC+XCHAL_DEBUGLEVEL	// get PC pointing to BREAK
	//l8ui	a3, a3, 1			// get first 4-bit operand of BREAK (in 2nd byte)
	//extui	a3, a3, (XCHAL_HAVE_BE*4), 4	// pos depends on endianness
	//bnei	a3, 1, 3f			// is it a BREAK 1,x instruction?
	rsr	a3, EPC+XCHAL_DEBUGLEVEL	// get PC pointing to BREAK
	addi	a3, a3, 3			// skip BREAK instruction
	wsr	a3, EPC+XCHAL_DEBUGLEVEL	// update PC
	j	3f

1:	bbci.l	a3, DEBUGCAUSE_BREAKN_SHIFT, 1f		// BREAK.N instruction?
	rsr	a3, EPC+XCHAL_DEBUGLEVEL	// get PC pointing to BREAK
	addi	a3, a3, 2			// skip BREAK.N instruction
	wsr	a3, EPC+XCHAL_DEBUGLEVEL	// update PC
	j	3f

1:	bbci.l	a3, DEBUGCAUSE_DEBUGINT_SHIFT, 1f	// debug interrupt?
	//  Nothing to do...
	j	3f

1:	//  Unknown debug case?  ignore

3:	rsr	a3, EXCSAVE+XCHAL_DEBUGLEVEL	// restore a3
	rfi	XCHAL_DEBUGLEVEL		// return from debug exception

	.size	xthal_debugexc_defhndlr_nw, . - xthal_debugexc_defhndlr_nw


#if XSHAL_DEBUG_VECTOR_ISROM
	.text		// in case this gets included by something else
#endif

#endif /* XCHAL_HAVE_DEBUG */

