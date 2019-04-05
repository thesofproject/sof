/*
 * Copyright (c) 2019, Intel Corporation
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
 * Author: Bartosz Kokoszko <bartoszx.kokoszko@linux.intel.com>
 */

#ifndef __INCLUDE_CAVS_MEM_MAP__
#define __INCLUDE_CAVS_MEM_MAP__

#include <config.h>

#define LPSRAM_BANK_SIZE (64 * 1024)

#define SRAM_BANK_SIZE			LPSRAM_BANK_SIZE

#define EBB_BANKS_IN_SEGMENT		32

#define EBB_SEGMENT_SIZE		EBB_BANKS_IN_SEGMENT

#define PLATFORM_CORE_COUNT		CONFIG_CORE_COUNT

#define PLATFORM_LPSRAM_EBB_COUNT	CONFIG_LP_MEMORY_BANKS

#define PLATFORM_HPSRAM_EBB_COUNT	CONFIG_HP_MEMORY_BANKS

#define MAX_MEMORY_SEGMENTS		PLATFORM_HPSRAM_SEGMENTS

#define PLATFORM_HPSRAM_SEGMENTS	((PLATFORM_HPSRAM_EBB_COUNT \
	+ EBB_BANKS_IN_SEGMENT - 1) / EBB_BANKS_IN_SEGMENT)

#define LPSRAM_MASK(ignored)	((1 << PLATFORM_LPSRAM_EBB_COUNT) - 1)

#define HPSRAM_MASK(seg_idx)	((1 << (PLATFORM_HPSRAM_EBB_COUNT \
	- EBB_BANKS_IN_SEGMENT * seg_idx)) - 1)

#define LPSRAM_SIZE (PLATFORM_LPSRAM_EBB_COUNT * LPSRAM_BANK_SIZE)

#if !defined(__ASSEMBLER__) && !defined(LINKER)
void platform_init_memmap(void);
#endif

#endif
