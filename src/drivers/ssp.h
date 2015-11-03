/* 
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 */

#ifndef __INCLUDE_SSP__
#define __INCLUDE_SSP__

#include <reef/dai.h>

#define SSP_CLK_AUDIO	0
#define SSP_CLK_NET_PLL	1
#define SSP_CLK_EXT	2
#define SSP_CLK_NET	3

extern const struct dai_ops ssp_ops;

#endif
