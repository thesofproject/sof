// 
// int_asm.S - assembly language interrupt utility routines
//
// $Id: //depot/rel/Eaglenest/Xtensa/OS/hal/int_asm.S#1 $

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


#if XCHAL_HAVE_INTERRUPTS
/*  Offsets of XtHalVPriState structure members (Xthal_vpri_state variable):  */
#define XTHAL_VPRI_VPRI_OFS		0x00
#define XTHAL_VPRI_LOCKLEVEL_OFS	0x01
#define XTHAL_VPRI_LOCKVPRI_OFS		0x02
#define XTHAL_VPRI_PAD0_OFS		0x03
#define XTHAL_VPRI_ENABLED_OFS		0x04
#define XTHAL_VPRI_LOCKMASK_OFS		0x08
#define XTHAL_VPRI_PAD1_OFS		0x0C
#define XTHAL_VPRI_ENABLEMAP_OFS	0x10
#define XTHAL_VPRI_RESOLVEMAP_OFS	(0x10+0x40*(XCHAL_NUM_INTLEVELS+1))
#define XTHAL_VPRI_END_OFS		(0x10+0x40*(XCHAL_NUM_INTLEVELS*2+1))
#endif /* XCHAL_HAVE_INTERRUPTS */


#if defined(__SPLIT__get_intenable)

//----------------------------------------------------------------------
// Access INTENABLE register from C
//----------------------------------------------------------------------

// unsigned xthal_get_intenable(void)
//
SYMT(xthal_get_intenable)
	abi_entry
# if XCHAL_HAVE_INTERRUPTS
	rsr	a2, INTENABLE
# else
	movi	a2, 0	// if no INTENABLE (no interrupts), tell caller nothing is enabled
# endif
	abi_return
	endfunc


#elif defined(__SPLIT__set_intenable)

// void xthal_set_intenable(unsigned)
//
SYMT(xthal_set_intenable)
	abi_entry
# if XCHAL_HAVE_INTERRUPTS
	wsr	a2, INTENABLE
# endif
	abi_return
	endfunc


//----------------------------------------------------------------------
// Access INTERRUPT, INTSET, INTCLEAR register from C
//----------------------------------------------------------------------

#elif defined(__SPLIT__get_interrupt)

// unsigned xthal_get_interrupt(void)
//
SYMT(xthal_get_interrupt)
SYMT(xthal_get_intread)
	abi_entry
# if XCHAL_HAVE_INTERRUPTS
	rsr	a2, INTERRUPT
# else
	movi	a2, 0	// if no INTERRUPT (no interrupts), tell caller nothing is pending
# endif
	abi_return
	endfunc

#elif defined(__SPLIT__set_intset)

// void xthal_set_intset(unsigned)
//
SYMT(xthal_set_intset)
	abi_entry
# if XCHAL_HAVE_INTERRUPTS
	wsr	a2, INTSET
# endif
	abi_return
	endfunc

#elif defined(__SPLIT__set_intclear)

// void xthal_set_intclear(unsigned)
//
SYMT(xthal_set_intclear)
	abi_entry
# if XCHAL_HAVE_INTERRUPTS
	wsr	a2, INTCLEAR
# endif
	abi_return
	endfunc



//----------------------------------------------------------------------
// Virtual PS.INTLEVEL support:
// allows running C code at virtual PS.INTLEVEL > 0
// using INTENABLE to simulate the masking that PS.INTLEVEL would do.
//----------------------------------------------------------------------


#elif defined(__SPLIT__get_vpri)

// unsigned xthal_get_vpri(void);

SYMBOL(xthal_get_vpri)
	abi_entry
# if XCHAL_HAVE_INTERRUPTS
	movi	a2, Xthal_vpri_state
	l8ui	a2, a2, XTHAL_VPRI_VPRI_OFS
# else
	movi	a2, 0	// no interrupts, report we're always at level 0
# endif
	abi_return
	endfunc


#elif defined(__SPLIT__get_vpri_nw)

// unsigned xthal_get_vpri_nw(void);

SYM(xthal_get_vpri_nw)

# if XCHAL_HAVE_INTERRUPTS
	movi	a2, Xthal_vpri_state
	l8ui	a2, a2, XTHAL_VPRI_VPRI_OFS
# else
	movi	a2, 0	// no interrupts, report we're always at level 0
