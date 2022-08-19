/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright 2019 NXP
 *
 * Author: Daniel Baluta <daniel.baluta@nxp.com>
 */

#ifndef __SOF_DRIVERS_EDMA_H__
#define __SOF_DRIVERS_EDMA_H__

#include <rtos/bit.h>
#include <sof/trace/trace.h>
#include <user/trace.h>

#include <errno.h>
#include <stddef.h>

#define EDMA_CH_CSR                     0x00
#define EDMA_CH_ES                      0x04
#define EDMA_CH_INT                     0x08
#define EDMA_CH_SBR                     0x0C
#define EDMA_CH_PRI                     0x10
#define EDMA_CH_MUX                     0x14
#define EDMA_TCD_SADDR                  0x20
#define EDMA_TCD_SOFF                   0x24
#define EDMA_TCD_ATTR                   0x26
#define EDMA_TCD_NBYTES                 0x28
#define EDMA_TCD_SLAST                  0x2C
#define EDMA_TCD_DADDR                  0x30
#define EDMA_TCD_DOFF                   0x34
#define EDMA_TCD_CITER_ELINK            0x36
#define EDMA_TCD_CITER                  0x36
#define EDMA_TCD_DLAST_SGA              0x38
#define EDMA_TCD_CSR                    0x3C
#define EDMA_TCD_BITER_ELINK            0x3E
#define EDMA_TCD_BITER                  0x3E

#define EDMA_CSR			0x00
#define EDMA_ES				0x04
#define EDMA_INT			0x08
#define EDMA_HRS			0x0C

#define EDMA_TCD_CSR_START		BIT(0)
#define EDMA_TCD_CSR_INTMAJOR		BIT(1)
#define EDMA_TCD_CSR_INTHALF		BIT(2)
#define EDMA_TCD_CSR_INTHALF_INTMAJOR	(BIT(1) | BIT(2))
#define EDMA_TCD_CSR_DREQ		BIT(3)
#define EDMA_TCD_CSR_ESG		BIT(4)
#define EDMA_CH_CSR_ERQ_EARQ		(BIT(0) | BIT(1))

#define EDMA_CH_ES_ERR			BIT(31)
#define EDMA_CH_ES_SAE			BIT(7)
#define EDMA_CH_ES_SOE			BIT(6)
#define EDMA_CH_ES_DAE			BIT(5)
#define EDMA_CH_ES_DOE			BIT(4)
#define EDMA_CH_ES_NCE			BIT(3)
#define EDMA_CH_ES_SGE			BIT(2)
#define EDMA_CH_ES_SBE			BIT(1)
#define EDMA_CH_ES_DBE			BIT(0)

#define EDMA_TCD_ATTR_SSIZE_8BIT	0x0000
#define EDMA_TCD_ATTR_SSIZE_16BIT	0x0100
#define EDMA_TCD_ATTR_SSIZE_32BIT	0x0200
#define EDMA_TCD_ATTR_SSIZE_64BIT	0x0300
#define EDMA_TCD_ATTR_SSIZE_16BYTE	0x0400
#define EDMA_TCD_ATTR_SSIZE_32BYTE	0x0500
#define EDMA_TCD_ATTR_SSIZE_64BYTE	0x0600

#define EDMA_TCD_ATTR_DSIZE_8BIT	0x0000
#define EDMA_TCD_ATTR_DSIZE_16BIT	0x0001
#define EDMA_TCD_ATTR_DSIZE_32BIT	0x0002
#define EDMA_TCD_ATTR_DSIZE_64BIT	0x0003
#define EDMA_TCD_ATTR_DSIZE_16BYTE	0x0004
#define EDMA_TCD_ATTR_DSIZE_32BYTE	0x0005
#define EDMA_TCD_ATTR_DSIZE_64BYTE	0x0006

#define EDMA_BUFFER_PERIOD_COUNT	2

#define EDMA_TCD_ALIGNMENT		32

#define EDMA_HS_GET_IRQ(hs) (((hs) & MASK(8, 0)) >> 0)
#define EDMA_HS_SET_IRQ(irq) SET_BITS(8, 0, irq)
#define EDMA_HS_GET_CHAN(hs) (((hs) & MASK(13, 9)) >> 9)
#define EDMA_HS_SET_CHAN(chan) SET_BITS(13, 9, chan)
#define EDMA_HS_GET_DMAMUX_CFG(hs) (((hs) & MASK(21, 14)) >> 14)
#define EDMA_HS_SET_DMAMUX_CFG(cfg) SET_BITS(21, 14, cfg)
#define EDMA_HANDSHAKE(irq, channel, cfg)\
	(EDMA_HS_SET_CHAN(channel) | EDMA_HS_SET_IRQ(irq) | EDMA_HS_SET_DMAMUX_CFG(cfg))

#define EDMA0_ESAI_CHAN_RX	6
#define EDMA0_ESAI_CHAN_TX	7
#define EDMA0_SAI_CHAN_RX	14
#define EDMA0_SAI_CHAN_TX	15
#define EDMA0_CHAN_MAX		32

#define EDMA0_ESAI_CHAN_RX_IRQ	442
#define EDMA0_ESAI_CHAN_TX_IRQ	442
#define EDMA0_SAI_CHAN_RX_IRQ	349
#define EDMA0_SAI_CHAN_TX_IRQ	349

/* EDMA doesn't bound channels to IPs, we make use of the first two channels for now */
#define IMX8ULP_EDMA2_CHAN0	0
#define IMX8ULP_EDMA2_CHAN1	1
#define IMX8ULP_EDMA2_CHAN_MAX	8

/* EDMA provides one interrupt per channel */
#define IMX8ULP_EDMA2_CHAN0_IRQ	6
#define IMX8ULP_EDMA2_CHAN1_IRQ	7
#define IMX8ULP_EDMA2_CHAN2_IRQ	8
#define IMX8ULP_EDMA2_CHAN3_IRQ	9
#define IMX8ULP_EDMA2_CHAN4_IRQ	10
#define IMX8ULP_EDMA2_CHAN5_IRQ	11
#define IMX8ULP_EDMA2_CHAN6_IRQ	12
#define IMX8ULP_EDMA2_CHAN7_IRQ	13

/* i.MXULP uses a MUX configuration to map EDMA channels to IPs.
 * This is a list with DMAMUX configuration for IPs used by SOF
 */
#define IMX8ULP_DMAMUX2_SAI5_RX	69
#define IMX8ULP_DMAMUX2_SAI5_TX	70
#define IMX8ULP_DMAMUX2_SAI6_RX	71
#define IMX8ULP_DMAMUX2_SAI6_TX	72

#endif /* __SOF_DRIVERS_EDMA_H__ */
