/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright 2019 NXP
 *
 * Author: Daniel Baluta <daniel.baluta@nxp.com>
 */

#ifndef __SOF_DRIVERS_EDMA_H__
#define __SOF_DRIVERS_EDMA_H__

#define EDMA_CH_CSR                     0x00
#define EDMA_CH_ES                      0x04
#define EDMA_CH_INT                     0x08
#define EDMA_CH_SBR                     0x0C
#define EDMA_CH_PRI                     0x10
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

/* Enable round robin, disable everything else, globally */
#define EDMA_DEFAULT_GLOBAL_CSR		0x00000004
/* Cancel current operation and halt other operations */
#define EDMA_GLOBAL_CSR_HALT		0x00000210
/* Source size and destination size 4 bytes; no modulo */
#define EDMA_DEFAULT_TCD_ATTR		0x00000202

/* Values to be put in SOFF, DOFF; transfers are multiple of 4 always */
#define EDMA_TRANSFER_OFFSET_MEM	4
#define EDMA_TRANSFER_OFFSET_DEV	0

/* Offset from controller to first ch */

int edma_init(void);

#define EDMA_HS_GET_IRQ(hs) (((hs) & MASK(8, 0)) >> 0)
#define EDMA_HS_SET_IRQ(irq) SET_BITS(8, 0, irq)
#define EDMA_HS_GET_CHAN(hs) (((hs) & MASK(13, 9)) >> 9)
#define EDMA_HS_SET_CHAN(chan) SET_BITS(13, 9, chan)
#define EDMA_HANDSHAKE(irq, channel) (EDMA_HS_SET_CHAN(channel) | EDMA_HS_SET_IRQ(irq))

#define trace_edma(format, ...) trace_event(TRACE_CLASS_DMA, format, ##__VA_ARGS__)
#define tracev_edma(format, ...) tracev_event(TRACE_CLASS_DMA, format, ##__VA_ARGS__)
#define trace_edma_error(format, ...) trace_error(TRACE_CLASS_DMA, format, ##__VA_ARGS__)

#endif /* __SOF_DRIVERS_EDMA_H__ */
