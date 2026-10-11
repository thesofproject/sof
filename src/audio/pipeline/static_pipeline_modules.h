// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2026 Sound Open Firmware (SOF) Project
 */

/**
 * \file audio/pipeline/static_pipeline_modules.h
 * \brief Generic internal interfaces for static pipeline module operations.
 * \author Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *
 * This private header declares internal helpers shared between the static
 * pipeline loader and module adapter implementations. It contains no
 * component-specific logic.
 */

#ifndef __SOF_AUDIO_PIPELINE_STATIC_PIPELINE_MODULES_H__
#define __SOF_AUDIO_PIPELINE_STATIC_PIPELINE_MODULES_H__

#include <sof/audio/pipeline/static_pipeline.h>
#include <module/ipc4/base-config.h>

/**
 * \brief Synthesize standard IPC4 base module configuration structure.
 * \param[out] base_cfg Target base configuration to populate.
 * \param[in] cdesc Static component descriptor with format and rate capabilities.
 * \param[in] period_us Pipeline scheduling period in microseconds.
 */
void sof_static_init_base_cfg(struct ipc4_base_module_cfg *base_cfg,
			      const struct sof_static_comp *cdesc,
			      uint32_t period_us);

/**
 * \brief Default fallback creation for standard IPC4 module adapters.
 * \param[in] drv SOF component driver.
 * \param[in] cfg IPC component configuration.
 * \param[in] cdesc Static component descriptor.
 * \param[in] period_us Owning pipeline scheduling period in microseconds.
 * \return Created comp_dev pointer, or NULL on failure.
 */
struct comp_dev *sof_static_module_create_default(const struct comp_driver *drv,
						  struct comp_ipc_config *cfg,
						  const struct sof_static_comp *cdesc,
						  uint32_t period_us);

#endif /* __SOF_AUDIO_PIPELINE_STATIC_PIPELINE_MODULES_H__ */
