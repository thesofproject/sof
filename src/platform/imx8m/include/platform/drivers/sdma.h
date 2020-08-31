/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright 2020 NXP
 *
 * Author: Paul Olaru <paul.olaru@nxp.com>
 */

#ifdef __SOF_DRIVERS_SDMA_H__

#ifndef __PLATFORM_DRIVERS_SDMA_H__
#define __PLATFORM_DRIVERS_SDMA_H__

#define SDMA_SCRIPT_AP2AP_OFF		644
#define SDMA_SCRIPT_AP2MCU_OFF		685
#define SDMA_SCRIPT_MCU2AP_OFF		749
#define SDMA_SCRIPT_SHP2MCU_OFF		893
#define SDMA_SCRIPT_MCU2SHP_OFF		962

#endif /* __PLATFORM_DRIVERS_SDMA_H__ */

#else /* __SOF_DRIVERS_SDMA_H__ */

#error "This file shouldn't be included from outside of sof/drivers/sdma.h"

#endif /* __SOF_DRIVERS_SDMA_H__ */
