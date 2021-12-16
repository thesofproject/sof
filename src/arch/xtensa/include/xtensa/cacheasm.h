/*
 * xtensa/cacheasm.h -- assembler-specific cache related definitions
 *			that depend on CORE configuration
 *
 *  This file is logically part of xtensa/coreasm.h ,
 *  but is kept separate for modularity / compilation-performance.
 */

/*
 * Copyright (c) 2001-2018 Cadence Design Systems, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef XTENSA_CACHEASM_H
#define XTENSA_CACHEASM_H

#include <xtensa/coreasm.h>
#include <xtensa/corebits.h>
#include <xtensa/xtensa-xer.h>
#include <xtensa/xtensa-versions.h>

/*
 *  This header file defines assembler macros of the form:
 *	<x>cache_<func>
 *  where <x> is 'i' or 'd' for instruction and data caches,
 *  and <func> indicates the function of the macro.
 *
 *  The following functions <func> are defined,
 *  and apply only to the specified cache (I or D):
 *
 *  reset
 *	Resets the cache (tags only).
 *
 *  reset_data
 *	Resets the cache data if necessary.
 *
 *  sync
 *	Makes sure any previous cache instructions have been completed;
 *	ie. makes sure any previous cache control operations
 *	have had full effect and been synchronized to memory.
 *	Eg. any invalidate completed [so as not to generate a hit],
 *	any writebacks or other pipelined writes written to memory, etc.
 *
 *  invalidate_line		(single cache line)
 *  invalidate_region		(specified memory range)
 *  invalidate_all		(entire cache)
 *	Invalidates all cache entries that cache
 *	data from the specified memory range.
 *	NOTE: locked entries are not invalidated.
 *
 *  writeback_line		(single cache line)
 *  writeback_region		(specified memory range)
 *  writeback_all		(entire cache)
 *	Writes back to memory all dirty cache entries
 *	that cache data from the specified memory range,
 *	and marks these entries as clean.
 *	NOTE: on some future implementations, this might
 *		also invalidate.
 *	NOTE: locked entries are written back, but never invalidated.
 *	NOTE: instruction caches never implement writeback.
 *
 *  writeback_inv_line		(single cache line)
 *  writeback_inv_region	(specified memory range)
 *  writeback_inv_all		(entire cache)
 *	Writes back to memory all dirty cache entries
 *	that cache data from the specified memory range,
 *	and invalidates these entries (including all clean
 *	cache entries that cache data from that range).
 *	NOTE: locked entries are written back but not invalidated.
 *	NOTE: instruction caches never implement writeback.
 *
 *  lock_line			(single cache line)
 *  lock_region			(specified memory range)
 *	Prefetch and lock the specified memory range into cache.
 *	NOTE:  if any part of the specified memory range cannot
 *	be locked, a Load/Store Error (for dcache) or Instruction
 *	Fetch Error (for icache) exception occurs.  These macros don't
 *	do anything special (yet anyway) to handle this situation.
 *
 *  unlock_line			(single cache line)
 *  unlock_region		(specified memory range)
 *  unlock_all			(entire cache)
 *	Unlock cache entries that cache the specified memory range.
 *	Entries not already locked are unaffected.
 *
 *  coherence_on
 *  coherence_off
 *      Turn off and on cache coherence
 *
 */



/***************************   GENERIC -- ALL CACHES   ***************************/


/*
 *  The following macros assume the following cache size/parameter limits
 *  in the current Xtensa core implementation:
 *	cache size:	1024 bytes minimum
 *	line size:	16 - 64 bytes
 *	way count:	1 - 4
 *
 *  Minimum entries per way (ie. per associativity) = 1024 / 64 / 4 = 4
 *  Hence the assumption that each loop can execute four cache instructions.
 *
 *  Correspondingly, the offset range of instructions is assumed able to cover
 *  four lines, ie. offsets {0,1,2,3} * line_size are assumed valid for
 *  both hit and indexed cache instructions.  Ie. these offsets are all
 *  valid:  0, 16, 32, 48, 64, 96, 128, 192 (for line sizes 16, 32, 64).
 *  This is true of all original cache instructions
 *  (dhi, ihi, dhwb, dhwbi, dii, iii) which have offsets
 *  of 0 to 1020 in multiples of 4 (ie. 8 bits shifted by 2).
 *  This is also true of subsequent cache instructions
 *  (dhu, ihu, diu, iiu, diwb, diwbi, dpfl, ipfl) which have offsets
 *  of 0 to 240 in multiples of 16 (ie. 4 bits shifted by 4).
 *
 *  (Maximum cache size, currently 32k, doesn't affect the following macros.
 *  Cache ways > MMU min page size cause aliasing but that's another matter.)
 */



/*
 *  Macro to apply an 'indexed' cache instruction to the entire cache.
 *
 *  Parameters:
 *	cainst      instruction/ that takes an address register parameter
 *              and an offset parameter (in range 0 .. 3*linesize).
 *	size        size of cache in bytes
 *	linesize    size of cache line in bytes (always power-of-2)
 *	assoc_or1   number of associativities (ways/sets) in cache
 *                  if all sets affected by cainst,
 *                  or 1 if only one set (or not all sets) of the cache
 *                  is affected by cainst (eg. DIWB or DIWBI [not yet ISA defined]).
 *	aa, ab      unique address registers (temporaries). 
 *	awb         set to other than a0 if wb type of instruction
 *	loopokay    1 allows use of zero-overhead loops, 0 does not
 *	immrange    range (max value) of cainst's immediate offset parameter, in bytes
 *              (NOTE: macro assumes immrange allows power-of-2 number of lines)
 */

	.macro	cache_index_all		cainst, size, linesize, assoc_or1, aa, ab, loopokay, maxofs, awb=a0

	//  Number of indices in cache (lines per way):
	.set	.Lindices, (\size / (\linesize * \assoc_or1))
	//  Number of indices processed per loop iteration (max 4):
	.set	.Lperloop, .Lindices
	.ifgt	.Lperloop - 4
	 .set	.Lperloop, 4
	.endif
	//  Also limit instructions per loop if cache line size exceeds immediate range:
	.set	.Lmaxperloop, (\maxofs / \linesize) + 1
	.ifgt	.Lperloop - .Lmaxperloop
	 .set	.Lperloop, .Lmaxperloop
	.endif
	//  Avoid addi of 128 which takes two instructions (addmi,addi):
	.ifeq	.Lperloop*\linesize - 128
	 .ifgt	.Lperloop - 1
	  .set	.Lperloop, .Lperloop / 2
	 .endif
	.endif

	//  \size byte cache, \linesize byte lines, \assoc_or1 way(s) affected by each \cainst.
	// XCHAL_ERRATUM_497 - don't execute using loop, to reduce the amount of added code
	.ifne	(\loopokay && XCHAL_HAVE_LOOPS && !XCHAL_ERRATUM_497)

	movi	\aa, .Lindices / .Lperloop		// number of loop iterations
	// Possible improvement: need only loop if \aa > 1 ;
	// however \aa == 1 is highly unlikely.
	movi	\ab, 0		// to iterate over cache
	loop		\aa, .Lend_cachex\@
	.set	.Li, 0 ;     .rept .Lperloop
	  \cainst	\ab, .Li*\linesize
	.set	.Li, .Li+1 ; .endr
	addi		\ab, \ab, .Lperloop*\linesize	// move to next line