# endif
	ret
	endfunc


#elif defined(__SPLIT__set_vpri_nw)

// unsigned xthal_set_vpri_nw(unsigned)
//
//  Must be called at PS.INTLEVEL <= 1.
//  Doesn't touch the stack (doesn't reference a1 at all).
//  Normally, PS should be restored with a6 after return from this call
//  (it isn't restored automatically because some exception handlers
//   want to keep ints locked for a while).
//
//  On entry:
//	a2 = new virtual interrupt priority (0x00 .. 0x1F)
//	a3-a6 = undefined
//	PS.INTLEVEL <= 1
//  On exit:
//	a2 = previous virtual interrupt priority (0x0F .. 0x1F, or 0 if no interrupts)
//	a3-a5 = clobbered
//	a6 = PS as it was on entry
//	PS.INTLEVEL = 1
//	!!!!!!!!! PS.WOE = 0 (but not if there are no interrupts; is this really needed???)
//	INTENABLE = updated according to new vpri

SYM(xthal_set_vpri_nw)

# if XCHAL_HAVE_INTERRUPTS
	/*  Make sure a2 is in the range 0x0F .. 0x1F:  */
	movi	a3, 0x1F	// highest legal virtual interrupt priority
	sub	a4, a2, a3	// (a4 = newlevel - maxlevel)
	movgez	a2, a3, a4	// newlevel = maxlevel if (newlevel - maxlevel) >= 0
	movi	a3, 15		// lowest legal virtual interrupt priority
	sub	a4, a2, a3	// (a4 = newlevel - 15)
	movltz	a2, a3, a4	// newlevel = 15 if newlevel < 15

xthal_set_vpri_nw_common:
	movi	a4, Xthal_vpri_state	// address of vpri state structure

	/*
	 *  Lockout interrupts for exclusive access to virtual priority structure
	 *  while we examine and modify it.
	 *  Note that we accessed a4 and don't access any further than a6,
	 *  so we won't cause any spills, so we could leave WOE enabled (if it is),
	 *  but we clear it because that might be what the caller wants,
	 *  and is cleaner.
	 */
	//  Get PS and mask off INTLEVEL:
	rsil	a6, 1		// save a6 = PS, set PS.INTLEVEL = 1

	//  Clear PS.WOE.  (Can we get rid of this?!!!!!):
	movi	a3, ~0x00040000	// mask to...
	rsr	a5, PS		// get and save a6 = PS
//a2,a3,a4,a5,a6
	and	a5, a5, a3	// ... clear a5.WOE
	wsr	a5, PS		// clear PS.WOE
	rsync

//a2,a4,a6
	/*  Get mask of interrupts to be turned off at requested level:  */
	l32i	a5, a4, XTHAL_VPRI_ENABLED_OFS		// get the global mask
	addx4	a3, a2, a4	// a3 = a4 + a2*4  (index into enablemap[] array)
//a2,a3,a4,a5,a6
	l32i	a3, a3, XTHAL_VPRI_ENABLEMAP_OFS	// get the per-level mask
	and	a3, a5, a3	// new INTENABLE value according to new intlevel
	wsr	a3, INTENABLE	// set it!
//a2,a4,a6

	l8ui	a5, a4, XTHAL_VPRI_VPRI_OFS	// previous virtual priority
	s8i	a2, a4, XTHAL_VPRI_VPRI_OFS	// new virtual priority

	//  Let the caller restore PS:
	//wsr	a6, PS		// restore PS.INTLEVEL
	//rsync

	mov	a2, a5		// return previous virtual intlevel

# else /* ! XCHAL_HAVE_INTERRUPTS */
xthal_set_vpri_nw_common:
#  if XCHAL_HAVE_EXCEPTIONS
	rsr	a6, PS	// return PS for caller to restore
#  else
	movi	a6, 0
#  endif
	movi	a2, 0	// no interrupts, report we're always at virtual priority 0
# endif /* XCHAL_HAVE_INTERRUPTS */
	ret
	endfunc



// unsigned xthal_set_vpri_intlevel_nw(unsigned);
//
//  Same as xthal_set_vpri_nw() except that it accepts
//  an interrupt level rather than a virtual interrupt priority.
//  This just converts intlevel to vpri and jumps to xthal_set_vpri_nw.

