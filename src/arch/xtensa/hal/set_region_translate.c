/*
 * Copyright (c) 2004-2014 Tensilica Inc.
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

#if XCHAL_HAVE_XEA2
/*
 * C-stubs to issue the tlb related instructions (with dsync and isync's if needed).
 *
 */
static inline void write_dtlb_entry(unsigned vpn_way, unsigned ppn_ca) {
	__asm__ __volatile__("wdtlb  %1, %0; dsync\n\t"
			: : "r" (vpn_way), "r" (ppn_ca) );
}

static inline void write_itlb_entry(unsigned vpn_way, unsigned ppn_ca) {
	__asm__ __volatile__("witlb  %1, %0; isync\n\t"
			: : "r" (vpn_way), "r" (ppn_ca) );
}

static inline unsigned read_dtlb1_entry(unsigned addr) {
	unsigned long tmp;
	__asm__ __volatile__("rdtlb1  %0, %1\n\t"
			: "=a" (tmp)
			: "a" (addr));
	return tmp;
}

static inline unsigned read_itlb1_entry(unsigned addr) {
	unsigned long tmp;
	__asm__ __volatile__("ritlb1  %0, %1\n\t"
			: "=a" (tmp)
			: "a" (addr));
	return tmp;
}

static inline unsigned probe_dtlb(unsigned addr) {
	unsigned long tmp;
	__asm__ __volatile__("pdtlb  %0, %1\n\t"
			: "=a" (tmp)
			: "a" (addr));
	return tmp;
}

static inline unsigned probe_itlb(unsigned addr) {
	unsigned long tmp;
	__asm__ __volatile__("pitlb  %0, %1\n\t"
			: "=a" (tmp)
			: "a" (addr));
	return tmp;
}

static inline void invalidate_dtlb_entry(unsigned addr) {
	__asm__ __volatile__("idtlb  %0; dsync \n\t"
			: : "a" (addr));
}

static inline void invalidate_itlb_entry(unsigned addr) {
	__asm__ __volatile__("iitlb  %0 ; isync\n\t"
			: : "a" (addr));
}

static inline unsigned read_dtlbcfg() {
	unsigned long tmp;
	__asm__ __volatile__("rsr.dtlbcfg %0\n\t"
			: "=a" (tmp));
	return tmp;
}

static inline unsigned read_itlbcfg() {
	unsigned long tmp;
	__asm__ __volatile__("rsr.itlbcfg %0\n\t"
			: "=a" (tmp));
	return tmp;
}

#endif

/*
 *  xthal_set_region_translation_raw is a quick and simple function
 *  to set both physical address <paddr> and cache attribute <cattr> for
 *  a 512MB region at <vaddr>.
 *
 *  Parameters:
 *  void* vaddr		512MB aligned pointer representing the start of virtual address region
 *  void* paddr		512MB aligned pointer representing the start of physical address region
 *  unsigned cattr	4 bit value encoding the caching properties and rights (MMU only).
 *
 *  returns 0 (XCHAL_SUCCESS) if successful
 *  returns non zero (XCHAL_UNSUPPORTED_ON_THIS_ARCH) on failure
 *
 *  This function has the following limitations:
 *
 *  1) Requires either the Region Translation Option or a v3 MMU running in the default mode (with spanning way)
 *  2) It does no error checking.
 *  3) Deals with one 512MB region (vaddr and paddr are required to be 512MB aligned although that is not explicitly checked)
 *  4) It requires the caller to do any cache flushing that is needed
 *  5) Doesn't support mnemonically setting the 'rights' (rwx, rw, ... ) bit on the MMU
 *  6) It is illegal to change the mapping of the region containing the current PC (not checked)
 *
 */
int xthal_set_region_translation_raw(void *vaddr, void *paddr, unsigned cattr) {
#if XCHAL_HAVE_XEA2
#if XCHAL_HAVE_XLT_CACHEATTR || (XCHAL_HAVE_PTP_MMU && XCHAL_HAVE_SPANNING_WAY)
# if XCHAL_HAVE_XLT_CACHEATTR
	unsigned vpn_way = (unsigned)vaddr;
# else
	unsigned vpn_way = ((unsigned) vaddr & 0xFFFFFFF0) + XCHAL_SPANNING_WAY;
# endif
	unsigned ppn_ca = ((unsigned) paddr & 0xFFFFFFF0) + (cattr & 0xF);
	write_dtlb_entry(vpn_way, ppn_ca);
	write_itlb_entry(vpn_way, ppn_ca);
	return XCHAL_SUCCESS;
#else
	return XCHAL_UNSUPPORTED_ON_THIS_ARCH;
#endif
#else
	return XCHAL_UNSUPPORTED_ON_THIS_ARCH;
#endif
}

