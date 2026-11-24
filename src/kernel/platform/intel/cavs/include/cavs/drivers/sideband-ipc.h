/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 *         Rander Wang <rander.wang@intel.com>
 */

/**
 * \file platform/intel/cavs/include/cavs/drivers/sideband-ipc.h
 * \brief Defines layout of IPC registers for cAVS platforms with sideband IPC.
 */

#ifndef __CAVS_DRIVERS_SIDEBAND_IPC_H__
#define __CAVS_DRIVERS_SIDEBAND_IPC_H__

#include <rtos/bit.h>

/* DIPCIDR */
#define IPC_DIPCIDR_BUSY	BIT(31)
#define IPC_DIPCIDR_MSG_MASK	MASK(30, 0)
#endif
