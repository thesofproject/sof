// 
// cache_asm.S - assembly language cache management routines
//
// $Id: //depot/rel/Eaglenest/Xtensa/OS/hal/cache_asm.S#5 $

// Copyright (c) 1999-2014 Cadence Design Systems, Inc.
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

/* For Call0 ABI, the xthal... and xthal..._nw versions are identical,
 * so we define both labels for the same function body.  The Makefile
 * does not define any of the __SPLIT__..._nw macros if Call0 ABI.
 * Use SYM() when we don't want .type information. */

#if defined (__XTENSA_CALL0_ABI__)
# define SYMBOL(x)  _SYMT(x); _SYM2(x ## _nw)
#else
# define SYMBOL(x)  _SYMT(x)
#endif



	.text

//----------------------------------------------------------------------
// Read CACHEATTR register
//----------------------------------------------------------------------

#if defined(__SPLIT__get_cacheattr)

//  unsigned xthal_get_cacheattr(void);

SYMBOL(xthal_get_cacheattr)
SYMBOL(xthal_get_dcacheattr)
# if XCHAL_HAVE_CACHEATTR	/* single CACHEATTR register used for both I and D */
SYMBOL(xthal_get_icacheattr)
# endif
	abi_entry
	dcacheattr_get
	abi_return
	endfunc

#endif
#if defined(__SPLIT__get_cacheattr_nw)

SYM(xthal_get_cacheattr_nw)
SYM(xthal_get_dcacheattr_nw)
# if XCHAL_HAVE_CACHEATTR	/* single CACHEATTR register used for both I and D */
SYM(xthal_get_icacheattr_nw)
# endif
	dcacheattr_get
	ret
	endfunc

#endif
#if defined(__SPLIT__get_icacheattr)

//  unsigned xthal_get_icacheattr(void);

# if !XCHAL_HAVE_CACHEATTR	/* possibly independent CACHEATTR states used for I and D */
SYMBOL(xthal_get_icacheattr)
	abi_entry
	icacheattr_get
	abi_return
	endfunc
# endif

#endif
#if defined(__SPLIT__get_icacheattr_nw)

# if !XCHAL_HAVE_CACHEATTR	/* possibly independent CACHEATTR states used for I and D */
SYM(xthal_get_icacheattr_nw)
	icacheattr_get
	ret
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

#if defined(__SPLIT__set_cacheattr)

# if XCHAL_HAVE_CACHEATTR	/* single CACHEATTR register used for both I and D accesses */
SYMBOL(xthal_set_icacheattr)
SYMBOL(xthal_set_dcacheattr)
# endif
SYMBOL(xthal_set_cacheattr)
	abi_entry
	cacheattr_set
	abi_return
	endfunc


#endif
#if defined(__SPLIT__set_cacheattr_nw)

#if XCHAL_HAVE_CACHEATTR	/* single CACHEATTR register used for both Instruction and Data accesses */
SYM(xthal_set_icacheattr_nw)
SYM(xthal_set_dcacheattr_nw)
#endif
SYM(xthal_set_cacheattr_nw)
	cacheattr_set
	ret
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

# if defined(__SPLIT__set_icacheattr)

SYMBOL(xthal_set_icacheattr)
	abi_entry
	icacheattr_set
	isync_retw_nop
	abi_return
	endfunc

#endif
#if defined(__SPLIT__set_icacheattr_nw)

SYM(xthal_set_icacheattr_nw)
	icacheattr_set
	ret
	endfunc

#endif
#if defined(__SPLIT__set_dcacheattr)

SYMBOL(xthal_set_dcacheattr)
	abi_entry
	dcacheattr_set
	abi_return
	endfunc

#endif
#if defined(__SPLIT__set_dcacheattr_nw)

	.align	4
SYM(xthal_set_dcacheattr_nw)
	dcacheattr_set
	ret
	endfunc

# endif /*split*/

#else /* full MMU (pre-v3): */

# if defined(__SPLIT__set_idcacheattr)

//  These functions aren't applicable to arbitrary MMU configurations.
//  Do nothing in this case.

