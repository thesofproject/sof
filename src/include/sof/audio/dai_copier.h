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

#if CONFIG_ZEPHYR_NATIVE_DRIVERS
int dai_zephyr_new(struct dai_data *dd, struct comp_dev *dev,
		   const struct ipc_config_dai *dai_cfg);

void dai_zephyr_free(struct dai_data *dd);

int dai_config_prepare(struct dai_data *dd, struct comp_dev *dev);

int dai_zephyr_prepare(struct dai_data *dd, struct comp_dev *dev);

void dai_zephyr_reset(struct dai_data *dd, struct comp_dev *dev);

int dai_zephyr_trigger(struct dai_data *dd, struct comp_dev *dev, int cmd);

int dai_playback_params(struct dai_data *dd, struct comp_dev *dev, uint32_t period_bytes,
			uint32_t period_count);

int dai_capture_params(struct dai_data *dd, struct comp_dev *dev, uint32_t period_bytes,
		       uint32_t period_count);

int dai_zephyr_params(struct dai_data *dd, struct comp_dev *dev,
		      struct sof_ipc_stream_params *params,
		      uint32_t *period_count,
		      uint32_t *period_bytes);

int dai_zephyr_copy(struct dai_data *dd, struct comp_dev *dev);
#else
static inline int dai_zephyr_new(struct dai_data *dd, struct comp_dev *dev,
				 const struct ipc_config_dai *dai_cfg)
{
	return 0;
}

static inline void dai_zephyr_free(struct dai_data *dd) {}

static inline int dai_config_prepare(struct dai_data *dd, struct comp_dev *dev)
{
	return 0;
}

static inline int dai_zephyr_prepare(struct dai_data *dd, struct comp_dev *dev)
{
	return 0;
}

static inline void dai_zephyr_reset(struct dai_data *dd, struct comp_dev *dev) {}

static inline int dai_zephyr_trigger(struct dai_data *dd, struct comp_dev *dev, int cmd)
{
	return 0;
}

static inline int dai_playback_params(struct dai_data *dd, struct comp_dev *dev,
				      uint32_t period_bytes, uint32_t period_count)
{
	return 0;
}

static inline int dai_capture_params(struct dai_data *dd, struct comp_dev *dev,
				     uint32_t period_bytes, uint32_t period_count)
{
	return 0;
}

static inline int dai_zephyr_params(struct dai_data *dd, struct comp_dev *dev,
				    struct sof_ipc_stream_params *params,
				    uint32_t *period_count,
				    uint32_t *period_bytes)
{
	return 0;
}

static inline int dai_zephyr_copy(struct dai_data *dd, struct comp_dev *dev)
{
	return 0;
}

#endif
#endif /* __SOF_LIB_DAI_COPIER_H__ */
