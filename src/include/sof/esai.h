/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright 2019 NXP
 *
 * Author: Daniel Baluta <daniel.baluta@nxp.com>
 */

#ifndef __SOF_ESAI_H__
#define __SOF_ESAI_H__

#include <sof/dai.h>
#include <sof/io.h>

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

extern const struct dai_driver esai_driver;
#endif /* __SOF_ESAI_H__ */