/*
 * xthal_v2p() takes a virtual address as input, and if that virtual address is mapped to a physical address
 * by the MMU, it returns the:
 * 		a) corresponding physical address
 * 		b) the tlb way that is used to translate the address
 * 		c) cache attribute for translation
 *
 * 	Parameters:
 * 	void* 		vaddr		A pointer representing the virtual address (there are no alignment requirements for this address)
 * 	void**		paddr		This value can be 0, or can point to a pointer variable which will be updated to contain the physical address
 * 	unsigned*	way			This value can be 0, or can point to an unsigned variable which will be updated to contain the TLB way.
 * 	unsigned*   cattr		This value can be 0, or can point to an unsigned variable which will be updated to contain the cache attr
 *
 *  Returns 	0 (XCHAL_SUCCESS) 				if successful
 * 				XTHAL_NO_MAPPING				if there is no current mapping for the virtual address
 * 				XCHAL_UNSUPPORTED_ON_THIS_ARCH 	if unsupported
 *
 * 	Limitations:
 * 					Assumes that architecture variable DVARWAY56 is "Variable"
 * 					Uses the D-TLBS for the translation ... assumption is that ITLB's have same mappings
 */
int xthal_v2p(void* vaddr, void** paddr, unsigned *way, unsigned* cattr) {
#if XCHAL_HAVE_XEA2
	unsigned long probe = probe_dtlb((unsigned) vaddr);
#if !XCHAL_HAVE_PTP_MMU
	if (!(0x1 & probe))
	return XTHAL_NO_MAPPING;
	if (way)
	*way = 1;
	if (paddr || cattr) {
		unsigned long temp;
		temp = read_dtlb1_entry(probe);
		unsigned ppn = 0xe0000000 & temp;
		unsigned att = 0xf & temp;
		if (paddr)
		*paddr = ((void*) (ppn + (((unsigned) vaddr) & 0x1fffffff)));
		if (cattr)
		*cattr = att;
	}
#else
	{
		unsigned iway;
		if (!(0x10 & probe))
			return XTHAL_NO_MAPPING;
		iway = 0xf & probe;
		if (way)
			*way = iway;
		if (paddr || cattr) {
			unsigned temp;
			unsigned ppn;
			unsigned ppn1;
			unsigned dtlbcfg = read_dtlbcfg();
			temp = read_dtlb1_entry(probe);
			unsigned att = 0xf & temp;
			if (cattr)
				*cattr = att;
			if (paddr)
				switch (iway) // followin code derived from fig 4-40 from ISA MMU Option Data (at) Format for RxTLB1
				{ /* 4k pages */
				case 0:
				case 1:
				case 2:
				case 3:
				case 7:
				case 8:
				case 9:
					ppn = 0xfffff000; // 4k pages
					break;
				case 4: {
					switch ((dtlbcfg & (0x3 << 16)) >> 16) // bits 16 & 17
					{
					case 0: // 1MB pages
						ppn = 0xfff00000;
						break;
					case 1: // 4MB pages
						ppn = 0xffc00000;
						break;
					case 2: // 16MB pages
						ppn = 0xff000000;
						break;
					case 3: // 64MB pages
						ppn = 0xfc000000;
						break;
					default:
						return XCHAL_UNSUPPORTED_ON_THIS_ARCH;
					}
				}
					break;
				case 5:
					if ((dtlbcfg & (1 << 20)))
						ppn = 0xf8000000; // 128MB pages
					else
						ppn = 0xf0000000; // 256MB pages
					break;
				case 6:
					if ((dtlbcfg & (1 << 24)))
						ppn = 0xe0000000; // 512MB pages
					else
						ppn = 0xf0000000; // 256MB pages
					break;
				default:
					return XCHAL_UNSUPPORTED_ON_THIS_ARCH;
					break;
				}
			ppn1 = ppn & temp;
			*paddr = ((void*) (ppn1 + (((unsigned) vaddr) & (~ppn))));
		}
	}
#endif
	return XCHAL_SUCCESS;
#else
	return XCHAL_UNSUPPORTED_ON_THIS_ARCH;
#endif
}

/* these constants borrowed from xthal_set_region_attribute */
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

/* internal function that returns 1 if the supplied attr indicates the
 * cache is in writeback mode.
 */
