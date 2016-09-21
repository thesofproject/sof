// 
// mem_ecc_parity.S - utility routines for the local memory ECC/parity option
//			(memory error checking and exceptions)
//
// $Id: //depot/rel/Eaglenest/Xtensa/OS/hal/mem_ecc_parity.S#1 $

// Copyright (c) 2006-2010 Tensilica Inc.
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
 *  For most functions, the link-time HAL defines two entry points:
 *  xthal_...() and xthal_..._nw().  The former is the main entry point
 *  invoked from C code, or assembly code that follows the C ABI.
 *  The latter is for use in assembly code that cannot easily follow
 *  all the requirements of the windowed ABI, e.g. in exception handlers;
 *  these use the call0 ABI instead (in most cases; some use their own conventions).
 *
 *  When software tools are configured to use the call0 ABI, both variants
 *  are identical (with some exceptions as noted).  To avoid duplicating
 *  code, we define both labels for the same function body.  The Makefile
 *  defines __SPLIT__..._nw macros with windowed ABI but not with Call0 ABI.
 *  Use SYM_NW() for the _nw variants defined with the __SPLIT_..._nw macros,
 *  i.e. for call0 ABI variants when windowed ABI is in use; these are not
 *  C callable so SYM_NW() does not specify .type information.
 *  Use SYMBOL() otherwise, which defines both symbols if call0 ABI is selected.
 */

#if defined (__XTENSA_CALL0_ABI__)
# define SYMBOL(x)	.global x ; .type x,@function ; \
			.global x ## _nw ; .type x ## _nw,@function ; \
			.align 4 ; x: ; x ## _nw:
#else
# define SYMBOL(x)	.global x ; .type x,@function ; .align 4 ; x:
#endif
#define SYM_NW(x)	.global x ; .align 4 ; x:


/*  Compute smaller of I and D cache line sizes:  */
#if XCHAL_ICACHE_LINEWIDTH < XCHAL_DCACHE_LINEWIDTH && XCHAL_ICACHE_SIZE > 0
# define CACHE_LINEWIDTH_MIN	XCHAL_ICACHE_LINEWIDTH
# define CACHE_LINESIZE_MIN	XCHAL_ICACHE_LINESIZE
#else
# define CACHE_LINEWIDTH_MIN	XCHAL_DCACHE_LINEWIDTH
# define CACHE_LINESIZE_MIN	XCHAL_DCACHE_LINESIZE
#endif


	.text

//------------------------------------------------------------------------
//  Inject errors into instruction and/or data RAMs, or cache data or tags
//------------------------------------------------------------------------

#if defined(__SPLIT__memep_inject_error)

// void xthal_memep_inject_error(void *addr, int size, int flags);
// where:
//	addr  (a2)	pointer to local memory, or cache address
//	size  (a3)	size in bytes (gets aligned to words or lines)
//	flags (a4)	is a combination of the following bits:
//		bit 31-5:	(reserved)
//		bit 4:		0 = inject non-correctable error,
//				16 = inject correctable error (if ECC)
//		bit 3:		(reserved)
//		bit 2:		0 = local memory, 4 = cache
//		bit 1:		0 = data cache, 2 = instruction cache
//		bit 0:		0 = cache data, 1 = cache tag
//
// (note: data cache data is handled same as local memories;
//  to access specific dcache data entries, you have to setup
//  a region or page in cache-isolate mode yourself)

SYMBOL(xthal_memep_inject_error)
	abi_entry

#if XCHAL_HAVE_MEM_ECC_PARITY

	//  These MOVIs may be L32Rs, load them before enabling test mode:
	movi	a6, 0x02020202	// XOR'ing this creates a correctable error
	bbsi.l	a4, 4, 1f	// branch if correctable error requested
	movi	a6, 0x03030303	// XOR'ing this creates a non-correctable error
