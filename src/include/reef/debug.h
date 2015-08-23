/* 
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 */

#ifndef __INCLUDE_DEBUG__
#define __INCLUDE_DEBUG__

// tmp hack
#include <platform/memory.h>

#define dbg() \
	*((int*)SHIM_BASE) = __LINE__;

#endif
