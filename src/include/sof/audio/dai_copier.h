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
/**
 * \brief dai new through dai_data
 */
int dai_zephyr_new(struct dai_data *dd, struct comp_dev *dev,
		   const struct ipc_config_dai *dai_cfg);

/**
 * \brief dai free through dai_data
 */
void dai_zephyr_free(struct dai_data *dd, struct comp_dev *dev);

/**
 * \brief dai config prepare through dai_data
 */
int dai_config_prepare(struct dai_data *dd, struct comp_dev *dev);

/**
 * \brief dai config prepare through dai_data
 */
int dai_zephyr_prepare(struct dai_data *dd, struct comp_dev *dev);

/**
 * \brief dai zephyr reset
 */
void dai_zephyr_reset(struct dai_data *dd, struct comp_dev *dev);

/**
 * \brief dai zephyr reset
 */
int dai_zephyr_trigger(struct dai_data *dd, struct comp_dev *dev, int cmd);

/**
 * \brief dai playback params
 */
int dai_playback_params(struct dai_data *dd, struct comp_dev *dev, uint32_t period_bytes,
			uint32_t period_count);

/**
 * \brief dai capture params
 */
int dai_capture_params(struct dai_data *dd, struct comp_dev *dev, uint32_t period_bytes,
		       uint32_t period_count);

/**
 * \brief dai zephyr params
 */
int dai_zephyr_params(struct dai_data *dd, struct comp_dev *dev,
		      struct sof_ipc_stream_params *params,
		      uint32_t *period_count,
		      uint32_t *period_bytes,
		      struct list_item *bsource_list,
		      struct list_item *bsink_list);
#else
/**
 * \brief dai new through dai_data
 */
static inline int dai_zephyr_new(struct dai_data *dd, struct comp_dev *dev,
				 const struct ipc_config_dai *dai_cfg)
{
	return 0;
}

/**
 * \brief dai free through dai_data
 */
static inline void dai_zephyr_free(struct dai_data *dd, struct comp_dev *dev) {}

/**
 * \brief dai config prepare through dai_data
 */
static inline int dai_config_prepare(struct dai_data *dd, struct comp_dev *dev)
{
	return 0;
}

/**
 * \brief dai config prepare through dai_data
 */
static inline int dai_zephyr_prepare(struct dai_data *dd, struct comp_dev *dev)
{
	return 0;
}

/**
 * \brief dai zephyr reset
 */
static inline void dai_zephyr_reset(struct dai_data *dd, struct comp_dev *dev) {}

/**
 * \brief dai zephyr reset
 */
static inline int dai_zephyr_trigger(struct dai_data *dd, struct comp_dev *dev, int cmd)
{
	return 0;
}

/**
 * \brief dai playback params
 */
static inline int dai_playback_params(struct dai_data *dd, struct comp_dev *dev,
				      uint32_t period_bytes, uint32_t period_count)
{
	return 0;
}

/**
 * \brief dai capture params
 */
static inline int dai_capture_params(struct dai_data *dd, struct comp_dev *dev,
				     uint32_t period_bytes, uint32_t period_count)
{
	return 0;
}

/**
 * \brief dai zephyr params
 */
static inline int dai_zephyr_params(struct dai_data *dd, struct comp_dev *dev,
				    struct sof_ipc_stream_params *params,
				    uint32_t *period_count,
				    uint32_t *period_bytes,
				    struct list_item *bsource_list,
				    struct list_item *bsink_list)
{
	return 0;
}

#endif
#endif /* __SOF_LIB_DAI_COPIER_H__ */
