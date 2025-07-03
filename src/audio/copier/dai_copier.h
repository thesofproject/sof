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
int dai_common_new(struct dai_data *dd, struct comp_dev *dev,
		   const struct ipc_config_dai *dai_cfg);

void dai_common_free(struct dai_data *dd);

int dai_common_config_prepare(struct dai_data *dd, struct comp_dev *dev);

int dai_common_prepare(struct dai_data *dd, struct comp_dev *dev);

void dai_common_reset(struct dai_data *dd, struct comp_dev *dev);

int dai_common_trigger(struct dai_data *dd, struct comp_dev *dev, int cmd);

int dai_common_position(struct dai_data *dd, struct comp_dev *dev,
			struct sof_ipc_stream_posn *posn);

int dai_common_params(struct dai_data *dd, struct comp_dev *dev,
		      struct sof_ipc_stream_params *params);

int dai_common_copy(struct dai_data *dd, struct comp_dev *dev, pcm_converter_func *converter);

int dai_common_ts_config_op(struct dai_data *dd, struct comp_dev *dev);

int dai_common_ts_start(struct dai_data *dd, struct comp_dev *dev);

int dai_common_ts_stop(struct dai_data *dd, struct comp_dev *dev);

int dai_common_ts_get(struct dai_data *dd, struct comp_dev *dev, struct dai_ts_data *tsd);

int dai_common_get_hw_params(struct dai_data *dd, struct comp_dev *dev,
			     struct sof_ipc_stream_params *params, int dir);

#if CONFIG_LIBRARY
static inline int dai_zephyr_multi_endpoint_copy(struct dai_data **dd, struct comp_dev *dev,
						 struct comp_buffer *multi_endpoint_buffer,
						 int num_endpoints)
{
	return 0;
}

static inline int dai_zephyr_unbind(struct dai_data *dd, struct comp_dev *dev, void *data)
{
	return 0;
}
#else
int dai_zephyr_multi_endpoint_copy(struct dai_data **dd, struct comp_dev *dev,
				   struct comp_buffer *multi_endpoint_buffer,
				   int num_endpoints);
int dai_zephyr_unbind(struct dai_data *dd, struct comp_dev *dev, struct bind_info *unbind_data);
#endif


struct ipc4_copier_module_cfg;
struct copier_data;
int copier_dai_create(struct comp_dev *dev, struct copier_data *cd,
		      const struct ipc4_copier_module_cfg *copier,
		      struct pipeline *pipeline);

void copier_dai_free(struct copier_data *cd);

int copier_dai_prepare(struct comp_dev *dev, struct copier_data *cd);

void copier_dai_adjust_params(const struct copier_data *cd,
			      struct ipc4_audio_format *in_fmt,
			      struct ipc4_audio_format *out_fmt);

int copier_dai_params(struct copier_data *cd, struct comp_dev *dev,
		      struct sof_ipc_stream_params *params, int dai_index);

void copier_dai_reset(struct copier_data *cd, struct comp_dev *dev);

int copier_dai_trigger(struct copier_data *cd, struct comp_dev *dev, int cmd);
#endif /* __SOF_LIB_DAI_COPIER_H__ */