SYMBOL(xthal_set_icacheattr)
SYMBOL(xthal_set_dcacheattr)
	abi_entry
	abi_return
	endfunc

#endif
#if defined(__SPLIT__set_idcacheattr_nw)

SYM(xthal_set_icacheattr_nw)
SYM(xthal_set_dcacheattr_nw)
	ret
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

# if defined(__SPLIT__idcache_is_enabled)

SYMBOL(xthal_icache_is_enabled)
SYMBOL(xthal_dcache_is_enabled)
	abi_entry
	cacheattr_is_enabled	2f
	movi	a2, 0
	abi_return
2:	movi	a2, 1
	abi_return
	endfunc

#endif
#if defined(__SPLIT__idcache_is_enabled_nw)

SYM(xthal_icache_is_enabled_nw)
SYM(xthal_dcache_is_enabled_nw)
	cacheattr_is_enabled	2f
	movi	a2, 0
	ret
2:	movi	a2, 1
	ret
	endfunc

# endif /*split*/

#elif XCHAL_HAVE_MIMIC_CACHEATTR || XCHAL_HAVE_XLT_CACHEATTR

# if defined(__SPLIT__icache_is_enabled)

SYMBOL(xthal_icache_is_enabled)
	abi_entry
	icacheattr_is_enabled	2f
	movi	a2, 0
	abi_return
2:	movi	a2, 1
	abi_return
	endfunc

#endif
#if defined(__SPLIT__icache_is_enabled_nw)

SYM(xthal_icache_is_enabled_nw)
	icacheattr_is_enabled	2f
	movi	a2, 0
	ret
2:	movi	a2, 1
	ret
	endfunc

#endif
#if defined(__SPLIT__dcache_is_enabled)

SYMBOL(xthal_dcache_is_enabled)
	abi_entry
	dcacheattr_is_enabled	2f
	movi	a2, 0
	abi_return
2:	movi	a2, 1
	abi_return
	endfunc

#endif
#if defined(__SPLIT__dcache_is_enabled_nw)

SYM(xthal_dcache_is_enabled_nw)
	dcacheattr_is_enabled	2f
	movi	a2, 0
	ret
2:	movi	a2, 1
	ret
	endfunc

# endif /*split*/

#else

//  These functions aren't applicable to arbitrary MMU configurations.
//  Assume caches are enabled in this case (!).

# if defined(__SPLIT__idcache_is_enabled)

SYMBOL(xthal_icache_is_enabled)
SYMBOL(xthal_dcache_is_enabled)
	abi_entry
	movi	a2, 1
	abi_return
	endfunc

#endif
#if defined(__SPLIT__idcache_is_enabled_nw)

SYM(xthal_icache_is_enabled_nw)
SYM(xthal_dcache_is_enabled_nw)
	movi	a2, 1
	ret
	endfunc

# endif /*split*/

#endif



//----------------------------------------------------------------------
// invalidate the icache
//----------------------------------------------------------------------

#if defined(__SPLIT__icache_all_invalidate)

// void xthal_icache_all_invalidate(void);

SYMBOL(xthal_icache_all_invalidate)
	abi_entry
	icache_invalidate_all	a2, a3
	isync_retw_nop
	abi_return
	endfunc

#endif
#if defined(__SPLIT__icache_all_invalidate_nw)

// void xthal_icache_all_invalidate_nw(void);

SYM(xthal_icache_all_invalidate_nw)
	icache_invalidate_all	a2, a3
	ret
	endfunc


//----------------------------------------------------------------------
// invalidate the dcache
//----------------------------------------------------------------------

#endif
#if defined(__SPLIT__dcache_all_invalidate)

// void xthal_dcache_all_invalidate(void);

SYMBOL(xthal_dcache_all_invalidate)
	abi_entry
	dcache_invalidate_all	a2, a3
	abi_return
	endfunc

#endif
#if defined(__SPLIT__dcache_all_invalidate_nw)

// void xthal_dcache_all_invalidate_nw(void);

