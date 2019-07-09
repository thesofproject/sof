// window-vectors-xea2.S - Register Window Overflow/Underflow Handlers for XEA2
// $Id: //depot/rel/Foxhill/dot.8/Xtensa/OS/xtos/xea2/window-vectors.S#1 $

// Copyright (c) 1999-2016 Tensilica Inc.
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
#include <xtensa/xtruntime-frames.h>

#if XCHAL_HAVE_XEA2 && XCHAL_HAVE_WINDOWED && !defined(__XTENSA_CALL0_ABI__)

# ifndef NO_SECTION_DIRECTIVES
// Exports
.global _WindowOverflow4
.global _WindowUnderflow4
.global _WindowOverflow8
.global _WindowUnderflow8
.global _WindowOverflow12
.global _WindowUnderflow12
.global _xtos_alloca_handler

	//  Note:  the current window exception vectors do not generate any
	//  literals.  Hence the literal_prefix directive is not necessary.
	//  Specifying it "just in case" creates an empty section (named
	//  ".WindowVectors.literal") which can in some cases cause linking
	//  problems (the linker scripts don't place it anywhere).
	//  So leave it commented out:
	//
	//.begin	literal_prefix	.WindowVectors

	.section		.WindowVectors.text, "ax"
# endif


//
// GENERAL NOTES:
//
// These window exception handlers need not be modified.
// They are specific to the windowed call ABI only.
//
// Underflow Handlers:
//
// The underflow handler for returning from call[i+1] to call[i]
// must preserve all the registers from call[i+1]'s window.
// In particular, a0 and a1 must be preserved because the RETW instruction
// will be reexecuted (and may even underflow again if an intervening
// exception has flushed call[i]'s registers).
// Registers a2 and up may contain return values.
//
// The caller could also potentially assume that the callee's a0 and a1
// (its own a4&a5 if call4, a8&a9 if call8, a12&a13 if call12)
// are correct for whatever reason (not a clean thing to do in general,
// but if it's possible, unless the ABI explicitly prohibits it,
// it will eventually be done :) -- whether the the ABI needs to
// prohibit this is a different question).
//
// Timing of Handlers:
//
// Here is an overview of the overhead of taking a window exception,
// ie. the number of additional cycles taken relative to case where
// an exception is not taken.
// NOTE:  these numbers do not take into account any cache misses,
// write buffer stalls, or other external stalls, if they occur.
// The totals consist of 5 cycles to enter the handler (or 6 or 7
// for optional longer pipelines in Xtensa LX), the number of instructions
// and interlocks (2nd and 3rd columns below), and 2 cycles jump delay
// on return (3 cycles for optional longer I-side pipeline in Xtensa LX):
//
//			Instruction+bubbles	Totals (5-stage)
//			XEA1	XEA2		XEA1	XEA2
//	Overflow-4	7	5		14	12
//	Overflow-8	14	10		21	17
//	Overflow-12	18	14		25	21
//	Underflow-4	6	5		13	12
//	Underflow-8	14	10		21	17
//	Underflow-12	18	14		25	21
//
//	Underflow-8	15	12		25	22	(7-stage; could be made 1 less)
//	Underflow-12	19	16		29	26	(7-stage; could be made 1 less)

#ifndef WINDOW_BASE_VECOFS
#define WINDOW_BASE_VECOFS	XCHAL_WINDOW_OF4_VECOFS
#endif


// 4-Register Window Overflow Vector (Handler)
//
// Invoked if a call[i] referenced a register (a4-a15)
// that contains data from ancestor call[j];
// call[j] had done a call4 to call[j+1].
// On entry here:
//	window rotated to call[j] start point;
//	a0-a3 are registers to be saved;
//	a4-a15 must be preserved;
//	a5 is call[j+1]'s stack pointer.

	.org	XCHAL_WINDOW_OF4_VECOFS - WINDOW_BASE_VECOFS
_WindowOverflow4:
	hw_erratum_487_fix
	s32e	a0, a5, -16	// save a0 to call[j+1]'s stack frame
	s32e	a1, a5, -12	// save a1 to call[j+1]'s stack frame
	s32e	a2, a5,  -8	// save a2 to call[j+1]'s stack frame
	s32e	a3, a5,  -4	// save a3 to call[j+1]'s stack frame
	rfwo			// rotates back to call[i] position

	.size	_WindowOverflow4, . - _WindowOverflow4


// ALLOCA exception handler
//
// NOTE: The alloca exception handler is squeezed in between the window exception
// handlers in order to save space, and also to allow short-range jumps to the
// window underflow handlers (see below for why). Because of the limited space in
// between the window handlers, this function is split into two to fit.
//
// Code written to the windowed ABI must use the MOVSP instruction to modify
// the stack pointer (except for startup code, which doesn't have a caller).
// The compiler uses MOVSP to allocate very large or variable size stack frames.
// MOVSP guarantees that the caller frame's a0-a3 registers, stored below the
// stack pointer, are moved atomically with respect to interrupts and exceptions
// to satisfy windowed ABI requirements.  When user code executes the MOVSP
// instruction and the caller frame is on the stack rather than in the register
// file, the processor takes an ALLOCA exception.
//
// The XTOS user exception dispatcher allocates an exception frame on the
// stack and saves a2-a4 into that frame before calling us. So we need to
// restore those registers and deallocate the stack frame before jumping
// to the window underflow handler - which will restore the spilled registers
// back into the register file.
// The fact the alloca exception was taken means the registers associated with
// the base-save area have been spilled and will be restored by the underflow
// handler, so those 4 registers are available for scratch.

	.align	4