.Lend_cachex\@:

	.else

	movi	\aa, (\size / \assoc_or1)
	// Possible improvement: need only loop if \aa > 1 ;
	// however \aa == 1 is highly unlikely.
	movi	\ab, 0		// to iterate over cache
	.ifne	((\awb !=a0) && XCHAL_ERRATUM_497)		// don't use awb if set to a0
	movi \awb, 0
	.endif
.Lstart_cachex\@:
	.set	.Li, 0 ;     .rept .Lperloop
	  \cainst	\ab, .Li*\linesize
	.set	.Li, .Li+1 ; .endr
	.ifne	((\awb !=a0) && XCHAL_ERRATUM_497)		// do memw after 8 cainst wb instructions
	addi \awb, \awb, .Lperloop
	blti \awb, 8, .Lstart_memw\@
	memw
	movi \awb, 0
.Lstart_memw\@:
	.endif
	addi		\ab, \ab, .Lperloop*\linesize	// move to next line
	bltu		\ab, \aa, .Lstart_cachex\@
	.endif

	.endm


	/*
	 *  Macro to apply an 'indexed' cache instruction to the entire cache,
	 *  while avoiding accessing the same cachetag in rapid succession.
	 *  This is important for performance if the cache puts tag info for
	 *  multiple lines in the same tag word.
	 *
	 *  Parameters:
	 *	cainst      instruction/ that takes an address register parameter
	 *              and an offset parameter (in range 0 .. 3*linesize).
	 *	size        size of cache in bytes
	 *	linesize    size of cache line in bytes (always power-of-2)
	 *	assoc_or1   number of associativities (ways/sets) in cache
	 *                  if all sets affected by cainst,
	 *                  or 1 if only one set (or not all sets) of the cache
	 *                  is affected by cainst (eg. DIWB or DIWBI [not yet ISA defined]).
	 *  log2_lines_per_tag
	 *	a1, a2, a3  unique address registers (temporaries).
	 *	loopokay    1 allows use of zero-overhead loops, 0 does not
	 *	immrange    range (max value) of cainst's immediate offset parameter, in bytes
	 *              (NOTE: macro assumes immrange allows power-of-2 number of lines)
	 */
	.macro	cache_index_all_NXtag cainst, size, linesize, assoc_or1,\
		log2_lines_per_tag, a1, a2, a3, loopokay, maxofs
		.set .lines_per_tag, (1<<\log2_lines_per_tag)
		//  Number of indices in cache (lines per way):
		.set	.Lindices, (\size / (\linesize * \assoc_or1))
		//  Number of indices processed per loop iteration (max 4):
		.set	.Lperloop, .Lindices
		.ifgt	.Lperloop - 4
		 .set	.Lperloop, 4
		.endif
		//  Also limit instructions per loop if cache line size exceeds immediate range:
		.set	.Lmaxperloop, (\maxofs / \linesize / .lines_per_tag) + 1
		.ifgt	.Lperloop - .Lmaxperloop
		 .set	.Lperloop, .Lmaxperloop
		.endif
		//  Avoid addi of 128 which takes two instructions (addmi,addi):
		.ifeq	.Lperloop* \linesize * .lines_per_tag - 128
		 .ifgt	.Lperloop - 1
		  .set	.Lperloop, .Lperloop / 2
		 .endif
		.endif
	.ifne (\loopokay)
		// set initial loop count for outer loop ... stride is \linesize
		movi \a1, .lines_per_tag * \linesize
		// compute # iterations for inner loop
		movi \a2, \size / \linesize / .lines_per_tag / .Lperloop / \assoc_or1
		1:
		addi \a1, \a1, - \linesize
		mov \a3, \a1 // initial tag index
		floop \a2, myloop\@
		// this generates .Lperloop * cainst to reduce loop overhead
		.set	.Li, 0 ;     .rept .Lperloop
		  \cainst	\a3, .Li * \linesize * .lines_per_tag
		.set	.Li, .Li+1 ; .endr
		addi.a \a3, \a3, .lines_per_tag * \linesize * .Lperloop
		floopend \a2, myloop\@
		bnez \a1, 1b
	.else
		// Same code without loop instructions (incase of exception handler, ...)
		movi \a1, .lines_per_tag * \linesize
		.Louter\@:
		movi \a2, \size / \linesize / .lines_per_tag / .Lperloop / \assoc_or1
		addi \a1, \a1, - \linesize
		mov \a3, \a1
		.Linner\@:
		.set	.Li, 0 ;     .rept .Lperloop
		  \cainst	\a3, .Li* \linesize * .lines_per_tag
		.set	.Li, .Li+1 ; .endr
		addi.a \a3, \a3, .lines_per_tag * \linesize * .Lperloop
		addi.a \a2, \a2, -1
		bnez \a2, .Linner\@
		bnez \a1, .Louter\@
	.endif
	.endm