SYM(xthal_set_vpri_intlevel_nw)
# if XCHAL_HAVE_INTERRUPTS
	movi	a3, 0x10
	movnez	a2, a3, a2	// a2 = (a2 ? 0x10 : 0)
	addi	a2, a2, 0x0F	// a2 += 0x0F
# endif
	j	xthal_set_vpri_nw_common	// set vpri to a2
	endfunc




#elif defined(__SPLIT__set_vpri)

// unsigned  xthal_set_vpri (unsigned newvpri);
//
//  Normal windowed call (PS.INTLEVEL=0 and PS.WOE=1 on entry and exit).
//  (PS.UM = 0 or 1)
//
//  Returns previous virtual interrupt priority
//  (0x0F .. 0x1F, or 0 if no interrupts).
//
//  On entry:
//	a2 = new virtual interrupt priority (0x00 .. 0x1F)
//  On exit:
//	a2 = previous vpri
//	INTENABLE = updated according to new vpri

SYMBOL(xthal_set_vpri)
	abi_entry
# if XCHAL_HAVE_INTERRUPTS
	/*  Make sure a2 is in the range 0x0F .. 0x1F:  */
	movi	a3, 0x1F	// highest legal virtual interrupt priority
	sub	a4, a2, a3	// (a4 = newlevel - maxlevel)
	movgez	a2, a3, a4	// newlevel = maxlevel if (newlevel - maxlevel) >= 0
	movi	a3, 15		// lowest legal virtual interrupt priority
	sub	a4, a2, a3	// (a4 = newlevel - 15)
	movltz	a2, a3, a4	// newlevel = 15 if newlevel < 15

xthal_set_vpri_common1:
	movi	a4, Xthal_vpri_state	// address of vpri state structure

	/*
	 *  Lockout interrupts for exclusive access to virtual priority structure
	 *  while we examine and modify it.
	 *  Note that we accessed a4 and don't access any further than a6,
	 *  so we won't cause any spills, so we can leave WOE enabled.
	 */
	//  Get PS and mask off INTLEVEL:
	rsil	a6, 1		// save a6 = PS, set PS.INTLEVEL = 1

	l8ui	a7, a4, XTHAL_VPRI_VPRI_OFS	// previous virtual priority (vpri)

	/*  Get mask of interrupts to be turned off at requested level:  */
	l32i	a5, a4, XTHAL_VPRI_ENABLED_OFS		// get the global mask
	addx4	a3, a2, a4	// a3 = a4 + a2*4  (index into enablemap[] array)
	l32i	a3, a3, XTHAL_VPRI_ENABLEMAP_OFS	// get the per-level mask
	s8i	a2, a4, XTHAL_VPRI_VPRI_OFS	// new virtual priority (in load-slot)
	and	a3, a5, a3	// new INTENABLE value according to new intlevel
	wsr	a3, INTENABLE	// set it!

	wsr	a6, PS		// restore PS.INTLEVEL
	rsync

	mov	a2, a7		// return previous vpri

# else /* ! XCHAL_HAVE_INTERRUPTS */
	movi	a2, 0	// no interrupts, report we're always at virtual priority 0
# endif /* XCHAL_HAVE_INTERRUPTS */
	abi_return
	endfunc



// unsigned  xthal_set_vpri_intlevel (unsigned intlevel);
//
//  Equivalent to xthal_set_vpri(XTHAL_VPRI(intlevel,0xF)).
//  This just converts intlevel to vpri and jumps inside xthal_set_vpri.

SYMBOL(xthal_set_vpri_intlevel)
	abi_entry
# if XCHAL_HAVE_INTERRUPTS
	movi	a3, 0x10
	movnez	a2, a3, a2	// a2 = (a2 ? 0x10 : 0)
	addi	a2, a2, 0x0F	// a2 += 0x0F
	j	xthal_set_vpri_common1	// set vpri to a2
# else
	movi	a2, 0	// no interrupts, report we're always at virtual priority 0
	abi_return
# endif
	endfunc



// unsigned  xthal_set_vpri_lock (void);
//
//  Equivalent to xthal_set_vpri(0x1F);
//  Returns previous virtual interrupt priority.
//
SYMT(xthal_set_vpri_lock)
	abi_entry
