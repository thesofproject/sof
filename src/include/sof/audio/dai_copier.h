/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2023 Intel Corporation. All rights reserved.
 *
 * Author: Baofeng Tian <baofeng.tian@intel.com>
 */

/**
 * \file audio/dai_copier.h
 * \brief dai copier shared header file
 * \authors Baofeng Tian <baofeng.tian@intel.com>
 */

#ifndef __SOF_LIB_DAI_COPIER_H__
#define __SOF_LIB_DAI_COPIER_H__

int dai_zephyr_new(struct dai_data *dd, struct comp_dev *dev,
		   const struct ipc_config_dai *dai_cfg);

void dai_zephyr_free(struct dai_data *dd);

#endif /* __SOF_LIB_DAI_COPIER_H__ */