/*
 *  Macro to apply a 'hit' cache instruction to a memory region,
 *  ie. to any cache entries that cache a specified portion (region) of memory.
 *  Takes care of the unaligned cases, ie. may apply to one
 *  more cache line than $asize / lineSize if $aaddr is not aligned.
 *
 *
 *  Parameters are:
 *	cainst	instruction/macro that takes an address register parameter
 *		and an offset parameter (currently always zero)
 *		and generates a cache instruction (eg. "dhi", "dhwb", "ihi", etc.)
 *	linesize_log2	log2(size of cache line in bytes)
 *	addr	register containing start address of region (clobbered)
 *	asize	register containing size of the region in bytes (clobbered)
 *	askew	unique register used as temporary
 *	awb		unique register used as temporary for erratum 497.
 *
 *  Note: A possible optimization to this macro is to apply the operation
 *  to the entire cache if the region exceeds the size of the cache
 *  by some empirically determined amount or factor.  Some experimentation
 *  is required to determine the appropriate factors, which also need
 *  to be tunable if required.
 */

	.macro	cache_hit_region	cainst, linesize_log2, addr, asize, askew, awb=a0

	//  Make \asize the number of iterations:
	extui	\askew, \addr, 0, \linesize_log2	// get unalignment amount of \addr
	add	\asize, \asize, \askew			// ... and add it to \asize
	addi	\asize, \asize, (1 << \linesize_log2) - 1	// round up!
	srli	\asize, \asize, \linesize_log2

	//  Iterate over region:
	.ifne	((\awb !=a0) && XCHAL_ERRATUM_497)		// don't use awb if set to a0
	movi \awb, 0
	.endif
	floopnez	\asize, cacheh\@
	\cainst		\addr, 0
	.ifne	((\awb !=a0) && XCHAL_ERRATUM_497)		// do memw after 8 cainst wb instructions
	addi \awb, \awb, 1
	blti \awb, 8, .Lstart_memw\@
	memw
	movi \awb, 0
.Lstart_memw\@:
	.endif
	addi		\addr, \addr, (1 << \linesize_log2)	// move to next line
	floopend	\asize, cacheh\@
	.endm


	.macro cache_hit_region_NXtag cainst, line_width, \
		log2_lines_per_tag, addr, size, aa, ab, ac
	/*
	If multiple cache line entries are contained in the same tag word, then it
	is important (for good performance) that cache operations (writeback,
	invalidate, writeback_inv) not be done in order because that causes
	significant synchronization overhead arround accessing the cachetag.

	If each tag holds the state for 2 cache lines, then a sequence like:
		0, 2, 4, 6, 8, 10, 12, 14, 1, 3, 3, 7, 9, 11, 13, 15
	gives optimal performance.

	In general we use the following pattern:
	for (i=0; i< LINES_PER_TAG ; i++)
		for (j=i * LINE_SIZE + start_of_region; addr(j) in region;
				j+= LINES_PER_TAG * LINE_SIZE)
			cache_op j;

	To implement this efficiently, the count for the inner loop is calculated
	before the loop is entered. The count for the first inner loop is
	(size - 1) / (LINES_PER_TAG * LINE_SIZE) + 1.

	Also for efficiency, we iterate backwards through both loops because addi(/addmi)
	can subtract any of the address increments in a single instruction, but if we
	were going forward through the loop, adding 128 requires 2 instructions.

	The count for the inner loop for subsequent iterations of the outer loop is
	either the same as the first loop or one less (depending on if the # of
	cache lines in the region is a multiple of LINES_PER_TAG). This can be
	implemented efficiently by checking if the inner loop updated that last
	cache line in the region ... if so, then the inner loop count is reduced
	by one for subsequent iterations of the inner loop.

	*  Parameters are:
	*	cainst	instruction/macro that takes an address register parameter
	*	and an offset parameter (currently always zero)
	*	and generates a cache instruction (eg. "dhi", "dhwb", "ihi", etc.)
	*	line_width log2( size of a cache line)
	*	log2_lines_per_tag	log2(number of cache lines controlled by a tag)
	*	addr	register containing start address of region (clobbered)
	*	size	register containing size of the region in bytes (clobbered)
	*	aa, ab, ac	unique registers used as temporaries
	*/
	.set .line_size, (1 << \line_width)
	.set .lines_per_tag, (1 << \log2_lines_per_tag)
	beqz \size, .Lend\@ // Skip 0 length regions
	extui \ab, \addr, 0, \line_width // get index within cache line

	sub \aa, \addr, \ab
	// Now \aa points to start of 1st cache line to operate on

	// Add the offset back to the size
	add \ab, \size, \ab

	// add and chop to extend \ab to be the actual size to
	// be operated on ...
	addi \ab, \ab, .line_size - 1
	extui \ac, \ab, 0, \line_width
	sub \ab, \ab, \ac
	/* \ab now contains the size for the entire operation:
	 *
	 * 	1) The specified region +
	 * 	2) Any part of the first cache line that is before ptr
	 * 	3) Any part of the last cache line that is past ptr + size
	 */

	// calculate address of last line in region (first line to be acted on)
	add \addr, \aa, \ab // this is the first address outside the region
	addi \addr, \addr, - .line_size

	// calculate initial inner loop counter
	addi \size, \ab, -1
	srai \size, \size, \line_width + \log2_lines_per_tag
	addi \size, \size, 1

	addi \ab, \addr, - (.line_size * (.lines_per_tag - 1))

	addi \aa, \aa, - (.line_size * .lines_per_tag)

	/* At this point:
	 * addr = address of first cache line to operate on (last line in the region) ... addr
	 *  is the counter for the outer loop
	 * size = # if iterations for the inner loop the first time through
	 * aa = address of 1st region - (line_size + lines_per_tag) ... if the inner loop address
	 * 	is equal to this value at the end of the inner loop, then 'size' needs to be decremented
	 * 	before the inner loop is started again
	 * ab = marker for ending the outer loop ... if outer loop counter is at this value then
	 *  the region op has completed
	 */

	.Lloop_start\@:
	mov \ac, \addr
	floop \size, inner_loop\@
	\cainst \ac, 0 // do the op
	// go to previous line with stride LINES_PER_TAG
	addi.a \ac, \ac, - (.line_size * .lines_per_tag)
	floopend \size, inner_loop\@
	beq \addr, \ab, .Lend\@ // test if done with outer loop
	addi.a \addr, \addr, - .line_size // decrement starting address
	bne \ac, \aa, .Lloop_start\@ // test if inner loop count needs to be decremented
	addi.a \size, \size, -1
	bnez \size, .Lloop_start\@ // if inner loop count is zero ... we are done
	.Lend\@:
	.endm

/***************************   INSTRUCTION CACHE   ***************************/


/*
 *  Reset/initialize the instruction cache by simply invalidating it:
 *  (need to unlock first also, if cache locking implemented):
 *
 *  Parameters:
 *	aa, ab		unique address registers (temporaries)
 */
	.macro	icache_reset	aa, ab, loopokay=0
	icache_unlock_all	\aa, \ab, \loopokay
	icache_invalidate_all	\aa, \ab, \loopokay
	.endm


