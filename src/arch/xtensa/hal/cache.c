// 
// cache.c -- cache management routines
//
// $Id: //depot/rel/Foxhill/dot.8/Xtensa/OS/hal/cache.c#1 $

// Copyright (c) 2002 Tensilica Inc.
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

#include <xtensa/config/core.h>

// size of the cache lines in log2(bytes)
const unsigned char Xthal_icache_linewidth = XCHAL_ICACHE_LINEWIDTH;
const unsigned char Xthal_dcache_linewidth = XCHAL_DCACHE_LINEWIDTH;

// size of the cache lines in bytes
const unsigned short Xthal_icache_linesize = XCHAL_ICACHE_LINESIZE;
const unsigned short Xthal_dcache_linesize = XCHAL_DCACHE_LINESIZE;

// number of cache sets in log2(lines per way)
const unsigned char Xthal_icache_setwidth = XCHAL_ICACHE_SETWIDTH;
const unsigned char Xthal_dcache_setwidth = XCHAL_DCACHE_SETWIDTH;

// cache set associativity (number of ways)
const unsigned int Xthal_icache_ways = XCHAL_ICACHE_WAYS;
const unsigned int Xthal_dcache_ways = XCHAL_DCACHE_WAYS;

// size of the caches in bytes (ways * 2^(linewidth + setwidth))
const unsigned int Xthal_icache_size = XCHAL_ICACHE_SIZE;
const unsigned int Xthal_dcache_size = XCHAL_DCACHE_SIZE;

// cache features
const unsigned char Xthal_dcache_is_writeback  = XCHAL_DCACHE_IS_WRITEBACK;
const unsigned char Xthal_icache_line_lockable = XCHAL_ICACHE_LINE_LOCKABLE;
const unsigned char Xthal_dcache_line_lockable = XCHAL_DCACHE_LINE_LOCKABLE;

