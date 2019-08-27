/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright 2019 NXP
 *
 * Author: Daniel Baluta <daniel.baluta@nxp.com>
 */

#ifndef __SOF_DRIVERS_ESAI_H__
#define __SOF_DRIVERS_ESAI_H__

#include <sof/lib/dai.h>

/* ESAI Register Map */
#define REG_ESAI_ETDR           0x00
#define REG_ESAI_ERDR           0x04
#define REG_ESAI_ECR            0x08
#define REG_ESAI_ESR            0x0C
#define REG_ESAI_TFCR           0x10
#define REG_ESAI_TFSR           0x14
#define REG_ESAI_RFCR           0x18
#define REG_ESAI_RFSR           0x1C
#define REG_ESAI_xFCR(tx)       (tx ? REG_ESAI_TFCR : REG_ESAI_RFCR)
#define REG_ESAI_xFSR(tx)       (tx ? REG_ESAI_TFSR : REG_ESAI_RFSR)
#define REG_ESAI_TX0            0x80
#define REG_ESAI_TX1            0x84
#define REG_ESAI_TX2            0x88
#define REG_ESAI_TX3            0x8C
#define REG_ESAI_TX4            0x90
#define REG_ESAI_TX5            0x94
#define REG_ESAI_TSR            0x98
#define REG_ESAI_RX0            0xA0
#define REG_ESAI_RX1            0xA4
#define REG_ESAI_RX2            0xA8
#define REG_ESAI_RX3            0xAC
#define REG_ESAI_SAISR          0xCC
#define REG_ESAI_SAICR          0xD0
#define REG_ESAI_TCR            0xD4
#define REG_ESAI_TCCR           0xD8
#define REG_ESAI_RCR            0xDC
#define REG_ESAI_RCCR           0xE0
#define REG_ESAI_xCR(tx)        (tx ? REG_ESAI_TCR : REG_ESAI_RCR)
#define REG_ESAI_xCCR(tx)       (tx ? REG_ESAI_TCCR : REG_ESAI_RCCR)
#define REG_ESAI_TSMA           0xE4
#define REG_ESAI_TSMB           0xE8
#define REG_ESAI_RSMA           0xEC
#define REG_ESAI_RSMB           0xF0
#define REG_ESAI_xSMA(tx)       (tx ? REG_ESAI_TSMA : REG_ESAI_RSMA)
#define REG_ESAI_xSMB(tx)       (tx ? REG_ESAI_TSMB : REG_ESAI_RSMB)
#define REG_ESAI_PRRC           0xF8
#define REG_ESAI_PCRC           0xFC

#define ESAI_ECR_ETI		BIT(19)

#define ESAI_xFCR_TIEN		BIT(19)
#define ESAI_xFCR_xWA_SHIFT	16
#define ESAI_xFCR_xWA_WIDTH	3
#define ESAI_xFCR_xWA_MASK	(((1 << ESAI_xFCR_xWA_WIDTH) - 1) << ESAI_xFCR_xWA_SHIFT)
#define ESAI_xFCR_xWA(v)	(((8 - ((v) >> 2)) << ESAI_xFCR_xWA_SHIFT) & ESAI_xFCR_xWA_MASK)
#define ESAI_xFCR_xFWM_SHIFT	8
#define ESAI_xFCR_xFWM_WIDTH	8
#define ESAI_xFCR_xFWM_MASK	(((1 << ESAI_xFCR_xFWM_WIDTH) - 1) << ESAI_xFCR_xFWM_SHIFT)
#define ESAI_xFCR_xFWM(v)	((((v) - 1) << ESAI_xFCR_xFWM_SHIFT) & ESAI_xFCR_xFWM_MASK)
#define ESAI_xFCR_xE_SHIFT	2
#define ESAI_xFCR_TE_WIDTH	6
#define ESAI_xFCR_RE_WIDTH	4
#define ESAI_xFCR_TE_MASK	(((1 << ESAI_xFCR_TE_WIDTH) - 1) << ESAI_xFCR_xE_SHIFT)
#define ESAI_xFCR_RE_MASK	(((1 << ESAI_xFCR_RE_WIDTH) - 1) << ESAI_xFCR_xE_SHIFT)
#define ESAI_xFCR_xFEN_SHIFT	0
#define ESAI_xFCR_xFEN		(1 << ESAI_xFCR_xFEN_SHIFT)
#define ESAI_xFCR_xFEN_MASK	ESAI_xFCR_xFEN
#define ESAI_xFCR_xE_SHIFT	2
#define ESAI_xFCR_TE_WIDTH	6
#define ESAI_xFCR_RE_WIDTH	4
#define ESAI_xFCR_TE_MASK	(((1 << ESAI_xFCR_TE_WIDTH) - 1) << ESAI_xFCR_xE_SHIFT)
#define ESAI_xFCR_RE_MASK	(((1 << ESAI_xFCR_RE_WIDTH) - 1) << ESAI_xFCR_xE_SHIFT)
#define ESAI_xFCR_TE(x)		((ESAI_xFCR_TE_MASK >> (ESAI_xFCR_TE_WIDTH - x)) & ESAI_xFCR_TE_MASK)
#define ESAI_xFCR_RE(x)		((ESAI_xFCR_RE_MASK >> (ESAI_xFCR_RE_WIDTH - x)) & ESAI_xFCR_RE_MASK)
#define ESAI_xFCR_xFR_SHIFT	1
#define ESAI_xFCR_xFR_MASK	(1 << ESAI_xFCR_xFR_SHIFT)
#define ESAI_xFCR_xFR		(1 << ESAI_xFCR_xFR_SHIFT)

