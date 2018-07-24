// 
// cache_asm.S - assembly language cache management routines
//
// $Id: //depot/rel/Foxhill/dot.8/Xtensa/OS/hal/cache_asm.S#1 $

// Copyright (c) 1999-2015 Cadence Design Systems, Inc.
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




//----------------------------------------------------------------------
//  Huge Range cache routines
//----------------------------------------------------------------------

	//  void  xthal_dcache_hugerange_<name>(void *addr, unsigned size);
	//
	//  Invalidate and/or writeback dcache entries for an arbitrary large
	//  virtual address range with a single scan of the dcache.
	//  Assumes no address translation, i.e. virtual = physical.
	//
	//  a2 = ptr to range
	//  a3 = size of range
	//
	//  Note:  -128 is a valid immediate for ADDI, but +128 is not,
	//  and ADDI can relax to ADDMI for multiples of 256.  So scanning
	//  cache backwards (from end to start) allows all cache line sizes
	//  without creating an extra instruction for the ADDI.
	//
	.macro dcache_hugefunc  name, instruction
	.text
	.align	4
	.type	xthal_dcache_hugerange_\name,@function
	.global	xthal_dcache_hugerange_\name
xthal_dcache_hugerange_\name:
	abi_entry
#if (!defined(XCHAL_HAVE_NX) || XCHAL_HAVE_NX == 0) && XCHAL_DCACHE_SIZE > 0 \
	&& XCHAL_HAVE_DCACHE_TEST && XCHAL_HAVE_MINMAX && XCHAL_HAVE_LOOPS
	movi	a4, XCHAL_DCACHE_SIZE*2		// size at which to use huge algorithm
	movi	a7, -XCHAL_DCACHE_LINESIZE	// for rounding to cache line size
	bltu	a3, a4, 7f			// use normal (line-by-line hit) function
#if XCHAL_HAVE_PREFETCH
	movi	a11, 0
	xsr.prefctl a11		// temporarily disable prefetch (invalidates prefetch bufs!)
#endif
	add	a5, a3, a2			// a5 = end of range
	and	a4, a2, a7			// a4 = low end, rounded to containing cache line
	addi	a5, a5, /*XCHAL_DCACHE_LINESIZE*/-1
	and	a5, a5, a7			// a5 = high end, rounded to containing cache line
	movi	a7, XCHAL_DCACHE_SIZE/XCHAL_DCACHE_LINESIZE	// a7 = number of lines in dcache
	movi	a3, XCHAL_DCACHE_SIZE-XCHAL_DCACHE_LINESIZE	// way index
	mov	a6, a5
	//movi	a8, -XCHAL_DCACHE_SETSIZE	// use if LDCT gives non-zero index bits
	movi	a10, (XCHAL_DCACHE_SIZE/XCHAL_DCACHE_WAYS) - 1

	loopgtz a7, 1f
	ldct	a7, a3				// a3 = cache tag for cache entry [a7]
	\instruction	a2, 0
	.begin schedule
	//extui	a9, a3, 0, XCHAL_DCACHE_SETWIDTH+XCHAL_DCACHE_LINEWIDTH
	and	a9, a3, a10
	addi	a3, a3, -XCHAL_DCACHE_LINESIZE
	.end schedule
	.begin schedule
	//and	a7, a7, a8	// uncomment if LDCT reports non-zero index bits
	maxu	a6, a6, a4	// a4 = low end of range
	minu	a2, a6, a5	// a5 = high end of range
	or	a6, a7, a9
	.end schedule
1:

	\instruction	a2, 0
	maxu	a6, a6, a4
	minu	a2, a6, a5
	\instruction	a2, 0
#if XCHAL_HAVE_PREFETCH
	wsr.prefctl a11		// restore prefetch
#endif
	isync_return_nop
	abi_return