/*
 * Synchronize after an instruction cache operation,
 * to be sure everything is in sync with memory as to be
 * expected following any previous instruction cache control operations.
 *
 * Even if a config doesn't have caches, an isync is still needed
 * when instructions in any memory are modified, whether by a loader
 * or self-modifying code.  Therefore, this macro always produces
 * an isync, whether or not an icache is present.
 *
 * Parameters are:
 *	ar	an address register (temporary) (currently unused, but may be used in future)
 */
	.macro	icache_sync	ar
	isync
	.endm



/*
 *  Invalidate a single line of the instruction cache.
 *  Parameters are:
 *	ar	address register that contains (virtual) address to invalidate
 *		(may get clobbered in a future implementation, but not currently)
 *	offset	(optional) offset to add to \ar to compute effective address to invalidate
 *		(note: some number of lsbits are ignored)
 */
	.macro	icache_invalidate_line	ar, offset
#if XCHAL_ICACHE_SIZE > 0
	ihi	\ar, \offset		// invalidate icache line
	icache_sync	\ar
#endif
	.endm




/*
 *  Invalidate instruction  cache entries that cache a specified portion of memory.
 *  Parameters are:
 *	astart	start address (register gets clobbered)
 *	asize	size of the region in bytes (register gets clobbered)
 *	ac	unique register used as temporary
 */
	.macro	icache_invalidate_region	astart, asize, ac
#if XCHAL_ICACHE_SIZE > 0
	//  Instruction cache region invalidation:
	cache_hit_region	ihi, XCHAL_ICACHE_LINEWIDTH, \astart, \asize, \ac
	icache_sync	\ac
	//  End of instruction cache region invalidation
#endif
	.endm



/*
 *  Invalidate entire instruction cache.
 *
 *  Parameters:
 *	aa, ab		unique address registers (temporaries)
 */
	.macro	icache_invalidate_all	aa, ab, loopokay=1
#if XCHAL_ICACHE_SIZE > 0
	//  Instruction cache invalidation:
	cache_index_all		iii, XCHAL_ICACHE_SIZE, XCHAL_ICACHE_LINESIZE, XCHAL_ICACHE_WAYS, \aa, \ab, \loopokay, 1020
	icache_sync	\aa
	//  End of instruction cache invalidation
#endif
	.endm



/*
 *  Lock (prefetch & lock) a single line of the instruction cache.
 *
 *  Parameters are:
 *	ar	address register that contains (virtual) address to lock
 *		(may get clobbered in a future implementation, but not currently)
 *	offset	offset to add to \ar to compute effective address to lock
 *		(note: some number of lsbits are ignored)
 */
	.macro	icache_lock_line	ar, offset
#if XCHAL_ICACHE_SIZE > 0 && XCHAL_ICACHE_LINE_LOCKABLE
	ipfl	\ar, \offset	/* prefetch and lock icache line */
	icache_sync	\ar
#endif
	.endm



/*
 *  Lock (prefetch & lock) a specified portion of memory into the instruction cache.
 *  Parameters are:
 *	astart	start address (register gets clobbered)
 *	asize	size of the region in bytes (register gets clobbered)
 *	ac	unique register used as temporary
 */
	.macro	icache_lock_region	astart, asize, ac
#if XCHAL_ICACHE_SIZE > 0 && XCHAL_ICACHE_LINE_LOCKABLE
	//  Instruction cache region lock:
	cache_hit_region	ipfl, XCHAL_ICACHE_LINEWIDTH, \astart, \asize, \ac
	icache_sync	\ac
	//  End of instruction cache region lock
#endif
	.endm



/*
 *  Unlock a single line of the instruction cache.
 *
 *  Parameters are:
 *	ar	address register that contains (virtual) address to unlock
 *		(may get clobbered in a future implementation, but not currently)
 *	offset	offset to add to \ar to compute effective address to unlock
 *		(note: some number of lsbits are ignored)
 */
	.macro	icache_unlock_line	ar, offset
#if XCHAL_ICACHE_SIZE > 0 && XCHAL_ICACHE_LINE_LOCKABLE
	ihu	\ar, \offset	/* unlock icache line */
	icache_sync	\ar
#endif
	.endm



/*
 *  Unlock a specified portion of memory from the instruction cache.
 *  Parameters are:
 *	astart	start address (register gets clobbered)
 *	asize	size of the region in bytes (register gets clobbered)
 *	ac	unique register used as temporary
 */
	.macro	icache_unlock_region	astart, asize, ac
#if XCHAL_ICACHE_SIZE > 0 && XCHAL_ICACHE_LINE_LOCKABLE
	//  Instruction cache region unlock:
	cache_hit_region	ihu, XCHAL_ICACHE_LINEWIDTH, \astart, \asize, \ac
	icache_sync	\ac
	//  End of instruction cache region unlock
#endif
	.endm



/*
 *  Unlock entire instruction cache.
 *
 *  Parameters:
 *	aa, ab		unique address registers (temporaries)
 */
	.macro	icache_unlock_all	aa, ab, loopokay=1
#if XCHAL_ICACHE_SIZE > 0 && XCHAL_ICACHE_LINE_LOCKABLE
	//  Instruction cache unlock:
	cache_index_all		iiu, XCHAL_ICACHE_SIZE, XCHAL_ICACHE_LINESIZE, 1, \aa, \ab, \loopokay, 240
	icache_sync	\aa
	//  End of instruction cache unlock
#endif
	.endm





/***************************   DATA CACHE   ***************************/



/*
 *  Reset/initialize the data cache by simply invalidating it
 *  (need to unlock first also, if cache locking implemented):
 *
 *  Parameters:
 *	aa, ab, ac		unique address registers (temporaries)
 */
	.macro	dcache_reset	aa, ab, ac, loopokay=0
	dcache_unlock_all	\aa, \ab, \loopokay
	dcache_invalidate_all	\aa, \ab, \ac, \loopokay
	dcache_reset_data	\aa, \ab, \ac, \loopokay
	.endm




/*
 * Synchronize after a data cache operation,
 * to be sure everything is in sync with memory as to be
 * expected following any previous data cache control operations.
 *
 * Parameters are:
 *	ar	an address register (temporary) (currently unused, but may be used in future)
 */
	.macro	dcache_sync	ar, wbtype=0
#if XCHAL_DCACHE_SIZE > 0
	//  No synchronization is needed.
	//  (memw may be desired e.g. after writeback operation to help ensure subsequent
	//   external accesses are seen to follow that writeback, however that's outside
	//   the scope of this macro)

	//dsync
	.ifne	(\wbtype & XCHAL_ERRATUM_497)
	memw
	.endif