SYM(xthal_dcache_all_invalidate_nw)
	dcache_invalidate_all	a2, a3
	ret
	endfunc


//----------------------------------------------------------------------
// write dcache dirty data
//----------------------------------------------------------------------

#endif
#if defined(__SPLIT__dcache_all_writeback)

// void xthal_dcache_all_writeback(void);

SYMBOL(xthal_dcache_all_writeback)
	abi_entry
	dcache_writeback_all	a2, a3
	abi_return
	endfunc

#endif
#if defined(__SPLIT__dcache_all_writeback_nw)

// void xthal_dcache_all_writeback_nw(void);

SYM(xthal_dcache_all_writeback_nw)
	dcache_writeback_all	a2, a3
	ret
	endfunc


//----------------------------------------------------------------------
// write dcache dirty data and invalidate
//----------------------------------------------------------------------

#endif
#if defined(__SPLIT__dcache_all_writeback_inv)

// void xthal_dcache_all_writeback_inv(void);

SYMBOL(xthal_dcache_all_writeback_inv)
	abi_entry
	dcache_writeback_inv_all	a2, a3
	abi_return
	endfunc

#endif
#if defined(__SPLIT__dcache_all_writeback_inv_nw)

// void xthal_dcache_all_writeback_inv_nw(void);

SYM(xthal_dcache_all_writeback_inv_nw)
	dcache_writeback_inv_all	a2, a3
	ret
	endfunc


//----------------------------------------------------------------------
// unlock instructions from icache
//----------------------------------------------------------------------

#endif
#if defined(__SPLIT__icache_all_unlock)

// void xthal_icache_all_unlock(void);

SYMBOL(xthal_icache_all_unlock)
	abi_entry
	icache_unlock_all	a2, a3
	abi_return
	endfunc

#endif
#if defined(__SPLIT__icache_all_unlock_nw)

// void xthal_icache_all_unlock_nw(void);

SYM(xthal_icache_all_unlock_nw)
	icache_unlock_all	a2, a3
	ret
	endfunc


//----------------------------------------------------------------------
// unlock data from dcache
//----------------------------------------------------------------------

#endif
#if defined(__SPLIT__dcache_all_unlock)

// void xthal_dcache_all_unlock(void);

SYMBOL(xthal_dcache_all_unlock)
	abi_entry
	dcache_unlock_all	a2, a3
	abi_return
	endfunc

#endif
#if defined(__SPLIT__dcache_all_unlock_nw)

// void xthal_dcache_all_unlock_nw(void);

SYM(xthal_dcache_all_unlock_nw)
	dcache_unlock_all	a2, a3
	ret
	endfunc


//----------------------------------------------------------------------
// invalidate the address range in the icache
//----------------------------------------------------------------------

#endif
#if defined(__SPLIT__icache_region_invalidate)

// void xthal_icache_region_invalidate( void *addr, unsigned size );

SYMBOL(xthal_icache_region_invalidate)
	abi_entry
	icache_invalidate_region	a2, a3, a4
	isync_retw_nop
	abi_return
	endfunc

#endif
#if defined(__SPLIT__icache_region_invalidate_nw)

// void xthal_icache_region_invalidate_nw( void *addr, unsigned size );

SYM(xthal_icache_region_invalidate_nw)
	icache_invalidate_region	a2, a3, a4
	ret
	endfunc


//----------------------------------------------------------------------
// invalidate the address range in the dcache
//----------------------------------------------------------------------

#endif
#if defined(__SPLIT__dcache_region_invalidate)

// void xthal_dcache_region_invalidate( void *addr, unsigned size );

SYMBOL(xthal_dcache_region_invalidate)
	abi_entry
	dcache_invalidate_region	a2, a3, a4
	abi_return
	endfunc

#endif
#if defined(__SPLIT__dcache_region_invalidate_nw)

// void xthal_dcache_region_invalidate_nw( void *addr, unsigned size );

SYM(xthal_dcache_region_invalidate_nw)
	dcache_invalidate_region	a2, a3, a4
	ret
	endfunc


