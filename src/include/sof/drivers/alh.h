// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Slawomir Blauciak <slawomir.blauciak@linux.intel.com>

#ifndef __SOF_DRIVERS_ALH__
#define __SOF_DRIVERS_ALH__

#include <ipc/dai-intel.h>
#include <platform/drivers/alh.h>
#include <sof/lib/dai.h>

struct alh_pdata {
	struct sof_ipc_dai_alh_params params;
};

extern const struct dai_driver alh_driver;

#endif /* __SOF_DRIVERS_ALH__ */
