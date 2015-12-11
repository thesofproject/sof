/* 
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 */

#include <reef/reef.h>
#include <reef/dai.h>
#include <reef/ssp.h>
#include <reef/audio/component.h>
#include <platform/memory.h>
#include <platform/interrupt.h>
#include <stdint.h>
#include <string.h>

static struct dai ssp[2] = {
{
	.uuid = COMP_UUID(COMP_VENDOR_INTEL, DAI_UUID_SSP0),
	.plat_data = {
		.base		= SSP0_BASE,
		.irq		= IRQ_NUM_EXT_SSP0,
	},
	.ops		= &ssp_ops,
},
{
	.uuid = COMP_UUID(COMP_VENDOR_INTEL, DAI_UUID_SSP1),
	.plat_data = {
		.base		= SSP1_BASE,
		.irq		= IRQ_NUM_EXT_SSP1,
	},
	.ops		= &ssp_ops,
},};

struct dai *dai_get(uint32_t uuid)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(ssp); i++) {
		if (ssp[i].uuid == uuid)
			return &ssp[i];
	}

	return NULL;
}
