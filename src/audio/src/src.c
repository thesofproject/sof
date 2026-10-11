// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2024 Intel Corporation. All rights reserved.
//
// Author: Tobiasz Dryjanski <tobiaszx.dryjanski@intel.com>

/*
 * This file has been extracted from src_common.c (formerly src.c)
 * to create separation between defines in different src types.
 */

#include <rtos/init.h>
#include <sof/lib/memory.h>
#include <module/module/llext.h>

#include "src_common.h"
#include "src_config.h"

#if SRC_SHORT || CONFIG_COMP_SRC_TINY
#include "coef/src_tiny_int16_define.h"
#include "coef/src_tiny_int16_table.h"
#elif CONFIG_COMP_SRC_SMALL
#include "coef/src_small_int32_define.h"
#include "coef/src_small_int32_table.h"
#elif CONFIG_COMP_SRC_STD
#include "coef/src_std_int32_define.h"
#include "coef/src_std_int32_table.h"
#elif CONFIG_COMP_SRC_IPC4_FULL_MATRIX
#include "coef/src_ipc4_int32_define.h"
#include "coef/src_ipc4_int32_table.h"
#else
#error "No valid configuration selected for SRC"
#endif

LOG_MODULE_DECLARE(src, CONFIG_SOF_LOG_LEVEL);

static int src_prepare(struct processing_module *mod,
		       struct sof_source **sources, int num_of_sources,
		       struct sof_sink **sinks, int num_of_sinks)
{
	struct comp_data *cd = module_get_private_data(mod);
	struct src_param *a = &cd->param;
	int ret;

	comp_info(mod->dev, "entry");

	if (num_of_sources != 1 || num_of_sinks != 1)
		return -EINVAL;

	a->in_fs = src_in_fs;
	a->out_fs = src_out_fs;
	a->num_in_fs = NUM_IN_FS;
	a->num_out_fs = NUM_OUT_FS;
	a->max_fir_delay_size_xnch = (PLATFORM_MAX_CHANNELS * MAX_FIR_DELAY_SIZE);
	a->max_out_delay_size_xnch = (PLATFORM_MAX_CHANNELS * MAX_OUT_DELAY_SIZE);

	src_get_source_sink_params(mod->dev, sources[0], sinks[0]);

	ret = src_param_set(mod->dev, cd);
	if (ret < 0)
		return ret;

	ret = src_allocate_copy_stages(mod, a,
				       src_table1[a->idx_out][a->idx_in],
				       src_table2[a->idx_out][a->idx_in]);
	if (ret < 0)
		return ret;

	ret = src_params_general(mod, sources[0], sinks[0]);
	if (ret < 0)
		return ret;

	return src_prepare_general(mod, sources[0], sinks[0]);
}

static const struct module_interface src_interface = {
	.init = src_init,
	.prepare = src_prepare,
	.process = src_process,
	.is_ready_to_process = src_is_ready_to_process,
	.reset = src_reset,
	.free = src_free,
};

#if CONFIG_COMP_SRC_MODULE
/* modular: llext dynamic link */

#include <module/module/api_ver.h>
#include <module/module/llext.h>
#include <rimage/sof/user/manifest.h>

#if CONFIG_COMP_SRC_LITE
extern const struct module_interface src_lite_interface;
#endif

static const struct sof_man_module_manifest mod_manifest[] __section(".module") __used = {
	SOF_LLEXT_MODULE_MANIFEST("SRC", &src_interface, 1, SOF_REG_UUID(src4), 1),
#if CONFIG_COMP_SRC_LITE
	SOF_LLEXT_MODULE_MANIFEST("SRC_LITE", &src_lite_interface, 1, SOF_REG_UUID(src_lite), 1),
#endif
};

SOF_LLEXT_BUILDINFO;

#else

DECLARE_MODULE_ADAPTER(src_interface, SRC_UUID, src_tr);
SOF_MODULE_INIT(src, sys_comp_module_src_interface_init);

#if CONFIG_STATIC_PIPELINE
#include <sof/audio/pipeline/static_pipeline.h>

#if CONFIG_IPC_MAJOR_4
static struct comp_dev *src_static_create(const struct comp_driver *drv,
					  struct comp_ipc_config *cfg,
					  const struct sof_static_comp *cdesc,
					  uint32_t period_us)
{
	struct ipc4_config_src src_cfg;

	memset(&src_cfg, 0, sizeof(src_cfg));
	sof_static_init_base_cfg(&src_cfg.base, cdesc, period_us);
	src_cfg.sink_rate = cdesc->caps.sink_rate ? cdesc->caps.sink_rate : cdesc->caps.default_rate;

	struct ipc_config_process spec = {
		.size = sizeof(src_cfg),
		.data = (const uint8_t *)&src_cfg,
	};
	return drv->ops.create(drv, cfg, &spec);
}

static struct sof_static_module_ops src_static_ops = {
	.uuid = &SRC_UUID,
	.create = src_static_create,
};

DECLARE_STATIC_MODULE_OPS(src, &src_static_ops);
#endif /* CONFIG_IPC_MAJOR_4 */
#endif /* CONFIG_STATIC_PIPELINE */

#endif
