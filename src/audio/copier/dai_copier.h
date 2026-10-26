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
struct ipc4_copier_module_cfg;
struct copier_data;

#if CONFIG_LIBRARY
static inline int dai_common_new(struct dai_data *dd, struct comp_dev *dev,
				 const struct ipc_config_dai *dai_cfg)
{
	return 0;
}

static inline void dai_common_free(struct dai_data *dd) {}

static inline int dai_common_config_prepare(struct dai_data *dd, struct comp_dev *dev)
{
	return 0;
}

static inline int dai_common_prepare(struct dai_data *dd, struct comp_dev *dev)
{
	return 0;
}

static inline void dai_common_reset(struct dai_data *dd, struct comp_dev *dev) {}

static inline int dai_common_trigger(struct dai_data *dd, struct comp_dev *dev, int cmd)
{
	return 0;
}

static inline int dai_common_position(struct dai_data *dd, struct comp_dev *dev,
				      struct sof_ipc_stream_posn *posn)
{
	return 0;
}

static inline int dai_common_params(struct dai_data *dd, struct comp_dev *dev,
				    struct sof_ipc_stream_params *params)
{
	return 0;
}

static inline int dai_common_copy(struct dai_data *dd, struct comp_dev *dev,
				  pcm_converter_func *converter)
{
	return 0;
}

static inline int dai_common_ts_config_op(struct dai_data *dd, struct comp_dev *dev)
{
	return 0;
}

static inline int dai_common_ts_start(struct dai_data *dd, struct comp_dev *dev)
{
	return 0;
}

static inline int dai_common_ts_stop(struct dai_data *dd, struct comp_dev *dev)
{
	return 0;
}

static inline int dai_common_ts_get(struct dai_data *dd, struct comp_dev *dev,
				    struct timestamp_data *tsd)
{
	return 0;
}

static inline int dai_common_get_hw_params(struct dai_data *dd, struct comp_dev *dev,
					   struct sof_ipc_stream_params *params, int dir)
{
	return 0;
}

static inline int dai_zephyr_multi_endpoint_copy(struct dai_data **dd, struct comp_dev *dev,
						 struct comp_buffer *multi_endpoint_buffer,
						 int num_endpoints)
{
	return 0;
}

static inline int dai_zephyr_unbind(struct dai_data *dd, struct comp_dev *dev, const void *data)
{
	return 0;
}

static inline int copier_dai_create(struct comp_dev *dev, struct copier_data *cd,
				    const struct ipc4_copier_module_cfg *copier,
				    struct pipeline *pipeline)
{
	return 0;
}

static inline void copier_dai_free(struct processing_module *mod) {}

static inline int copier_dai_prepare(struct comp_dev *dev, struct copier_data *cd)
{
	return 0;
}

static inline void copier_dai_adjust_params(const struct copier_data *cd,
					    struct ipc4_audio_format *in_fmt,
					    struct ipc4_audio_format *out_fmt) {}

static inline int copier_dai_params(struct copier_data *cd, struct comp_dev *dev,
				    struct sof_ipc_stream_params *params, int dai_index)
{
	return 0;
}

static inline void copier_dai_reset(struct copier_data *cd, struct comp_dev *dev) {}

static inline int copier_dai_trigger(struct copier_data *cd, struct comp_dev *dev, int cmd)
{
	return 0;
}
#else
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

#if CONFIG_ZEPHYR_NATIVE_DRIVERS
int dai_common_ts_get(struct dai_data *dd, struct comp_dev *dev, struct dai_ts_data *tsd);
#else
int dai_common_ts_get(struct dai_data *dd, struct comp_dev *dev, struct timestamp_data *tsd);
#endif

int dai_common_get_hw_params(struct dai_data *dd, struct comp_dev *dev,
			     struct sof_ipc_stream_params *params, int dir);

int dai_zephyr_multi_endpoint_copy(struct dai_data **dd, struct comp_dev *dev,
				   struct comp_buffer *multi_endpoint_buffer,
				   int num_endpoints);
int dai_zephyr_unbind(struct dai_data *dd, struct comp_dev *dev,
		      const struct bind_info *unbind_data);

int copier_dai_create(struct comp_dev *dev, struct copier_data *cd,
		      const struct ipc4_copier_module_cfg *copier,
		      struct pipeline *pipeline);

void copier_dai_free(struct processing_module *mod);

int copier_dai_prepare(struct comp_dev *dev, struct copier_data *cd);

void copier_dai_adjust_params(const struct copier_data *cd,
			      struct ipc4_audio_format *in_fmt,
			      struct ipc4_audio_format *out_fmt);

int copier_dai_params(struct copier_data *cd, struct comp_dev *dev,
		      struct sof_ipc_stream_params *params, int dai_index);

void copier_dai_reset(struct copier_data *cd, struct comp_dev *dev);

int copier_dai_trigger(struct copier_data *cd, struct comp_dev *dev, int cmd);
#endif
#endif /* __SOF_LIB_DAI_COPIER_H__ */
