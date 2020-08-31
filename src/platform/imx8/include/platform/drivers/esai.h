/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright 2020 NXP
 *
 * Author: Paul Olaru <paul.olaru@nxp.com>
 */

#ifdef __SOF_DRIVERS_ESAI_H__

#ifndef __PLATFORM_DRIVERS_ESAI_H__
#define __PLATFORM_DRIVERS_ESAI_H__

#define EDMA_ESAI_IRQ		442

#define EDMA_ESAI_TX_CHAN	7
#define EDMA_ESAI_RX_CHAN	6

#endif /* __PLATFORM_DRIVERS_ESAI_H__ */

#else /* __SOF_DRIVERS_ESAI_H__ */

#error "This file shouldn't be included from outside of sof/drivers/esai.h"

#endif /* __SOF_DRIVERS_ESAI_H__ */