#endif /* dcache supports hugerange */
// Jump to non-huge routine
7:	j.l	xthal_dcache_region_\name + ABI_ENTRY_MINSIZE, a4
	.size xthal_dcache_hugerange_\name, . - xthal_dcache_hugerange_\name
	.endm



	//  void  xthal_icache_hugerange_<name>(void *addr, unsigned size);
	//
	//  Invalidate icache entries for an arbitrary large
	//  virtual address range with a single scan of the icache.
	//  Assumes no address translation, i.e. virtual = physical.
	//
	//  a2 = ptr to range
	//  a3 = size of range
	//
	//  Note:  -128 is a valid immediate for ADDI, but +128 is not,
	//  and ADDI can relax to ADDMI for multiples of 256.  So scanning
	//  cache backwards (from end to start) allows all cache line sizes
	//  without creating an extra instruction for the ADDI.
	//
	.macro icache_hugefunc  name, instruction
	.text
	.align	4
	.type	xthal_icache_hugerange_\name,@function
	.global	xthal_icache_hugerange_\name
xthal_icache_hugerange_\name:
	abi_entry
#if (!defined(XCHAL_HAVE_NX) || XCHAL_HAVE_NX == 0) &&XCHAL_ICACHE_SIZE > 0 && \
	XCHAL_HAVE_ICACHE_TEST && XCHAL_HAVE_MINMAX && XCHAL_HAVE_LOOPS
	movi	a4, XCHAL_ICACHE_SIZE*2		// size at which to use huge algorithm
	movi	a7, -XCHAL_ICACHE_LINESIZE	// for rounding to cache line size
	bltu	a3, a4, 7f			// use normal (line-by-line hit) function
	add	a5, a3, a2			// a5 = end of range
	and	a4, a2, a7			// a4 = low end, rounded to containing cache line
	addi	a5, a5, XCHAL_ICACHE_LINESIZE-1
	and	a5, a5, a7			// a5 = high end, rounded to containing cache line
	movi	a7, XCHAL_ICACHE_SIZE/XCHAL_ICACHE_LINESIZE	// a7 = number of lines in dcache
	movi	a3, XCHAL_ICACHE_SIZE-XCHAL_ICACHE_LINESIZE	// way index
	mov	a6, a5
	//movi	a8, -XCHAL_ICACHE_SETSIZE	// use if LICT gives non-zero index bits
	movi	a10, (XCHAL_ICACHE_SIZE/XCHAL_ICACHE_WAYS) - 1

	loopgtz a7, 1f
	lict	a7, a3				// a3 = cache tag for cache entry [a7]
	\instruction	a2, 0
	.begin schedule
	//extui	a9, a3, 0, XCHAL_ICACHE_SETWIDTH+XCHAL_ICACHE_LINEWIDTH
	and	a9, a3, a10
	addi	a3, a3, -XCHAL_ICACHE_LINESIZE
	.end schedule
	.begin schedule
	//and	a7, a7, a8	// uncomment if LDCT reports non-zero index bits
	maxu	a6, a6, a4	// a4 = low end of range
	minu	a2, a6, a5	// a5 = high end of range
	or	a6, a7, a9
	.end schedule
1:

	\instruction	a2, 0
	maxu	a6, a6, a4
	minu	a2, a6, a5
	\instruction	a2, 0
	isync_return_nop
	abi_return
#endif /* icache supports hugerange */
7:	j.l	xthal_icache_region_\name + ABI_ENTRY_MINSIZE, a4
	.size xthal_icache_hugerange_\name, . - xthal_icache_hugerange_\name
	.endm




	.text

//----------------------------------------------------------------------
// Read CACHEATTR register
//----------------------------------------------------------------------

#if defined(__SPLIT__get_cacheattr) ||\
    defined(__SPLIT__get_cacheattr_nw)

//  unsigned xthal_get_cacheattr(void);

DECLFUNC(xthal_get_cacheattr)
DECLFUNC(xthal_get_dcacheattr)
# if XCHAL_HAVE_CACHEATTR	/* single CACHEATTR register used for both I and D */
DECLFUNC(xthal_get_icacheattr)
# endif
	abi_entry
	dcacheattr_get
	abi_return
	endfunc

#endif

#if defined(__SPLIT__get_icacheattr) ||\
    defined(__SPLIT__get_icacheattr_nw)

//  unsigned xthal_get_icacheattr(void);