#endif
	.endm



/*
 * Turn on cache coherence.
 *
 * WARNING This macro assumes exclusive access to the L2CC.  It must
 * be protected with a mutex to ensure that the Coherence Control register
 * is not corrupted by another core.
 *
 * WARNING Any interrupt that tries to change MEMCTL will see its changes
 * dropped if the interrupt comes in the middle of this routine.  If this
 * might be an issue, call this routine with interrupts disabled.
 *
 * Parameters are:
 *	ar, as, at	three scratch address registers (all clobbered)
 */
	.macro	cache_coherence_on_memctl        ar at
#if XCHAL_DCACHE_IS_COHERENT
# if XCHAL_HW_MIN_VERSION >= XTENSA_HWVERSION_RE_2012_0
	/*  Have MEMCTL.  Enable snoop responses.  */
	rsr.memctl	\ar
	movi		\at, MEMCTL_SNOOP_EN
	or		\ar, \ar, \at
	wsr.memctl	\ar
# elif XCHAL_HAVE_EXTERN_REGS && XCHAL_HAVE_MX
	/* Opt into coherence for MX (for backward compatibility / testing).  */
	movi	\ar, 1
	movi	\at, XER_CCON
	wer	\ar, \at
	extw
# endif
#endif
	.endm

        .macro  cache_coherence_on_L2 ar as at
#if XCHAL_DCACHE_IS_COHERENT && XCHAL_HAVE_L2 && XCHAL_HAVE_PRID
        rsr.prid        \ar
        movi            \at, XCHAL_L2CC_NUM_CORES-1
        and             \ar, \ar, \at
        movi            \at, 0x100
        ssl             \ar
        sll             \at, \at
        movi            \ar, XCHAL_L2_REGS_PADDR
        l32i            \as, \ar, L2CC_REG_COHERENCE_CTRL
        or              \as, \as, \at
        s32i            \as, \ar, L2CC_REG_COHERENCE_CTRL
#endif
        .endm

        .macro  cache_coherence_on        ar as at
        cache_coherence_on_memctl \ar \as
        cache_coherence_on_L2 \ar \as \at
        .endm


/*
 * Turn off cache coherence.
 *
 * NOTE:  this is generally preceded by emptying the cache;
 * see xthal_cache_coherence_optout() in hal/coherence.c for details.
 *
 * WARNING This macro assumes exclusive access to the L2CC.  It must
 * be protected with a mutex to ensure that the Coherence Control register
 * is not corrupted by another core.
 *
 * WARNING: any interrupt that tries
 * to change MEMCTL will see its changes dropped if the interrupt comes
 * in the middle of this routine.  If this might be an issue, call this
 * routine with interrupts disabled.
 *
 * Parameters are:
 *	ar,as,at	three scratch address registers (all clobbered)
 */

        .macro  cache_coherence_off_L2  ar as at
#if XCHAL_HAVE_L2 && XCHAL_HAVE_PRID
        rsr.prid        \ar
        movi            \at, XCHAL_L2CC_NUM_CORES-1
        and             \ar, \ar, \at
        movi            \at, 0x100
        ssl             \ar
        sll             \at, \at
        movi            \ar, XCHAL_L2_REGS_PADDR
        l32i            \as, \ar, L2CC_REG_COHERENCE_CTRL
        xor             \as, \as, \at
        s32i            \as, \ar, L2CC_REG_COHERENCE_CTRL
#endif
        .endm

        .macro cache_coherence_off_memctl ar at
#if XCHAL_DCACHE_IS_COHERENT
        /*  Have MEMCTL.  Disable snoop responses.  */
        rsr.memctl      \ar
        movi            \at, ~MEMCTL_SNOOP_EN
        and             \ar, \ar, \at
        wsr.memctl      \ar
#endif
        .endm

	.macro	cache_coherence_off	ar as at
	cache_coherence_off_memctl \ar \at
	cache_coherence_off_L2  \ar \as \at
	.endm



/*
 * Synchronize after a data store operation,
 * to be sure the stored data is completely off the processor
 * (and assuming there is no buffering outside the processor,
 *  that the data is in memory).  This may be required to
 * ensure that the processor's write buffers are emptied.
 * A MEMW followed by a read guarantees this, by definition.
 * We also try to make sure the read itself completes.
 *
 * Parameters are:
 *	ar	an address register (temporary)
 */
	.macro	write_sync	ar
	memw			// ensure previous memory accesses are complete prior to subsequent memory accesses
	l32i	\ar, sp, 0	// completing this read ensures any previous write has completed, because of MEMW
	//slot
	add	\ar, \ar, \ar	// use the result of the read to help ensure the read completes (in future architectures)
	.endm


/*
 *  Invalidate a single line of the data cache.
 *  Parameters are:
 *	ar	address register that contains (virtual) address to invalidate
 *		(may get clobbered in a future implementation, but not currently)
 *	offset	(optional) offset to add to \ar to compute effective address to invalidate
 *		(note: some number of lsbits are ignored)
 */
	.macro	dcache_invalidate_line	ar, offset
#if XCHAL_DCACHE_SIZE > 0
	dhi	\ar, \offset
	dcache_sync	\ar
#endif
	.endm





/*
 *  Invalidate data cache entries that cache a specified portion of memory.
 *  Parameters are:
 *	astart	start address (register gets clobbered)
 *	asize	size of the region in bytes (register gets clobbered)
 *	ac, ad, ae	unique registers used as temporaries
 */
	.macro	dcache_invalidate_region	astart, asize, ac, ad, ae
#if XCHAL_DCACHE_SIZE > 0
	//  Data cache region invalidation:
#if XCHAL_DCACHE_IS_COHERENT
	cache_hit_region_NXtag dci, XCHAL_DCACHE_LINEWIDTH, XCHAL_DCACHE_LINES_PER_TAG_LOG2, \astart, \asize, \ac, \ad, \ae
#else
#if defined(XCHAL_DCACHE_LINES_PER_TAG_LOG2) && XCHAL_DCACHE_LINES_PER_TAG_LOG2
	cache_hit_region_NXtag dhi, XCHAL_DCACHE_LINEWIDTH, XCHAL_DCACHE_LINES_PER_TAG_LOG2, \astart, \asize, \ac, \ad, \ae
