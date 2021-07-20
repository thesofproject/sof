/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright 2019 NXP
 *
 * Author: Daniel Baluta <daniel.baluta@nxp.com>
 */

#ifdef __SOF_LIB_DAI_H__

#ifndef __PLATFORM_LIB_DAI_H__
#define __PLATFORM_LIB_DAI_H__

#define DMAMUX2_SAI5_RX_NUM 69
#define DMAMUX2_SAI5_TX_NUM 70
#define DMAMUX2_SAI6_RX_NUM 71
#define DMAMUX2_SAI6_TX_NUM 72
#define DMAMUX2_SAI7_RX_NUM 73
#define DMAMUX2_SAI7_TX_NUM 74

#endif /* __PLATFORM_LIB_DAI_H__ */

#else

#error "This file shouldn't be included from outside of sof/lib/dai.h"

#endif /* __SOF_LIB_DAI_H__ */