# if !XCHAL_HAVE_CACHEATTR	/* possibly independent CACHEATTR states used for I and D */
DECLFUNC(xthal_get_icacheattr)
	abi_entry
	icacheattr_get
	abi_return
	endfunc
# endif

#endif /*split*/


//----------------------------------------------------------------------
//  Write CACHEATTR register, or equivalent.
//----------------------------------------------------------------------

/*
 *  Set CACHEATTR register in a safe manner.
 *
 *	void  xthal_set_cacheattr( unsigned new_cacheattr );
 *	void  xthal_set_icacheattr( unsigned new_cacheattr );
 *	void  xthal_set_dcacheattr( unsigned new_cacheattr );
 */

#if defined(__SPLIT__set_cacheattr) ||\
	defined(__SPLIT__set_cacheattr_nw)

# if XCHAL_HAVE_CACHEATTR	/* single CACHEATTR register used for both I and D accesses */
DECLFUNC(xthal_set_icacheattr)
DECLFUNC(xthal_set_dcacheattr)
# endif
DECLFUNC(xthal_set_cacheattr)
	abi_entry
	cacheattr_set
	abi_return
	endfunc

#endif /*split*/


#if XCHAL_HAVE_CACHEATTR

	/*
	 *  Already done above.
	 *
	 *  Since we can't enable/disable the icache and dcache independently,
	 *  and don't have a nice place to store a state which would enable
	 *  us to only enable them both when both have been requested to be
	 *  enabled, we simply enable both for any request to enable either,
	 *  and disable both for any request to disable either cache.
	 */

#elif XCHAL_HAVE_MIMIC_CACHEATTR || XCHAL_HAVE_XLT_CACHEATTR || (XCHAL_HAVE_PTP_MMU && XCHAL_HAVE_SPANNING_WAY)

# if defined(__SPLIT__set_icacheattr) \
	 || defined(__SPLIT__set_icacheattr_nw)

DECLFUNC(xthal_set_icacheattr)
	abi_entry
	icacheattr_set
	isync_return_nop
	abi_return
	endfunc

# endif

# if defined(__SPLIT__set_dcacheattr) \
	 || defined(__SPLIT__set_dcacheattr_nw)

DECLFUNC(xthal_set_dcacheattr)
	abi_entry
	dcacheattr_set
	abi_return
	endfunc

# endif /*split*/

#else /* full MMU (pre-v3): */

# if defined(__SPLIT__set_idcacheattr) \
	 || defined(__SPLIT__set_idcacheattr_nw)

//  These functions aren't applicable to arbitrary MMU configurations.
//  Do nothing in this case.

DECLFUNC(xthal_set_icacheattr)
DECLFUNC(xthal_set_dcacheattr)
	abi_entry
	abi_return
	endfunc

# endif /*split*/

#endif /* cacheattr/MMU type */


//----------------------------------------------------------------------
// Determine (guess) whether caches are "enabled"
//----------------------------------------------------------------------

/*
 *  There is no "cache enable" bit in the Xtensa architecture,
 *  but we can use CACHEATTR (if it or its equivalent exists)
 *  as an indication that caches have been enabled.
 */

#if XCHAL_HAVE_CACHEATTR

# if defined(__SPLIT__idcache_is_enabled) || \
     defined(__SPLIT__idcache_is_enabled_nw)

DECLFUNC(xthal_icache_is_enabled)
DECLFUNC(xthal_dcache_is_enabled)
	abi_entry
	cacheattr_is_enabled	2f
	movi	a2, 0
	abi_return
2:	movi	a2, 1
	abi_return
	endfunc

# endif /*split*/

#elif XCHAL_HAVE_MIMIC_CACHEATTR || XCHAL_HAVE_XLT_CACHEATTR

# if defined(__SPLIT__icache_is_enabled) || \
     defined(__SPLIT__icache_is_enabled_nw)

DECLFUNC(xthal_icache_is_enabled)
	abi_entry
	icacheattr_is_enabled	2f
	movi	a2, 0
	abi_return
2:	movi	a2, 1
	abi_return
	endfunc

# endif

# if defined(__SPLIT__dcache_is_enabled) || \
     defined(__SPLIT__dcache_is_enabled_nw)