#else
	cache_hit_region	dhi, XCHAL_DCACHE_LINEWIDTH, \astart, \asize, \ac
#endif
	dcache_sync	\ac
	//  End of data cache region invalidation
#endif
#endif
	.endm



/*
 *  Invalidate entire data cache.
 *
 *  Parameters:
 *	aa, ab, ac		unique address registers (temporaries)
 *	loopokay		if 1, then use loop instructions if available
 */
	.macro	dcache_invalidate_all	aa, ab, ac, loopokay=1
#if XCHAL_DCACHE_SIZE > 0
	//  Data cache invalidation:
#if defined(XCHAL_DCACHE_LINES_PER_TAG_LOG2) && XCHAL_DCACHE_LINES_PER_TAG_LOG2
        // On NX, dii invalidates all ways and all lines associated with
        // a cache tag.  So we can use a single loop and invalidate only
        // the first line in associated with a cache tag.
        // in essence we are just replacing the line size with the sector size
        // and reusing the 1-line/tag code.
        // not this applies only to dii and not to diwb or diwbi
	cache_index_all	dii, XCHAL_DCACHE_SIZE, \
		XCHAL_DCACHE_LINESIZE  * (1<<XCHAL_DCACHE_LINES_PER_TAG_LOG2), \
		XCHAL_DCACHE_WAYS, \aa, \ab, \loopokay, 1020
#else
	cache_index_all	dii, XCHAL_DCACHE_SIZE, XCHAL_DCACHE_LINESIZE, XCHAL_DCACHE_WAYS, \aa, \ab, \loopokay, 1020
#endif
	dcache_sync	\aa
	//  End of data cache invalidation
#endif
	.endm



/*
 *  Initialize data (not tags) of the entire data cache, if needed.
 *  At present, this is only needed on block-prefetch in combination with ECC.
 *  This is only available with RG and later releases, which also have SDCW.
 *  Issue:  prefetch-and-modify marks a line valid without touching its data,
 *  so any load of the prefetch-and-modified area before storing to it, or
 *  store narrower than ECC width (if ECC is wider than one byte, meaning
 *  XCHAL_DCACHE_ECC_WIDTH > 1), may encounter an invalid ECC if this is the
 *  first time since reset this particular line got used.
 *  Although possibly deemed more serious on stores, this workaround is applied
 *  on loads too, thus regardless of XCHAL_DCACHE_ECC_WIDTH, and regardless of
 *  ECC vs PARITY..
 *
 *  NOTE:  Right now this is only done when cache test instructions (including
 *  SDCW) are configured; an alternate method given a range of memory at least
 *  as large as dcache needs to be used otherwise.
 *
 *  Parameters:
 *	aa, ab, ac		unique address registers (temporaries)
 */
	.macro	dcache_reset_data	aa, ab, ac, loopokay=1
#if XCHAL_DCACHE_SIZE > 0 && XCHAL_HW_MIN_VERSION >= XTENSA_HWVERSION_RG_2015_0 && XCHAL_HAVE_CACHE_BLOCKOPS && XCHAL_DCACHE_ECC_PARITY != 0 && XCHAL_HAVE_DCACHE_TEST
	movi	\aa, 0
# if XCHAL_HAVE_LOOPS && !XCHAL_ERRATUM_497
	.ifne	\loopokay
	movi	\ab, XCHAL_DCACHE_SIZE/4
	movi	\ac, 0
	loop	\ab, .Lend_dcache_reset_data\@
	sdcw	\aa, \ac
	addi	\ac, \ac, 4
.Lend_dcache_reset_data\@:
# else
	.ifne	0
# endif
	.else
	movi	\ab, XCHAL_DCACHE_SIZE
.Loop_dcache_reset_data\@:
	addi	\ab, \ab, -4
	sdcw	\aa, \ab
	bnez	\ab, .Loop_dcache_reset_data\@
	.endif
#endif
	.endm


/*
 *  Writeback a single line of the data cache.
 *  Parameters are:
 *	ar	address register that contains (virtual) address to writeback
 *		(may get clobbered in a future implementation, but not currently)
 *	offset	offset to add to \ar to compute effective address to writeback
 *		(note: some number of lsbits are ignored)
 */
	.macro	dcache_writeback_line	ar, offset
#if XCHAL_DCACHE_SIZE > 0 && XCHAL_DCACHE_IS_WRITEBACK
	dhwb	\ar, \offset
	dcache_sync	\ar, wbtype=1
#endif
	.endm



/*
 *  Writeback dirty data cache entries that cache a specified portion of memory.
 *  Parameters are:
 *	astart	start address (register gets clobbered)
 *	asize	size of the region in bytes (register gets clobbered)
 *	ac, ad, ae	unique registes used as temporaries
 */
	.macro	dcache_writeback_region		astart, asize, ac, ad, ae
#if XCHAL_DCACHE_SIZE > 0 && XCHAL_DCACHE_IS_WRITEBACK
	//  Data cache region writeback:
#if XCHAL_DCACHE_IS_COHERENT
	cache_hit_region_NXtag dcwb, XCHAL_DCACHE_LINEWIDTH, XCHAL_DCACHE_LINES_PER_TAG_LOG2, \astart, \asize, \ac, \ad, \ae
#else
#if defined(XCHAL_DCACHE_LINES_PER_TAG_LOG2) && XCHAL_DCACHE_LINES_PER_TAG_LOG2
	cache_hit_region_NXtag dhwb, XCHAL_DCACHE_LINEWIDTH, XCHAL_DCACHE_LINES_PER_TAG_LOG2, \astart, \asize, \ac, \ad, \ae
#else
	cache_hit_region	dhwb, XCHAL_DCACHE_LINEWIDTH, \astart, \asize, \ac, \ad
#endif
	dcache_sync	\ac, wbtype=1
	//  End of data cache region writeback
#endif
#endif
	.endm



/*
 *  Writeback entire data cache.
 *  Parameters:
 *	aa, ab, ac		unique address registers (temporaries)
 *	loopokay		if 1, then use loop instructions if available
 */
	.macro	dcache_writeback_all	aa, ab, ac, loopokay=1
#if XCHAL_DCACHE_SIZE > 0 && XCHAL_DCACHE_IS_WRITEBACK
	//  Data cache writeback:
#if defined(XCHAL_DCACHE_LINES_PER_TAG_LOG2) && XCHAL_DCACHE_LINES_PER_TAG_LOG2
	cache_index_all_NXtag	diwb, XCHAL_DCACHE_SIZE, \
		XCHAL_DCACHE_LINESIZE, 1, XCHAL_DCACHE_LINES_PER_TAG_LOG2, \
		\aa, \ab, \ac, \loopokay, 240
