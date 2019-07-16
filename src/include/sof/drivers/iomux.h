/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 *
 * Author: Guennadi Liakhovetski <guennadi.liakhovetski@linux.intel.com>
 */

#ifndef __SOF_DRIVERS_IOMUX_H__
#define __SOF_DRIVERS_IOMUX_H__

#include <stdint.h>

#define IOMUX_PIN_NUM 32

struct iomux {
	uint32_t base;
	/* 0 -- unconfigured; > 0 -- configured for function (state - 1) */
	uint8_t pin_state[IOMUX_PIN_NUM];
};

struct iomux_pin_config {
	unsigned int bit;
	uint32_t mask;
	uint32_t fn;
};

extern struct iomux iomux_data[];
extern const int n_iomux;

int iomux_configure(struct iomux *iomux, const struct iomux_pin_config *cfg);
struct iomux *iomux_get(unsigned int id);
int iomux_probe(struct iomux *iomux);

#endif /* __SOF_DRIVERS_IOMUX_H__ */
