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
#include <platform/interrupt.h>
#include <platform/dai.h>
#include <stdint.h>
#include <string.h>

static struct dai ssp[2] = {
{
	.plat_data = {
		.base		= SSP0_BASE,
		.irq		= IRQ_NUM_EXT_SSP0,
	},
	.ops		= &ssp_ops,
},
{
	.plat_data = {
		.base		= SSP1_BASE,
		.irq		= IRQ_NUM_EXT_SSP1,
	},
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
