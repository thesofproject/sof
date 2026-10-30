/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 */

/**
 * \file include/sof/audio/component_dai.h
 * \brief DAI configuration helper for components
 *
 * Kept separate from sof/audio/component_ext.h because it needs the DAI
 * driver API, which most components do not use.
 */

#ifndef __SOF_AUDIO_COMPONENT_DAI_H__
#define __SOF_AUDIO_COMPONENT_DAI_H__

#include <sof/audio/component.h>
#include <sof/lib/dai.h>

#if CONFIG_IPC_MAJOR_3
/**
 * \brief Configure DAI component.
 * \param[in,out] dev Component device.
 * \param[in] config IPC DAI configuration.
 * \param[in] spec_config Spec-specific configuration data.
 * \return 0 on success, negative error code on failure.
 */
static inline int comp_dai_config(struct comp_dev *dev, struct ipc_config_dai *config,
				  const void *spec_config)
{
	struct dai_data *dd = comp_get_drvdata(dev);

	if (dev->drv->ops.dai_config)
		return dev->drv->ops.dai_config(dd, dev, config, spec_config);

	return 0;
}
#elif CONFIG_IPC_MAJOR_4
/**
 * \brief Configure DAI component (IPC4).
 * \param[in,out] dd DAI driver data.
 * \param[in,out] dev Component device.
 * \param[in] config IPC DAI configuration.
 * \param[in] spec_config Spec-specific configuration data.
 * \return 0 on success, negative error code on failure.
 */
static inline int comp_dai_config(struct dai_data *dd, struct comp_dev *dev,
				  struct ipc_config_dai *config, const void *spec_config)
{
	return dai_config(dd, dev, config, spec_config);
}
#else
#error Unknown IPC major version
#endif

#endif /* __SOF_AUDIO_COMPONENT_DAI_H__ */
