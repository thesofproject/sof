// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright 2019 NXP
//
// Author: Daniel Baluta <daniel.baluta@nxp.com>

#include <sof/sof.h>
#include <sof/dai.h>
#include <sof/esai.h>

static struct dai esai[] = {
{
	.index = 0,
	.plat_data = {
		.base = ESAI_BASE,
	},
	.drv = &esai_driver,
},
};

static struct dai_type_info dti[] = {
	{
		.type = SOF_DAI_IMX_ESAI,
		.dai_array = esai,
		.num_dais = ARRAY_SIZE(esai)
	},
};

int dai_init(void)
{
	dai_install(dti, ARRAY_SIZE(dti));
	return 0;
}
