/* 
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 */

#include <reef/dai.h>
#include <reef/ssp.h>
#include <platform/memory.h>
#include <platform/dai.h>
#include <stdint.h>
#include <string.h>

static struct dai ssp[2] = {
{
	.base		= SSP0_BASE,
	.ops		= &ssp_ops,
},
{
	.base		= SSP1_BASE,
	.ops		= &ssp_ops,
},};

struct dai *dai_get(int dai_id)
{
	switch (dai_id) {
	case DAI_ID_SSP0:
		return &ssp[0];
	case DAI_ID_SSP1:
		return &ssp[1];
	default:
		return NULL;
	}
}
