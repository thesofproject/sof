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
#define ESAI_ECR_ETO		BIT(18)
#define ESAI_ECR_ERI		BIT(17)
#define ESAI_ECR_ERO		BIT(16)
#define ESAI_ECR_ERST		BIT(1)
#define ESAI_ECR_ESAIEN		BIT(0)

#define ESAI_ESR_TINIT		BIT(10)
#define ESAI_ESR_RFF		BIT(9)
#define ESAI_ESR_TFE		BIT(8)
#define ESAI_ESR_TLS		BIT(7)
#define ESAI_ESR_TDE		BIT(6)
#define ESAI_ESR_TED		BIT(5)
#define ESAI_ESR_TD		BIT(4)
#define ESAI_ESR_RLS		BIT(3)
#define ESAI_ESR_RDE		BIT(2)
#define ESAI_ESR_RED		BIT(1)
#define ESAI_ESR_RD		BIT(0)

#define ESAI_xFCR_TIEN		BIT(19)
#define ESAI_xFCR_xWA_SHIFT	16
#define ESAI_xFCR_xWA_WIDTH	3
#define ESAI_xFCR_xWA_MASK	MASK(18, 16)
#define ESAI_xFCR_xWA(v)	SET_BITS(18, 16, 8 - ((v) >> 2))
#define ESAI_xFCR_xFWM_SHIFT	8
#define ESAI_xFCR_xFWM_WIDTH	8
#define ESAI_xFCR_xFWM_MASK	MASK(15, 8)
#define ESAI_xFCR_xFWM(v)	SET_BITS(15, 8, (v) - 1) // Dubious, why -1?
#define ESAI_xFCR_xE_SHIFT	2
#define ESAI_xFCR_TE_WIDTH	6
#define ESAI_xFCR_RE_WIDTH	4
#define ESAI_xFCR_TE_MASK	MASK(7, 2)
#define ESAI_xFCR_RE_MASK	MASK(5, 2)
#define ESAI_xFCR_TE(x)		SET_BITS(7, 2, MASK((x)-1, 0))
#define ESAI_xFCR_RE(x)		SET_BITS(5, 2, MASK((x)-1, 0))
#define ESAI_xFCR_xFR_SHIFT	1
#define ESAI_xFCR_xFR_MASK	BIT(1)
#define ESAI_xFCR_xFR		BIT(1)
#define ESAI_xFCR_xFEN_SHIFT	0
#define ESAI_xFCR_xFEN		BIT(0)
#define ESAI_xFCR_xFEN_MASK	BIT(0)

#define ESAI_xFSR_NTFO_MASK	MASK(14, 12)
#define ESAI_xFSR_NTFI_MASK	MASK(10, 8)
#define ESAI_xFSR_NRFO_MASK	MASK(9, 8)
#define ESAI_xFSR_NRFI_MASK	MASK(13, 12)
#define ESAI_xFSR_xFCNT_MASK	MASK(7, 0)

#define ESAI_SAISR_TODFE	BIT(17)
#define ESAI_SAISR_TEDE		BIT(16)
#define ESAI_SAISR_TDE		BIT(15)
#define ESAI_SAISR_TUE		BIT(14)
#define ESAI_SAISR_TFS		BIT(13)
#define ESAI_SAISR_RODF		BIT(10)
#define ESAI_SAISR_REDF		BIT(9)
#define ESAI_SAISR_RDF		BIT(8)
#define ESAI_SAISR_ROE		BIT(7)
#define ESAI_SAISR_RFS		BIT(6)
#define ESAI_SAISR_IF2		BIT(2)
#define ESAI_SAISR_IF1		BIT(1)
#define ESAI_SAISR_IF0		BIT(0)

#define ESAI_SAICR_ALC		BIT(8)
#define ESAI_SAICR_TEBE		BIT(7)
#define ESAI_SAICR_SYN		BIT(6)
#define ESAI_SAICR_OF2		BIT(2)
#define ESAI_SAICR_OF1		BIT(1)
#define ESAI_SAICR_OF0		BIT(0)