//----------------------------------------------------------------------
// write dcache region dirty data
//----------------------------------------------------------------------

#endif
#if defined(__SPLIT__dcache_region_writeback)

// void xthal_dcache_region_writeback( void *addr, unsigned size );

SYMBOL(xthal_dcache_region_writeback)
	abi_entry
	dcache_writeback_region		a2, a3, a4
	abi_return
	endfunc

#endif
#if defined(__SPLIT__dcache_region_writeback_nw)

// void xthal_dcache_region_writeback_nw( void *addr, unsigned size );

SYM(xthal_dcache_region_writeback_nw)
	dcache_writeback_region		a2, a3, a4
	ret
	endfunc


//----------------------------------------------------------------------
// write dcache region dirty data and invalidate
//----------------------------------------------------------------------

#endif
#if defined(__SPLIT__dcache_region_writeback_inv)

// void xthal_dcache_region_writeback_inv( void *addr, unsigned size );

SYMBOL(xthal_dcache_region_writeback_inv)
	abi_entry
	dcache_writeback_inv_region	a2, a3, a4
	abi_return
	endfunc

#endif
#if defined(__SPLIT__dcache_region_writeback_inv_nw)

// void xthal_dcache_region_writeback_inv_nw( void *addr, unsigned size );

SYM(xthal_dcache_region_writeback_inv_nw)
	dcache_writeback_inv_region	a2, a3, a4
	ret
	endfunc


//----------------------------------------------------------------------
// lock instructions in icache region
//----------------------------------------------------------------------

#endif
#if defined(__SPLIT__icache_region_lock)

// void xthal_icache_region_lock(void);

SYMBOL(xthal_icache_region_lock)
	abi_entry
	icache_lock_region	a2, a3, a4
	abi_return
	endfunc

#endif
#if defined(__SPLIT__icache_region_lock_nw)

// void xthal_icache_region_lock_nw(void);

SYM(xthal_icache_region_lock_nw)
	icache_lock_region	a2, a3, a4
	ret
	endfunc


//----------------------------------------------------------------------
// lock data in dcache region
//----------------------------------------------------------------------

#endif
#if defined(__SPLIT__dcache_region_lock)

// void xthal_dcache_region_lock(void);

SYMBOL(xthal_dcache_region_lock)
	abi_entry
	dcache_lock_region	a2, a3, a4
	abi_return
	endfunc

#endif
#if defined(__SPLIT__dcache_region_lock_nw)

// void xthal_dcache_region_lock_nw(void);

SYM(xthal_dcache_region_lock_nw)
	dcache_lock_region	a2, a3, a4
	ret
	endfunc


//----------------------------------------------------------------------
// unlock instructions from icache region
//----------------------------------------------------------------------

#endif
#if defined(__SPLIT__icache_region_unlock)

// void xthal_icache_region_unlock(void);

SYMBOL(xthal_icache_region_unlock)
	abi_entry
	icache_unlock_region	a2, a3, a4
	abi_return
	endfunc

#endif
#if defined(__SPLIT__icache_region_unlock_nw)

// void xthal_icache_region_unlock_nw(void);

SYM(xthal_icache_region_unlock_nw)
	icache_unlock_region	a2, a3, a4
	ret
	endfunc


//----------------------------------------------------------------------
// unlock data from dcache region
//----------------------------------------------------------------------

#endif
#if defined(__SPLIT__dcache_region_unlock)

// void xthal_dcache_region_unlock(void);

SYMBOL(xthal_dcache_region_unlock)
	abi_entry
	dcache_unlock_region	a2, a3, a4
	abi_return
	endfunc

#endif
#if defined(__SPLIT__dcache_region_unlock_nw)

// void xthal_dcache_region_unlock_nw(void);

SYM(xthal_dcache_region_unlock_nw)
	dcache_unlock_region	a2, a3, a4
	ret
	endfunc


//----------------------------------------------------------------------
// invalidate single icache line
//----------------------------------------------------------------------

#endif
#if defined(__SPLIT__icache_line_invalidate)

