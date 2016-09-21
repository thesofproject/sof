/*  attribute.c - Cache attribute (memory access mode) related functions  */

/* $Id: //depot/rel/Eaglenest/Xtensa/OS/hal/attribute.c#1 $ */

/*
 * Copyright (c) 2004-2009 Tensilica Inc.
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

#include <xtensa/config/core.h>



/*
 *  Set the "cache attribute" (encoded memory access modes)
 *  of the region of memory specified by <vaddr> and <size>.
 *
 *  This function is only supported on processor configurations
 *  with region protection (or XEA1).  It has no effect on
 *  a processor configured with an MMU (with autorefill).
 *
 *  SPECIFYING THE MEMORY REGION
 *  The full (4 GB) address space may be specified with an
 *  address of zero and a size of 0xFFFFFFFF (or -1);
 *  in fact whenever <vaddr>+<size> equal 0xFFFFFFFF, <size>
 *  is interpreted as one byte greater than that specified.
 *
 *  If the specified memory range exactly covers a series
 *  of consecutive 512 MB regions, the cache attributes of
 *  these regions are updated with the requested attribute.
 *  If this is not the case, e.g. if either or both the
 *  start and end of the range only partially cover a 512 MB
 *  region, one of three results are possible:
 *
 *	1.  By default, the cache attribute of all regions
 *	    covered, even just partially, is changed to
 *	    the requested attribute.
 *
 *	2.  If the XTHAL_CAFLAG_EXACT flag is specified,
 *	    a non-zero error code is returned.
 *
 *	3.  If the XTHAL_CAFLAG_NO_PARTIAL flag is specified
 *	    (but not the EXACT flag), only regions fully
 *	    covered by the specified range are updated with
 *	    the requested attribute.
 *
 *  WRITEBACK CACHE HANDLING
 *  This function automatically writes back dirty data when
 *  switching a region from writeback mode to a non-writeback mode.
 *  This writeback is done safely, ie. by first switching to writethrough
 *  mode, then invoking xthal_dcache_all_writeback(), then switching to
 *  the selected <cattr> mode.  Such a sequence is necessary to ensure
 *  there is no longer any dirty data in the memory region by the time
 *  this function returns, even in the presence of interrupts, speculation, etc.
 *  This avoids memory coherency problems when switching from writeback
 *  to bypass mode (in bypass mode, loads go directly to memory, ignoring
 *  any dirty data in the cache; also, such dirty data can still be castout
 *  due to seemingly unrelated stores).
 *  This automatic write-back can be disabled using the XTHAL_CAFLAG_NO_AUTO_WB flag.
 *
 *  CACHE DISABLE THEN ENABLE HANDLING
 *  To avoid cache coherency issues when the cache is disabled, then
 *  memory is modified, then then cache is re-enabled (thus making
 *  visible stale cache entries), this function automatically
 *  invalidates the cache when any region switches to bypass mode.
 *  For efficiency, the entire cache is invalidated -- this is done
 *  using writeback-invalidate operations to ensure coherency even
 *  when other regions still have write-back caches enabled.
 *  This automatic invalidate can be disabled using the XTHAL_CAFLAG_NO_AUTO_INV flag.
 *
 *  Parameters:
 *	vaddr	starting virtual address of region of memory
 *
 *	size	number of bytes in region of memory
 *		(see above, SPECIFYING THE MEMORY REGION)
 *
 *	cattr	cache attribute (encoded);
 *		typically taken from compile-time HAL constants
 *		XCHAL_CA_{BYPASS, WRITETHRU, WRITEBACK[_NOALLOC], ILLEGAL}
 *		(defined in <xtensa/config/core.h>);
 *		in XEA1, this corresponds to the value of a nibble
 *		in the CACHEATTR register;
 *		in XEA2, this corresponds to the value of the
 *		cache attribute (CA) field of each TLB entry
 *
 *	flags	bitwise combination of flags XTHAL_CAFLAG_*
 *		(see xtensa/hal.h for brief description of each flag);
 *		(see also various descriptions above);
 *
 *		The XTHAL_CAFLAG_EXPAND flag prevents attribute changes
 *		to regions whose current cache attribute already provide
 *		greater access than the requested attribute.
 *		This ensures access to each region can only "expand",
 *		and thus continue to work correctly in most instances,
 *		possibly at the expense of performance.  This helps
 *		make this flag safer to use in a variety of situations.
 *		For the purposes of this flag, cache attributes are
 *		ordered (in "expansion" order, from least to greatest
 *		access) as follows:
 *			XCHAL_CA_ILLEGAL	no access allowed
 *			(various special and reserved attributes)
 *			XCHAL_CA_WRITEBACK	writeback cached
 *			XCHAL_CA_WRITEBACK_NOALLOC writeback no-write-alloc
 *			XCHAL_CA_WRITETHRU	writethrough cached
 *			XCHAL_CA_BYPASS		bypass (uncached)
 *		This is consistent with requirements of certain
 *		devices that no caches be used, or in certain cases
 *		that writethrough caching is allowed but not writeback.
 *		Thus, bypass mode is assumed to work for most/all types
 *		of devices and memories (albeit at reduced performance
 *		compared to cached modes), and is ordered as providing
 *		greatest access (to most devices).
 *		Thus, this XTHAL_CAFLAG_EXPAND flag has no effect when
 *		requesting the XCHAL_CA_BYPASS attribute (one can always
 *		expand to bypass mode).  And at the other extreme,
 *		no action is ever taken by this function when specifying
 *		both the XTHAL_CAFLAG_EXPAND flag and the XCHAL_CA_ILLEGAL
 *		cache attribute.
 *
 *  Returns:
 *	0	successful, or size is zero
 *	-1	XTHAL_CAFLAG_NO_PARTIAL flag specified and address range
 *		is valid with a non-zero size, however no 512 MB region (or page)
 *		is completely covered by the range
 *	-2	XTHAL_CAFLAG_EXACT flag specified, and address range does
 *		not exactly specify a 512 MB region (or page)
 *	-3	invalid address range specified (wraps around the end of memory)
 *	-4	function not supported in this processor configuration
 */
