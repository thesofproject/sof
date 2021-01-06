/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2020 Intel Corporation. All rights reserved.
 *
 * Author: Janusz Jankowski <janusz.jankowski@linux.intel.com>
 */

#ifdef __PLATFORM_DRIVERS_MN_H__

#ifndef __CAVS_DRIVERS_MN_H__
#define __CAVS_DRIVERS_MN_H__

#include <sof/bit.h>

/** \brief Offset of MCLK Divider Control Register. */
#define MN_MDIVCTRL 0x0

/** \brief Enables the output of MCLK Divider. */
#define MN_MDIVCTRL_M_DIV_ENABLE(x) BIT(x)

/** \brief Offset of MCLK Divider x Ratio Register. */
#define MN_MDIVR(x) (0x80 + (x) * 0x4)

/** \brief Bits for setting MCLK source clock. */
#define MCDSS(x)	SET_BITS(17, 16, x)

/** \brief Offset of BCLK x M/N Divider M Value Register. */
#define MN_MDIV_M_VAL(x) (0x100 + (x) * 0x8 + 0x0)

/** \brief Offset of BCLK x M/N Divider N Value Register. */
#define MN_MDIV_N_VAL(x) (0x100 + (x) * 0x8 + 0x4)

/** \brief Bits for setting M/N source clock. */
#define MNDSS(x)	SET_BITS(21, 20, x)

/** \brief Mask for clearing mclk and bclk source in MN_MDIVCTRL */
#define MN_SOURCE_CLKS_MASK 0x3

#endif /* __CAVS_DRIVERS_MN_H__ */

#else

#error "This file shouldn't be included from outside of platform/drivers/mn.h"

#endif /* __PLATFORM_DRIVERS_MN_H__ */