// void xthal_icache_line_invalidate(void *addr);

SYMBOL(xthal_icache_line_invalidate)
	abi_entry
	icache_invalidate_line	a2, 0
	isync_retw_nop
	abi_return
	endfunc

#endif
#if defined(__SPLIT__icache_line_invalidate_nw)

// void xthal_icache_line_invalidate_nw(void *addr);

SYM(xthal_icache_line_invalidate_nw)
	icache_invalidate_line	a2, 0
	ret
	endfunc


//----------------------------------------------------------------------
// invalidate single dcache line
//----------------------------------------------------------------------

#endif
#if defined(__SPLIT__dcache_line_invalidate)

// void xthal_dcache_line_invalidate(void *addr);

SYMBOL(xthal_dcache_line_invalidate)
	abi_entry
	dcache_invalidate_line	a2, 0
	abi_return
	endfunc

#endif
#if defined(__SPLIT__dcache_line_invalidate_nw)

// void xthal_dcache_line_invalidate_nw(void *addr);

SYM(xthal_dcache_line_invalidate_nw)
	dcache_invalidate_line	a2, 0
	ret
	endfunc


//----------------------------------------------------------------------
// write single dcache line dirty data
//----------------------------------------------------------------------

#endif
#if defined(__SPLIT__dcache_line_writeback)

// void xthal_dcache_line_writeback(void *addr);

SYMBOL(xthal_dcache_line_writeback)
	abi_entry
	dcache_writeback_line	a2, 0
	abi_return
	endfunc

#endif
#if defined(__SPLIT__dcache_line_writeback_nw)

// void xthal_dcache_line_writeback_nw(void *addr);

SYM(xthal_dcache_line_writeback_nw)
	dcache_writeback_line	a2, 0
	ret
	endfunc


//----------------------------------------------------------------------
// write single dcache line dirty data and invalidate
//----------------------------------------------------------------------

#endif
#if defined(__SPLIT__dcache_line_writeback_inv)

// void xthal_dcache_line_writeback_inv(void *addr);

SYMBOL(xthal_dcache_line_writeback_inv)
	abi_entry
	dcache_writeback_inv_line	a2, 0
	abi_return
	endfunc

#endif
#if defined(__SPLIT__dcache_line_writeback_inv_nw)

// void xthal_dcache_line_writeback_inv_nw(void *addr);

SYM(xthal_dcache_line_writeback_inv_nw)
	dcache_writeback_inv_line	a2, 0
	ret
	endfunc


//----------------------------------------------------------------------
// lock instructions in icache line
//----------------------------------------------------------------------

#endif
#if defined(__SPLIT__icache_line_lock)

// void xthal_icache_line_lock(void);

SYMBOL(xthal_icache_line_lock)
	abi_entry
	icache_lock_line	a2, 0
	abi_return
	endfunc

#endif
#if defined(__SPLIT__icache_line_lock_nw)

// void xthal_icache_line_lock_nw(void);

SYM(xthal_icache_line_lock_nw)
	icache_lock_line	a2, 0
	ret
	endfunc


//----------------------------------------------------------------------
// lock data in dcache line
//----------------------------------------------------------------------

#endif
#if defined(__SPLIT__dcache_line_lock)

// void xthal_dcache_line_lock(void);

SYMBOL(xthal_dcache_line_lock)
	abi_entry
	dcache_lock_line	a2, 0
	abi_return
	endfunc

#endif
#if defined(__SPLIT__dcache_line_lock_nw)

// void xthal_dcache_line_lock_nw(void);

SYM(xthal_dcache_line_lock_nw)
	dcache_lock_line	a2, 0
	ret
	endfunc


//----------------------------------------------------------------------
// unlock instructions from icache line
//----------------------------------------------------------------------

#endif
#if defined(__SPLIT__icache_line_unlock)

// void xthal_icache_line_unlock(void);

SYMBOL(xthal_icache_line_unlock)
	abi_entry
	icache_unlock_line	a2, 0
	abi_return
	endfunc

