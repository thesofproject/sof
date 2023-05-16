/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 AMD.All rights reserved.
 *
 * Author:      Basavaraj Hiregoudar <basavaraj.hiregoudar@amd.com>
 *              Bala Kishore <balakishore.pati@amd.com>
 */
#ifdef __SOF_LIB_DAI_H__

#ifndef __PLATFORM_LIB_DAI_H__
#define __PLATFORM_LIB_DAI_H__


#endif /* __PLATFORM_LIB_DAI_H__ */

//DMA HANDSHAKE

#define SDW0_ACP_SW_HS_RX_EN_CH 0
#define SDW0_ACP_SW_HS_TX_EN_CH 1
#define SDW1_ACP_P1_SW_BT_RX_EN_CH 2
#define SDW1_ACP_P1_SW_BT_TX_EN_CH 3
#define SDW0_ACP_SW_Audio_RX_EN_CH 4
#define SDW0_ACP_SW_Audio_TX_EN_CH 5
#define SDW0_ACP_SW_BT_RX_EN_CH 6
#define SDW0_ACP_SW_BT_TX_EN_CH 7
#define SDW0_ACP_SW_BT_CH_OFFSET 4

#else

#error "This file shouldn't be included from outside of sof/lib/dai.h"

#endif /* __SOF_LIB_DAI_H__ */