static inline int is_writeback(unsigned attr) {
#if XCHAL_HAVE_XLT_CACHEATTR
	return attr == CA_WRITEBACK || attr == CA_WRITEBACK_NOALLOC;
#endif
#if XCHAL_HAVE_PTP_MMU && XCHAL_HAVE_SPANNING_WAY
	return (attr | 0x3) == CA_WRITEBACK;
#endif
	return -1; /* unsupported */
}

/*
 *  xthal_set_region_translation()
 *
 *  Establishes a new mapping (with the supplied cache attributes)
 *  between a virtual address region, and a physical address region.
 *
 *  This function is only supported with following processor configurations:
 *  				a) Region Translation
 *  				b) v3 MMU with a spanning way running in the default mode
 *
 *  If the specified memory range exactly covers a series
 *  of consecutive 512 MB regions, the address mapping and cache
 *  attributes of these regions are updated.
 *
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
 *  CACHE HANDLING
 *
 *  This function automatically writes back dirty data before remapping a
 *  virtual address region.
 *
 *  This writeback is done safely, ie. by first switching to writethrough
 *  mode, and then invoking xthal_dcache_all_writeback(). Such a sequence is
 *  necessary to ensure there is no longer any dirty data in the memory region by the time
 *  this function returns, even in the presence of interrupts, speculation, etc.
 *  This automatic write-back can be disabled using the XTHAL_CAFLAG_NO_AUTO_WB flag.
 *
 *	This function also invalidates the caches after remapping a region because the
 *	cache could contain (now invalid) data from the previous mapping.
 *  This automatic invalidate can be disabled using the XTHAL_CAFLAG_NO_AUTO_INV flag.
 *
 *  Parameters:
 *	vaddr	starting virtual address of region of memory
 *
 *	paddr	starting physical address for the mapping (this should be 512MB aligned to vaddr such that ((vaddr ^ paddr) & 0x10000000 == 0)
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
 *
 *			XTHAL_CAFLAG_EXACT - If this flag is present,
 *			the mapping will only be done if the specified
 *			region exactly matches on or more 512MB pages otherwise
 *			XCHAL_INEXACT is returned (and no mapping is done).
 *
 *			XTHAL_CAFLAG_NO_PARTIAL - If this flag is specified, then
 *			only pages that are completely covered by the specified region
 *			are affected.  If this flag is specified, and no pages are completely
 *			covered by the region, then no pages are affected and XCHAL_NO_PAGES_MAPPED
 *			is returned.
 *
 *
 *
 *  Returns:
 *	XCHAL_SUCCESS 	-			successful, or size is zero
 *
 *	XCHAL_NO_PAGES_MAPPED - 	XTHAL_CAFLAG_NO_PARTIAL flag specified and address range
 *								is valid with a non-zero size, however no 512 MB region (or page)
 *								is completely covered by the range
 *
 *	XCHAL_INEXACT 				XTHAL_CAFLAG_EXACT flag specified, and address range does
 *								not exactly specify a 512 MB region (or page)
 *
 *	XCHAL_INVALID_ADDRESS		invalid address range specified (wraps around the end of memory)
 *
 *	XCHAL_ADDRESS_MISALIGNED	virtual and physical addresses are not aligned (512MB)
 *
 *
 *	XCHAL_UNSUPPORTED_ON_THIS_ARCH	function not supported in this processor configuration
 */
