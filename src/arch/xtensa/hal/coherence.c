/*  coherence.c - Cache coherence opt-in / opt-out functions  */

/* $Id: //depot/rel/Eaglenest/Xtensa/OS/hal/coherence.c#1 $ */

/*
 * Copyright (c) 2008 Tensilica Inc.
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
 *  Opt-out of cache coherence.
 *
 *  Caveat:  on a core with full MMU, cache attribute handling done here only
 *  works well with the default (reset) TLB mapping of eight 512MB regions.
 *  It likely won't work correctly when other page sizes are in use (it may
 *  appear to work but be open to race conditions, depending on situation).
 */
void  xthal_cache_coherence_optout( void )
{
#if XCHAL_HAVE_EXTERN_REGS && XCHAL_DCACHE_IS_COHERENT
  unsigned ca = xthal_get_cacheattr();
  /*  Writeback all dirty entries.  Writethru mode avoids new dirty entries.  */
  xthal_set_region_attribute(0,0xFFFFFFFF, XCHAL_CA_WRITETHRU, XTHAL_CAFLAG_EXPAND);
  xthal_dcache_all_writeback();
  /*  Invalidate all cache entries.  Cache-bypass mode avoids new entries.  */
  xthal_set_region_attribute(0,0xFFFFFFFF, XCHAL_CA_BYPASS, XTHAL_CAFLAG_EXPAND);
  xthal_dcache_all_writeback_inv();
  /*  Wait for everything to settle.  */
  asm("memw");
  xthal_dcache_sync();
  xthal_icache_sync();
  /*  Opt-out of cache coherency protocol.  */
  xthal_cache_coherence_off();
  /*  Restore cache attributes, as of entry to this function.  */
  xthal_set_cacheattr(ca);
#endif
}