DECLFUNC(xthal_dcache_is_enabled)
	abi_entry
	dcacheattr_is_enabled	2f
	movi	a2, 0
	abi_return
2:	movi	a2, 1
	abi_return
	endfunc

# endif /*split*/

#else

//  These functions aren't applicable to arbitrary MMU configurations.
//  Assume caches are enabled in this case (!).

# if defined(__SPLIT__idcache_is_enabled) || \
     defined(__SPLIT__idcache_is_enabled_nw)

DECLFUNC(xthal_icache_is_enabled)
DECLFUNC(xthal_dcache_is_enabled)
	abi_entry
	movi	a2, 1
	abi_return
	endfunc
# endif /*split*/

#endif



//----------------------------------------------------------------------
// invalidate the icache
//----------------------------------------------------------------------

#if defined(__SPLIT__icache_all_invalidate) || \
    defined(__SPLIT__icache_all_invalidate_nw)

// void xthal_icache_all_invalidate(void);

DECLFUNC(xthal_icache_all_invalidate)
	abi_entry
	icache_invalidate_all	a2, a3
	isync_return_nop
	abi_return
	endfunc

//----------------------------------------------------------------------
// invalidate the dcache
//----------------------------------------------------------------------

#endif

#if defined(__SPLIT__dcache_all_invalidate) || \
    defined(__SPLIT__dcache_all_invalidate_nw)

// void xthal_dcache_all_invalidate(void);

DECLFUNC(xthal_dcache_all_invalidate)
	abi_entry
	dcache_invalidate_all	a2, a3
	abi_return
	endfunc

//----------------------------------------------------------------------
// write dcache dirty data
//----------------------------------------------------------------------

#endif

#if defined(__SPLIT__dcache_all_writeback) || \
    defined(__SPLIT__dcache_all_writeback_nw)

// void xthal_dcache_all_writeback(void);

DECLFUNC(xthal_dcache_all_writeback)
	abi_entry
	dcache_writeback_all	a2, a3, a4
	abi_return
	endfunc

//----------------------------------------------------------------------
// write dcache dirty data and invalidate
//----------------------------------------------------------------------

#endif

#if defined(__SPLIT__dcache_all_writeback_inv) || \
    defined(__SPLIT__dcache_all_writeback_inv_nw)

// void xthal_dcache_all_writeback_inv(void);

DECLFUNC(xthal_dcache_all_writeback_inv)
	abi_entry
	dcache_writeback_inv_all	a2, a3, a4
	abi_return
	endfunc

//----------------------------------------------------------------------
// unlock instructions from icache
//----------------------------------------------------------------------

#endif

#if defined(__SPLIT__icache_all_unlock) || \
    defined(__SPLIT__icache_all_unlock_nw)

// void xthal_icache_all_unlock(void);

DECLFUNC(xthal_icache_all_unlock)
	abi_entry
	icache_unlock_all	a2, a3
	abi_return
	endfunc

//----------------------------------------------------------------------
// unlock data from dcache
//----------------------------------------------------------------------

#endif

#if defined(__SPLIT__dcache_all_unlock) || \
    defined(__SPLIT__dcache_all_unlock_nw)

// void xthal_dcache_all_unlock(void);

DECLFUNC(xthal_dcache_all_unlock)
	abi_entry
	dcache_unlock_all	a2, a3
	abi_return
	endfunc

//----------------------------------------------------------------------
// invalidate the address range in the icache
//----------------------------------------------------------------------

#endif

#if defined(__SPLIT__icache_region_invalidate) || \
    defined(__SPLIT__icache_region_invalidate_nw)

// void xthal_icache_region_invalidate( void *addr, unsigned size );

DECLFUNC(xthal_icache_region_invalidate)
	abi_entry
	icache_invalidate_region	a2, a3, a4
	isync_return_nop
	abi_return
	endfunc

#endif

#if defined(__SPLIT__icache_hugerange_invalidate)

// void xthal_icache_hugerange_invalidate( void *addr, unsigned size );
icache_hugefunc	invalidate,	ihi

#endif