#else
		cache_index_all		diwb, XCHAL_DCACHE_SIZE, XCHAL_DCACHE_LINESIZE, 1, \aa, \ab, \loopokay, 240, \ac
#endif
	dcache_sync	\aa, wbtype=1
	//  End of data cache writeback
#endif
	.endm



/*
 *  Writeback and invalidate a single line of the data cache.
 *  Parameters are:
 *	ar	address register that contains (virtual) address to writeback and invalidate
 *		(may get clobbered in a future implementation, but not currently)
 *	offset	offset to add to \ar to compute effective address to writeback and invalidate
 *		(note: some number of lsbits are ignored)
 */
	.macro	dcache_writeback_inv_line	ar, offset
#if XCHAL_DCACHE_SIZE > 0
	dhwbi	\ar, \offset	/* writeback and invalidate dcache line */
	dcache_sync	\ar, wbtype=1
#endif
	.endm



/*
 *  Writeback and invalidate data cache entries that cache a specified portion of memory.
 *  Parameters are:
 *	astart	start address (register gets clobbered)
 *	asize	size of the region in bytes (register gets clobbered)
 *	ac, ad, ae	unique registers used as temporaries
 */
	.macro	dcache_writeback_inv_region	astart, asize, ac, ad, ae
#if XCHAL_DCACHE_SIZE > 0
	//  Data cache region writeback and invalidate:
#if XCHAL_DCACHE_IS_COHERENT
	cache_hit_region_NXtag dcwbi, XCHAL_DCACHE_LINEWIDTH, XCHAL_DCACHE_LINES_PER_TAG_LOG2, \astart, \asize, \ac, \ad, \ae
#else
#if defined(XCHAL_DCACHE_LINES_PER_TAG_LOG2) && XCHAL_DCACHE_LINES_PER_TAG_LOG2
	cache_hit_region_NXtag dhwbi, XCHAL_DCACHE_LINEWIDTH, XCHAL_DCACHE_LINES_PER_TAG_LOG2, \astart, \asize, \ac, \ad, \ae
#else
	cache_hit_region	dhwbi, XCHAL_DCACHE_LINEWIDTH, \astart, \asize, \ac, \ad
#endif
	dcache_sync	\ac, wbtype=1
	//  End of data cache region writeback and invalidate
#endif
#endif
	.endm



/*
 *  Writeback and invalidate entire data cache.
 *  Parameters:
 *	aa, ab, ac		unique address registers (temporaries)
 *	loopokay		if 1, then use loop instructions if available
 */
	.macro	dcache_writeback_inv_all	aa, ab, ac, loopokay=1
#if XCHAL_DCACHE_SIZE > 0
	//  Data cache writeback and invalidate:
#if XCHAL_DCACHE_IS_WRITEBACK
#if defined(XCHAL_DCACHE_LINES_PER_TAG_LOG2) && XCHAL_DCACHE_LINES_PER_TAG_LOG2
	cache_index_all_NXtag	diwbi, XCHAL_DCACHE_SIZE, \
			XCHAL_DCACHE_LINESIZE, 1, XCHAL_DCACHE_LINES_PER_TAG_LOG2, \
			\aa, \ab, \ac, \loopokay, 240
#else
	cache_index_all		diwbi, XCHAL_DCACHE_SIZE, XCHAL_DCACHE_LINESIZE, 1, \aa, \ab, \loopokay, 240, \ac
#endif
	dcache_sync	\aa, wbtype=1
#else /*writeback*/
	//  Data cache does not support writeback, so just invalidate: */
	dcache_invalidate_all	\aa, \ab, \ac, \loopokay
#endif /*writeback*/
	//  End of data cache writeback and invalidate
#endif
	.endm




/*
 *  Lock (prefetch & lock) a single line of the data cache.
 *
 *  Parameters are:
 *	ar	address register that contains (virtual) address to lock
 *		(may get clobbered in a future implementation, but not currently)
 *	offset	offset to add to \ar to compute effective address to lock
 *		(note: some number of lsbits are ignored)
 */
	.macro	dcache_lock_line	ar, offset
#if XCHAL_DCACHE_SIZE > 0 && XCHAL_DCACHE_LINE_LOCKABLE
	dpfl	\ar, \offset	/* prefetch and lock dcache line */
	dcache_sync	\ar
#endif
	.endm



/*
 *  Lock (prefetch & lock) a specified portion of memory into the data cache.
 *  Parameters are:
 *	astart	start address (register gets clobbered)
 *	asize	size of the region in bytes (register gets clobbered)
 *	ac	unique register used as temporary
 */
	.macro	dcache_lock_region	astart, asize, ac
#if XCHAL_DCACHE_SIZE > 0 && XCHAL_DCACHE_LINE_LOCKABLE
	//  Data cache region lock:
	cache_hit_region	dpfl, XCHAL_DCACHE_LINEWIDTH, \astart, \asize, \ac
	dcache_sync	\ac
	//  End of data cache region lock
#endif
	.endm



/*
 *  Unlock a single line of the data cache.
 *
 *  Parameters are:
 *	ar	address register that contains (virtual) address to unlock
 *		(may get clobbered in a future implementation, but not currently)
 *	offset	offset to add to \ar to compute effective address to unlock
 *		(note: some number of lsbits are ignored)
 */
	.macro	dcache_unlock_line	ar, offset
#if XCHAL_DCACHE_SIZE > 0 && XCHAL_DCACHE_LINE_LOCKABLE
	dhu	\ar, \offset	/* unlock dcache line */
	dcache_sync	\ar
#endif
	.endm



/*
 *  Unlock a specified portion of memory from the data cache.
 *  Parameters are:
 *	astart	start address (register gets clobbered)
 *	asize	size of the region in bytes (register gets clobbered)
 *	ac	unique register used as temporary
 */
	.macro	dcache_unlock_region	astart, asize, ac
#if XCHAL_DCACHE_SIZE > 0 && XCHAL_DCACHE_LINE_LOCKABLE
	//  Data cache region unlock:
	cache_hit_region	dhu, XCHAL_DCACHE_LINEWIDTH, \astart, \asize, \ac
	dcache_sync	\ac
	//  End of data cache region unlock
#endif
	.endm



/*
 *  Unlock entire data cache.
 *
 *  Parameters:
 *	aa, ab		unique address registers (temporaries)
 */
	.macro	dcache_unlock_all	aa, ab, loopokay=1
