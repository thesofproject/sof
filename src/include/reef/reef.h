/* 
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 */

#ifndef __INCLUDE_REEF_REEF__
#define __INCLUDE_REEF_REEF__

#include <stdint.h>
#include <arch/reef.h>

/* C memcpy for arch that dont have arch_memcpy() */
void cmemcpy(void *dest, void *src, size_t size);

// TODO: add detection for arch memcpy
#if 1
#define rmemcpy(dest, src, size) \
	arch_memcpy(dest, src, size)
#else
#define rmemcpy(dest, src, size) \
	cmemcpy(dest, src, size)
#endif

#endif