_xtos_alloca_handler:

	l32i	a2, a1, UEXC_a2 // restore a2-a4 and deallocate frame
	l32i	a3, a1, UEXC_a3
	l32i	a4, a1, UEXC_a4
	addi	a1, a1, ESF_TOTALSIZE
	wsr.excsave1	a0      // save a0
	rsr.windowbase	a0      // grab WINDOWBASE before rotw changes it
	rotw	-1              // WINDOWBASE goes to a4, new a0-a3 are scratch
	rsr.ps	a2
	extui	a3, a2, XCHAL_PS_OWB_SHIFT, XCHAL_PS_OWB_BITS
	xor	a3, a3, a4      // bits changed from old to current windowbase
	j	_xtos_alloca_2  // not enough room here...

	.size	_xtos_alloca_handler, . - _xtos_alloca_handler


// 4-Register Window Underflow Vector (Handler)
//
// Invoked by RETW returning from call[i+1] to call[i]
// where call[i]'s registers must be reloaded (not live in ARs);
// call[i] had done a call4 to call[i+1].
// On entry here:
//      window rotated to call[i] start point;
//      a0-a3 are undefined, must be reloaded with call[i].reg[0..3];
//      a4-a15 must be preserved (they are call[i+1].reg[0..11]);
//      a5 is call[i+1]'s stack pointer.

	.org	XCHAL_WINDOW_UF4_VECOFS - WINDOW_BASE_VECOFS 
_WindowUnderflow4:
	l32e	a0, a5, -16	// restore a0 from call[i+1]'s stack frame
	l32e	a1, a5, -12	// restore a1 from call[i+1]'s stack frame
	l32e	a2, a5,  -8	// restore a2 from call[i+1]'s stack frame
	l32e	a3, a5,  -4	// restore a3 from call[i+1]'s stack frame
	rfwu

	.size	_WindowUnderflow4, . - _WindowUnderflow4


// This is the second part of the alloca handler.

	.align	4

_xtos_alloca_2:

	rsr.excsave1	a4      // restore original a0 (now in a4)
	slli	a3, a3, XCHAL_PS_OWB_SHIFT
	xor	a2, a2, a3      // flip changed bits in old window base
	wsr.ps	a2              // update PS.OWB to new window base
	rsync
	_bbci.l	a4, 31,	_WindowUnderflow4
	rotw	-1              // original a0 goes to a8
	_bbci.l	a8, 30,	_WindowUnderflow8
	rotw	-1
	j		_WindowUnderflow12

	.size	_xtos_alloca_2, . - _xtos_alloca_2


// 8-Register Window Overflow Vector (Handler)
//
// Invoked if a call[i] referenced a register (a4-a15)
// that contains data from ancestor call[j];
// call[j] had done a call8 to call[j+1].
// On entry here:
//	window rotated to call[j] start point;
//	a0-a7 are registers to be saved;
//	a8-a15 must be preserved;
//	a9 is call[j+1]'s stack pointer.

	.org	XCHAL_WINDOW_OF8_VECOFS - WINDOW_BASE_VECOFS
_WindowOverflow8:
	hw_erratum_487_fix
	s32e	a0, a9, -16	// save a0 to call[j+1]'s stack frame
	l32e	a0, a1, -12	// a0 <- call[j-1]'s sp (used to find end of call[j]'s frame)
	s32e	a1, a9, -12	// save a1 to call[j+1]'s stack frame
	s32e	a2, a9,  -8	// save a2 to call[j+1]'s stack frame
	s32e	a3, a9,  -4	// save a3 to call[j+1]'s stack frame
	s32e	a4, a0, -32	// save a4 to call[j]'s stack frame
	s32e	a5, a0, -28	// save a5 to call[j]'s stack frame
	s32e	a6, a0, -24	// save a6 to call[j]'s stack frame
	s32e	a7, a0, -20	// save a7 to call[j]'s stack frame
	rfwo			// rotates back to call[i] position

	.size	_WindowOverflow8, . - _WindowOverflow8


// 8-Register Window Underflow Vector (Handler)
//
// Invoked by RETW returning from call[i+1] to call[i]
// where call[i]'s registers must be reloaded (not live in ARs);
// call[i] had done a call8 to call[i+1].
// On entry here:
//	window rotated to call[i] start point;
//	a0-a7 are undefined, must be reloaded with call[i].reg[0..7];
//	a8-a15 must be preserved (they are call[i+1].reg[0..7]);
//	a9 is call[i+1]'s stack pointer.

	.org	XCHAL_WINDOW_UF8_VECOFS - WINDOW_BASE_VECOFS