#endif
#if defined(__SPLIT__icache_line_unlock_nw)

// void xthal_icache_line_unlock_nw(void);

SYM(xthal_icache_line_unlock_nw)
	icache_unlock_line	a2, 0
	ret
	endfunc


//----------------------------------------------------------------------
// unlock data from dcache line
//----------------------------------------------------------------------

#endif
#if defined(__SPLIT__dcache_line_unlock)

// void xthal_dcache_line_unlock(void);

SYMBOL(xthal_dcache_line_unlock)
	abi_entry
	dcache_unlock_line	a2, 0
	abi_return
	endfunc

#endif
#if defined(__SPLIT__dcache_line_unlock_nw)

// void xthal_dcache_line_unlock_nw(void);

SYM(xthal_dcache_line_unlock_nw)
	dcache_unlock_line	a2, 0
	ret
	endfunc


//----------------------------------------------------------------------
// sync icache and memory (???)
//----------------------------------------------------------------------

#endif
#if defined(__SPLIT__icache_sync)

// void xthal_icache_sync(void);

SYMBOL(xthal_icache_sync)
	abi_entry
	icache_sync	a2
	isync_retw_nop
	abi_return
	endfunc

#endif
#if defined(__SPLIT__icache_sync_nw)

// void xthal_icache_sync_nw(void);

SYM(xthal_icache_sync_nw)
	icache_sync	a2
	ret
	endfunc


//----------------------------------------------------------------------
// sync dcache and memory (???)
//----------------------------------------------------------------------

#endif
#if defined(__SPLIT__dcache_sync)

// void xthal_dcache_sync(void);

SYMBOL(xthal_dcache_sync)
	abi_entry
	dcache_sync	a2
	abi_return
	endfunc

#endif
#if defined(__SPLIT__dcache_sync_nw)

// void xthal_dcache_sync_nw(void)

SYM(xthal_dcache_sync_nw)
	dcache_sync	a2
	ret
	endfunc

//----------------------------------------------------------------------
// Get/Set icache number of ways enabled
//----------------------------------------------------------------------

#elif defined (__SPLIT__icache_get_ways)

// unsigned int xthal_icache_get_ways(void);

SYMBOL(xthal_icache_get_ways)
	abi_entry
	icache_get_ways	a2
	abi_return
	endfunc

#elif defined (__SPLIT__icache_set_ways)

// void xthal_icache_set_ways(unsigned int ways);

SYMBOL(xthal_icache_set_ways)
	abi_entry
	icache_set_ways	a2 a3 a4
	abi_return
	endfunc

#elif defined (__SPLIT__icache_get_ways_nw)

// unsigned int xthal_icache_get_ways_nw(void);

SYM(xthal_icache_get_ways_nw)
	icache_get_ways	a2
	ret
	endfunc

#elif defined (__SPLIT__icache_set_ways_nw)

// void xthal_icache_set_ways_nw(unsigned int ways);

SYM(xthal_icache_set_ways_nw)
	icache_set_ways a2 a3 a4
	ret
	endfunc

//----------------------------------------------------------------------
// Get/Set dcache number of ways enabled
//----------------------------------------------------------------------

#elif defined (__SPLIT__dcache_get_ways)

// unsigned int xthal_dcache_get_ways(void);

SYMBOL(xthal_dcache_get_ways)
	abi_entry
	dcache_get_ways a2
	abi_return
	endfunc

#elif defined (__SPLIT__dcache_set_ways)

// void xthal_dcache_set_ways(unsigned int ways);

SYMBOL(xthal_dcache_set_ways)
	abi_entry
	dcache_set_ways a2 a3 a4
	abi_return
	endfunc

#elif defined (__SPLIT__dcache_get_ways_nw)

// unsigned int xthal_dcache_get_ways_nw(void);

SYM(xthal_dcache_get_ways_nw)
	dcache_get_ways a2
	ret
	endfunc

#elif defined (__SPLIT__dcache_set_ways_nw)

// void xthal_dcache_set_ways_nw(unsigned int ways);

