/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 *
 * Author: Guennadi Liakhovetski <guennadi.liakhovetski@linux.intel.com>
 */

#ifndef __SOF_GPIO_H__
#define __SOF_GPIO_H__

#include <stdint.h>

#include <sof/iomux.h>

enum gpio_direction {
	GPIO_DIRECTION_INPUT,
	GPIO_DIRECTION_OUTPUT,
};

enum gpio_level {
	GPIO_LEVEL_LOW,
	GPIO_LEVEL_HIGH,
};

struct gpio_config {
	enum gpio_direction direction;
};

struct gpio_pin_config {
	unsigned int mux_id;
	struct iomux_pin_config mux_config;
};

struct gpio;

extern const struct gpio_pin_config gpio_data[];
extern const int n_gpios;

int gpio_write(const struct gpio *gpio, unsigned int port,
	       enum gpio_level level);
int gpio_read(const struct gpio *gpio, unsigned int port);
int gpio_configure(const struct gpio *gpio, unsigned int port,
		   const struct gpio_config *config);
const struct gpio *gpio_get(unsigned int id);
int gpio_probe(const struct gpio *gpio);

#endif /* __SOF_GPIO_H__ */