_WindowUnderflow8:
	l32e	a0, a9, -16	// restore a0 from call[i+1]'s stack frame
	l32e	a1, a9, -12	// restore a1 from call[i+1]'s stack frame
	l32e	a2, a9,  -8	// restore a2 from call[i+1]'s stack frame
	l32e	a7, a1, -12	// a7 <- call[i-1]'s sp (used to find end of call[i]'s frame)
	l32e	a3, a9,  -4	// restore a3 from call[i+1]'s stack frame
	l32e	a4, a7, -32	// restore a4 from call[i]'s stack frame
	l32e	a5, a7, -28	// restore a5 from call[i]'s stack frame
	l32e	a6, a7, -24	// restore a6 from call[i]'s stack frame
	l32e	a7, a7, -20	// restore a7 from call[i]'s stack frame
	rfwu

	.size	_WindowUnderflow8, . - _WindowUnderflow8


// 12-Register Window Overflow Vector (Handler)
//
// Invoked if a call[i] referenced a register (a4-a15)
// that contains data from ancestor call[j];
// call[j] had done a call12 to call[j+1].
// On entry here:
//	window rotated to call[j] start point;
//	a0-a11 are registers to be saved;
//	a12-a15 must be preserved;
//	a13 is call[j+1]'s stack pointer.

	.org	XCHAL_WINDOW_OF12_VECOFS - WINDOW_BASE_VECOFS
_WindowOverflow12:
	hw_erratum_487_fix
	s32e	a0,  a13, -16	// save a0 to call[j+1]'s stack frame
	l32e	a0,  a1,  -12	// a0 <- call[j-1]'s sp (used to find end of call[j]'s frame)
	s32e	a1,  a13, -12	// save a1 to call[j+1]'s stack frame
	s32e	a2,  a13,  -8	// save a2 to call[j+1]'s stack frame
	s32e	a3,  a13,  -4	// save a3 to call[j+1]'s stack frame
	s32e	a4,  a0,  -48	// save a4 to end of call[j]'s stack frame
	s32e	a5,  a0,  -44	// save a5 to end of call[j]'s stack frame
	s32e	a6,  a0,  -40	// save a6 to end of call[j]'s stack frame
	s32e	a7,  a0,  -36	// save a7 to end of call[j]'s stack frame
	s32e	a8,  a0,  -32	// save a8 to end of call[j]'s stack frame
	s32e	a9,  a0,  -28	// save a9 to end of call[j]'s stack frame
	s32e	a10, a0,  -24	// save a10 to end of call[j]'s stack frame
	s32e	a11, a0,  -20	// save a11 to end of call[j]'s stack frame
	rfwo			// rotates back to call[i] position

	.size	_WindowOverflow12, . - _WindowOverflow12


// 12-Register Window Underflow Vector (Handler)
//
// Invoked by RETW returning from call[i+1] to call[i]
// where call[i]'s registers must be reloaded (not live in ARs);
// call[i] had done a call12 to call[i+1].
// On entry here:
//	window rotated to call[i] start point;
//	a0-a11 are undefined, must be reloaded with call[i].reg[0..11];
//	a12-a15 must be preserved (they are call[i+1].reg[0..3]);
//	a13 is call[i+1]'s stack pointer.

	.org	XCHAL_WINDOW_UF12_VECOFS - WINDOW_BASE_VECOFS
_WindowUnderflow12:
	l32e	a0,  a13, -16	// restore a0 from call[i+1]'s stack frame
	l32e	a1,  a13, -12	// restore a1 from call[i+1]'s stack frame
	l32e	a2,  a13,  -8	// restore a2 from call[i+1]'s stack frame
	l32e	a11, a1,  -12	// a11 <- call[i-1]'s sp (used to find end of call[i]'s frame)
	l32e	a3,  a13,  -4	// restore a3 from call[i+1]'s stack frame
	l32e	a4,  a11, -48	// restore a4 from end of call[i]'s stack frame
	l32e	a5,  a11, -44	// restore a5 from end of call[i]'s stack frame
	l32e	a6,  a11, -40	// restore a6 from end of call[i]'s stack frame
	l32e	a7,  a11, -36	// restore a7 from end of call[i]'s stack frame
	l32e	a8,  a11, -32	// restore a8 from end of call[i]'s stack frame
	l32e	a9,  a11, -28	// restore a9 from end of call[i]'s stack frame
	l32e	a10, a11, -24	// restore a10 from end of call[i]'s stack frame
	l32e	a11, a11, -20	// restore a11 from end of call[i]'s stack frame
	rfwu

	.size	_WindowUnderflow12, . - _WindowUnderflow12


# ifndef NO_SECTION_DIRECTIVES
	//.end	literal_prefix
	.text
# endif


#endif /* XCHAL_HAVE_XEA2 && XCHAL_HAVE_WINDOWED && !defined(__XTENSA_CALL0_ABI__) */

