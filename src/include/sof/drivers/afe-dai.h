// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Mediatek
//
// Author: Bo Pan <bo.pan@mediatek.com>
//         YC Hung <yc.hung@mediatek.com>

#ifndef __SOF_DRIVERS_AFE_DAI_H__
#define __SOF_DRIVERS_AFE_DAI_H__

#include <rtos/bit.h>
#include <sof/common.h>
#include <sof/lib/dma.h>
#include <sof/trace/trace.h>
#include <user/trace.h>
#include <stdint.h>

#define AFE_HS_GET_DAI(hs) (((hs) & MASK(7, 0)) >> 0)
#define AFE_HS_SET_DAI(dai) SET_BITS(7, 0, dai)
#define AFE_HS_GET_IRQ(hs) (((hs) & MASK(15, 8)) >> 8)
#define AFE_HS_SET_IRQ(irq) SET_BITS(15, 8, irq)
#define AFE_HS_GET_CHAN(hs) (((hs) & MASK(23, 16)) >> 16)
#define AFE_HS_SET_CHAN(chan) SET_BITS(23, 16, chan)
#define AFE_HANDSHAKE(dai, irq, channel)\
	(AFE_HS_SET_DAI(dai) | AFE_HS_SET_CHAN(channel) | AFE_HS_SET_IRQ(irq))

extern const struct dai_driver afe_dai_driver;
#endif /* __SOF_DRIVERS_AFE_MEMIF_H__ */