#define ESAI_xCR_xFSR_SHIFT	16
#define ESAI_xCR_xFSR_MASK	(1 << ESAI_xCR_xFSR_SHIFT)
#define ESAI_xCR_xFSR		(1 << ESAI_xCR_xFSR_SHIFT)
#define ESAI_xCR_xE_SHIFT	0
#define ESAI_xCR_TE_WIDTH	6
#define ESAI_xCR_RE_WIDTH	4
#define ESAI_xCR_TE_MASK	(((1 << ESAI_xCR_TE_WIDTH) - 1) << ESAI_xCR_xE_SHIFT)
#define ESAI_xCR_RE_MASK	(((1 << ESAI_xCR_RE_WIDTH) - 1) << ESAI_xCR_xE_SHIFT)
#define ESAI_xCR_TE(x)		((ESAI_xCR_TE_MASK >> (ESAI_xCR_TE_WIDTH - x)) & ESAI_xCR_TE_MASK)
#define ESAI_xCR_RE(x)		((ESAI_xCR_RE_MASK >> (ESAI_xCR_RE_WIDTH - x)) & ESAI_xCR_RE_MASK)

#define ESAI_xSMA_xS_SHIFT	0
#define ESAI_xSMA_xS_WIDTH	16
#define ESAI_xSMA_xS_MASK	(((1 << ESAI_xSMA_xS_WIDTH) - 1) << ESAI_xSMA_xS_SHIFT)
#define ESAI_xSMA_xS(v)		((v) & ESAI_xSMA_xS_MASK)
#define ESAI_xSMB_xS_SHIFT	0
#define ESAI_xSMB_xS_WIDTH	16
#define ESAI_xSMB_xS_MASK	(((1 << ESAI_xSMB_xS_WIDTH) - 1) << ESAI_xSMB_xS_SHIFT)
#define ESAI_xSMB_xS(v)		(((v) >> ESAI_xSMA_xS_WIDTH) & ESAI_xSMB_xS_MASK)

#define ESAI_xCR_xIE_SHIFT	22
#define ESAI_xCR_xIE_MASK	(1 << ESAI_xCR_xIE_SHIFT)
#define ESAI_xCR_xIE		(1 << ESAI_xCR_xIE_SHIFT)
#define ESAI_xCR_xEIE_SHIFT	20
#define ESAI_xCR_xEIE_MASK	(1 << ESAI_xCR_xEIE_SHIFT)
#define ESAI_xCR_xEIE		(1 << ESAI_xCR_xEIE_SHIFT)
#define ESAI_xCR_PADC_SHIFT	17
#define ESAI_xCR_PADC_MASK	(1 << ESAI_xCR_PADC_SHIFT)
#define ESAI_xCR_PADC		(1 << ESAI_xCR_PADC_SHIFT)
#define ESAI_xCR_xFSL_SHIFT	15
#define ESAI_xCR_xFSL_MASK	(1 << ESAI_xCR_xFSL_SHIFT)
#define ESAI_xCR_xFSL		(1 << ESAI_xCR_xFSL_SHIFT)
#define ESAI_xCR_xSWS_SHIFT	10
#define ESAI_xCR_xSWS_WIDTH	5
#define ESAI_xCR_xSWS_MASK	(((1 << ESAI_xCR_xSWS_WIDTH) - 1) << ESAI_xCR_xSWS_SHIFT)
#define ESAI_xCR_xSWS(s, w)	((w < 24 ? (s - w + ((w - 8) >> 2)) : (s < 32 ? 0x1e : 0x1f)) << ESAI_xCR_xSWS_SHIFT)
#define ESAI_xCR_xWA_SHIFT	7
#define ESAI_xCR_xWA_MASK	(1 << ESAI_xCR_xWA_SHIFT)
#define ESAI_xCR_xWA		(1 << ESAI_xCR_xWA_SHIFT)

