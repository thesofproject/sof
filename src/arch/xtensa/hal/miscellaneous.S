// 
// miscellaneous.S - miscellaneous assembly language routines
//
// $Id: //depot/rel/Eaglenest/Xtensa/OS/hal/miscellaneous.S#1 $

// Copyright (c) 2011 Tensilica Inc.
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

#include <xtensa/cacheasm.h>
#include <xtensa/cacheattrasm.h>
#include <xtensa/xtensa-versions.h>

#define GBL(x)   .global x
#define TYP(x)   .type x,@function
#define ALN(x)   .align x
#define SIZ(x)   .size x, . - x
#define MKEND(x) .purgem endfunc ; .macro endfunc ; SIZ(x) ; .purgem endfunc ; .macro endfunc ; .endm ; .endm
#define SYMT(x)  GBL(x); MKEND(x); TYP(x); ALN(4); x:
#define SYM(x)   GBL(x); MKEND(x); ALN(4); x:
	.macro endfunc ; .endm

/* For Call0 ABI, the xthal... and xthal..._nw versions are identical,
 * so we define both labels for the same function body.  The Makefile
 * does not define any of the __SPLIT__..._nw macros if Call0 ABI.
 * Use SYM() when we don't want .type information. */

#if defined (__XTENSA_CALL0_ABI__)
# define SYMBOL(x)  SYMT(x); SYMT(x ## _nw)
#else
# define SYMBOL(x)  SYMT(x)
#endif



	.text

//----------------------------------------------------------------------
// Clear any remnant register state pointing to (or containing) code.
// Specifically, clear loop registers (LCOUNT) to avoid hardware loopback
// from LEND to LBEGIN when new code is loaded where code containing a
// zero-overhead loop was located.  See the HAL chapter of the Tensilica
// System Software Reference Manual for details on the use of this function.
//----------------------------------------------------------------------

#if defined(__SPLIT__clear_regcached_code)

SYMBOL(xthal_clear_regcached_code)
	abi_entry
# if XCHAL_HAVE_LOOPS
	movi		a2, 0
	wsr.lcount	a2
# endif
	isync_retw_nop
	abi_return
	endfunc

#endif
#if defined(__SPLIT__clear_regcached_code_nw)

SYM(xthal_clear_regcached_code_nw)
# if XCHAL_HAVE_LOOPS
	movi		a2, 0
	wsr.lcount	a2
# endif
	ret
	endfunc

#endif

