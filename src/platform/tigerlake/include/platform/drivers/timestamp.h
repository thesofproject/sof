/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2020 Intel Corporation. All rights reserved.
 *
 * Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

#ifdef __SOF_DRIVERS_TIMESTAMP_H__

#ifndef __PLATFORM_DRIVERS_TIMESTAMP_H__
#define __PLATFORM_DRIVERS_TIMESTAMP_H__

#include <cavs/drivers/timestamp.h>

#define TS_DMIC_LOCAL_TSCTRL	0x000
#define TS_DMIC_LOCAL_OFFS	0x004
#define TS_DMIC_LOCAL_SAMPLE	0x008
#define TS_DMIC_LOCAL_WALCLK	0x010
#define TS_DMIC_TSCC		0x018
#define TS_HDA_LOCAL_TSCTRL	(0x0e0 + 0x00)
#define TS_HDA_LOCAL_OFFS	(0x0e0 + 0x04)
#define TS_HDA_LOCAL_SAMPLE	(0x0e0 + 0x08)
#define TS_HDA_LOCAL_WALCLK	(0x0e0 + 0x10)
#define TS_HDA_TSCC		(0x0e0 + 0x18)
#define TS_I2S_LOCAL_TSCTRL(x)	(0x100 + 0x20 * (x) + 0x00)
#define TS_I2S_LOCAL_OFFS(x)	(0x100 + 0x20 * (x) + 0x04)
#define TS_I2S_LOCAL_SAMPLE(x)	(0x100 + 0x20 * (x) + 0x08)
#define TS_I2S_LOCAL_WALCLK(x)	(0x100 + 0x20 * (x) + 0x10)
#define TS_I2S_TSCC(x)		(0x100 + 0x20 * (x) + 0x18)

#endif /* __PLATFORM_DRIVERS_TIMESTAMP_H__ */

#else

#error "This file shouldn't be included from outside of sof/drivers/timestamp.h"

#endif /* __SOF_DRIVERS_TIMESTAMP_H__ */