1:

	//  Lock out all interrupts, to avoid interrupt handlers running with
	//  test mode enabled (corrupting their stores, likely leading to
	//  non-correctable memory errors).
	//
	//  If NMI is possible, you're toast
	//  (no stores during NMI handler will have properly computed ECC/parity bits)
	//  although you might make the NMI handler check MESR.ERRTEST and save/clear
	//  it if it's set on entry, so that its stores work correctly.
	//
	//  If memory exceptions are possible, might be okay as long as the
	//  handler checks whether test mode is on, and turns it off temporarily
	//  to do its work.
	//
# if XCHAL_HAVE_INTERRUPTS
	rsil	a11, 15
# endif

	//  Save current MESR and set test mode:

	rsr	a8, MESR
	bbsi.l	a8, MESR_ERRTEST_SHIFT, .Lproceed // already in test mode?
	addmi	a9, a8, MESR_ERRTEST		// enable test mode
	bbci.l	a8, MESR_ERRENAB_SHIFT, 1f
	addmi	a9, a9, - MESR_ERRENAB		// disable error checks
1:	xsr	a9, MESR
	beq	a8, a9, .Lproceed		// clean update, continue
	bbci.l	a9, MESR_RCE_SHIFT, .Lproceed	// we likely restored a lost RCE, just keep it
	//  At this point, either we:
	//	a) cleared an RCE record that got created between RSR and XSR
	//	b) cleared LCE bits that got set between RSR and XSR
	//	c) more eclectic, and presumably much less likely, cases of
	//	   RCE/LCE bits being cleared and set again between RSR and XSR
	//	   due to multiple memory errors and memory error exceptions
	//	   in that period; for now, we ignore this possibility
	//	   (decreasing returns on addressing these arbitrarily complex cases)
	//  Assuming (a) or (b), restore the bits we took away.
	//addmi	a8, a8, MESR_ERRTEST
	addmi	a9, a9, MESR_ERRTEST
	bbci.l	a9, MESR_ERRENAB_SHIFT, 1f
	addmi	a9, a9, - MESR_ERRENAB		// disable error checks
1:	wsr	a9, MESR
	//xsr	a9, MESR
	//beq	a8, a9, .Lproceed		// updated fine, continue
	//
	//  Above we could have used XSR instead of WSR.
	//  However, it's not clear at this point what's the cleanest thing
	//  to do if what we read back doesn't match what we expected,
	//  because at that point we have multiple errors to deal with.
	//  Unless we have code here to handle (fix and/or log) these errors,
	//  we have to chuck something away or write a bunch more code to
	//  handle another LCE bit getting set etc (also starting to be
	//  a low probability occurrence).
.Lproceed:
	//  Test mode enabled.  From this point until we restore MESR,
	//  the only loads and stores done are for injecting errors.

# if XCHAL_ICACHE_SIZE || XCHAL_DCACHE_SIZE
	bbci.l	a4, 2, .L_inject_local	// branch if injecting to local memory
	bbsi.l	a4, 1, .L_inject_icache	// branch if injecting to icache
	//  Inject errors in dcache:
	bbci.l	a4, 0, .L_inject_local	// branch if injecting to dcache data
#  if XCHAL_DCACHE_SIZE && XCHAL_HAVE_DCACHE_TEST
	//  Inject errors in dcache tags:

	//  Round addr/size to fully rather than partially cover
	//  all aligned cache lines:
	extui	a9, a2, 0, XCHAL_DCACHE_LINEWIDTH
	sub	a2, a2, a9
	add	a3, a3, a9
	addi	a3, a3, XCHAL_DCACHE_LINESIZE-1
	srli	a3, a3, XCHAL_DCACHE_LINEWIDTH	// size in cache lines

	floopgtz	a3, .Ldctagloop
	ldct	a9, a2		// load dcache line tag
	rsr	a7, MECR	// get check bits
	xor	a7, a7, a6	// ECC: single-bit error; Parity: NO-OP
	wsr	a7, MECR	// setup modified check bits
	sdct	a9, a2		// store tag with modified check bits
	addi	a2, a2, XCHAL_DCACHE_LINESIZE	// increment to next line
	floopend	a3, .Ldctagloop
#  endif /* have dcache */
	j	.L_inject_done

	//  Inject errors in icache:
.L_inject_icache:
#  if XCHAL_ICACHE_SIZE && XCHAL_HAVE_ICACHE_TEST
	bbci.l	a4, 0, .L_inject_icw	// branch if injecting to icache data

	//  Inject errors in icache tags:
	//  Round addr/size to fully rather than partially cover
	//  all aligned cache lines:
	extui	a9, a2, 0, XCHAL_ICACHE_LINEWIDTH
	sub	a2, a2, a9
	add	a3, a3, a9
	addi	a3, a3, XCHAL_ICACHE_LINESIZE-1
	srli	a3, a3, XCHAL_ICACHE_LINEWIDTH	// size in cache lines

	floopgtz	a3, .Lictagloop
	lict	a9, a2		// load icache line tag
	rsr	a7, MECR	// get check bits
	xor	a7, a7, a6	// ECC: single-bit error; Parity: NO-OP
	wsr	a7, MECR	// setup modified check bits
	sict	a9, a2		// store tag with modified check bits
	addi	a2, a2, XCHAL_ICACHE_LINESIZE	// increment to next line
	floopend	a3, .Lictagloop
	j	.L_inject_done

.L_inject_icw:
#   if XCHAL_ICACHE_ACCESS_SIZE <= 4	/* SICW does not work usefully (replicates data) if accessWidth > 32 bits */
	//  Inject errors in icache data words:
	//  Round addr/size to fully rather than partially cover
	//  all aligned 32-bit words:
	extui	a9, a2, 0, 2
	sub	a2, a2, a9
	add	a3, a3, a9
	addi	a3, a3, 3
	srli	a3, a3, 2	// size in words

	floopgtz	a3, .Licwloop
	licw	a9, a2		// load word of icache line data
	rsr	a7, MECR	// get check bits
	xor	a7, a7, a6	// ECC: single-bit error; Parity: NO-OP
	wsr	a7, MECR	// setup modified check bits
	sicw	a9, a2		// store data with modified check bits
	addi	a2, a2, 4	// increment to next word
	floopend	a3, .Licwloop
#   endif
#  endif /* have icache */
	j	.L_inject_done
# endif /* have icache or dcache */

.L_inject_local:
	//  Round addr/size to fully rather than partially cover
	//  all aligned 32-bit words:
	extui	a9, a2, 0, 2
	sub	a2, a2, a9
	add	a3, a3, a9
	addi	a3, a3, 3
	srli	a3, a3, 2	// size in words

	floopgtz	a3, .Lendloop
	l32i	a9, a2, 0	// load data
	rsr	a7, MECR	// get check bits
	xor	a7, a7, a6	// ECC: single-bit error; Parity: NO-OP
	wsr	a7, MECR	// setup modified check bits
	s32i	a9, a2, 0	// store data with modified check bits
	addi	a2, a2, 4	// increment to next word
	floopend	a3, .Lendloop

.L_inject_done:
	//  Restore MESR (a8 is the saved original MESR):
	bbsi.l	a8, MESR_ERRTEST_SHIFT, 2f	// was already in test mode
	rsr	a6, MESR
	addmi	a9, a6, - MESR_ERRTEST		// disable test mode
	bbci.l	a8, MESR_ERRENAB_SHIFT, 1f
	addmi	a9, a9, MESR_ERRENAB		// enable error checks
1:	xsr	a9, MESR
	beq	a6, a9, 2f			// clean update, done
	bbci.l	a9, MESR_RCE_SHIFT, 2f		// we likely restored a lost RCE, just keep it
	addmi	a9, a9, - MESR_ERRTEST
	bbci.l	a8, MESR_ERRENAB_SHIFT, 1f
	addmi	a9, a9, MESR_ERRENAB		// disable error checks
1:	wsr	a9, MESR
2:

	//  Restore PS.INTLEVEL:
# if XCHAL_HAVE_INTERRUPTS
	wsr	a11, PS
	rsync
# endif
#endif /* XCHAL_HAVE_MEM_ECC_PARITY */

	abi_return

	.size	xthal_memep_inject_error, . - xthal_memep_inject_error


#endif /*split*/

//----------------------------------------------------------------------