int xthal_set_region_translation(void* vaddr, void* paddr, unsigned size,
		unsigned cattr, unsigned flags) {
#if XCHAL_HAVE_XEA2
#if XCHAL_HAVE_XLT_CACHEATTR || (XCHAL_HAVE_PTP_MMU && XCHAL_HAVE_SPANNING_WAY)
	const unsigned CA_MASK = 0xF;
	const unsigned addr_mask = 0x1fffffff;
	const unsigned addr_shift = 29;
	unsigned vaddr_a = (unsigned) vaddr;
	unsigned paddr_a = (unsigned) paddr;
	unsigned end_vaddr;
	unsigned end_paddr;
	unsigned start_va_reg;
	unsigned end_va_reg;
	unsigned start_pa_reg;
	unsigned icache_attr = 0;
	int rv;
	int i;
	if (size == 0)
		return XCHAL_SUCCESS;
	if ((vaddr_a & addr_mask) ^ (paddr_a & addr_mask))
		return XCHAL_ADDRESS_MISALIGNED;
	icache_attr = cattr & CA_MASK;
#if (XCHAL_HAVE_PTP_MMU && XCHAL_HAVE_SPANNING_WAY)
	// if using the mmu in spanning way mode then 'and in' the R, RX, RW, RWX bits
	if ((cattr & 0x40000000) && (icache_attr < 12))
		icache_attr = icache_attr & ((cattr & 0xF0) >> 4);
#endif
	end_vaddr = vaddr_a + size - 1;
	end_paddr = paddr_a + size - 1;

	if ((end_vaddr < vaddr_a) || (end_paddr < paddr_a))
		return XCHAL_INVALID_ADDRESS;
	start_va_reg = vaddr_a >> addr_shift;
	end_va_reg = end_vaddr >> addr_shift;
	start_pa_reg = paddr_a >> addr_shift;
	if ((flags & XTHAL_CAFLAG_EXACT)
			&& ((size & addr_mask) || (vaddr_a & addr_mask)
					|| (paddr_a & addr_mask)))
		return XCHAL_INEXACT;
	if (flags & XTHAL_CAFLAG_NO_PARTIAL) {
		if (vaddr_a & addr_mask) {
			start_va_reg++;
			start_pa_reg++;
		}
		if ((end_vaddr & addr_mask) != addr_mask)
			end_va_reg--;
	}
	if (end_va_reg < start_va_reg)
		return XCHAL_NO_PAGES_MAPPED;
	/*
	 * Now we need to take care of any uncommitted cache writes in the affected regions
	 * 1) first determine if any regions are in write back mode
	 * 2) change those pages to write through
	 * 3) force the writeback of d-cache by calling xthal_dcach_all_writeback()
	 */
#if ((XCHAL_DCACHE_SIZE >0) && XCHAL_DCACHE_IS_WRITEBACK)
	if (!(flags & XTHAL_CAFLAG_NO_AUTO_WB)) {
		unsigned old_cache_attr = xthal_get_cacheattr();
		unsigned cachewrtr = old_cache_attr;
		unsigned need_safe_writeback = 0;
		for (i = start_va_reg; i <= end_va_reg; i++) {
			unsigned sh = i << 2;
			unsigned old_attr = (old_cache_attr >> sh) & CA_MASK;
			if (is_writeback(old_attr)) {
				need_safe_writeback = 1;
				cachewrtr = (cachewrtr & ~(CA_MASK << sh))
						| (CA_WRITETHRU << sh);
			}
		}

		if (need_safe_writeback) {
			xthal_set_cacheattr(cachewrtr); /* set to writethru first, to safely writeback any dirty data */
			xthal_dcache_all_writeback(); /* much quicker than scanning entire 512MB region(s) */
		}
	}
#endif
	/* Now we set the affected region translations */
	for (i = start_va_reg; i <= end_va_reg; i++) {
		if ((rv = xthal_set_region_translation_raw(
				(void*) ((start_va_reg++) << addr_shift),
				(void*) ((start_pa_reg++) << addr_shift), icache_attr)))
			return rv;
	}

	/*
	 * Now we need to invalidate the cache in the affected regions. For now invalidate entire cache,
	 * but investigate if there are faster alternatives on some architectures.
	 */
	if (!(flags & XTHAL_CAFLAG_NO_AUTO_INV)) {
# if XCHAL_DCACHE_SIZE > 0
		xthal_dcache_all_writeback_inv(); /* some areas in memory (outside the intended region) may have uncommitted
		 data so we need the writeback_inv(). */
#endif
#if	XCHAL_ICACHE_SIZE >0
		xthal_icache_all_invalidate();
#endif
	}
	return XCHAL_SUCCESS;
#else
	return XCHAL_UNSUPPORTED_ON_THIS_ARCH;
#endif
#else
	return XCHAL_UNSUPPORTED_ON_THIS_ARCH;
#endif
}

/* xthal_invalidate_region()
 * invalidates the tlb entry for the specified region.
 *
 * This function is only supported on processor configurations 
 * with a v3 MMU with a spanning way.
 *
 * Parameter
 * vaddr - virtual address of region to invalidate (512MB aligned)
 *
 * returns:
 * XCHAL_SUCCESS 					- Success
 * XCHAL_UNSUPPORTED_ON_THIS_ARCH 			- Unsupported
 *
 */
int xthal_invalidate_region(void* vaddr) {
#if XCHAL_HAVE_XEA2
#if (XCHAL_HAVE_PTP_MMU && XCHAL_HAVE_SPANNING_WAY)
	unsigned addr = (unsigned) vaddr;
	if (addr & 0x1fffffff)
		return XCHAL_INVALID_ADDRESS;
	addr += XCHAL_SPANNING_WAY;
	invalidate_dtlb_entry(addr);
	invalidate_itlb_entry(addr);
	return XCHAL_SUCCESS;
#else
	return XCHAL_UNSUPPORTED_ON_THIS_ARCH;
#endif
#else
	return XCHAL_UNSUPPORTED_ON_THIS_ARCH;
#endif
}