#define ESAI_xCCR_xHCKD_SHIFT	23
#define ESAI_xCCR_xHCKD_MASK	BIT(23)
#define ESAI_xCCR_xHCKD		BIT(23)
#define ESAI_xCCR_xFSD_SHIFT	22
#define ESAI_xCCR_xFSD_MASK	(1 << ESAI_xCCR_xFSD_SHIFT)
#define ESAI_xCCR_xFSD		(1 << ESAI_xCCR_xFSD_SHIFT)
#define ESAI_xCCR_xCKD_SHIFT	21
#define ESAI_xCCR_xCKD_MASK	(1 << ESAI_xCCR_xCKD_SHIFT)
#define ESAI_xCCR_xCKD		(1 << ESAI_xCCR_xCKD_SHIFT)
#define ESAI_xCCR_xHCKP_SHIFT	20
#define ESAI_xCCR_xHCKP_MASK	(1 << ESAI_xCCR_xHCKP_SHIFT)
#define ESAI_xCCR_xHCKP		(1 << ESAI_xCCR_xHCKP_SHIFT)
#define ESAI_xCCR_xFSP_SHIFT	19
#define ESAI_xCCR_xFSP_MASK	(1 << ESAI_xCCR_xFSP_SHIFT)
#define ESAI_xCCR_xFSP		(1 << ESAI_xCCR_xFSP_SHIFT)
#define ESAI_xCCR_xCKP_SHIFT	18
#define ESAI_xCCR_xCKP_MASK	(1 << ESAI_xCCR_xCKP_SHIFT)
#define ESAI_xCCR_xCKP		(1 << ESAI_xCCR_xCKP_SHIFT)
#define ESAI_xCCR_xDC_SHIFT	9
#define ESAI_xCCR_xDC_WIDTH	5
#define ESAI_xCCR_xDC_MASK	((BIT(5)-1) << 9)
#define ESAI_xCCR_xDC(v)	((((v) - 1) << ESAI_xCCR_xDC_SHIFT) & ESAI_xCCR_xDC_MASK)
#define ESAI_xCCR_xFP_SHIFT	14
#define ESAI_xCCR_xFP_WIDTH	4
#define ESAI_xCCR_xFP_MASK	(((1 << ESAI_xCCR_xFP_WIDTH) - 1) << ESAI_xCCR_xFP_SHIFT)
#define ESAI_xCCR_xFP(v)	((((v) - 1) << ESAI_xCCR_xFP_SHIFT) & ESAI_xCCR_xFP_MASK)
#define ESAI_xCCR_xPSR_SHIFT	8
#define ESAI_xCCR_xPSR_MASK	(1 << ESAI_xCCR_xPSR_SHIFT)
#define ESAI_xCCR_xPSR_BYPASS	(1 << ESAI_xCCR_xPSR_SHIFT)
#define ESAI_xCCR_xPSR_DIV8	(0 << ESAI_xCCR_xPSR_SHIFT)

#define ESAI_xMOD_MASK		((3 << 8) | (15 << 10))
#define ESAI_xMOD_NORMAL	(1 << 8)
#define ESAI_xMOD_NETWORKCH(channels) ((channels-1) << 10)

#define ESAI_PRRC_PDC_SHIFT	0
#define ESAI_PRRC_PDC_WIDTH	12
#define ESAI_PRRC_PDC_MASK	(((1 << ESAI_PRRC_PDC_WIDTH) - 1) << ESAI_PRRC_PDC_SHIFT)
#define ESAI_PRRC_PDC(v)	((v) & ESAI_PRRC_PDC_MASK)

#define ESAI_PCRC_PC_SHIFT	0
#define ESAI_PCRC_PC_WIDTH	12
#define ESAI_PCRC_PC_MASK	(((1 << ESAI_PCRC_PC_WIDTH) - 1) << ESAI_PCRC_PC_SHIFT)
#define ESAI_PCRC_PC(v)		((v) & ESAI_PCRC_PC_MASK)

#define ESAI_GPIO		0xfff

#define ESAI_IRQ		441

#define trace_esai(format, ...) trace_event(TRACE_CLASS_DAI, format, ##__VA_ARGS__)
#define tracev_esai(format, ...) tracev_event(TRACE_CLASS_DAI, format, ##__VA_ARGS__)
#define trace_esai_error(format, ...) trace_error(TRACE_CLASS_DAI, format, ##__VA_ARGS__)

extern const struct dai_driver esai_driver;
#endif /* __SOF_DRIVERS_ESAI_H__ */