SYM(xthal_dcache_set_ways_nw)
	dcache_set_ways a2 a3 a4
	ret
	endfunc

//----------------------------------------------------------------------
// opt into and out of coherence
//----------------------------------------------------------------------

#endif
#if defined(__SPLIT__cache_coherence_on)

// The opt-in routine assumes cache was initialized at reset,
// so it's equivalent to the low-level coherence_on routine.

// void xthal_cache_coherence_optin(void)
// void xthal_cache_coherence_on(void)

SYMBOL(xthal_cache_coherence_optin)
SYMBOL(xthal_cache_coherence_on)
	abi_entry
	cache_coherence_on	a2, a3
	abi_return
	endfunc
	
#endif
#if defined(__SPLIT__cache_coherence_on_nw)

// void xthal_cache_coherence_on_nw(void)

SYM(xthal_cache_coherence_on_nw)
	cache_coherence_on	a2, a3
	ret
	endfunc
	
#endif
#if defined(__SPLIT__cache_coherence_off)

// The coherence_off routines should not normally be called directly.
// Use the xthal_cache_coherence_optout() C routine instead
// (which first empties the cache).

// void xthal_cache_coherence_off

SYMBOL(xthal_cache_coherence_off)
	abi_entry
	cache_coherence_off	a2, a3
	abi_return
	endfunc
	
#endif
#if defined(__SPLIT__cache_coherence_off_nw)

// void xthal_cache_coherence_on_nw

SYM(xthal_cache_coherence_off_nw)
	cache_coherence_off	a2, a3
	ret
	endfunc

//----------------------------------------------------------------------
// Control cache prefetch
//----------------------------------------------------------------------

#endif
#if defined(__SPLIT__set_cache_prefetch_long)

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
SYMBOL(xthal_set_cache_prefetch_long)
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
#if defined(__SPLIT__set_cache_prefetch_long_nw)

# if XCHAL_HAVE_BE
#  define aH a2	/* msb word = prefctl mask */
#  define aL a3 /* lsb word = prefctl value */
# else
#  define aH a3	/* msb word = prefctl mask */
#  define aL a2 /* lsb word = prefctl value */
# endif

// int  xthal_set_cache_prefetch_long_nw( unsigned long long )
SYM(xthal_set_cache_prefetch_long_nw)
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
	ret
	endfunc

//----------------------------------------------------------------------

#endif
#if defined(__SPLIT__set_cache_prefetch)

// FOR BACKWARD COMPATIBILITY WITH PRE-RF RELEASE OBJECT CODE ONLY.
// Set cache prefetch state (-1=enable, 0=disable, and see the
//   definitions of XTHAL_*PREFETCH_* with only the lower 32 bits set),
// and return previous one.
// int  xthal_set_cache_prefetch( int )
//
SYMBOL(xthal_set_cache_prefetch)
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
#if defined(__SPLIT__set_cache_prefetch_nw)

// FOR BACKWARD COMPATIBILITY WITH PRE-RF RELEASE OBJECT CODE ONLY.
// int  xthal_set_cache_prefetch_nw( int )
SYM(xthal_set_cache_prefetch_nw)
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
	ret
	endfunc

//----------------------------------------------------------------------

#endif
#if defined(__SPLIT__get_cache_prefetch)

// Return current cache prefetch state.
// int  xthal_get_cache_prefetch( void )
SYMBOL(xthal_get_cache_prefetch)
	abi_entry
# if XCHAL_HAVE_PREFETCH
	rsr.prefctl a2
# else
	movi	a2, 0
# endif
	abi_return
	endfunc

#endif
#if defined(__SPLIT__get_cache_prefetch_nw)

// int  xthal_get_cache_prefetch_nw( void )

SYM(xthal_get_cache_prefetch_nw)
# if XCHAL_HAVE_PREFETCH
	rsr.prefctl a2
# else
	movi	a2, 0
# endif
	ret
	endfunc

//----------------------------------------------------------------------
// Misc configuration info
//----------------------------------------------------------------------
	
// Eventually these will move to their own file:
#endif
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