#define ESAI_xCR_xLIE		BIT(23)
#define ESAI_xCR_xIE		BIT(22)
#define ESAI_xCR_xEDIE		BIT(21)
#define ESAI_xCR_xEIE		BIT(20)
#define ESAI_xCR_xPR		BIT(19)
#define ESAI_xCR_PADC		BIT(17) // tx only
#define ESAI_xCR_xFSR		BIT(16)
#define ESAI_xCR_xFSL		BIT(15)
#define ESAI_xCR_xSWS_MASK	MASK(14, 10)
#define ESAI_xCR_xSWS_VAL(s, w)	((w) < 24 ? ((s) - (w) + (((w) - 8) >> 2)) : ((s) < 32 ? 0x1e : 0x1f))
#define ESAI_xCR_xSWS(s, w)	SET_BITS(14, 10, ESAI_xCR_xSWS_VAL(s, w))
#define ESAI_xCR_xMOD_MASK	MASK(9, 8)
#define ESAI_xCR_xMOD_NORMAL	SET_BITS(9, 8, 0)
#define ESAI_xCR_xMOD_NETWORK	SET_BITS(9, 8, 1)
#define ESAI_xCR_xMOD_AC97	SET_BITS(9, 8, 3)
#define ESAI_xCR_xWA		BIT(7)
#define ESAI_xCR_xSHFD		BIT(6)
#define ESAI_xCR_TE_MASK	MASK(5, 0)
#define ESAI_xCR_TE_SET(v)	SET_BITS(5, 0, v)
#define ESAI_xCR_TE(v)		ESAI_xCR_TE_SET(MASK((v) - 1, 0))
#define ESAI_xCR_RE_MASK	MASK(3, 0)
#define ESAI_xCR_RE_SET(v)	SET_BITS(3, 0, v)
#define ESAI_xCR_RE(v)		ESAI_xCR_RE_SET(MASK((v) - 1, 0))

#define ESAI_xCCR_xHCKD		BIT(23)
#define ESAI_xCCR_xFSD		BIT(22)
#define ESAI_xCCR_xCKD		BIT(21)
#define ESAI_xCCR_xHCKP		BIT(20)
#define ESAI_xCCR_xFSP		BIT(19)
#define ESAI_xCCR_xCKP		BIT(18)
#define ESAI_xCCR_xFP_MASK	MASK(17, 14)
#define ESAI_xCCR_xFP(div)	SET_BITS(17, 14, (div) - 1)
#define ESAI_xCCR_xDC_MASK	MASK(13, 9)
#define ESAI_xCCR_xDC(v)	SET_BITS(13, 9, (v) - 1)
#define ESAI_xCCR_xDC_AC97	SET_BITS(13, 9, 0x0C)
#define ESAI_xCCR_xPSR_MASK	MASK(8, 8)
#define ESAI_xCCR_xPSR_BYPASS	SET_BITS(8, 8, 1)
#define ESAI_xCCR_xPSR_DIV8	SET_BITS(8, 8, 0)
#define ESAI_xCCR_xPM_MASK	MASK(7, 0)
#define ESAI_xCCR_xPM(v)	SET_BITS(7, 0, (v) - 1)

#define ESAI_xSMA_xS_MASK	MASK(15, 0)
#define ESAI_xSMA_xS(v)		SET_BITS(15, 0, v)
#define ESAI_xSMB_xS_MASK	MASK(15, 0)
#define ESAI_xSMB_xS(v)		SET_BITS(15, 0, (v) >> 16)

#define ESAI_PRRC_PDC_MASK	MASK(11, 0)
#define ESAI_PRRC_PDC(v)	SET_BITS(11, 0, v)

#define ESAI_PCRC_PC_MASK	MASK(11, 0)
#define ESAI_PCRC_PC(v)		SET_BITS(11, 0, v)

#define ESAI_GPIO		MASK(11, 0)

#define ESAI_IRQ		441
#define EDMA_ESAI_IRQ		442

#define trace_esai(format, ...) trace_event(TRACE_CLASS_DAI, format, ##__VA_ARGS__)
#define tracev_esai(format, ...) tracev_event(TRACE_CLASS_DAI, format, ##__VA_ARGS__)
#define trace_esai_error(format, ...) trace_error(TRACE_CLASS_DAI, format, ##__VA_ARGS__)

extern const struct dai_driver esai_driver;
#endif /* __SOF_DRIVERS_ESAI_H__ */