int  xthal_set_region_attribute( void *vaddr, unsigned size, unsigned cattr, unsigned flags )
{
#if XCHAL_HAVE_PTP_MMU && !XCHAL_HAVE_SPANNING_WAY
    return -4;		/* full MMU not supported */
#else
/*  These cache attribute encodings are valid for XEA1 and region protection only:  */
# if XCHAL_HAVE_PTP_MMU
#  define CA_BYPASS		XCHAL_CA_BYPASS
#  define CA_WRITETHRU		XCHAL_CA_WRITETHRU
#  define CA_WRITEBACK		XCHAL_CA_WRITEBACK
#  define CA_WRITEBACK_NOALLOC	XCHAL_CA_WRITEBACK_NOALLOC
#  define CA_ILLEGAL		XCHAL_CA_ILLEGAL
# else
/*  Hardcode these, because they get remapped when caches or writeback not configured:  */
#  define CA_BYPASS		2
#  define CA_WRITETHRU		1
#  define CA_WRITEBACK		4
#  define CA_WRITEBACK_NOALLOC	5
#  define CA_ILLEGAL		15
# endif
# define CA_MASK	0xF	/*((1L<<XCHAL_CA_BITS)-1)*/	/* mask of cache attribute bits */

    unsigned start_region, start_offset, end_vaddr, end_region, end_offset;
    unsigned cacheattr, cachewrtr, i, disabled_cache = 0;

    if (size == 0)
	return 0;
    end_vaddr = (unsigned)vaddr + size - 1;
    if (end_vaddr < (unsigned)vaddr)
	return -3;		/* address overflow/wraparound error */
    if (end_vaddr == 0xFFFFFFFE /*&& (unsigned)vaddr == 0*/ )
	end_vaddr = 0xFFFFFFFF;	/* allow specifying 4 GB */
    start_region = ((unsigned)vaddr >> 29);
    start_offset = ((unsigned)vaddr & 0x1FFFFFFF);
    end_region = (end_vaddr >> 29);
    end_offset = ((end_vaddr+1) & 0x1FFFFFFF);
    if (flags & XTHAL_CAFLAG_EXACT) {
	if (start_offset != 0 || end_offset != 0)
	    return -2;		/* not an exact-sized range */
    } else if (flags & XTHAL_CAFLAG_NO_PARTIAL) {
	if (start_offset != 0)
	    start_region++;
	if (end_offset != 0)
	    end_region--;
	if (start_region > end_region)
	    return -1;		/* nothing fully covered by specified range */
    }
    cacheattr = cachewrtr = xthal_get_cacheattr();
    cattr &= CA_MASK;
# if XCHAL_ICACHE_SIZE == 0 && XCHAL_DCACHE_SIZE == 0
    if (cattr == CA_WRITETHRU || cattr == CA_WRITEBACK || cattr == CA_WRITEBACK_NOALLOC)
	cattr = CA_BYPASS;	/* no caches configured, only do protection */
# elif XCHAL_DCACHE_IS_WRITEBACK == 0
    if (cattr == CA_WRITEBACK || cattr == CA_WRITEBACK_NOALLOC)
	cattr = CA_WRITETHRU;	/* no writeback configured for data cache */
# endif
    for (i = start_region; i <= end_region; i++) {
	unsigned sh = (i << 2);		/* bit offset of nibble for region i */
	unsigned oldattr = ((cacheattr >> sh) & CA_MASK);
	unsigned newattr = cattr;
	if (flags & XTHAL_CAFLAG_EXPAND) {
	    /*  This array determines whether a cache attribute can be changed
	     *  from <a> to <b> with the EXPAND flag; an attribute's "pri"
	     *  value (from this array) can only monotonically increase:  */
	    const static signed char _Xthal_ca_pri[16] = {[CA_ILLEGAL] = -1,
			[CA_WRITEBACK] = 3, [CA_WRITEBACK_NOALLOC] = 3, [CA_WRITETHRU] = 4, [CA_BYPASS] = 9 };
	    if (_Xthal_ca_pri[newattr] < _Xthal_ca_pri[oldattr])
		newattr = oldattr;	/* avoid going to lesser access */
	}
	if (newattr == CA_BYPASS && oldattr != CA_BYPASS)
	    disabled_cache = 1;		/* we're disabling the cache for some region */
# if XCHAL_DCACHE_IS_WRITEBACK
	{
	unsigned tmpattr = newattr;
	if ((oldattr == CA_WRITEBACK || oldattr == CA_WRITEBACK_NOALLOC)
	     && newattr != CA_WRITEBACK && newattr != CA_WRITEBACK_NOALLOC)	/* leaving writeback mode? */
	    tmpattr = CA_WRITETHRU;				/* leave it safely! */
	cachewrtr = ((cachewrtr & ~(CA_MASK << sh)) | (tmpattr << sh));
	}
# endif
	cacheattr = ((cacheattr & ~(CA_MASK << sh)) | (newattr << sh));
    }
# if XCHAL_DCACHE_IS_WRITEBACK
    if (cacheattr != cachewrtr		/* need to leave writeback safely? */
	&& (flags & XTHAL_CAFLAG_NO_AUTO_WB) == 0) {
	xthal_set_cacheattr(cachewrtr);	/* set to writethru first, to safely writeback any dirty data */
	xthal_dcache_all_writeback();	/* much quicker than scanning entire 512MB region(s) */
    }
# endif
    xthal_set_cacheattr(cacheattr);
    /*  After disabling the cache, invalidate cache entries
     *  to avoid coherency issues when later re-enabling it:  */
    if (disabled_cache && (flags & XTHAL_CAFLAG_NO_AUTO_INV) == 0) {
	xthal_dcache_all_writeback_inv();	/* we might touch regions of memory still enabled write-back,
						   so must use writeback-invalidate, not just invalidate */
	xthal_icache_all_invalidate();
    }
    return( 0 );
#endif /* !(XCHAL_HAVE_PTP_MMU && !XCHAL_HAVE_SPANNING_WAY) */
}

