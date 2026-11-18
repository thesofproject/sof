/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2023 AMD.All rights reserved.
 *
 * Author:      Basavaraj Hiregoudar <basavaraj.hiregoudar@amd.com>
 *              SaiSurya, Ch <saisurya.chakkaveeravenkatanaga@amd.com>
 */
#ifdef __SOF_LIB_DAI_H__

#ifndef __PLATFORM_LIB_DAI_H__
#define __PLATFORM_LIB_DAI_H__

#define SDW0_ACP_SW_HS_RX_EN_CH 0
#define SDW0_ACP_SW_HS_TX_EN_CH 1
#define SDW1_ACP_P1_SW_BT_RX_EN_CH 2
#define SDW1_ACP_P1_SW_BT_TX_EN_CH 3
#define SDW0_ACP_SW_AUDIO_RX_EN_CH 4
#define SDW0_ACP_SW_AUDIO_TX_EN_CH 5
#define SDW0_ACP_SW_BT_RX_EN_CH 6
#define SDW0_ACP_SW_BT_TX_EN_CH 7
#define SDW1_ACP_P1_SW_AUDIO_RX_EN_CH 8
#define SDW1_ACP_P1_SW_AUDIO_TX_EN_CH 9
#define SDW1_ACP_P1_SW_HS_RX_EN_CH 10
#define SDW1_ACP_P1_SW_HS_TX_EN_CH 11

#define DI_SDW0_ACP_SW_AUDIO_TX 0
#define DI_SDW0_ACP_SW_BT_TX 1
#define DI_SDW0_ACP_SW_HS_TX 2
#define DI_SDW0_ACP_SW_AUDIO_RX 3
#define DI_SDW0_ACP_SW_BT_RX 4
#define DI_SDW0_ACP_SW_HS_RX 5
#define DI_SDW1_ACP_P1_SW_AUDIO_TX 64
#define DI_SDW1_ACP_P1_SW_BT_TX 65
#define DI_SDW1_ACP_P1_SW_HS_TX 66
#define DI_SDW1_ACP_P1_SW_AUDIO_RX 67
#define DI_SDW1_ACP_P1_SW_BT_RX 68
#define DI_SDW1_ACP_P1_SW_HS_RX 69

#endif /* __PLATFORM_LIB_DAI_H__ */

#else

#error "This file shouldn't be included from outside of sof/lib/dai.h"

#endif /* __SOF_LIB_DAI_H__ */
