/* 
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 */

#ifndef __INCLUDE_ARCH_REEF__
#define __INCLUDE_ARCH_REEF__

#include <stdint.h>
#include <stddef.h>

void *xthal_memcpy(void *dst, const void *src, size_t len);

#define arch_memcpy(dest, src, size) \
	xthal_memcpy(dest, src, size)

#endif
