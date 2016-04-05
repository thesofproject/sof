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

/* SSP register offsets */
#define SSCR0		0x00
#define SSCR1		0x04
#define SSSR		0x08
#define SSITR		0x0C
#define SSDR		0x10
#define SSTO		0x28
#define SSPSP		0x2C
#define SSTSA		0x30
#define SSRSA		0x34
#define SSTSS		0x38
#define SFIFOTT		0x6C

extern const struct dai_ops ssp_ops;

#endif
