/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2020 Intel Corporation. All rights reserved.
 *
 * Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

#ifdef __PLATFORM_DRIVERS_TIMESTAMP_H__

#ifndef __CAVS_DRIVERS_TIMESTAMP_H__
#define __CAVS_DRIVERS_TIMESTAMP_H__

#include <rtos/bit.h>

#define TS_LOCAL_TSCTRL_NTK_BIT		BIT(31)
#define TS_LOCAL_TSCTRL_IONTE_BIT	BIT(30)
#define TS_LOCAL_TSCTRL_SIP_BIT		BIT(8)
#define TS_LOCAL_TSCTRL_HHTSE_BIT	BIT(7)
#define TS_LOCAL_TSCTRL_ODTS_BIT	BIT(5)
#define TS_LOCAL_TSCTRL_CDMAS(x)	SET_BITS(4, 0, x)
#define TS_LOCAL_OFFS_FRM		GET_BITS(15, 12)
#define TS_LOCAL_OFFS_CLK		GET_BITS(11, 0)

#endif /* __CAVS_DRIVERS_TIMESTAMP_H__ */

#else

#error "This file shouldn't be included from outside of platform/drivers/timestamp.h"

#endif /* __PLATFORM_DRIVERS_TIMESTAMP_H__ */
