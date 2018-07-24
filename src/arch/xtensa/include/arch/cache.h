/*
 * Copyright (c) 2017, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the Intel Corporation nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifndef __INCLUDE_ARCH_CACHE__
#define __INCLUDE_ARCH_CACHE__

#include <stdint.h>
#include <stddef.h>
#include <xtensa/config/core.h>
#include <xtensa/hal.h>

static inline void dcache_writeback_region(void *addr, size_t size)
{
#if XCHAL_DCACHE_SIZE > 0
	xthal_dcache_region_writeback(addr, size);
#endif
}

static inline void dcache_writeback_all()
{
#if XCHAL_DCACHE_SIZE > 0
	xthal_dcache_all_writeback();
#endif
}

static inline void dcache_invalidate_region(void *addr, size_t size)
{
#if XCHAL_DCACHE_SIZE > 0
	xthal_dcache_region_invalidate(addr, size);
#endif
}

static inline void dcache_invalidate_all()
{
#if XCHAL_DCACHE_SIZE > 0
	xthal_dcache_all_invalidate();
#endif
}

static inline void icache_invalidate_region(void *addr, size_t size)
{
#if XCHAL_ICACHE_SIZE > 0
	xthal_icache_region_invalidate(addr, size);
#endif
}

static inline void icache_invalidate_all()
{
#if XCHAL_ICACHE_SIZE > 0
	xthal_icache_all_invalidate();
#endif
}

static inline void dcache_writeback_invalidate_region(void *addr, size_t size)
{
#if XCHAL_DCACHE_SIZE > 0
	xthal_dcache_region_writeback_inv(addr, size);
#endif
}

static inline void dcache_writeback_invalidate_all()
{
#if XCHAL_DCACHE_SIZE > 0
	xthal_dcache_all_writeback_inv();
#endif
}

#endif

