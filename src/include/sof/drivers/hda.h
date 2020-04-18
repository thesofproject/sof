/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2018 Intel Corporation. All rights reserved.
 *
 * Author: Marcin Maka <marcin.maka@linux.intel.com>
 */

#ifndef __SOF_DRIVERS_HDA_H__
#define __SOF_DRIVERS_HDA_H__

#include <ipc/dai-intel.h>
#include <sof/lib/dai.h>

struct hda_pdata {
	struct sof_ipc_dai_hda_params params;
};

extern const struct dai_driver hda_driver;

#endif /* __SOF_DRIVERS_HDA_H__ */
