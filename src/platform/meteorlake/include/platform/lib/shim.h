/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 *         Rander Wang <rander.wang@intel.com>
 */

#ifdef __SOF_LIB_SHIM_H__

#ifndef __PLATFORM_LIB_SHIM_H__
#define __PLATFORM_LIB_SHIM_H__

#include <rtos/bit.h>
#include <sof/lib/memory.h>

/** \brief Request HP RING Oscillator Clock */
#define SHIM_CLKCTL_RHROSCC		BIT(31)

/** \brief Request WOVCRO Clock */
#define SHIM_CLKCTL_WOV_CRO_REQUEST	BIT(4)

/** \brief Request LP RING Oscillator Clock */
#define SHIM_CLKCTL_RLROSCC		BIT(29)

/** \brief Oscillator Clock Select*/
#define SHIM_CLKCTL_OCS_HP_RING		BIT(2)
#define SHIM_CLKCTL_OCS_LP_RING		0
#define SHIM_CLKCTL_WOVCROSC		BIT(3)

/** \brief LP Memory Clock Select */
#define SHIM_CLKCTL_LMCS_DIV4		BIT(1)

/** \brief HP Memory Clock Select */
#define SHIM_CLKCTL_HMCS_DIV2		0

/** \brief HP RING Oscillator Clock Status */
#define SHIM_CLKSTS_HROSCCS		BIT(31)

/** \brief WOVCRO Clock Status */
#define SHIM_CLKSTS_WOV_CRO		BIT(4)

/** \brief LP RING Oscillator Clock Status */
#define SHIM_CLKSTS_LROSCCS		BIT(29)

#define L2HSBPM(x)			(0x17A800 + 0x0008 * (x))
#define SHIM_HSPGCTL(x)			(L2HSBPM(x) + 0x0000)

#define LSPGCTL				0x71D80

#endif /* __PLATFORM_LIB_SHIM_H__ */

#else

#error "This file shouldn't be included from outside of sof/lib/shim.h"

#endif /* __SOF_LIB_SHIM_H__ */
