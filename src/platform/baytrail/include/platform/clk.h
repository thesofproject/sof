/* 
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 */

#ifndef __PLATFORM_CLOCK__
#define __PLATFORM_CLOCK__

#define CLK_CPU		0
#define CLK_SSP		1

#define CLK_DEFAULT_CPU_HZ	50000000
#define CLK_MAX_CPU_HZ		343000000

void init_platform_clocks(void);

#endif