# if XCHAL_HAVE_INTERRUPTS
	movi	a2, 0x1F		// lock at intlevel 1
	j	xthal_set_vpri_common1
# else
	movi	a2, 0	// no interrupts, report we're always at virtual priority 0
	abi_return
# endif
	endfunc



#elif defined(__SPLIT__get_intpending_nw)

// unsigned xthal_get_intpending_nw(void)
//
//  Of the pending level-1 interrupts, returns
//  the bitmask of interrupts at the highest software priority,
//  and the index of the first of these.
//  It also disables interrupts of that software priority and lower
//  via INTENABLE.
//
//	On entry:
//		a0 = return PC
//		a1 = sp
//		a2-a6 = (available) (undefined)
//		PS.INTLEVEL = 1
//		PS.WOE = 0
//	On exit:
//		a0 = return PC
//		a1 = sp (NOTE: stack is untouched, a1 is never referenced)
//		a2 = index of first highest-soft-pri pending l1 interrupt (0..31), or -1 if none
//		a3 = bitmask of highest-soft-pri pending l1 interrupts (0 if none) (may be deprecated)
//		a4 = (clobbered)
//		a5 = new vpri (not typically used by caller? so might get deprecated...?)
//		a6 = old vpri (eg. to be saved as part of interrupt context's state)
//		INTENABLE = updated according to new vpri
//		INTERRUPT bit cleared for interrupt returned in a2 (if any), if software or edge-triggered or write-error
//		all others = preserved

SYM(xthal_get_intpending_nw)
# if XCHAL_HAVE_INTERRUPTS
	// Give us one more register to play with
	//wsr	a4, EXCSAVE_1

	// Figure out which interrupt to process

	/*
	Perform a binary search to find a mask of the interrupts that are
	ready at the highest virtual priority level.
	Xthal_vpri_resolvemap is a binary tree implemented within an array,
	sorted by priority: each node contains the set of interrupts in
	the range of priorities corresponding to the right half of its branch.
	The mask of enabled & pending interrupts is compared with each node to
	determine in which subbranch (left or right) the highest priority one is
	present.  After 4 such masks and comparisons (for 16 priorities), we have
	determined the priority of the highest priority enabled&pending interrupt.

	Table entries for intlevel 'i' are bitmasks defined as follows (map=Xthal_vpri_resolvemap[i-1]):
	    map[8+(x=0)]          = ints at pri x + 8..15 (8-15)
	    map[4+(x=0,8)]        = ints at pri x + 4..7  (4-7,12-15)
	    map[2+(x=0,4,8,12)]   = ints at pri x + 2..3  (2-3,6-7,10-11,14-15)
	    map[1+(x=0,2..12,14)] = ints at pri x + 1     (1,3,5,7,9,11,13,15)
	    map[0]                = 0  (unused; for alignment)
	*/

	rsr	a4, INTERRUPT	// a4 = mask of interrupts pending, including those disabled
	rsr	a2, INTENABLE	// a2 = mask of interrupts enabled
	movi	a3, Xthal_vpri_state
	and	a4, a2, a4	// a4 = mask of enabled interrupts pending
	beqz	a4, gipfail	// if none (can happen for spurious level-triggered interrupts,
				//  or ???), we're done

	mov	a5, a3
	l32i	a2, a5, XTHAL_VPRI_RESOLVEMAP_OFS+8*4
	bnone	a2, a4, 1f
	addi	a5, a5, 8*4
1:	l32i	a2, a5, XTHAL_VPRI_RESOLVEMAP_OFS+4*4
	bnone	a2, a4, 1f
	addi	a5, a5, 4*4
1:	l32i	a2, a5, XTHAL_VPRI_RESOLVEMAP_OFS+2*4
	bnone	a2, a4, 1f
	addi	a5, a5, 2*4
1:	l32i	a2, a5, XTHAL_VPRI_RESOLVEMAP_OFS+1*4
	bnone	a2, a4, 1f
	addi	a5, a5, 1*4
1:

