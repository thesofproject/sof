/*
 * Copyright (c) 2019, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the Intel Corporation nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Guennadi Liakhovetski <guennadi.liakhovetski@linux.intel.com>
 */

#ifndef __INCLUDE_GPIO__
#define __INCLUDE_GPIO__

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

#endif
