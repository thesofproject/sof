/*
 * Copyright (c) 2017, Intel Corporation
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
 * Author: Wu Zhigang <zhigang.wu@intel.com>
 */

#ifndef __GPIO_H__
#define __GPIO_H__

#define SUE_GPIO_OFFSET(x)		(x + 0x00080C00)
#define SUE_GPIO_PORTA_DAT_REG	SUE_GPIO_OFFSET(0x00)
#define SUE_GPIO_PORTA_DIR_REG	SUE_GPIO_OFFSET(0x04)
#define SUE_GPIO_PORTA_CTL_REG	SUE_GPIO_OFFSET(0x08)

#define SUE_IOMUX_OFFSET(x)		(x + 0x00081C00)
#define SUE_IOMUX_CTL0_REG	SUE_IOMUX_OFFSET(0x30)
#define SUE_IOMUX_CTL1_REG	SUE_IOMUX_OFFSET(0x34)


#define SUE_LEVEL_HI	(1)
#define SUE_LEVEL_LO	(0)

#define SUE_GPIO_DIR_OUT	(1)
#define SUE_GPIO_DIE_IN		(0)


/* the GPIO pin number */
enum GPIO {
	GPIO0 = 0,
	GPIO1,
	GPIO2,
	GPIO3,
	GPIO4,
	GPIO5,
	GPIO6,
	GPIO7,
	GPIO8,
	GPIO9,
	GPIO10,
	GPIO11,
	GPIO12,
	GPIO13,
	GPIO14,
	GPIO15,
	GPIO16,
	GPIO17,
	GPIO18,
	GPIO19,
	GPIO20,
	GPIO21,
	GPIO22,
	GPIO23,
	GPIO24,
	GPIO25,
	GPIO_NUM,
};

struct gpio_device_config {
	/* 0 -- not configured as GPIO; 1 -- configured as GPIO */
	uint8_t	gpio_state[GPIO_NUM];
};

static struct gpio_device_config gpio_dev_cfg;

static inline int gpio_config(enum GPIO port, int dir)
{
	struct gpio_device_config *gpio_cfg = &gpio_dev_cfg;
	uint32_t shift, value;

	value = 1;	/* value to enable GPIO */

	switch (port) {
	case GPIO0 ... GPIO7:
		shift = port << 1;
		io_reg_update_bits(SUE_IOMUX_CTL1_REG, 3 << shift, value << shift);
		break;
	case GPIO8:
		io_reg_update_bits(SUE_IOMUX_CTL1_REG, 1 << 16, value << 16);
		break;
	case GPIO9 ... GPIO12:
		io_reg_update_bits(SUE_IOMUX_CTL0_REG, 1 << 11, value << 11);
		break;
	case GPIO13:
		io_reg_update_bits(SUE_IOMUX_CTL0_REG, 1 << 0, value << 0);
		break;
	case GPIO14:
		io_reg_update_bits(SUE_IOMUX_CTL0_REG, 1 << 1, value << 1);
		break;
	case GPIO15 ... GPIO18:
		io_reg_update_bits(SUE_IOMUX_CTL0_REG, 1 << 9, value << 9);
		break;
	case GPIO19 ... GPIO22:
		io_reg_update_bits(SUE_IOMUX_CTL0_REG, 1 << 10, value << 10);
		break;
	case GPIO23:
	case GPIO24:
		io_reg_update_bits(SUE_IOMUX_CTL0_REG, 1 << 16, value << 16);
		break;
	case GPIO25:
		io_reg_update_bits(SUE_IOMUX_CTL0_REG, 1 << 26, value << 26);
		break;
	default:
		return -EINVAL;
	}

	gpio_cfg->gpio_state[port] = 1;

	/* set the direction of GPIO */
	io_reg_update_bits(SUE_GPIO_PORTA_DIR_REG, 1 << port, dir << port);

	/* set the configuration register for software mode or hardware mode
	 * default with software mode (0).
	 * io_reg_update_bits(GPIO_PORTA_CTL_REG, 1<<port, 0<<port);
	 */
	return 0;
}

static inline int gpio_read(enum GPIO port)
{
	struct gpio_device_config *gpio_cfg = &gpio_dev_cfg;
	int level;

	if (!gpio_cfg->gpio_state[port])
		return -EINVAL;

	level = (io_reg_read(SUE_GPIO_PORTA_DAT_REG) & (1 << port)) >> port;

	return level;
}

static inline int gpio_write(enum GPIO port, int level)
{
	struct gpio_device_config *gpio_cfg = &gpio_dev_cfg;

	if (!gpio_cfg->gpio_state[port])
		return -EINVAL;

	io_reg_update_bits(SUE_GPIO_PORTA_DAT_REG, 1 << port, level << port);

	return 0;
}

#endif