#if XCHAL_DCACHE_SIZE > 0 && XCHAL_DCACHE_LINE_LOCKABLE
	//  Data cache unlock:
	cache_index_all		diu, XCHAL_DCACHE_SIZE, XCHAL_DCACHE_LINESIZE, 1, \aa, \ab,  \loopokay, 240
	dcache_sync	\aa
	//  End of data cache unlock
#endif
	.endm



/*
 * Get the number of enabled icache ways. Note that this may
 * be different from the value read from the MEMCTL register.
 *
 * Parameters:
 *	aa	address register where value is returned
 */
	.macro	icache_get_ways		aa
#if XCHAL_ICACHE_SIZE > 0
#if XCHAL_HAVE_ICACHE_DYN_ENABLE
	// Read from MEMCTL and shift/mask
	rsr.memctl	\aa
	extui	\aa, \aa, MEMCTL_ICWU_SHIFT, MEMCTL_ICWU_BITS
	blti	\aa, XCHAL_ICACHE_WAYS, .Licgw
	movi	\aa, XCHAL_ICACHE_WAYS
.Licgw:
#else
	// All ways are always enabled
	movi	\aa, XCHAL_ICACHE_WAYS
#endif
#else
	// No icache
	movi	\aa, 0
#endif
	.endm



/*
 * Set the number of enabled icache ways.
 *
 * Parameters:
 *	aa      address register specifying number of ways (trashed)
 *	ab,ac	address register for scratch use (trashed)
 */
	.macro  icache_set_ways		aa, ab, ac
#if XCHAL_ICACHE_SIZE > 0 && XCHAL_HAVE_ICACHE_DYN_ENABLE
# if ! XCHAL_HAVE_ICACHE_DYN_WAYS
	beqz	\aa, 1f				// cannot disable the cache
	movi	\aa, XCHAL_ICACHE_WAYS		// can only enable all ways
# endif
	movi	\ac, ~MEMCTL_ICWU_MASK		// set up to clear bits 18-22
	rsr.memctl	\ab
	and	\ab, \ab, \ac
	movi	\ac, MEMCTL_INV_EN		// set bit 23
	slli	\aa, \aa, MEMCTL_ICWU_SHIFT	// move to right spot
	or	\ab, \ab, \aa
	or	\ab, \ab, \ac
	wsr.memctl	\ab
1:
#else
	// No icache, or all ways are always enabled
#endif
	.endm



/*
 * Get the number of enabled dcache ways. Note that this may
 * be different from the value read from the MEMCTL register.
 *
 * Parameters:
 *	aa	address register where value is returned
 */
	.macro	dcache_get_ways		aa
#if XCHAL_DCACHE_SIZE > 0
#if XCHAL_HAVE_DCACHE_DYN_ENABLE
	// Read from MEMCTL and shift/mask
	rsr.memctl	\aa
	extui	\aa, \aa, MEMCTL_DCWU_SHIFT, MEMCTL_DCWU_BITS
	blti	\aa, XCHAL_DCACHE_WAYS, .Ldcgw
	movi	\aa, XCHAL_DCACHE_WAYS
.Ldcgw:
#else
	// All ways are always enabled  
	movi	\aa, XCHAL_DCACHE_WAYS
#endif
#else
	// No dcache
	movi	\aa, 0
#endif
	.endm



/*
 * Set the number of enabled dcache ways.
 *
 * Parameters:
 *	aa	address register specifying number of ways (trashed)
 *	ab,ac	address register for scratch use (trashed)
 */
	.macro	dcache_set_ways		aa, ab, ac
#if (XCHAL_DCACHE_SIZE > 0) && XCHAL_HAVE_DCACHE_DYN_ENABLE
# if ! XCHAL_HAVE_DCACHE_DYN_WAYS
	beqz	\aa, .Ldsw4			// cannot disable the cache
	movi	\aa, XCHAL_DCACHE_WAYS		// can only enable all ways
# endif
	movi	\ac, ~MEMCTL_DCWA_MASK		// set up to clear bits 13-17
	rsr.memctl	\ab
	and	\ab, \ab, \ac			// clear ways allocatable
	slli	\ac, \aa, MEMCTL_DCWA_SHIFT
	or	\ab, \ab, \ac			// set ways allocatable
	wsr.memctl	\ab
# if XCHAL_DCACHE_IS_WRITEBACK && XCHAL_HAVE_DCACHE_DYN_WAYS
	// Assumption is that if DAllocWay = N, then DUseWay can be set to N
	rsr.memctl      \ab                     // there is no guarentee the
	                                        // number of ways requested
	                                        // is actually what was set
	                                        // so we read it back, mask it off
	                                        // and use that as the number of ways
	movi    \ac, MEMCTL_DCWA_MASK
	and     \ac, \ab, \ac
	srli    \aa, \ac, MEMCTL_DCWA_SHIFT
	// Check if the way count is increasing or decreasing
	extui	\ac, \ab, MEMCTL_DCWU_SHIFT, MEMCTL_DCWU_BITS			// bits 8-12 - ways in use
	bge	\aa, \ac, .Ldsw3						// equal or increasing
	slli	\ab, \aa, XCHAL_DCACHE_LINEWIDTH + XCHAL_DCACHE_SETWIDTH	// start way number
	sub     \ac, \ac, \aa // ways to writeback+invalidate
	// compute number of lines to be written back
	// multiply by number of lines in the cache way
	slli	\ac, \ac, XCHAL_DCACHE_SIZE_LOG2 - XCHAL_DCACHE_LINEWIDTH - XCHAL_DCACHE_WAYS_LOG2
	floop \ac, myloop\@
	diwbui.p	\ab		// auto-increments ab
	floopend \ac, myloop\@
	rsr.memctl	\ab
# endif
.Ldsw3:
	// No dirty data to write back, just set the new number of ways
	movi	\ac, ~MEMCTL_DCWU_MASK			// set up to clear bits 8-12
	and	\ab, \ab, \ac				// clear ways in use
	movi	\ac, MEMCTL_INV_EN
	or	\ab, \ab, \ac				// set bit 23
	slli	\aa, \aa, MEMCTL_DCWU_SHIFT
	or	\ab, \ab, \aa				// set ways in use
	wsr.memctl	\ab
.Ldsw4:
#else
	// No dcache or no way disable support
#endif
	.endm


#endif /*XTENSA_CACHEASM_H*/

