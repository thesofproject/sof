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

struct ipc_config_dai;
struct comp_dev;
struct dai_data;
int dai_zephyr_new(struct dai_data *dd, struct comp_dev *dev,
		   const struct ipc_config_dai *dai_cfg);

void dai_zephyr_free(struct dai_data *dd);

int dai_zephyr_config_prepare(struct dai_data *dd, struct comp_dev *dev);

int dai_zephyr_prepare(struct dai_data *dd, struct comp_dev *dev);

void dai_zephyr_reset(struct dai_data *dd, struct comp_dev *dev);

int dai_zephyr_trigger(struct dai_data *dd, struct comp_dev *dev, int cmd);

int dai_zephyr_position(struct dai_data *dd, struct comp_dev *dev,
			struct sof_ipc_stream_posn *posn);

int dai_zephyr_params(struct dai_data *dd, struct comp_dev *dev,
		      struct sof_ipc_stream_params *params);

int dai_zephyr_copy(struct dai_data *dd, struct comp_dev *dev, pcm_converter_func *converter);

int dai_zephyr_ts_config_op(struct dai_data *dd, struct comp_dev *dev);

int dai_zephyr_ts_start(struct dai_data *dd, struct comp_dev *dev);

int dai_zephyr_ts_stop(struct dai_data *dd, struct comp_dev *dev);

int dai_zephyr_ts_get(struct dai_data *dd, struct comp_dev *dev, struct timestamp_data *tsd);

int dai_zephyr_get_hw_params(struct dai_data *dd, struct comp_dev *dev,
			     struct sof_ipc_stream_params *params, int dir);

int dai_zephyr_multi_endpoint_copy(struct dai_data **dd, struct comp_dev *dev,
				   struct comp_buffer *multi_endpoint_buffer,
				   int num_endpoints);

int dai_zephyr_unbind(struct dai_data *dd, struct comp_dev *dev, void *data);

#endif /* __SOF_LIB_DAI_COPIER_H__ */
