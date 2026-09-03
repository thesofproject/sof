// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Guennadi Liakhovetski <guennadi.liakhovetski@linux.intel.com>

#include <sof/drivers/gpio.h>
#include <sof/drivers/iomux.h>
#include <sof/lib/io.h>
#include <sof/lib/memory.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

#define PORTA_DAT_REG	0x00
#define PORTA_DIR_REG	0x04
#define PORTA_CTL_REG	0x08

struct gpio {
	uint32_t base;
};

static struct gpio dw_gpio = {
	.base = DW_GPIO_BASE,
};

int gpio_write(const struct gpio *gpio, unsigned int port,
	       enum gpio_level level)
{
	io_reg_update_bits(gpio->base + PORTA_DAT_REG, 1 << port,
			   (level == GPIO_LEVEL_HIGH) << port);

	return 0;
}

int gpio_read(const struct gpio *gpio, unsigned int port)
{
	return ((io_reg_read(gpio->base + PORTA_DAT_REG) >> port) & 1) ?
		GPIO_LEVEL_HIGH : GPIO_LEVEL_LOW;
}

int gpio_configure(const struct gpio *gpio, unsigned int port,
		   const struct gpio_config *config)
{
	const struct gpio_pin_config *gpio_cfg = gpio_data + port;
	const struct iomux_pin_config *pin_cfg = &gpio_cfg->mux_config;
	struct iomux *mux = iomux_get(gpio_cfg->mux_id);
	int ret;

	if (!mux)
		return -ENODEV;

	ret = iomux_configure(mux, pin_cfg);
	if (ret < 0)
		return ret;

	/* set the direction of GPIO */
	io_reg_update_bits(gpio->base + PORTA_DIR_REG, 1 << port,
			(config->direction == GPIO_DIRECTION_OUTPUT) << port);

	return 0;
}

const struct gpio *gpio_get(unsigned int id)
{
	return id ? NULL : &dw_gpio;
}

int gpio_probe(const struct gpio *gpio)
{
	return gpio == &dw_gpio ? 0 : -ENODEV;
}