#if defined(__SPLIT__icache_hugerange_unlock)

# if XCHAL_ICACHE_LINE_LOCKABLE
// void xthal_icache_hugerange_unlock( void *addr, unsigned size );
icache_hugefunc	unlock,		ihu
# endif

#endif

#if defined(__SPLIT__dcache_hugerange_invalidate)

// void xthal_dcache_hugerange_invalidate( void *addr, unsigned size );
dcache_hugefunc	invalidate,	dhi

#endif

#if defined(__SPLIT__dcache_hugerange_unlock)

# if XCHAL_DCACHE_LINE_LOCKABLE
// void xthal_dcache_hugerange_unlock( void *addr, unsigned size );
dcache_hugefunc	unlock,		dhu
# endif

#endif

#if defined(__SPLIT__dcache_hugerange_writeback)

// void xthal_dcache_hugerange_writeback( void *addr, unsigned size );
dcache_hugefunc	writeback,	dhwb

#endif

#if defined(__SPLIT__dcache_hugerange_writeback_inv)

// void xthal_dcache_hugerange_writeback_inv( void *addr, unsigned size );
dcache_hugefunc	writeback_inv,	dhwbi



//----------------------------------------------------------------------
// invalidate the address range in the dcache
//----------------------------------------------------------------------

#endif

#if defined(__SPLIT__dcache_region_invalidate) || \
    defined(__SPLIT__dcache_region_invalidate_nw)

// void xthal_dcache_region_invalidate( void *addr, unsigned size );

DECLFUNC(xthal_dcache_region_invalidate)
	abi_entry
	dcache_invalidate_region	a2, a3, a4
	abi_return
	endfunc

//----------------------------------------------------------------------
// write dcache region dirty data
//----------------------------------------------------------------------

#endif

#if defined(__SPLIT__dcache_region_writeback) || \
    defined(__SPLIT__dcache_region_writeback_nw)

// void xthal_dcache_region_writeback( void *addr, unsigned size );

DECLFUNC(xthal_dcache_region_writeback)
	abi_entry
	dcache_writeback_region		a2, a3, a4, a5
	abi_return
	endfunc

//----------------------------------------------------------------------
// write dcache region dirty data and invalidate
//----------------------------------------------------------------------

#endif

#if defined(__SPLIT__dcache_region_writeback_inv) || \
	defined(__SPLIT__dcache_region_writeback_inv_nw)

// void xthal_dcache_region_writeback_inv( void *addr, unsigned size );

DECLFUNC(xthal_dcache_region_writeback_inv)
	abi_entry
	dcache_writeback_inv_region	a2, a3, a4, a5
	abi_return
	endfunc

//----------------------------------------------------------------------
// lock instructions in icache region
//----------------------------------------------------------------------

#endif

#if defined(__SPLIT__icache_region_lock) || \
	defined(__SPLIT__icache_region_lock_nw)

// void xthal_icache_region_lock(void);

DECLFUNC(xthal_icache_region_lock)
	abi_entry
	icache_lock_region	a2, a3, a4
	abi_return
	endfunc

//----------------------------------------------------------------------
// lock data in dcache region
//----------------------------------------------------------------------

#endif

#if defined(__SPLIT__dcache_region_lock) || \
	defined(__SPLIT__dcache_region_lock_nw)

// void xthal_dcache_region_lock(void);

DECLFUNC(xthal_dcache_region_lock)
	abi_entry
	dcache_lock_region	a2, a3, a4
	abi_return
	endfunc

//----------------------------------------------------------------------
// unlock instructions from icache region
//----------------------------------------------------------------------

#endif

#if defined(__SPLIT__icache_region_unlock) || \
	defined(__SPLIT__icache_region_unlock_nw)

// void xthal_icache_region_unlock(void);

DECLFUNC(xthal_icache_region_unlock)
	abi_entry
	icache_unlock_region	a2, a3, a4
	abi_return
	endfunc

//----------------------------------------------------------------------
// unlock data from dcache region
//----------------------------------------------------------------------

#endif

#if defined(__SPLIT__dcache_region_unlock) || \
	defined(__SPLIT__dcache_region_unlock_nw)