#  if 0
	a5 = address of map ...
	l32i	a2, a5, XTHAL_VPRI_RESOLVEMAP_OFS+8*4
	addi	a?, a5, 8*4
	and	a2, a2, a4
	movnez	a5, a?, a2
	l32i	a2, a5, XTHAL_VPRI_RESOLVEMAP_OFS+4*4
	addi	a?, a5, 4*4
	and	a2, a2, a4
	movnez	a5, a?, a2
	l32i	a2, a5, XTHAL_VPRI_RESOLVEMAP_OFS+2*4
	addi	a?, a5, 2*4
	and	a2, a2, a4
	movnez	a5, a?, a2
	l32i	a2, a5, XTHAL_VPRI_RESOLVEMAP_OFS+1*4
	addi	a?, a5, 1*4
	and	a2, a2, a4
	movnez	a5, a?, a2
#  endif

	//  Here:
	//	a3 = Xthal_vpri_state
	//	a5 = Xthal_vpri_state + softpri*4
	//	a4 = mask of enabled interrupts pending
	//	a2,a6 = available

	//  Lock interrupts during virtual priority data structure transaction:
	//rsil	a6, 1			// set PS.INTLEVEL = 1 (a6 ignored)
	//	a2,a6 = available

	//  The highest priority interrupt(s) in a4 is at softpri = (a5-a3) / 4.
	//  So interrupts in enablemap[1][softpri] are not in a4 (they are higher priority).
	//  The set of interrupts at softpri are:
	//	enablemap[1][softpri-1] - enablemap[1][softpri]
	//  So and'ing a4 with enablemap[1][softpri - 1] will give us
	//  the set of interrupts pending at the highest soft priority.
	//
	l32i	a2, a5, XTHAL_VPRI_ENABLEMAP_OFS + 16*4 - 4	// get enablemap[1][softpri-1]
	and	a4, a2, a4		// only keep interrupts of highest pri (softpri)

	//  a4 now has mask of pending interrupts at highest ready level (new vpri)

	//  Update INTENABLE for this new virtual priority
	l32i	a2, a5, XTHAL_VPRI_ENABLEMAP_OFS + 16*4	// get vpri-specific mask = enablemap[1][softpri]
	l32i	a6, a3, XTHAL_VPRI_ENABLED_OFS		// get global mask
	sub	a5, a5, a3		// a5 = softpri * 4 (for below; here for efficiency)
	and	a2, a2, a6				// and together
	wsr	a2, INTENABLE		// disable interrupts at or below new vpri
	//	a2,a6 = available

	//  Update new virtual priority:
	l8ui	a6, a3, XTHAL_VPRI_VPRI_OFS		// get old vpri (returned)
	srli	a5, a5, 2		// a5 = softpri  (0..15)
	addi	a5, a5, 0x10		// a5 = 0x10 + softpri = new virtual priority
	s8i	a5, a3, XTHAL_VPRI_VPRI_OFS		// store new vpri (returned)

	//  Undo the temporary lock (if was at PS.INTLEVEL > 1):
	//rsil	a2, 1

	mov	a3, a4		// save for the caller (in case it wants it?)

	//  Choose one of the set of highest-vpri pending interrupts to process.
	//  For speed (and simplicity), use this simple two-instruction sequence
	//  to select the least significant bit set in a4.  This implies that
	//  interrupts with a lower interrupt number take precedence over those
	//  with a higher interrupt number (!!).
	//
	neg	a2, a4		// keep only the least-significant bit that is set...
	and	a4, a2, a4	// ... in a4

	//  Software, edge-triggered, and write-error interrupts are cleared by writing to the
	//  INTCLEAR pseudo-reg (to clear relevant bits of the INTERRUPT register).
	//  To simplify interrupt handlers (so they avoid tracking which type of
	//  interrupt they handle and act accordingly), clear such interrupts here.
	//  To avoid race conditions, the clearing must occur *after* we undertake
	//  to process the interrupt, and *before* actually handling the interrupt.
	//  Interrupt handlers may additionally clear the interrupt themselves
	//  at appropriate points if needed to avoid unnecessary interrupts.
	//
#define CLEARABLE_INTLEVEL1_MASK	(XCHAL_INTLEVEL1_MASK & XCHAL_INTCLEARABLE_MASK)
#  if CLEARABLE_INTLEVEL1_MASK != 0
	//movi	a2, CLEARABLE_INTLEVEL1_MASK
	//and	a2, a2, a4
	//wsr	a2, INTCLEAR
	wsr	a4, INTCLEAR	// no effect if a4 not a software or edge-triggered or write-error interrupt
