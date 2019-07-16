// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Guennadi Liakhovetski <guennadi.liakhovetski@linux.intel.com>

#include <sof/drivers/iomux.h>
#include <sof/lib/io.h>
#include <errno.h>
#include <stddef.h>

#define IOMUX_PIN_UNCONFIGURED 0

int iomux_configure(struct iomux *iomux, const struct iomux_pin_config *cfg)
{
	if (iomux->pin_state[cfg->bit] != IOMUX_PIN_UNCONFIGURED)
		return -EBUSY;

	io_reg_update_bits(iomux->base, cfg->mask << cfg->bit,
			   cfg->fn << cfg->bit);

	iomux->pin_state[cfg->bit] = cfg->fn + 1;

	return 0;
}

struct iomux *iomux_get(unsigned int id)
{
	return id >= n_iomux ? NULL : iomux_data + id;
}

int iomux_probe(struct iomux *iomux)
{
	unsigned int i;

	for (i = 0; i < n_iomux; i++)
		if (iomux_data + i == iomux)
			return 0;

	return -ENODEV;
}
