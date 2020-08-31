/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright 2020 NXP
 *
 * Author: Paul Olaru <paul.olaru@nxp.com>
 */

#ifdef __SOF_DRIVERS_EDMA_H__

#ifndef __PLATFORM_DRIVERS_EDMA_H__
#define __PLATFORM_DRIVERS_EDMA_H__

#define EDMA0_ESAI_CHAN_RX	6
#define EDMA0_ESAI_CHAN_TX	7
#define EDMA0_SAI_CHAN_RX	14
#define EDMA0_SAI_CHAN_TX	15
#define EDMA0_CHAN_MAX		32

#define EDMA0_ESAI_CHAN_RX_IRQ	442
#define EDMA0_ESAI_CHAN_TX_IRQ	442
#define EDMA0_SAI_CHAN_RX_IRQ	349
#define EDMA0_SAI_CHAN_TX_IRQ	349

#endif /* __PLATFORM_DRIVERS_EDMA_H__ */

#else /* __SOF_DRIVERS_EDMA_H__ */

#error "This file shouldn't be included from outside of sof/drivers/edma.h"

#endif /* __SOF_DRIVERS_EDMA_H__ */