#  endif

	//  Convert the single-bit interrupt mask to an interrupt number.
	//  (ie. compute log2 using either the NSAU instruction or a binary search)

	find_ms_setbit	a2, a4, a2, 0	// set a2 to index of lsbit set in a4 (0..31)
				// NOTE: assumes a4 != 0 (otherwise a2 is undefined[?])

	//	a2 has vector number (0..31)

	//rsr	a4, EXCSAVE_1
	ret

gipfail:
	l8ui	a6, a3, XTHAL_VPRI_VPRI_OFS		// get old vpri
	mov	a5, a6					// is also new vpri (unchanged)
# else /* XCHAL_HAVE_INTERRUPTS */
	//  No interrupts configured!
	movi	a5, 0		// return zero new vpri
	movi	a6, 0		// return zero old vpri
# endif /* XCHAL_HAVE_INTERRUPTS */
	movi	a2, -1		// return bogus vector number (eg. can be quickly tested for negative)
	movi	a3, 0		// return zero bitmask of interrupts pending
	ret
	endfunc

// -----------------------------------------------------------------

#elif defined(__SPLIT__vpri_lock)

// void xthal_vpri_lock()
//
// Used internally by the Core HAL to block interrupts of higher or equal
// priority than Xthal_vpri_locklevel during virtual interrupt operations.
//
SYMT(xthal_vpri_lock)
	abi_entry
# if XCHAL_HAVE_INTERRUPTS
	rsil	a6, 1				// save a6 = PS, set PS.INTLEVEL = 1

	//     if( Xthal_vpri_level < Xthal_vpri_locklevel )
	//
	movi	a2, Xthal_vpri_state		// a2 := address of global var. Xthal_vpri_state
	//interlock
	l8ui	a3, a2, XTHAL_VPRI_VPRI_OFS	// a3 := Xthal_vpri_level == Xthal_vpri_state.vpri
	l8ui	a5, a2, XTHAL_VPRI_LOCKLEVEL_OFS  // a5 := Xthal_vpri_locklevel
	l32i	a4, a2, XTHAL_VPRI_ENABLED_OFS	// a4 := Xthal_vpri_enabled 
	bgeu	a3, a5, xthal_vpri_lock_done

	//  xthal_set_intenable( Xthal_vpri_enablemap[0][Xthal_vpri_locklevel] & Xthal_vpri_enabled );
	//
	addx4	a3, a5, a2			// a3 := a2 + a5*4  (index into enablemap[] array)
	l32i	a3, a3, XTHAL_VPRI_ENABLEMAP_OFS // a3 := Xthal_vpri_enablemap[0][Xthal_vpri_locklevel]
	//interlock
	and	a2, a4, a3
  	wsr	a2, INTENABLE

xthal_vpri_lock_done:
	wsr	a6, PS				// restore PS.INTLEVEL
	rsync
# endif
	abi_return
	endfunc

#elif defined(__SPLIT__vpri_unlock)

// void xthal_vpri_unlock(void)
//
// Enable interrupts according to the current virtual interrupt priority.
// This effectively "unlocks" interrupts disabled by xthal_vpri_lock()
// (assuming the virtual interrupt priority hasn't changed).
//
SYMT(xthal_vpri_unlock)
	abi_entry
# if XCHAL_HAVE_INTERRUPTS
        //
	//  This should be free of race-conditions.
	//
        //  xthal_set_intenable( Xthal_vpri_enablemap[0][Xthal_vpri_level] & Xthal_vpri_enabled );
	//
	movi	a2, Xthal_vpri_state		 // a2 := address of global var. Xthal_vpri_state
	//interlock
	l8ui	a3, a2, XTHAL_VPRI_VPRI_OFS      // a3 := Xthal_vpri_level == Xthal_vpri_state.vpri
	l32i	a4, a2, XTHAL_VPRI_ENABLED_OFS	 // a4 := Xthal_vpri_enabled 
	addx4	a3, a3, a2                       // a3 := a2 + a3*4  (index into enablemap[] array)
	l32i	a3, a3, XTHAL_VPRI_ENABLEMAP_OFS // a3 := Xthal_vpri_enablemap[0][Xthal_vpri_level]
	//interlock
	and	a2, a4, a3
  	wsr	a2, INTENABLE
# endif
	abi_return
	endfunc

#endif /*SPLIT*/

