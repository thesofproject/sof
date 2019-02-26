/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2017 Intel Corporation. All rights reserved.
 *
 * Author: Guennadi Liakhovetski <guennadi.liakhovetski@linux.intel.com>
 */

#ifndef __SOF_DRIVERS_SPI_H__
#define __SOF_DRIVERS_SPI_H__

#include <stddef.h>
#include <stdint.h>

enum spi_type {
	SOF_SPI_INTEL_SLAVE,
	SOF_SPI_INTEL_MASTER,
};

enum spi_xfer_direction {
	SPI_DIR_RX,
	SPI_DIR_TX,
};

struct spi_plat_fifo_data {
	uint32_t handshake;
};

struct spi_platform_data {
	uint32_t base;
	struct spi_plat_fifo_data fifo[2];
	enum spi_type type;
};

struct spi;

int spi_push(struct spi *spi, const void *data, size_t size);
int spi_probe(struct spi *spi);
struct spi *spi_get(enum spi_type type);
int spi_install(const struct spi_platform_data *plat, size_t n);
void spi_init(void);

#endif /* __SOF_DRIVERS_SPI_H__ */