// void xthal_dcache_region_unlock(void);

DECLFUNC(xthal_dcache_region_unlock)
	abi_entry
	dcache_unlock_region	a2, a3, a4
	abi_return
	endfunc


//----------------------------------------------------------------------
// invalidate single icache line
//----------------------------------------------------------------------

#endif

#if	defined(__SPLIT__icache_line_invalidate) || \
	defined(__SPLIT__icache_line_invalidate_nw)

// void xthal_icache_line_invalidate(void *addr);

DECLFUNC(xthal_icache_line_invalidate)
	abi_entry
	icache_invalidate_line	a2, 0
	isync_return_nop
	abi_return
	endfunc


//----------------------------------------------------------------------
// invalidate single dcache line
//----------------------------------------------------------------------

#endif

#if defined(__SPLIT__dcache_line_invalidate) || \
	defined(__SPLIT__dcache_line_invalidate_nw)

// void xthal_dcache_line_invalidate(void *addr);

DECLFUNC(xthal_dcache_line_invalidate)
	abi_entry
	dcache_invalidate_line	a2, 0
	abi_return
	endfunc

//----------------------------------------------------------------------
// write single dcache line dirty data
//----------------------------------------------------------------------

#endif

#if defined(__SPLIT__dcache_line_writeback) || \
	defined(__SPLIT__dcache_line_writeback_nw)

// void xthal_dcache_line_writeback(void *addr);

DECLFUNC(xthal_dcache_line_writeback)
	abi_entry
	dcache_writeback_line	a2, 0
	abi_return
	endfunc

//----------------------------------------------------------------------
// write single dcache line dirty data and invalidate
//----------------------------------------------------------------------

#endif

#if defined(__SPLIT__dcache_line_writeback_inv) || \
	defined(__SPLIT__dcache_line_writeback_inv_nw)

// void xthal_dcache_line_writeback_inv(void *addr);

DECLFUNC(xthal_dcache_line_writeback_inv)
	abi_entry
	dcache_writeback_inv_line	a2, 0
	abi_return
	endfunc

//----------------------------------------------------------------------
// lock instructions in icache line
//----------------------------------------------------------------------

#endif

#if defined(__SPLIT__icache_line_lock) || \
	defined(__SPLIT__icache_line_lock_nw)

// void xthal_icache_line_lock(void);

DECLFUNC(xthal_icache_line_lock)
	abi_entry
	icache_lock_line	a2, 0
	abi_return
	endfunc

//----------------------------------------------------------------------
// lock data in dcache line
//----------------------------------------------------------------------

#endif

#if defined(__SPLIT__dcache_line_lock) || \
	defined(__SPLIT__dcache_line_lock_nw)

// void xthal_dcache_line_lock(void);

DECLFUNC(xthal_dcache_line_lock)
	abi_entry
	dcache_lock_line	a2, 0
	abi_return
	endfunc

//----------------------------------------------------------------------
// unlock instructions from icache line
//----------------------------------------------------------------------

#endif

#if defined(__SPLIT__icache_line_unlock) || \
	defined(__SPLIT__icache_line_unlock_nw)

// void xthal_icache_line_unlock(void);

DECLFUNC(xthal_icache_line_unlock)
	abi_entry
	icache_unlock_line	a2, 0
	abi_return
	endfunc

//----------------------------------------------------------------------
// unlock data from dcache line
//----------------------------------------------------------------------

#endif

#if defined(__SPLIT__dcache_line_unlock) || \
	defined(__SPLIT__dcache_line_unlock_nw)

// void xthal_dcache_line_unlock(void);

DECLFUNC(xthal_dcache_line_unlock)
	abi_entry
	dcache_unlock_line	a2, 0
	abi_return
	endfunc

//----------------------------------------------------------------------
// sync icache and memory (???)
//----------------------------------------------------------------------

#endif

#if defined(__SPLIT__icache_sync) || \
	defined(__SPLIT__icache_sync_nw)

// void xthal_icache_sync(void);

DECLFUNC(xthal_icache_sync)
	abi_entry
	icache_sync	a2
	isync_return_nop
	abi_return
	endfunc

//----------------------------------------------------------------------
// sync dcache and memory (???)
//----------------------------------------------------------------------

#endif

#if defined(__SPLIT__dcache_sync) || \
	defined(__SPLIT__dcache_sync_nw)

// void xthal_dcache_sync(void);

DECLFUNC(xthal_dcache_sync)
	abi_entry
	dcache_sync	a2
	abi_return
	endfunc

//----------------------------------------------------------------------
// Get/Set icache number of ways enabled
//----------------------------------------------------------------------

#endif

#if defined (__SPLIT__icache_get_ways) || \
	defined (__SPLIT__icache_get_ways_nw)

// unsigned int xthal_icache_get_ways(void);

DECLFUNC(xthal_icache_get_ways)
	abi_entry
	icache_get_ways	a2
	abi_return
	endfunc

#endif

#if defined (__SPLIT__icache_set_ways) || \
	defined(__SPLIT__icache_set_ways_nw)

/// void xthal_icache_set_ways(unsigned int ways);

DECLFUNC(xthal_icache_set_ways)
	abi_entry
	icache_set_ways	a2 a3 a4
	abi_return
	endfunc

//----------------------------------------------------------------------
// Get/Set dcache number of ways enabled
//----------------------------------------------------------------------

#endif

#if defined (__SPLIT__dcache_get_ways) || \
	defined (__SPLIT__dcache_get_ways_nw)

// unsigned int xthal_dcache_get_ways(void);

DECLFUNC(xthal_dcache_get_ways)
	abi_entry
	dcache_get_ways a2
	abi_return
	endfunc

#endif

#if defined (__SPLIT__dcache_set_ways) || \
	defined (__SPLIT__dcache_set_ways_nw)

// void xthal_dcache_set_ways(unsigned int ways);

DECLFUNC(xthal_dcache_set_ways)
	abi_entry
	dcache_set_ways a2 a3 a4
	abi_return
	endfunc

//----------------------------------------------------------------------
// opt into and out of coherence
//----------------------------------------------------------------------

#endif

#if defined(__SPLIT__cache_coherence_on) || \
	defined(__SPLIT__cache_coherence_on_nw)

// The opt-in routine assumes cache was initialized at reset,
// so it's equivalent to the low-level coherence_on routine.

// void xthal_cache_coherence_optin(void)
// void xthal_cache_coherence_on(void)

DECLFUNC(xthal_cache_coherence_optin)
DECLFUNC(xthal_cache_coherence_on)
	abi_entry
	cache_coherence_on	a2, a3
	abi_return
	endfunc
	
#endif

#if defined(__SPLIT__cache_coherence_off) || \
	defined(__SPLIT__cache_coherence_off_nw)

// The coherence_off routines should not normally be called directly.
// Use the xthal_cache_coherence_optout() C routine instead
// (which first empties the cache).

// void xthal_cache_coherence_off

DECLFUNC(xthal_cache_coherence_off)
	abi_entry
	cache_coherence_off	a2, a3
	abi_return
	endfunc
	

//----------------------------------------------------------------------
// Control cache prefetch
//----------------------------------------------------------------------

#endif

#if defined(__SPLIT__set_cache_prefetch_long) || \
	defined(__SPLIT__set_cache_prefetch_long_nw)

# if XCHAL_HAVE_BE
#  define aH a2	/* msb word = prefctl mask */
#  define aL a3 /* lsb word = prefctl value */
# else
#  define aH a3	/* msb word = prefctl mask */
#  define aL a2 /* lsb word = prefctl value */
# endif

// Set cache prefetch state (-1=enable, 0=disable, and see XTHAL_*PREFETCH_*),
// and return previous one.
//
// int  xthal_set_cache_prefetch_long( unsigned long long );
//
DECLFUNC(xthal_set_cache_prefetch_long)
	abi_entry
# if XCHAL_HAVE_PREFETCH
	movi	a5, XCHAL_CACHE_PREFCTL_DEFAULT
	addi	a4, aL, 1	// does prefctl value aL == -1 ?
	moveqz	aL, a5, a4	// if yes (XTHAL_PREFETCH_ENABLE), set it to default
        movgez  a2, aL, aL      // if the high bit is not set, then we want to transfer the contents of aL to prefctl
				// so we move it to a2
	bgez	aL, 1f		// high bit set indicates masked update
	ssai	16		// 16-bit right shifts
	src	a5, aL, aH	// get 16-bit-swapped 32-bit value
	src	a5, a5, a5	// get 32-bit value (rotate by 16)
	rsr.prefctl a4
	src	a3, aH, aL	// get 32-bit mask
	or	a4, a4, a3	// set masked bits
	xor	a4, a4, a3	// clear masked bits
	and	a5, a5, a3	// only use masked bits
	or	a2, a4, a5	// combine masked bits
1:
#  if XCHAL_HW_MIN_VERSION <= XTENSA_HWVERSION_RC_2010_1    /* for erratum #325 */
	j 1f ; .align 8 ; 1: xsr.prefctl a2 ; isync	// ensure XSR.PREFCTL;ISYNC wholly within an icache line
#  else
	xsr.prefctl a2
#  endif
# else
	movi	a2, 0
# endif
	abi_return
	endfunc

//----------------------------------------------------------------------

#endif

#if defined(__SPLIT__set_cache_prefetch) || \
	defined(__SPLIT__set_cache_prefetch_nw)

// FOR BACKWARD COMPATIBILITY WITH PRE-RF RELEASE OBJECT CODE ONLY.
// Set cache prefetch state (-1=enable, 0=disable, and see the
//   definitions of XTHAL_*PREFETCH_* with only the lower 32 bits set),
// and return previous one.
// int  xthal_set_cache_prefetch( int )
//
DECLFUNC(xthal_set_cache_prefetch)
	abi_entry
# if XCHAL_HAVE_PREFETCH
	movi	a3, XCHAL_CACHE_PREFCTL_DEFAULT
	addi	a4, a2, 1	// does a2 == -1 ?
	moveqz	a2, a3, a4	// if yes (XTHAL_PREFETCH_ENABLE), set it to default
	bbci.l	a2, 31, 1f	// high bit set indicates masked update
	rsr.prefctl a4
	extui	a5, a2, 16, 15
	or	a4, a4, a5	// set masked bits
	xor	a4, a4, a5	// clear masked bits
	and	a2, a2, a5	// only use masked bits
	or	a2, a4, a2	// combine masked bits
1:
#  if XCHAL_HW_MIN_VERSION <= XTENSA_HWVERSION_RC_2010_1    /* for erratum #325 */
	j 1f ; .align 8 ; 1: xsr.prefctl a2 ; isync	// ensure XSR.PREFCTL;ISYNC wholly within an icache line
#  else
	xsr.prefctl a2
#  endif
# else
	movi	a2, 0
# endif
	abi_return
	endfunc

//----------------------------------------------------------------------

#endif

#if defined(__SPLIT__get_cache_prefetch) ||\
	defined(__SPLIT__get_cache_prefetch_nw)

// Return current cache prefetch state.
// int  xthal_get_cache_prefetch( void )
DECLFUNC(xthal_get_cache_prefetch)
	abi_entry
# if XCHAL_HAVE_PREFETCH
	rsr.prefctl a2
# else
	movi	a2, 0
# endif
	abi_return
	endfunc

//----------------------------------------------------------------------
// Misc configuration info
//----------------------------------------------------------------------
#endif

// Eventually these will move to their own file:
#if defined(__SPLIT__hw_configid0)
	.set	xthals_hw_configid0, XCHAL_HW_CONFIGID0
#endif

#if defined(__SPLIT__hw_configid1)
	.set	xthals_hw_configid1, XCHAL_HW_CONFIGID1
#endif

#if defined(__SPLIT__release_major)
	.set	xthals_release_major, XTHAL_RELEASE_MAJOR
#endif

#if defined(__SPLIT__release_minor)
	.set	xthals_release_minor, XTHAL_RELEASE_MINOR

#endif /*split*/

	.global	xthals_hw_configid0, xthals_hw_configid1
	.global	xthals_release_major, xthals_release_minor

//----------------------------------------------------------------------

